// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// GltfRenderLayer material state: the material mapping (glTF material -> renderer material, ASOBO / KHR extension
// defines and uniforms, textures and samplers), material-only updates and model texture release.

#include "GltfRenderLayerInternal.h"

namespace {
// ASOBO_material_invisible (MSFS SDK schema, resources/schemas/asobo): the material is not
// rendered when the extension is declared, present and "enabled" (its default is true).
bool isAsoboInvisible(const gltf::asobo::ExtensionSet& extensions)
{
    const gltf::asobo::ExtensionData* invisible = extensions.find("ASOBO_material_invisible");
    return invisible != nullptr && invisible->boolean("enabled").value_or(true);
}

// ASOBO_material_environment_occluder: a volume around the cabin that hides MSFS rain, snow
// and vegetation inside it (docs/ASOBO_GBUFFER_STUDY.md 4a). It is never meant to be seen,
// and Glider draws no weather or vegetation to occlude, so it is hidden like an invisible
// material.
bool isAsoboEnvironmentOccluder(const gltf::asobo::ExtensionSet& extensions)
{
    const gltf::asobo::ExtensionData* occluder = extensions.find("ASOBO_material_environment_occluder");
    return occluder != nullptr && occluder->boolean("enabled").value_or(true);
}

// A1.4: the material classes ASOBO_material_draw_order affects. Decals keep their own
// offset order; glass and windshield add the offset to the distance key.
DrawOrderClass asoboDrawOrderClass(const gltf::asobo::ExtensionSet& extensions)
{
    const auto has = [&extensions](const char* name) { return extensions.find(name) != nullptr; };
    const auto decalEnabled = [&extensions](const char* name) {
        const gltf::asobo::ExtensionData* ext = extensions.find(name);
        return ext != nullptr && ext->boolean("enabled").value_or(true);
    };
    if (decalEnabled("ASOBO_material_geometry_decal") || decalEnabled("ASOBO_material_blend_gbuffer")) {
        return DrawOrderClass::Decal;
    }
    if (has("ASOBO_material_glass_v2") || has("ASOBO_material_glass") || has("ASOBO_material_kitty_glass")
        || has("ASOBO_material_windshield_v3") || has("ASOBO_material_windshield")) {
        return DrawOrderClass::Glass;
    }
    return DrawOrderClass::None;
}

// The MSFS SDK range; the extension schema itself does not constrain the number.
int asoboDrawOrderOffset(const gltf::asobo::ExtensionSet& extensions)
{
    const gltf::asobo::ExtensionData* drawOrder = extensions.find("ASOBO_material_draw_order");
    if (drawOrder == nullptr) {
        return 0;
    }
    const double value = drawOrder->number("drawOrderOffset").value_or(0.0);
    return static_cast<int>(std::lround(std::clamp(value, -999.0, 999.0)));
}

// A2.2: the material-type extensions the MSFS exporter writes (docs/ASOBO_EXTENSIONS_REFERENCE
// section 3). Options such as UV_options, draw_order or emissive are not types; a standard
// material with only those is still a receiver.
constexpr std::string_view kAsoboMaterialTypeExtensions[] = {
    "ASOBO_material_geometry_decal", "ASOBO_material_blend_gbuffer", "ASOBO_material_clear_coat",
    "ASOBO_material_clear_coat_v2", "ASOBO_material_iridescent", "ASOBO_material_pearlescent",
    "ASOBO_material_anisotropic", "ASOBO_material_anisotropic_v2", "ASOBO_material_detail_map",
    "ASOBO_material_glass_v2", "ASOBO_material_glass", "ASOBO_material_kitty_glass",
    "ASOBO_material_windshield", "ASOBO_material_windshield_v3",
    "ASOBO_material_fresnel_fade", "ASOBO_material_ghost_effect", "ASOBO_material_SSS",
    "ASOBO_material_sail", "ASOBO_material_parallax_window", "ASOBO_material_wind_flex",
    "ASOBO_material_fake_terrain",
};

// The five receiver classes of the SDK (Standard has no type extension). Everything else
// ignores the decal buffer, including glass, windshields, MASK and BLEND materials.
bool isAsoboDecalReceiver(const gltf::Material& gltfMaterial, const gltf::asobo::ExtensionSet* extensions,
                          DrawOrderClass drawOrderClass, bool transmissionApplied = true)
{
    if (drawOrderClass == DrawOrderClass::Decal) {
        return false;
    }
    if (gltfMaterial.getAlphaModeOrDefault() != gltf::AlphaMode::Opaque) {
        return false;
    }
    if (transmissionApplied && gltfMaterial.getTransmission().has_value()) {
        return false;
    }
    if (extensions == nullptr) {
        return true;
    }
    for (const std::string_view type : kAsoboMaterialTypeExtensions) {
        if (extensions->find(type) == nullptr) {
            continue;
        }
        if (type == "ASOBO_material_clear_coat" || type == "ASOBO_material_clear_coat_v2"
            || type == "ASOBO_material_anisotropic" || type == "ASOBO_material_anisotropic_v2"
            || type == "ASOBO_material_fake_terrain" || type == "ASOBO_material_SSS") {
            return true;
        }
        return false;
    }
    return true;
}

// Keeps an unsupported compressed KTX2 texture visually the same as the previous failed
// decode (opaque white) without leaving an incomplete GL texture behind.
const image::Image& whiteTexturePlaceholder()
{
    static const image::Image image = []() {
        image::Image placeholder;
        const uint8_t white[4] = { 255, 255, 255, 255 };
        placeholder.initializeLayout(8, 8, 8, 8, image::Format::NORM, false, false, false, false, 0, 1, 2, 3);
        placeholder.initialize(white, sizeof(white), 1, 1, 1, 1, 1, 1);
        return placeholder;
    }();
    return image;
}
}

void GltfRenderLayer::initMaterialMapping(gl::GL* gl)
{
    // Materials
    auto setMaterialDefaults = [this](renderer::Material* material) {
        material->samplers.clear();
        material->defines.clear();

        material->setUniform<float>("u_OcclusionStrength", 1.f);
        material->setUniform<float>("u_NormalScale", 1.f);

        // base material defaults
        material->setUniform<math::vec3>("u_EmissiveFactor", math::vec3(0.f, 0.f, 0.f));
        material->setUniform<float>("u_EmissiveDayMultiplier", 1.0f);
        material->setUniform<float>("u_EmissiveNightMultiplier", 1.0f);

        material->defines.define("ALPHAMODE_OPAQUE", static_cast<int>(gltf::AlphaMode::Opaque));
        material->defines.define("ALPHAMODE_MASK", static_cast<int>(gltf::AlphaMode::Mask));
        material->defines.define("ALPHAMODE_BLEND", static_cast<int>(gltf::AlphaMode::Blend));
        material->defines.define("ALPHAMODE", static_cast<int>(gltf::AlphaMode::Mask));
        material->setUniform<float>("u_AlphaCutoff", 0.0f); // default alpha mode is OPAQUE so cutoff needs to be disabled in our shader

        material->defines.define("MATERIAL_METALLICROUGHNESS");
        material->renderPass = m_resourceManager.getOpaquePass();

        // MR defaults
        material->setUniform<float>("u_MetallicFactor", 1.f);
        material->setUniform<float>("u_RoughnessFactor", 1.f);
        material->setUniform<math::vec4>("u_BaseColorFactor", math::vec4(1.f));

        // Specular Glossiness defaults
        material->setUniform<math::vec3>("u_SpecularFactor", math::vec3(1.f));
        material->setUniform<math::vec4>("u_DiffuseFactor", math::vec4(1.f));
        material->setUniform<float>("u_GlossinessFactor", 1.f);

        // clear coat defaults
        material->setUniform<float>("u_ClearcoatFactor", 0.f);
        material->setUniform<float>("u_ClearcoatRoughnessFactor", 0.f);

        // specular defaults
        material->setUniform<math::vec3>("u_KHR_materials_specular_specularColorFactor", math::vec3(1.0f));
        material->setUniform<float>("u_KHR_materials_specular_specularFactor", 1.f);

        // sheen defaults
        material->setUniform<float>("u_SheenRoughnessFactor", 0.f);
        material->setUniform<math::vec3>("u_SheenColorFactor", math::vec3(0.f));

        // IOR default
        material->setUniform<float>("u_Ior", 1.5f);

        // transmission defaults
        material->setUniform<float>("u_TransmissionFactor", 0.f);

        // volume defaults
        material->setUniform<float>("u_ThicknessFactor", 0.f);
        material->setUniform<float>("u_AttenuationDistance", 0.f);
        material->setUniform<math::vec3>("u_AttenuationColor", math::vec3(0.f));

        // iridescence defaults
        material->setUniform<float>("u_IridescenceFactor", 0.f);
        material->setUniform<float>("u_IridescenceIor", 1.3f);
        material->setUniform<float>("u_IridescenceThicknessMinimum", 100.f);
        material->setUniform<float>("u_IridescenceThicknessMaximum", 400.f);

        // anisotropy defaults (KHR_materials_anisotropy: rotation 0, strength 0)
        material->setUniform<math::vec3>("u_Anisotropy", math::vec3(1.f, 0.f, 0.f));

        if (m_resourceManager.getFramebufferTexture() != nullptr && m_resourceManager.getFramebufferSamplerState() != nullptr) {
            material->defines.define("HAS_OPAQUE_FRAMEBUFFER");
            material->samplers.emplace_back("u_TransmissionFramebufferSampler", m_resourceManager.getFramebufferTexture(), m_resourceManager.getFramebufferSamplerState());
        } else {
            material->defines.undefine("HAS_OPAQUE_FRAMEBUFFER");
        }
    };

    m_materialMapping.updateRendererobjectFromGltf = [this, setMaterialDefaults, gl](const gltf::Material* gltfMaterial, renderer::Material* material) -> bool {
        setMaterialDefaults(material);
        if (gltfMaterial == nullptr) {
            return false;
        }
        material->gltfMaterialId = gltfMaterial->m_ID;
        material->nonTransmissivePass = gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Blend
            ? static_cast<RenderPass*>(m_resourceManager.getTransparentPass()) : m_resourceManager.getOpaquePass();
        material->asobo.reset();
        material->asoboMaterialCode = Material::AsoboMaterialCode::None;
        const auto extras = nlohmann::json::parse(gltfMaterial->getExtras(), nullptr, false);
        if (extras.is_object()) {
            const auto code = extras.find("ASOBO_material_code");
            if (code != extras.end() && code->is_string()) {
                if (*code == "Glass") {
                    material->asoboMaterialCode = Material::AsoboMaterialCode::Glass;
                } else if (*code == "Windshield") {
                    material->asoboMaterialCode = Material::AsoboMaterialCode::Windshield;
                }
            }
        }
        if (m_syncingGltf != nullptr) {
            auto extensions = gltf::asobo::parseExtensions(*m_syncingGltf, *gltfMaterial, gltf::asobo::Scope::Material);
            if (!extensions.empty()) {
                material->asobo = std::make_shared<const gltf::asobo::ExtensionSet>(std::move(extensions));
            }
        }
        material->invisible = material->asobo != nullptr && isAsoboInvisible(*material->asobo);
        material->environmentOccluder = material->asobo != nullptr && isAsoboEnvironmentOccluder(*material->asobo);
        material->drawOrderClass = material->asobo != nullptr ? asoboDrawOrderClass(*material->asobo) : DrawOrderClass::None;
        if (material->drawOrderClass == DrawOrderClass::None
            && material->asoboMaterialCode != Material::AsoboMaterialCode::None) {
            material->drawOrderClass = DrawOrderClass::Glass;
        }
        material->drawOrderOffset = material->asobo != nullptr ? asoboDrawOrderOffset(*material->asobo) : 0;
        material->drawOrderActive = false;
        material->decalBiasActive = false;
        material->samplers.clear();
        material->defines.clear();

        // A2.2: only OPAQUE materials of a file that carries decals, and only the five
        // receiver classes, compile the decal-reader variant. The per-viewport uniform
        // guards the actual reads, so the define itself is asset-level and cache-stable.
        material->decalReceiverWithoutTransmission = m_assetHasDecalMaterials
            && material->asoboMaterialCode == Material::AsoboMaterialCode::None
            && isAsoboDecalReceiver(*gltfMaterial, material->asobo.get(), material->drawOrderClass, false);
        if (material->decalReceiverWithoutTransmission && !gltfMaterial->getTransmission().has_value()) {
            material->defines.define("ASOBO_DECAL_RECEIVER");
        }

        // A1.12: the same declaration selects the packaged texture convention: normal-map
        // X/Y and the COMP R channel are square-root encoded. Decoding is per material so
        // undeclared files keep the plain glTF sampling path.
        if (m_assetOptimized) {
            material->defines.define("ASOBO_ASSET_OPTIMIZED");
        }

        // A4.2: windshield defines are gated dynamically per-viewport in createScene().

        material->setUniform<float>("u_AlphaCutoff", gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Mask ? static_cast<float>(gltfMaterial->getAlphaCutoffOrDefault()) : 0.f);

        // Select renderpass for transparent, transmissive, or opaque material
        if ((gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Mask || gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Opaque) && gltfMaterial->getTransmission().has_value() == false) {
            material->renderPass = m_resourceManager.getOpaquePass();
        }

        if (gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Blend && gltfMaterial->getTransmission().has_value() == false) {
            material->renderPass = m_resourceManager.getTransparentPass();
        }

        material->defines.define("ALPHAMODE_OPAQUE", static_cast<int>(gltf::AlphaMode::Opaque));
        material->defines.define("ALPHAMODE_MASK", static_cast<int>(gltf::AlphaMode::Mask));
        material->defines.define("ALPHAMODE_BLEND", static_cast<int>(gltf::AlphaMode::Blend));
        material->defines.define("ALPHAMODE", static_cast<int>(gltfMaterial->getAlphaModeOrDefault()));

        // A1.7: screen-door transparency for OPAQUE materials (the files write core OPAQUE, or the
        // exporter's literal "DITHER", read as OPAQUE) that declare ASOBO_material_alphamode_dither and do
        // not switch it off with {"enabled": false}. They stay in the opaque pass: no sorting, no blending,
        // depth writes on. Whether the viewport applies it is a frame-block flag, so the variant is
        // viewport-independent and a viewport with the switch off renders the core OPAQUE result.
        if (gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Opaque
            && !gltfMaterial->getTransmission().has_value()) {
            const gltf::asobo::ExtensionData* dither = material->asobo != nullptr
                ? material->asobo->find("ASOBO_material_alphamode_dither") : nullptr;
            // The extension's own "enabled" decides when it is present; a bare core "DITHER" dithers too.
            const bool applies = dither != nullptr ? dither->boolean("enabled").value_or(true)
                                                   : gltfMaterial->isAlphaModeDither();
            if (applies) {
                material->defines.define("ASOBO_ALPHA_DITHER");
            }
        }

        if (gltfMaterial->getTransmission().has_value()) {
            material->renderPass = m_resourceManager.getTransmissivePass();
        }

        material->isDoubleSided = gltfMaterial->isDoubleSidedOrDefault();

        // Creates a sampler with a given name from one glTF texture. It is only created if
        // the texture and its image are present. Returns true if it was actually created.
        auto createSamplerFromTexture = [this, material, gl](const std::string& baseName, const gltf::Texture* gltfTexture, uint32_t texCoord, bool isLinear) -> bool {
            if (m_syncingGltf != nullptr && m_syncingGltf->imagesOmitted()) return false;
            std::string capitalized;
            for (size_t i = 0; i < baseName.length(); ++i) {
                char c = baseName[i];
                if (i > 0 && std::isupper(c)) {
                    capitalized.push_back('_');
                }
                capitalized.push_back(std::toupper(c));
            }
            const std::string samplerEnabledDefine = "HAS_" + capitalized + "_MAP";

            if (gltfTexture == nullptr) {
                return false;
            }

            bool ddsDeclared = m_syncingGltf != nullptr && m_syncingGltf->isExtensionDeclared("MSFT_texture_dds");
            const gltf::Image* chosenImage = pickImageForTexture(gltfTexture, ddsDeclared);
            auto texIt = m_textureMapping.map.find({ chosenImage, isLinear });
            if (texIt == m_textureMapping.map.end() || texIt->second == nullptr) {
                // fall back to standard texture source if chosen image was not found
                if (chosenImage != gltfTexture->getSource()) {
                    texIt = m_textureMapping.map.find({ gltfTexture->getSource(), isLinear });
                }
                if (texIt == m_textureMapping.map.end() || texIt->second == nullptr) {
                    return false;
                }
            }
            renderer::gl::Texture* glTexture = texIt->second;
            const gltf::Image* gltfImage = std::get<const gltf::Image*>(texIt->first);

            // Packaged MSFS 2024 textures are BCn KTX2. Upload the block data and all mip
            // levels as they are; only an unsupported vkFormat or supercompression falls back.
            const core::Vector<uint8_t>* encodedBytes = gltfImage->getEncodedBytes();
            imageformats::KTX2ImageInfo ktx2Info;
            const bool ktx2Compressed = encodedBytes != nullptr
                && imageformats::getKTX2ImageInfo(*encodedBytes, ktx2Info)
                && ktx2Info.blockCompressed;
            const bool ktx2Supported = ktx2Compressed && imageformats::isSupportedCompressedKTX2(ktx2Info);
            if (ktx2Compressed) {
                if (ktx2Supported && ktx2Info.format == gpu::Format::BC5_SNORM_BLOCK) {
                    if (baseName == "Normal") {
                        material->defines.define("NORMAL_TEXTURE_RG_SNORM");
                    } else if (baseName == "ClearcoatNormal") {
                        material->defines.define("CLEARCOAT_NORMAL_TEXTURE_RG_SNORM");
                    } else if (baseName == "DetailNormal") {
                        // A5.1: the detail normal follows the base normal's decode exactly.
                        material->defines.define("DETAIL_NORMAL_TEXTURE_RG_SNORM");
                    } else if (baseName == "ScratchesNormal") {
                        material->defines.define("SCRATCHES_NORMAL_TEXTURE_RG_SNORM");
                    }
                }
                if (!ktx2Supported) {
                    core::logWarning("[KTX2] '%s': unsupported vkFormat %u (supercompression %u); using a white placeholder",
                        gltfImage->getUri().c_str(), static_cast<uint32_t>(ktx2Info.format), ktx2Info.supercompressionScheme);
                }
            }

            const renderer::gl::SamplerState* samplerState = nullptr;
            auto samplerIt = m_samplerStateMapping.map.find(gltfTexture->getSampler());
            if (samplerIt != m_samplerStateMapping.map.end()) {
                samplerState = samplerIt->second;
            }

            if (glTexture->currentMapID != gltfImage->encodedMapID) {
                glTexture->currentMapID = gltfImage->encodedMapID;
                if (ktx2Compressed) {
                    if (!ktx2Supported || !glTexture->writeCompressed(gltfImage->getData(), ktx2Info, isLinear)) {
                        glTexture->write(whiteTexturePlaceholder(), isLinear);
                    }
                } else {
                    glTexture->write(gltfImage->getData(), isLinear);
                    // Texture::write already generated the mip chain. Repeating it for
                    // each material using the texture serialized large 8K uploads.
                }
            }

            material->samplers.emplace_back("u_" + baseName + "Sampler", glTexture, samplerState);
            material->setUniform<uint32_t>("u_" + baseName + "UVSet", texCoord);
            material->defines.define(samplerEnabledDefine);

            return true;
        };

        // Creates a sampler with a given name from the given textureInfo and wires its
        // KHR_texture_transform (including animation pointer targets when the caller names
        // the property). The sampler is only created if the given texture is not null.
        auto createSampler = [this, gltfMaterial, material, &createSamplerFromTexture](const std::string& baseName, const std::optional<gltf::TextureInfo>& texture, const char* texturePointer, bool isLinear = true) -> bool {
            if (texture.has_value() == false) {
                return false;
            }

            const gltf::TextureInfo& info = texture.value();
            if (createSamplerFromTexture(baseName, info.getTexture(), info.getEffectiveTexCoord(), isLinear) == false) {
                return false;
            }

            if (info.getTextureTransform()) {
                std::string uniformName = "u_" + baseName + "UVTransform";
                material->setUniform<math::mat3>(uniformName, info.calculateTransformMatrix());

                auto transform = std::make_shared<gltf::TextureTransformExtension>(*info.getTextureTransform());

                auto setupAnimatedTextureTransform = [this, gltfMaterial, material, uniformName, transform](std::string propertyPrefix) {
                    m_animationPointerTargets[{ gltfMaterial, propertyPrefix + "/extensions/KHR_texture_transform/offset" }].callback = KHRAnimationPointerTarget::Callback<glm::vec2> {
                        [material, uniformName, transform](const glm::vec2& value) -> void {
                            transform->setOffset(value);
                            material->setUniform<glm::mat3>(uniformName, transform->calculateTransformMatrix());
                        }
                    };
                    m_animationPointerTargets[{ gltfMaterial, propertyPrefix + "/extensions/KHR_texture_transform/scale" }].callback = KHRAnimationPointerTarget::Callback<glm::vec2> {
                        [material, uniformName, transform](const glm::vec2& value) -> void {
                            transform->setScale(value);
                            material->setUniform<glm::mat3>(uniformName, transform->calculateTransformMatrix());
                        }
                    };
                    m_animationPointerTargets[{ gltfMaterial, propertyPrefix + "/extensions/KHR_texture_transform/rotation" }].callback = KHRAnimationPointerTarget::Callback<float> {
                        [material, uniformName, transform](const float& value) -> void {
                            transform->setRotation(value);
                            material->setUniform<glm::mat3>(uniformName, transform->calculateTransformMatrix());
                        }
                    };
                };

                if (texturePointer != nullptr) {
                    setupAnimatedTextureTransform(texturePointer);
                }

                std::string upperBaseName = baseName;
                std::transform(baseName.begin(), baseName.end(), upperBaseName.begin(), ::toupper);
                material->defines.define("HAS_" + upperBaseName + "_UV_TRANSFORM");
            }

            return true;
        };

        createSampler("Normal", gltfMaterial->getNormalTextureInfo(), "normalTexture");
        createSampler("Emissive", gltfMaterial->getEmissiveTextureInfo(), "emissiveTexture", false);

        // A1.10: the MSFS package tool drops occlusionTexture from ASOBO_asset_optimized files
        // whose exporter wrote the ORM ("COMP") texture into both slots. Read occlusion from
        // the R channel of metallicRoughnessTexture instead: same texture, UV set and transform,
        // strength 1. Files without the declaration are unchanged.
        std::optional<gltf::TextureInfo> occlusionTexture;
        if (gltfMaterial->getOcclusionTextureInfo().has_value()) {
            occlusionTexture.emplace(gltfMaterial->getOcclusionTextureInfo().value());
        } else if (m_assetOptimized) {
            const auto& metallicRoughnessInfo = gltfMaterial->getPbrMetallicRoughness();
            if (metallicRoughnessInfo.has_value() && metallicRoughnessInfo->getMetallicRoughnessTextureInfo().has_value()) {
                occlusionTexture.emplace(metallicRoughnessInfo->getMetallicRoughnessTextureInfo().value());
                occlusionTexture->unsetScaleOrStrength(); // occlusion strength defaults to 1
            }
        }
        createSampler("Occlusion", occlusionTexture, "occlusionTexture");

        // A5.1: ASOBO_material_detail_map. Its four textures are optional and each goes
        // through the same sampler path as the core textures (DDS preference, KTX2 block
        // upload, BC5 SNORM decode), so detail normals decode exactly like the base normal.
        // Only a material that actually carries a detail layer compiles the variant;
        // whether the viewport samples it is a frame-block uniform (like A1.2/A4).
        // On a windshield the same slots are the windshield layers, not a detail map (MSFS SDK,
        // FlightSim material textures): detail colour = masks (R details 1, G icing, B details 2),
        // detail OMR = detail reflection, detail normal = icing normal. Icing is a weather state
        // and the detail layers belong to the windshield shading (A4), so the generic A5.1
        // operators never apply to windshield v3, windshield v1 or material code "Windshield".
        // A1.5: ASOBO_occlusion_strength replaces the core occlusion strength (on windshields it is
        // the reflection-mask strength, which acts through the same occlusion term).
        if (const gltf::asobo::ExtensionData* occlusionStrength = material->asobo != nullptr
                ? material->asobo->find("ASOBO_occlusion_strength") : nullptr) {
            if (occlusionStrength->number("strength").has_value()) {
                material->defines.define("ASOBO_OCCLUSION_STRENGTH");
                material->setUniform<float>("u_AsoboOcclusionStrength",
                                            static_cast<float>(*occlusionStrength->number("strength")));
            }
        }

        // A4.4: view-angle opacity of BLEND materials. The SDK documents both extensions in words only
        // (docs/ASOBO_EXTENSIONS_SEMANTICS.md section 13); values are clamped to the exporter ranges.
        // Whether the viewport applies them is a frame-block flag, so the variant stays viewport-independent.
        if (gltfMaterial->getAlphaModeOrDefault() == gltf::AlphaMode::Blend && material->asobo != nullptr) {
            if (const gltf::asobo::ExtensionData* fresnel = material->asobo->find("ASOBO_material_fresnel_fade")) {
                // fresnelFactor defaults to the exporter's 1.0; an absent offset subtracts nothing.
                material->defines.define("ASOBO_FRESNEL_FADE");
                material->setUniform<math::vec2>("u_AsoboFresnelFade", math::vec2(
                    static_cast<float>(std::clamp(fresnel->number("fresnelFactor").value_or(1.0), 0.001, 100.0)),
                    static_cast<float>(std::clamp(fresnel->number("fresnelOpacityOffset").value_or(0.0), -1.0, 1.0))));
            }
            if (const gltf::asobo::ExtensionData* ghost = material->asobo->find("ASOBO_material_ghost_effect")) {
                material->defines.define("ASOBO_GHOST_EFFECT");
                material->setUniform<math::vec3>("u_AsoboGhost", math::vec3(
                    static_cast<float>(std::clamp(ghost->number("bias").value_or(0.0), 0.0, 1.0)),
                    static_cast<float>(std::clamp(ghost->number("scale").value_or(1.0), 0.0, 1.0)),
                    static_cast<float>(std::clamp(ghost->number("power").value_or(1.0), 0.001, 64.0))));
            }
        }

        // A5.2: declared, typed extension only. Defaults stay on the original shader variant.
        if (const gltf::asobo::ExtensionData* uv = material->asobo != nullptr
                ? material->asobo->find("ASOBO_material_UV_options") : nullptr) {
            const math::vec2 tiling(static_cast<float>(uv->number("UVTilingU").value_or(1.0)),
                                    static_cast<float>(uv->number("UVTilingV").value_or(1.0)));
            const math::vec2 offset(static_cast<float>(uv->number("UVOffsetU").value_or(0.0)),
                                    static_cast<float>(uv->number("UVOffsetV").value_or(0.0)));
            const float rotation = static_cast<float>(uv->number("UVRotation").value_or(0.0));
            const bool clampU = uv->boolean("clampUVX").value_or(false);
            const bool clampV = uv->boolean("clampUVY").value_or(false);
            if (tiling != math::vec2(1.0f) || offset != math::vec2(0.0f) || rotation != 0.0f || clampU || clampV) {
                material->defines.define("ASOBO_MATERIAL_UV_OPTIONS");
                material->setUniform<math::vec2>("u_AsoboUVTiling", tiling);
                material->setUniform<math::vec2>("u_AsoboUVOffset", offset);
                const float angle = math::radians(rotation);
                material->setUniform<math::vec2>("u_AsoboUVRotation", math::vec2(std::cos(angle), std::sin(angle)));
                material->setUniform<math::vec2>("u_AsoboUVClamp", math::vec2(clampU ? 1.f : 0.f, clampV ? 1.f : 0.f));
            }
            // The typed parser prefers the exporter spelling. Report an authored conflict
            // once per process, including material refreshes caused by Inspector edits.
            static bool reportedOffsetAlias = false;
            if (!reportedOffsetAlias) {
                const auto& extensions = gltfMaterial->getUnknownExtensions();
                const auto entry = extensions.find("ASOBO_material_UV_options");
                if (entry != extensions.end()) {
                    const auto json = nlohmann::json::parse(entry->second.c_str(), nullptr, false);
                    if (json.is_object() && ((json.contains("UVOffsetU") && json.contains("UVOffSetU"))
                        || (json.contains("UVOffsetV") && json.contains("UVOffSetV")))) {
                        core::logWarning("[ASOBO UV] Both offset spellings are present; UVOffsetU/V takes precedence over UVOffSetU/V");
                        reportedOffsetAlias = true;
                    }
                }
            }
        }

        const bool windshieldMaterial = material->asoboMaterialCode == Material::AsoboMaterialCode::Windshield
            || (material->asobo != nullptr && (material->asobo->find("ASOBO_material_windshield_v3") != nullptr
                                               || material->asobo->find("ASOBO_material_windshield") != nullptr));
        if (const gltf::asobo::ExtensionData* detailMap = material->asobo != nullptr && !windshieldMaterial
                ? material->asobo->find("ASOBO_material_detail_map") : nullptr) {
            auto createDetailSampler = [this, detailMap, &createSamplerFromTexture](const char* key, const char* baseName, bool isLinear) -> bool {
                const gltf::asobo::TextureRef* reference = detailMap->texture(key);
                if (reference == nullptr || m_syncingGltf == nullptr || reference->index < 0
                    || static_cast<size_t>(reference->index) >= m_syncingGltf->getTextures().size()) {
                    return false;
                }
                const gltf::Texture* gltfTexture =
                    &m_syncingGltf->getTextures().at(static_cast<size_t>(reference->index));
                return createSamplerFromTexture(baseName, gltfTexture, static_cast<uint32_t>(reference->texCoord), isLinear);
            };
            const bool hasColor = createDetailSampler("detailColorTexture", "DetailColor", false);
            const bool hasNormal = createDetailSampler("detailNormalTexture", "DetailNormal", true);
            const bool hasOrm = createDetailSampler("detailMetalRoughAOTexture", "DetailOrm", true);
            const bool hasBlendMask = createDetailSampler("blendMaskTexture", "DetailBlendMask", true);
            if (hasColor || hasNormal || hasOrm) {
                material->defines.define("ASOBO_MATERIAL_DETAIL_MAP");
                material->defines.defineIf(hasBlendMask, "ASOBO_DETAIL_BLEND_MASK");
                material->setUniform<float>("u_AsoboDetailUVScale", static_cast<float>(detailMap->number("UVScale").value_or(1.0)));
                material->setUniform<float>("u_AsoboDetailBlendThreshold", static_cast<float>(detailMap->number("blendThreshold").value_or(0.001)));
                // Neither the schema nor the exporter defines a detail UV offset; A5.1 applies
                // one after UVScale. A number offsets both axes, a two-element array U and V.
                math::vec2 detailUVOffset(0.0f);
                const nlohmann::json& unknownKeys = detailMap->unknownKeys();
                const auto offset = unknownKeys.find("UVOffset");
                if (offset != unknownKeys.end()) {
                    if (offset->is_number()) {
                        detailUVOffset = math::vec2(offset->get<float>());
                    } else if (offset->is_array() && offset->size() == 2 && (*offset)[0].is_number() && (*offset)[1].is_number()) {
                        detailUVOffset = math::vec2((*offset)[0].get<float>(), (*offset)[1].get<float>());
                    }
                }
                material->setUniform<math::vec2>("u_AsoboDetailUVOffset", detailUVOffset);
                const gltf::asobo::TextureRef* detailNormal = detailMap->texture("detailNormalTexture");
                material->setUniform<float>("u_AsoboDetailNormalScale",
                    static_cast<float>(detailNormal != nullptr ? detailNormal->scale : 1.0));
            }
        } else if (windshieldMaterial) {
            const gltf::asobo::ExtensionData* windshieldV3 = material->asobo != nullptr
                ? material->asobo->find("ASOBO_material_windshield_v3") : nullptr;
            const gltf::asobo::ExtensionData* detailMap = material->asobo != nullptr
                ? material->asobo->find("ASOBO_material_detail_map") : nullptr;

            auto createAsoboSampler = [this, &createSamplerFromTexture](const gltf::asobo::ExtensionData* ext, const char* key, const char* baseName, bool isLinear) -> bool {
                if (ext == nullptr) return false;
                const gltf::asobo::TextureRef* reference = ext->texture(key);
                if (reference == nullptr || m_syncingGltf == nullptr || reference->index < 0
                    || static_cast<size_t>(reference->index) >= m_syncingGltf->getTextures().size()) {
                    return false;
                }
                const gltf::Texture* gltfTexture =
                    &m_syncingGltf->getTextures().at(static_cast<size_t>(reference->index));
                return createSamplerFromTexture(baseName, gltfTexture, static_cast<uint32_t>(reference->texCoord), isLinear);
            };

            createAsoboSampler(detailMap, "detailColorTexture", "DetailColor", false);
            createAsoboSampler(detailMap, "detailMetalRoughAOTexture", "DetailOrm", true);
            bool hasNormal = createAsoboSampler(detailMap, "detailNormalTexture", "DetailNormal", true);
            if (!hasNormal && windshieldV3 != nullptr) {
                createAsoboSampler(windshieldV3, "windshieldDetailNormalTexture", "DetailNormal", true);
            }
            if (windshieldV3 != nullptr) {
                createAsoboSampler(windshieldV3, "scratchesNormalTexture", "ScratchesNormal", true);
            }

            float uvScale = 1.0f;
            // Only an authored windshield UVScale wins; its schema default (1) must not hide the detail map's tiling.
            if (windshieldV3 != nullptr && windshieldV3->isExplicit("UVScale") && windshieldV3->number("UVScale").has_value()) {
                uvScale = static_cast<float>(*windshieldV3->number("UVScale"));
            } else if (detailMap != nullptr && detailMap->number("UVScale").has_value()) {
                uvScale = static_cast<float>(*detailMap->number("UVScale"));
            }
            material->setUniform<float>("u_WindshieldUVScale", uvScale);

            float normalScale = 1.0f;
            if (detailMap != nullptr && detailMap->texture("detailNormalTexture") != nullptr) {
                normalScale = static_cast<float>(detailMap->texture("detailNormalTexture")->scale);
            } else if (windshieldV3 != nullptr && windshieldV3->texture("windshieldDetailNormalTexture") != nullptr) {
                normalScale = static_cast<float>(windshieldV3->texture("windshieldDetailNormalTexture")->scale);
            }
            material->setUniform<float>("u_WindshieldDetailNormalScale", normalScale);

            float detail1Rough = 0.0f;
            float detail1Opacity = 0.0f;
            float detail2Rough = 0.0f;
            float detail2Opacity = 0.0f;
            float microScratchesTiling = 1.0f;
            float microScratchesStrength = 0.0f;

            if (windshieldV3 != nullptr) {
                detail1Rough = static_cast<float>(windshieldV3->number("detail1Rough").value_or(0.0));
                detail1Opacity = static_cast<float>(windshieldV3->number("detail1Opacity").value_or(0.0));
                detail2Rough = static_cast<float>(windshieldV3->number("detail2Rough").value_or(0.0));
                detail2Opacity = static_cast<float>(windshieldV3->number("detail2Opacity").value_or(0.0));
                microScratchesTiling = static_cast<float>(windshieldV3->number("microScratchesTiling").value_or(1.0));
                // The exporter writes only non-default values; its strength default is 1.0
                // (semantics §13), and it only matters when a scratches normal texture exists.
                microScratchesStrength = static_cast<float>(windshieldV3->number("microScratchesStrength").value_or(1.0));
            }
            // MSFS 2020 legacy windshields (v1 / material code) carry no detail parameters and the
            // SDK documents no defaults, so they keep the exporter's v3 defaults: detail 1/2 rough
            // and opacity 0 (the masks change nothing) and no micro-scratch texture.

            material->setUniform<float>("u_WindshieldDetail1Rough", detail1Rough);
            material->setUniform<float>("u_WindshieldDetail1Opacity", detail1Opacity);
            material->setUniform<float>("u_WindshieldDetail2Rough", detail2Rough);
            material->setUniform<float>("u_WindshieldDetail2Opacity", detail2Opacity);
            material->setUniform<float>("u_MicroScratchesTiling", microScratchesTiling);
            material->setUniform<float>("u_MicroScratchesStrength", microScratchesStrength);
        }

        // A2.3: the geometry decal's own blendMaskTexture (not the detail map's key of the same
        // name). Only the decal accumulation shader reads it, and only for mode "blendMasked"
        // (DecalAccumulationLayer). The sampler is created for any mode so an Inspector mode
        // edit needs no texture reload; the HAS_ define is removed again so the forward (A1.4
        // fallback) program variant stays exactly the one it was before A2.3.
        if (const gltf::asobo::ExtensionData* geometryDecal = material->asobo != nullptr
                ? material->asobo->find("ASOBO_material_geometry_decal") : nullptr) {
            const gltf::asobo::TextureRef* reference = geometryDecal->texture("blendMaskTexture");
            if (reference != nullptr && m_syncingGltf != nullptr && reference->index >= 0
                && static_cast<size_t>(reference->index) < m_syncingGltf->getTextures().size()) {
                const gltf::Texture* gltfTexture = &m_syncingGltf->getTextures().at(static_cast<size_t>(reference->index));
                if (createSamplerFromTexture("DecalBlendMask", gltfTexture, static_cast<uint32_t>(reference->texCoord), true)) {
                    material->defines.undefine("HAS_DECAL_BLEND_MASK_MAP");
                }
            }
        }

        material->setUniform<math::vec3>("u_EmissiveFactor", gltfMaterial->getEmissiveFactorOrDefault());
        float dayMultiplier = 1.0f;
        float nightMultiplier = 1.0f;
        if (material->asobo != nullptr) {
            if (const gltf::asobo::ExtensionData* emissiveExt = material->asobo->find("ASOBO_material_emissive")) {
                dayMultiplier = static_cast<float>(emissiveExt->number("emissiveDayMultiplier").value_or(1.0));
                nightMultiplier = static_cast<float>(emissiveExt->number("emissiveNightMultiplier").value_or(1.0));
            }
        }
        material->setUniform<float>("u_EmissiveDayMultiplier", dayMultiplier);
        material->setUniform<float>("u_EmissiveNightMultiplier", nightMultiplier);
        material->setUniform<float>("u_NormalScale", gltfMaterial->getNormalTextureInfo().has_value() ? static_cast<float>(gltfMaterial->getNormalTextureInfo()->getScale()) : 1.f);
        material->setUniform<float>("u_OcclusionStrength", occlusionTexture.has_value() ? static_cast<float>(occlusionTexture->getScale()) : 1.f);

        setUniformAsAnimationPointerTarget<float>(gltfMaterial, "alphaCutoff", material, "u_AlphaCutoff");
        setUniformAsAnimationPointerTarget<glm::vec3>(gltfMaterial, "emissiveFactor", material, "u_EmissiveFactor");
        setUniformAsAnimationPointerTarget<float>(gltfMaterial, "normalTexture/scale", material, "u_NormalScale");
        setUniformAsAnimationPointerTarget<float>(gltfMaterial, "occlusionTexture/strength", material, "u_OcclusionStrength");

        const auto& metallicRoughness = gltfMaterial->getPbrMetallicRoughness();
        if (metallicRoughness.has_value()) {
            createSampler("BaseColor", metallicRoughness->getBaseColorTextureInfo(), "pbrMetallicRoughness/baseColorTexture", false);
            createSampler("MetallicRoughness", metallicRoughness->getMetallicRoughnessTextureInfo(), "pbrMetallicRoughness/metallicRoughnessTexture");
            material->setUniform<float>("u_MetallicFactor", static_cast<float>(metallicRoughness->getMetallicFactorOrDefault()));
            material->setUniform<float>("u_RoughnessFactor", static_cast<float>(metallicRoughness->getRoughnessFactorOrDefault()));
            material->setUniform<math::vec4>("u_BaseColorFactor", metallicRoughness->getBaseColorFactorOrDefault());

            setUniformAsAnimationPointerTarget<glm::vec4>(gltfMaterial, "pbrMetallicRoughness/baseColorFactor", material, "u_BaseColorFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "pbrMetallicRoughness/metallicFactor", material, "u_MetallicFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "pbrMetallicRoughness/roughnessFactor", material, "u_RoughnessFactor");
        }

        const auto& specularGlossiness = gltfMaterial->getPbrSpecularGlossiness();
        if (specularGlossiness.has_value()) {
            createSampler("SpecularGlossiness", specularGlossiness->getSpecularGlossinessTextureInfo(), nullptr, false);
            createSampler("Diffuse", specularGlossiness->getDiffuseTextureInfo(), nullptr, false);
            material->setUniform<float>("u_GlossinessFactor", static_cast<float>(specularGlossiness->getGlossinessFactorOrDefault()));
            material->setUniform<math::vec3>("u_SpecularFactor", specularGlossiness->getSpecularFactorOrDefault());
            material->setUniform<math::vec4>("u_DiffuseFactor", specularGlossiness->getDiffuseFactorOrDefault());
        }

        const auto& clearCoat = gltfMaterial->getClearcoat();
        if (clearCoat.has_value()) {
            createSampler("Clearcoat", clearCoat->getClearcoatTexture(), "extensions/KHR_materials_clearcoat/clearcoatTexture");
            createSampler("ClearcoatRoughness", clearCoat->getClearcoatRoughnessTexture(), "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture");
            createSampler("ClearcoatNormal", clearCoat->getClearcoatNormalTexture(), "extensions/KHR_materials_clearcoat/clearcoatNormalTexture");
            material->setUniform<float>("u_ClearcoatNormalScale", clearCoat->getClearcoatNormalTexture().has_value() ? static_cast<float>(clearCoat->getClearcoatNormalTexture()->getScale()) : 1.f);
            material->setUniform<float>("u_ClearcoatFactor", static_cast<float>(clearCoat->getClearcoatFactorOrDefault()));
            material->setUniform<float>("u_ClearcoatRoughnessFactor", static_cast<float>(clearCoat->getClearcoatRoughnessFactorOrDefault()));

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatFactor", material, "u_ClearcoatFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessFactor", material, "u_ClearcoatRoughnessFactor");
        }

        const auto& specular = gltfMaterial->getSpecular();
        if (specular.has_value()) {
            material->setUniform<math::vec3>("u_KHR_materials_specular_specularColorFactor", specular->getSpecularColorFactorOrDefault());
            material->setUniform<float>("u_KHR_materials_specular_specularFactor", static_cast<float>(specular->getSpecularFactorOrDefault()));
            createSampler("Specular", specular->getSpecularTextureInfo(), "extensions/KHR_materials_specular/specularTexture");
            createSampler("SpecularColor", specular->getSpecularColorTextureInfo(), "extensions/KHR_materials_specular/specularColorTexture", false);

            setUniformAsAnimationPointerTarget<glm::vec3>(gltfMaterial, "extensions/KHR_materials_specular/specularColorFactor", material, "u_KHR_materials_specular_specularColorFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_specular/specularFactor", material, "u_KHR_materials_specular_specularFactor");
        }

        const auto& emissiveStrength = gltfMaterial->getEmissiveStrength();
        if (emissiveStrength.has_value()) {
            material->defines.define("MATERIAL_EMISSIVE_STRENGTH");
            material->setUniform<float>("u_EmissiveStrength", emissiveStrength->getEmissiveStrengthOrDefault());

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_emissive_strength/emissiveStrength", material, "u_EmissiveStrength");
        }

        const auto& sheen = gltfMaterial->getSheen();
        if (sheen.has_value()) {
            createSampler("SheenColor", sheen->getSheenColorTextureInfo(), "extensions/KHR_materials_sheen/sheenColorTexture", false);
            createSampler("SheenRoughness", sheen->getSheenRoughnessTextureInfo(), "extensions/KHR_materials_sheen/sheenRoughnessTexture");
            material->setUniform<float>("u_SheenRoughnessFactor", static_cast<float>(sheen->getSheenRoughnessFactorOrDefault()));
            material->setUniform<glm::vec3>("u_SheenColorFactor", sheen->getSheenColorFactorOrDefault());

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessFactor", material, "u_SheenRoughnessFactor");
            setUniformAsAnimationPointerTarget<glm::vec3>(gltfMaterial, "extensions/KHR_materials_sheen/sheenColorFactor", material, "u_SheenColorFactor");
        }

        const auto& ior = gltfMaterial->getIor();
        if (ior.has_value() && renderSettings.enableIOR) {
            material->setUniform<float>("u_Ior", static_cast<float>(ior->getIorOrDefault()));

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_ior/ior", material, "u_Ior");
        }

        const auto& transmission = gltfMaterial->getTransmission();
        if (transmission.has_value()) {
            createSampler("Transmission", transmission->getTransmissionTextureInfo(), "extensions/KHR_materials_transmission/transmissionTexture");
            material->setUniform<float>("u_TransmissionFactor", static_cast<float>(transmission->getTransmissionFactorOrDefault()));

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_transmission/transmissionFactor", material, "u_TransmissionFactor");
        }

        const auto& volume = gltfMaterial->getVolume();
        if (volume.has_value()) {
            createSampler("Thickness", volume->getThicknessTextureInfo(), "extensions/KHR_materials_volume/thicknessTexture");
            material->setUniform<float>("u_ThicknessFactor", static_cast<float>(volume->getThicknessFactorOrDefault()));
            material->setUniform<float>("u_AttenuationDistance", static_cast<float>(volume->getAttenuationDistance().value_or(0)));
            material->setUniform<math::vec3>("u_AttenuationColor", volume->getAttenuationColorOrDefault());

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_volume/thicknessFactor", material, "u_ThicknessFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_volume/attenuationDistance", material, "u_AttenuationDistance");
            setUniformAsAnimationPointerTarget<glm::vec3>(gltfMaterial, "extensions/KHR_materials_volume/attenuationColor", material, "u_AttenuationColor");
        }

        const auto& iridescence = gltfMaterial->getIridescence();
        if (iridescence.has_value()) {
            createSampler("Iridescence", iridescence->getIridescenceTextureInfo(), "extensions/KHR_materials_iridescence/iridescenceTexture");
            createSampler("IridescenceThickness", iridescence->getIridescenceThicknessTextureInfo(), "extensions/KHR_materials_iridescence/iridescenceThicknessTexture");
            material->setUniform<float>("u_IridescenceFactor", static_cast<float>(iridescence->getIridescenceFactorOrDefault()));
            material->setUniform<float>("u_IridescenceIor", static_cast<float>(iridescence->getIridescenceIorOrDefault()));
            material->setUniform<float>("u_IridescenceThicknessMinimum", static_cast<float>(iridescence->getIridescenceThicknessMinimumOrDefault()));
            material->setUniform<float>("u_IridescenceThicknessMaximum", static_cast<float>(iridescence->getIridescenceThicknessMaximumOrDefault()));

            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceFactor", material, "u_IridescenceFactor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceIor", material, "u_IridescenceIor");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessMaximum", material, "u_IridescenceThicknessMaximum");
            setUniformAsAnimationPointerTarget<float>(gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessMinimum", material, "u_IridescenceThicknessMinimum");
        }

        // A7.1: KHR_materials_anisotropy. A strength of 0 (the default) compiles no anisotropic code,
        // so it renders byte-identically to the material without the extension; the texture B channel
        // can only lower the strength. ASOBO_material_anisotropic_v2 maps onto the same lobe when its
        // direction/roughness-Y texture is bound (without it the second roughness is unknown and the
        // material stays isotropic); createScene applies its per-viewport switch.
        // A texture whose image failed to load (the loader's placeholder) is not used: its colour would
        // read as a direction and a strength / roughness Y.
        material->khrAnisotropy = false;
        material->asoboAnisotropy = false;
        auto anisotropyImageUsable = [this](const gltf::Texture* gltfTexture) {
            const bool ddsDeclared = m_syncingGltf != nullptr && m_syncingGltf->isExtensionDeclared("MSFT_texture_dds");
            const gltf::Image* image = pickImageForTexture(gltfTexture, ddsDeclared);
            return image != nullptr && !image->isErrorImage();
        };
        const auto& anisotropy = gltfMaterial->getAnisotropy();
        if (anisotropy.has_value()) {
            const auto& anisotropyTexture = anisotropy->getAnisotropyTextureInfo();
            if (anisotropyTexture.has_value() && anisotropyImageUsable(anisotropyTexture->getTexture())) {
                createSampler("Anisotropy", anisotropyTexture, "extensions/KHR_materials_anisotropy/anisotropyTexture");
            }
            const double rotation = anisotropy->getAnisotropyRotationOrDefault();
            const double strength = std::clamp(anisotropy->getAnisotropyStrengthOrDefault(), 0.0, 1.0);
            material->setUniform<math::vec3>("u_Anisotropy", math::vec3(static_cast<float>(std::cos(rotation)),
                static_cast<float>(std::sin(rotation)), static_cast<float>(strength)));
            material->khrAnisotropy = strength > 0.0;
        } else if (const gltf::asobo::ExtensionData* asoboAnisotropy = material->asobo != nullptr
                       ? material->asobo->find("ASOBO_material_anisotropic_v2") : nullptr) {
            const gltf::asobo::TextureRef* reference = asoboAnisotropy->texture("anisoDirectionRoughnessTexture");
            if (reference != nullptr && m_syncingGltf != nullptr && reference->index >= 0
                && static_cast<size_t>(reference->index) < m_syncingGltf->getTextures().size()) {
                const gltf::Texture* gltfTexture = &m_syncingGltf->getTextures().at(static_cast<size_t>(reference->index));
                material->asoboAnisotropy = anisotropyImageUsable(gltfTexture) && createSamplerFromTexture("Anisotropy",
                    gltfTexture, static_cast<uint32_t>(std::max(0, reference->texCoord)), true);
            }
        }

        // A3.4: ASOBO_material_SSS on OPAQUE / MASK materials (BLEND, transmissive and unlit ones keep the
        // core shading). SSSColor is linear RGB(A), clamped to 0..1; alpha is ignored. The exporters default it
        // to white, and the typed model rejects a 2-item list (the schema's length) with a warning, so it
        // falls back to white too. opacityTexture.r is an opacity: the transmission weight is 1 - r
        // (docs/ASOBO_EXTENSIONS_SEMANTICS.md section 18). A failed image (the loader's placeholder) is not
        // bound, so it does not read as opaque.
        material->asoboSss = false;
        if (const gltf::asobo::ExtensionData* sss = material->asobo != nullptr
                ? material->asobo->find("ASOBO_material_SSS") : nullptr;
            sss != nullptr && gltfMaterial->getAlphaModeOrDefault() != gltf::AlphaMode::Blend
            && !gltfMaterial->getTransmission().has_value() && !gltfMaterial->isUnlit()) {
            math::vec3 sssColor(1.0f);
            if (const std::vector<double>* color = sss->numbers("SSSColor"); color != nullptr && color->size() >= 3) {
                for (int c = 0; c < 3; ++c) {
                    sssColor[c] = static_cast<float>(std::clamp((*color)[static_cast<size_t>(c)], 0.0, 1.0));
                }
            }
            material->setUniform<math::vec3>("u_AsoboSSSColor", sssColor);
            const gltf::asobo::TextureRef* opacity = sss->texture("opacityTexture");
            if (opacity != nullptr && m_syncingGltf != nullptr && opacity->index >= 0
                && static_cast<size_t>(opacity->index) < m_syncingGltf->getTextures().size()) {
                const gltf::Texture* gltfTexture = &m_syncingGltf->getTextures().at(static_cast<size_t>(opacity->index));
                if (anisotropyImageUsable(gltfTexture)) {
                    createSamplerFromTexture("AsoboSssOpacity", gltfTexture, static_cast<uint32_t>(std::max(0, opacity->texCoord)), true);
                }
            }
            material->asoboSss = true;
        }

        if (m_resourceManager.getFramebufferTexture() != nullptr && m_resourceManager.getFramebufferSamplerState() != nullptr) {
            material->defines.define("HAS_OPAQUE_FRAMEBUFFER");
            material->samplers.emplace_back("u_TransmissionFramebufferSampler", m_resourceManager.getFramebufferTexture(), m_resourceManager.getFramebufferSamplerState());

            math::ivec2 framebufferSize;
            gl->bindTexture(GL_TEXTURE_2D, m_resourceManager.getFramebufferTexture()->handle);
            gl->getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &framebufferSize.x);
            gl->getTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &framebufferSize.y);

            material->setUniform<math::ivec2>("u_TransmissionFramebufferSize", framebufferSize);

        } else {
            material->defines.undefine("HAS_OPAQUE_FRAMEBUFFER");
        }

        material->defines.defineIf(true, "MATERIAL_METALLICROUGHNESS"); // Always set as fallback
        material->defines.defineIf(specularGlossiness.has_value(), "MATERIAL_SPECULARGLOSSINESS");
        material->defines.defineIf(gltfMaterial->isUnlit(), "MATERIAL_UNLIT");
        material->defines.defineIf(clearCoat.has_value(), "MATERIAL_CLEARCOAT");
        material->defines.defineIf(specular.has_value(), "MATERIAL_SPECULAR");
        material->defines.defineIf(sheen.has_value(), "MATERIAL_SHEEN");
        material->defines.defineIf(ior.has_value(), "MATERIAL_IOR");
        material->defines.defineIf(transmission.has_value(), "MATERIAL_TRANSMISSION");
        material->defines.defineIf(volume.has_value(), "MATERIAL_VOLUME");
        material->defines.defineIf(iridescence.has_value(), "MATERIAL_IRIDESCENCE");
        material->defines.defineIf(material->khrAnisotropy, "MATERIAL_ANISOTROPY");

        return true;
    };

    m_materialMapping.createRendererObjectFromGltf = [this, setMaterialDefaults](const gltf::Material* gltfMaterial) -> renderer::Material* {
        auto material = new renderer::Material { m_resourceManager.m_gl };
        material->fragmentShader = "pbr.frag";

        setMaterialDefaults(material);

        m_materialMapping.updateRendererobjectFromGltf(gltfMaterial, material);

        return material;
    };

    m_materialMapping.destroyRendererobjectFromGltf = [this](const gltf::Material* gltfMaterial, renderer::Material* material) {
        delete material;

        m_animationPointerTargets.erase({ gltfMaterial, "alphaCutoff" });
        m_animationPointerTargets.erase({ gltfMaterial, "emissiveFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "normalTexture/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "occlusionTexture/strength" });

        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/baseColorFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/metallicFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/roughnessFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/metallicRoughnessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/metallicRoughnessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "pbrMetallicRoughness/metallicRoughnessTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatNormalTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatNormalTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_clearcoat/clearcoatNormalTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularColorFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularColorTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularColorTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_specular/specularColorTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_emissive_strength/emissiveStrength" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenColorFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenColorTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenColorTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenColorTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_ior/ior" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_transmission/transmissionFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_transmission/transmissionTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_transmission/transmissionTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_transmission/transmissionTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/thicknessFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/attenuationDistance" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/attenuationColor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/thicknessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/thicknessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_volume/thicknessTexture/extensions/KHR_texture_transform/scale" });

        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceFactor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceIor" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessMaximum" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessMinimum" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceTexture/extensions/KHR_texture_transform/scale" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions/KHR_texture_transform/offset" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions/KHR_texture_transform/rotation" });
        m_animationPointerTargets.erase({ gltfMaterial, "extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions/KHR_texture_transform/scale" });
    };
    m_materialMapping.needsRecreation = [](renderer::Material* material, const gltf::Material* gltfMaterial) -> bool {
        return false;
    };
}

bool GltfRenderLayer::updateMaterialStates(const gltf::Gltf& gltf)
{
    // A2.2: a material-only edit. updateGltf re-uploads every accessor through
    // m_bufferMapping and updateScene rebuilds the meshes; neither is needed when only
    // material values changed. This is the same material sync updateGltf uses, so defines,
    // sampler bindings and uniforms (including ASOBO factors) are refreshed.
    bool meshUpdateNeeded = false;
    for (const gltf::Material& material : gltf.getMaterials()) {
        const auto known = m_materialNormalUvSets.find(&material);
        if (known != m_materialNormalUvSets.end() && known->second != materialNormalUvSet(material, m_modelImagesOmitted)) {
            meshUpdateNeeded = true;
            break;
        }
    }
    ++m_gltfGeneration;
    m_traversal.frameToken = 0;
    // A3.7: an Inspector edit of a portal's parameters arrives as an Extension change.
    updateSkyPortals(gltf);
    const gltf::asobo::ExtensionSet assetExtensions =
        gltf::asobo::parseExtensions(gltf, gltf.getAsset(), gltf::asobo::Scope::Asset);
    const gltf::asobo::ExtensionData* convention = assetExtensions.find("ASOBO_normal_map_convention");
    const std::string* tangentSpaceConvention =
        convention != nullptr ? convention->string("tangent_space_convention") : nullptr;
    m_assetNormalMapConventionDirectX = tangentSpaceConvention != nullptr && *tangentSpaceConvention == "DirectX";

    m_assetHasAsoboExtensions = false;
    for (const auto& ext : gltf.getDeclaredExtensions()) {
        if (gltf::asobo::isAsoboExtension(ext.c_str())) {
            m_assetHasAsoboExtensions = true;
            break;
        }
    }

    m_syncingGltf = &gltf;
    updateMaterials(gltf.getMaterials());
    m_syncingGltf = nullptr;
    return meshUpdateNeeded;
}

void GltfRenderLayer::releaseModelTextures()
{
    // A reset can reuse Image addresses and encodedMapID values. Those are useful
    // within one model, but must not preserve another file's uploaded texels.
    for (const auto& [key, texture] : m_textureMapping.map)
        m_textureMapping.destroyRendererobjectFromGltf(key, texture);
    m_textureMapping.map.clear();
}

void GltfRenderLayer::updateMaterials(const core::LinkedList<gltf::Material>& materials)
{
    core::LinkedList<const gltf::Material*> gltfMaterials;
    for (const auto& gltfMat : materials) {
        gltfMaterials.emplace_back(&gltfMat);
    }

    // insert a default material
    gltfMaterials.emplace_back(nullptr);

    m_materialMapping.update(gltfMaterials);

    // A2.2: remember the normal-map UV set each material was synced with, so a later
    // material-only sync can detect a UV-set change that requires rebuilding the tangents.
    m_materialNormalUvSets.clear();
    for (const gltf::Material& gltfMaterial : materials) {
        m_materialNormalUvSets.emplace(&gltfMaterial, materialNormalUvSet(gltfMaterial, m_modelImagesOmitted));
    }
}
