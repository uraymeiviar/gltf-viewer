// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// GltfRenderLayer: construction, the glTF / scene synchronisation entry points, the environment map, cameras,
// lights, audio emitters, sky portals and world-transform queries. The mapping callbacks by resource kind are
// in GltfRenderLayerMaterials.cpp, GltfRenderLayerMeshes.cpp and GltfRenderLayerNodes.cpp; scene traversal,
// culling and the renderer scene in GltfRenderLayerTraversal.cpp.

#include "GltfRenderLayerInternal.h"

GltfRenderLayer::GltfRenderLayer(gl::GL* gl, const filesystem::Path& shaderPath)
    : m_resourceManager(gl)
    , m_cameraMapping(*this)
    , m_audioMapping(*this)
    , m_lightMapping(*this)
    , m_iblSampler(gl, shaderPath)
{
    GLint maxTextureSize;
    gl->getIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    m_max2DTextureSize = static_cast<size_t>(math::pow(maxTextureSize, 2));
    GLint maxArraySize;
    gl->getIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArraySize);
    m_maxTextureArrayLength = maxArraySize;

    // KTX2 BCn upload support is extension-based on an OpenGL 4.1 context. RGTC (BC5) is
    // core; S3TC (BC1/BC3) and BPTC (BC7) are checked here once and again before upload.
    if (!gl->hasExtension("GL_ARB_texture_compression_bptc") && !gl->hasExtension("GL_EXT_texture_compression_bptc")) {
        core::logWarning("[KTX2] BC7 compressed textures are unsupported: GL_ARB_texture_compression_bptc is missing");
    }
    if (!gl->hasExtension("GL_EXT_texture_compression_s3tc")) {
        core::logWarning("[KTX2] BC1/BC3 compressed textures are unsupported: GL_EXT_texture_compression_s3tc is missing");
    }

    // The GltfMappings' create / update / destroy callbacks, by resource kind.
    initResourceMappings();
    initMaterialMapping(gl);
    initMeshMapping();
    initNodeMappings(gl);
}

// GlTextures, GlBuffers and GlSamplerStates.
void GltfRenderLayer::initResourceMappings()
{
    // GlTextures
    m_textureMapping.createRendererObjectFromGltf = [this](std::tuple<const gltf::Image*, bool> imageAndIsLinear) -> renderer::gl::Texture* {
        auto& [gltfImage, isLinear] = imageAndIsLinear;
        if (gltfImage != nullptr && gltfImage->getEncodedBytes() != nullptr) {
            return new gl::Texture { m_resourceManager.m_gl, gl::Texture::extractTarget(gltfImage->getData()) };
        }
        return nullptr;
    };
    m_textureMapping.needsRecreation = [](renderer::gl::Texture* texture, std::tuple<const gltf::Image*, bool> imageAndIsLinear) -> bool {
        // TODO VERY BAD CODE!! REVIEW THIS!!
        auto& [gltfImage, isLinear] = imageAndIsLinear;
        gltf::Image* nonConstImg = const_cast<gltf::Image*>(gltfImage);
        return nonConstImg->hasTextureChangedForRenderer();
    };

    m_textureMapping.destroyRendererobjectFromGltf = [this](std::tuple<const gltf::Image*, bool>, renderer::gl::Texture* texture) {
        delete texture;
    };

    // GlBuffers
    m_bufferMapping.updateRendererobjectFromGltf = [](const gltf::Accessor* accessor, renderer::gl::Buffer* buffer) -> bool {
        // we need to reupload all buffers, as there is no way to check if the accessors
        // properties have changed (e.g. the component type)

        if (accessor->getBufferView() == nullptr || buffer == nullptr) {
            return false;
        }

        // ensure buffer and bufferview have same length
        const gltf::BufferView* bufferView = accessor->getBufferView();
        if (bufferView->getBuffer()->getByteLength() < bufferView->getByteLength() + bufferView->getByteOffset()) {
            return false;
        }

        core::MemoryRange<const uint8_t> range {};
        core::Slice<uint8_t> data;
        if (accessor->getSparse().has_value()) {
            if (core::Succeeded(accessor->getSparseDataAs(data))) {
                range = data.getMemoryRange();
            }
        } else {
            accessor->getData(range);
        }

        // ensure range is not bigger than buffer
        size_t rangeByteSize = range.byteSize();
        if (rangeByteSize > bufferView->getBuffer()->getByteLength()) {
            return false;
        }

        buffer->write(range);
        return true;
    };
    m_bufferMapping.createRendererObjectFromGltf = [this](const gltf::Accessor* gltfAccessor) -> renderer::gl::Buffer* {
        if (gltfAccessor->getBufferView() == nullptr) {
            return nullptr;
        }
        auto buffer = new gl::Buffer { m_resourceManager.m_gl };

        m_bufferMapping.updateRendererobjectFromGltf(gltfAccessor, buffer);

        // remove empty buffers to avoid crash errors in the draw call.
        if (buffer->getSize() == 0) {
            m_bufferMapping.destroyRendererobjectFromGltf(gltfAccessor, buffer);
            return nullptr;
        }

        return buffer;
    };
    m_bufferMapping.destroyRendererobjectFromGltf = [this](const gltf::Accessor*, renderer::gl::Buffer* buffer) {
        delete buffer;
    };

    // GlSamplerStates
    m_samplerStateMapping.updateRendererobjectFromGltf = [](const gltf::Sampler* gltfSampler, renderer::gl::SamplerState* samplerState) -> bool {
        samplerState->minFilter = renderer::gl::TextureMinFilter(gltfSampler->getMinFilter().value_or(gltf::TextureFilterType::LinearMipmapLinear));
        samplerState->magFilter = renderer::gl::TextureMagFilter(gltfSampler->getMagFilter().value_or(gltf::TextureFilterType::Linear));
        samplerState->wrapS = renderer::gl::TextureWrap(gltfSampler->getWrapSOrDefault());
        samplerState->wrapT = renderer::gl::TextureWrap(gltfSampler->getWrapTOrDefault());
        return true;
    };
    m_samplerStateMapping.createRendererObjectFromGltf = [this](const gltf::Sampler* gltfSampler) -> renderer::gl::SamplerState* {
        auto samplerState = new renderer::gl::SamplerState;
        m_samplerStateMapping.updateRendererobjectFromGltf(gltfSampler, samplerState);
        return samplerState;
    };
    m_samplerStateMapping.destroyRendererobjectFromGltf = [this](const gltf::Sampler*, renderer::gl::SamplerState* samplerState) {
        delete samplerState;
    };
}

void GltfRenderLayer::updateScene(const gltf::Gltf& gltf, const gltf::Scene& scene)
{
    m_gltfScene = &scene;
    ++m_sceneGeneration;
    ++m_poseGeneration;
    updateTransformHierarchy(gltf.getNodes(), scene.getNodes());

    // ensure transforms from parent nodes are resolved into local transforms (relevant for triangle winding direction)
    m_resourceManager.m_transformHierarchy.apply();
    if (m_buffersUploadedForScene != &gltf) m_bufferMapping.update(gltf.getAccessors());
    m_buffersUploadedForScene = nullptr;

    updateLights(scene.getNodes());
    updateMeshes(scene.getNodes());
    // Unused meshes may have been prepared as well; do not retain their CPU copies.
    m_preparedTangents.clear();
    updateCameras(scene.getNodes());
    updateAudioEmitters(scene.getNodes());
    updateSkins(scene.getNodes());
    updateSkyPortals(gltf);

    m_animationMapping.update(gltf.getAnimations());

    gl::checkGLError(m_resourceManager.m_gl, "GltfRenderLayer: Error");
}

void GltfRenderLayer::updateGltf(const gltf::Gltf& gltf)
{
    ++m_gltfGeneration;
    m_syncingGltf = &gltf;
    m_modelImagesOmitted = gltf.imagesOmitted();

    // A1.2: the asset-level convention applies to every normal map of the asset. Only a
    // declared DirectX convention flips green; missing, OpenGL and unknown values do not.
    const gltf::asobo::ExtensionSet assetExtensions = gltf::asobo::parseExtensions(gltf, gltf.getAsset(), gltf::asobo::Scope::Asset);
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

    // A1.4: an extension the file does not declare is ignored. Without this declaration the
    // draw-order rule stays off even when the viewport applies the extension, so files that
    // do not use it keep the previous transparent order byte for byte.
    m_assetDrawOrderDeclared = gltf.isExtensionDeclared("ASOBO_material_draw_order");

    // A2.1: the shared decal buffer exists only for a file that actually carries a decal
    // material. The declaration alone is not enough: both extensions may be declared by an
    // exporter and left unused. A2.2 tracks the two extensions separately for their switches.
    m_assetHasDecalMaterials = false;
    m_assetHasGeometryDecalMaterials = false;
    m_assetHasBlendGbufferMaterials = false;
    if (gltf.isExtensionDeclared("ASOBO_material_geometry_decal")
        || gltf.isExtensionDeclared("ASOBO_material_blend_gbuffer")) {
        for (const auto& material : gltf.getMaterials()) {
            const gltf::asobo::ExtensionSet extensions =
                gltf::asobo::parseExtensions(gltf, material, gltf::asobo::Scope::Material);
            if (extensions.find("ASOBO_material_geometry_decal") != nullptr) {
                m_assetHasGeometryDecalMaterials = true;
            }
            if (extensions.find("ASOBO_material_blend_gbuffer") != nullptr) {
                m_assetHasBlendGbufferMaterials = true;
            }
            if (m_assetHasGeometryDecalMaterials || m_assetHasBlendGbufferMaterials) {
                m_assetHasDecalMaterials = true;
            }
        }
    }

    // A1.9: ASOBO_asset_optimized describes how the packaged file stores the vertex data,
    // not an effect the viewport can switch. Its types are decoded whenever the file
    // declares it; every other file keeps the accessor's own component type and
    // normalization.
    m_assetOptimized = gltf.isExtensionDeclared("ASOBO_asset_optimized");

    // Textures: first create glTextures for all textures present in the gltf
    // We have to scan the material for used textures, as whether the texture is in
    // Linear or sRGB format is only determined by the textures usage in the material.
    // Therefore in the texture mapping the primary key is `std::tuple<gltf::Image*, bool>`
    // where the second element of bool type is true if the texture has linear format.
    std::set<std::tuple<gltf::Image*, bool>> images; // use a set to avoid duplicates
    const bool ddsDeclaredForImages = gltf.isExtensionDeclared("MSFT_texture_dds");
    auto insertTextureImage = [&images, &gltf, ddsDeclaredForImages](const gltf::Texture* gltfTexture, bool isLinear) {
        if (gltf.imagesOmitted() || gltfTexture == nullptr) {
            // in gltf this is not allowed but the renderer should be able to handle this state
            return;
        }
        gltf::Image* gltfImage = pickImageForTexture(gltfTexture, ddsDeclaredForImages);
        if (gltfImage != nullptr) {
            images.insert({ gltfImage, isLinear });
        }
    };
    for (auto& material : gltf.getMaterials()) {
        auto insertImage = [&insertTextureImage](const std::optional<gltf::TextureInfo>& textureInfo, bool isLinear) {
            if (textureInfo.has_value() == false) {
                return;
            }
            insertTextureImage(textureInfo.value().getTexture(), isLinear);
        };

        // now access all elements of the material and set the correct linear / sRGB format

        insertImage(material.getNormalTextureInfo(), true);
        insertImage(material.getEmissiveTextureInfo(), false);
        insertImage(material.getOcclusionTextureInfo(), true);

        const auto& metallicRoughness = material.getPbrMetallicRoughness();
        if (metallicRoughness.has_value()) {
            insertImage(metallicRoughness->getBaseColorTextureInfo(), false);
            insertImage(metallicRoughness->getMetallicRoughnessTextureInfo(), true);
        }

        const auto& specularGlossiness = material.getPbrSpecularGlossiness();
        if (specularGlossiness.has_value()) {
            insertImage(specularGlossiness->getSpecularGlossinessTextureInfo(), false);
            insertImage(specularGlossiness->getDiffuseTextureInfo(), false);
        }

        const auto& clearCoat = material.getClearcoat();
        if (clearCoat.has_value()) {
            insertImage(clearCoat->getClearcoatTexture(), true);
            insertImage(clearCoat->getClearcoatRoughnessTexture(), true);
            insertImage(clearCoat->getClearcoatNormalTexture(), true);
        }

        const auto& specular = material.getSpecular();
        if (specular.has_value()) {
            insertImage(specular->getSpecularTextureInfo(), true);
            insertImage(specular->getSpecularColorTextureInfo(), false);
        }

        const auto& sheen = material.getSheen();
        if (sheen.has_value()) {
            insertImage(sheen->getSheenColorTextureInfo(), false);
            insertImage(sheen->getSheenRoughnessTextureInfo(), true);
        }

        const auto& transmission = material.getTransmission();
        if (transmission.has_value()) {
            insertImage(transmission->getTransmissionTextureInfo(), true);
        }

        const auto& volume = material.getVolume();
        if (volume.has_value()) {
            insertImage(volume->getThicknessTextureInfo(), true);
        }

        const auto& iridescence = material.getIridescence();
        if (iridescence.has_value()) {
            insertImage(iridescence->getIridescenceTextureInfo(), true);
            insertImage(iridescence->getIridescenceThicknessTextureInfo(), true);
        }

        // A7.1: KHR_materials_anisotropy and ASOBO_material_anisotropic_v2 direction textures are linear.
        const auto& anisotropy = material.getAnisotropy();
        if (anisotropy.has_value()) {
            insertImage(anisotropy->getAnisotropyTextureInfo(), true);
        } else if (gltf.isExtensionDeclared("ASOBO_material_anisotropic_v2")) {
            const gltf::asobo::ExtensionSet materialExtensions =
                gltf::asobo::parseExtensions(gltf, material, gltf::asobo::Scope::Material);
            if (const gltf::asobo::ExtensionData* asoboAnisotropy = materialExtensions.find("ASOBO_material_anisotropic_v2")) {
                const gltf::asobo::TextureRef* reference = asoboAnisotropy->texture("anisoDirectionRoughnessTexture");
                if (reference != nullptr && reference->index >= 0
                    && static_cast<size_t>(reference->index) < gltf.getTextures().size()) {
                    insertTextureImage(&gltf.getTextures().at(static_cast<size_t>(reference->index)), true);
                }
            }
        }

        // A3.4: the ASOBO_material_SSS opacity map is linear.
        if (gltf.isExtensionDeclared("ASOBO_material_SSS")) {
            const gltf::asobo::ExtensionSet materialExtensions =
                gltf::asobo::parseExtensions(gltf, material, gltf::asobo::Scope::Material);
            if (const gltf::asobo::ExtensionData* sss = materialExtensions.find("ASOBO_material_SSS")) {
                const gltf::asobo::TextureRef* reference = sss->texture("opacityTexture");
                if (reference != nullptr && reference->index >= 0
                    && static_cast<size_t>(reference->index) < gltf.getTextures().size()) {
                    insertTextureImage(&gltf.getTextures().at(static_cast<size_t>(reference->index)), true);
                }
            }
        }

        // A5.1: the four ASOBO_material_detail_map textures are not core slots, so register
        // them here. The colour map is sRGB, the ORM, normal and blend-mask maps are linear.
        if (gltf.isExtensionDeclared("ASOBO_material_detail_map")) {
            const gltf::asobo::ExtensionSet materialExtensions =
                gltf::asobo::parseExtensions(gltf, material, gltf::asobo::Scope::Material);
            if (const gltf::asobo::ExtensionData* detailMap = materialExtensions.find("ASOBO_material_detail_map")) {
                auto insertDetailImage = [&gltf, &insertTextureImage, detailMap](const char* key, bool isLinear) {
                    const gltf::asobo::TextureRef* reference = detailMap->texture(key);
                    if (reference == nullptr || reference->index < 0
                        || static_cast<size_t>(reference->index) >= gltf.getTextures().size()) {
                        return;
                    }
                    insertTextureImage(&gltf.getTextures().at(static_cast<size_t>(reference->index)), isLinear);
                };
                insertDetailImage("detailColorTexture", false);
                insertDetailImage("detailMetalRoughAOTexture", true);
                insertDetailImage("detailNormalTexture", true);
                insertDetailImage("blendMaskTexture", true);
            }
        }

        // A2.3: the geometry decal's blend mask (mode blendMasked) is a linear mask texture.
        if (gltf.isExtensionDeclared("ASOBO_material_geometry_decal")) {
            const gltf::asobo::ExtensionSet materialExtensions =
                gltf::asobo::parseExtensions(gltf, material, gltf::asobo::Scope::Material);
            if (const gltf::asobo::ExtensionData* geometryDecal = materialExtensions.find("ASOBO_material_geometry_decal")) {
                const gltf::asobo::TextureRef* reference = geometryDecal->texture("blendMaskTexture");
                if (reference != nullptr && reference->index >= 0
                    && static_cast<size_t>(reference->index) < gltf.getTextures().size()) {
                    insertTextureImage(&gltf.getTextures().at(static_cast<size_t>(reference->index)), true);
                }
            }
        }

        // A4.2: windshield v3 detail normal and micro-scratches normal textures.
        if (gltf.isExtensionDeclared("ASOBO_material_windshield_v3")) {
            const gltf::asobo::ExtensionSet materialExtensions =
                gltf::asobo::parseExtensions(gltf, material, gltf::asobo::Scope::Material);
            if (const gltf::asobo::ExtensionData* windshieldV3 = materialExtensions.find("ASOBO_material_windshield_v3")) {
                auto insertWindshieldImage = [&gltf, &insertTextureImage, windshieldV3](const char* key, bool isLinear) {
                    const gltf::asobo::TextureRef* reference = windshieldV3->texture(key);
                    if (reference == nullptr || reference->index < 0
                        || static_cast<size_t>(reference->index) >= gltf.getTextures().size()) {
                        return;
                    }
                    insertTextureImage(&gltf.getTextures().at(static_cast<size_t>(reference->index)), isLinear);
                };
                insertWindshieldImage("windshieldDetailNormalTexture", true);
                insertWindshieldImage("scratchesNormalTexture", true);
            }
        }
    }
    core::LinkedList<std::tuple<gltf::Image*, bool>> imagesList; // `update` requires a core::LinkedList
    for (auto& pair : images) {
        imagesList.emplace_back(std::move(pair));
    }
    m_textureMapping.update(imagesList);

    // Buffers: create glBuffers for all Accessorss in the gltf. If you need to reduce memory transfer, try to
    // increase the size of the buffer views in the gltf by for example letting multiple accessors share the samce
    // buffer view
    m_bufferMapping.update(gltf.getAccessors());

    m_samplerStateMapping.update(gltf.getSamplers());

    // Materials: determine the fragment shader and which textures are used within that.
    updateMaterials(gltf.getMaterials());
    m_syncingGltf = nullptr;
    m_buffersUploadedForScene = &gltf;
}

void GltfRenderLayer::setEnvironmentMap(const filesystem::Path& panoramaIrradiancePath)
{
    if (!gl::checkGLError(m_resourceManager.m_gl, "GltfRenderLayer: Error", false)) {
        return;
    }

    m_resourceManager.destroyAllOf<EnvironmentMap>();
    m_externalEnvironment = false;
    m_environmentMapPath = panoramaIrradiancePath;
    m_iblSampler.setSamplingParamters(iblSampling);
    m_iblSampler.init(panoramaIrradiancePath, iblParameters);

    m_activeEnvironmentMap = m_resourceManager.loadEnvironmentMap(m_iblSampler);
}

void GltfRenderLayer::setExternalEnvironment(GLuint lambertian, GLuint ggx, GLuint sheen, GLuint ggxLut, GLuint charlieLut,
                                             GLuint sheenLut, uint32_t mipCount)
{
    m_resourceManager.destroyAllOf<EnvironmentMap>();
    m_iblSampler.release();
    m_environmentMapPath = filesystem::Path();
    m_externalEnvironment = true;
    m_activeEnvironmentMap = m_resourceManager.loadEnvironmentMap(lambertian, ggx, sheen, ggxLut, charlieLut, sheenLut,
                                                                  mipCount, false);
}

void GltfRenderLayer::retargetExternalEnvironment(GLuint lambertian, GLuint ggx, GLuint sheen, uint32_t mipCount)
{
    if (m_externalEnvironment && m_activeEnvironmentMap != nullptr) {
        m_activeEnvironmentMap->retargetCubes(lambertian, ggx, sheen, mipCount);
    }
}

bool GltfRenderLayer::filterEnvironmentMap()
{
    return m_iblSampler.filter();
}

void GltfRenderLayer::updateSkyPortals(const gltf::Gltf& gltf)
{
    // A3.7: ASOBO_sky_portal is a node extension. Only a file that declares it is read (the typed model ignores
    // undeclared extensions), so files without portals keep an empty table and pay nothing per frame.
    m_skyPortalSources.clear();
    if (!gltf.isExtensionDeclared("ASOBO_sky_portal")) {
        return;
    }
    constexpr double halfDegree = 3.14159265358979323846 / 360.0;
    for (const gltf::Node& node : gltf.getNodes()) {
        if (node.getUnknownExtensions().find("ASOBO_sky_portal") == node.getUnknownExtensions().end()) {
            continue;
        }
        const std::optional<gltf::asobo::SkyPortal> portal =
            gltf::asobo::readSkyPortal(gltf::asobo::parseExtensions(gltf, node, gltf::asobo::Scope::Node));
        if (!portal.has_value() || portal->shapeType != 1) {
            continue;
        }
        SkyPortalSource source;
        source.radiusMetres = static_cast<float>(portal->radiusMetres());
        source.innerRadiusMetres = static_cast<float>(portal->innerCircleRadiusMetres());
        source.cosInner = static_cast<float>(std::cos(portal->innerConeDeg * halfDegree));
        source.cosOuter = static_cast<float>(std::cos(portal->outerConeDeg * halfDegree));
        m_skyPortalSources[&node] = source;
    }
}

void GltfRenderLayer::updateCameras(const core::Vector<gltf::Node*>& cameraNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Camera*>> gltfTuples;

    std::function<void(const gltf::Node*)> gatherNodesRecursive;
    gatherNodesRecursive = [&](const gltf::Node* gltfNode) {
        const gltf::Camera* gltfCamera = gltfNode->getCamera();
        if (gltfCamera != nullptr) {
            gltfTuples.emplace_back(gltfNode, gltfCamera);
        }

        for (const gltf::Node* gltfChild : gltfNode->getChildren()) {
            gatherNodesRecursive(gltfChild);
        }
    };

    for (const gltf::Node* gltfNode : cameraNodes) {
        gatherNodesRecursive(gltfNode);
    }

    m_cameraMapping.update(gltfTuples);
}

void GltfRenderLayer::updateLights(const core::Vector<gltf::Node*>& lightNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Light*>> gltfTuples;
    std::function<void(const gltf::Node*)> gatherNodesRecursive;
    gatherNodesRecursive = [&](const gltf::Node* gltfNode) {
        const gltf::Light* pGltfLight = gltfNode->getLight();
        if (pGltfLight != nullptr && gltfNode->getVisible()) {
            gltfTuples.emplace_back(gltfNode, pGltfLight);
        }

        for (const gltf::Node* gltfChild : gltfNode->getChildren()) {
            gatherNodesRecursive(gltfChild);
        }
    };

    for (const gltf::Node* gltfNode : lightNodes) {
        gatherNodesRecursive(gltfNode);
    }

    m_lightMapping.update(gltfTuples);
}

void GltfRenderLayer::updateAudioEmitters(const core::Vector<gltf::Node*>& audioNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::AudioEmitter*>> gltfTuples;
    std::function<void(const gltf::Node*)> gatherNodesRecursive;
    gatherNodesRecursive = [&](const gltf::Node* node) {
        const gltf::AudioEmitter* audio = node->getAudioEmitter();
        if (audio != nullptr && node->getVisible()) {
            gltfTuples.emplace_back(node, audio);
        }

        for (const gltf::Node* gltfChild : node->getChildren()) {
            gatherNodesRecursive(gltfChild);
        }
    };

    for (const gltf::Node* gltfNode : audioNodes) {
        gatherNodesRecursive(gltfNode);
    }

    m_audioMapping.update(gltfTuples);
}

void GltfRenderLayer::setActiveGltfCameraByNode(const gltf::Node* _CameraNode)
{
    auto cameraIt = std::find_if(m_cameraMapping.map.begin(), m_cameraMapping.map.end(), [_CameraNode](auto& pair) {
        return std::get<const gltf::Node*>(pair.first) == _CameraNode;
    });

    if (cameraIt != m_cameraMapping.map.end()) {
        m_activeCamera = std::get<Camera*>(cameraIt->second);
    } else {
        m_activeCamera = nullptr;
    }
}

const gltf::Node* GltfRenderLayer::getActiveGltfCameraNode() const
{
    auto cameraIt = std::find_if(m_cameraMapping.map.begin(), m_cameraMapping.map.end(), [this](auto& pair) {
        return std::get<Camera*>(pair.second) == m_activeCamera;
    });

    if (cameraIt != m_cameraMapping.map.end()) {
        return std::get<const gltf::Node*>(cameraIt->first);
    }

    return nullptr;
}

const gltf::Camera* GltfRenderLayer::getActiveGltfCamera() const
{
    auto cameraIt = std::find_if(m_cameraMapping.map.begin(), m_cameraMapping.map.end(), [this](auto& pair) {
        return std::get<Camera*>(pair.second) == m_activeCamera;
    });

    if (cameraIt != m_cameraMapping.map.end()) {
        return std::get<const gltf::Camera*>(cameraIt->first);
    }

    return nullptr;
}

math::mat4 GltfRenderLayer::getWorldTransform(const gltf::Node* searchNode) const
{
    if (searchNode) {
        const auto& result = m_transformHierarchyMapping.map.find(searchNode);
        if (result != m_transformHierarchyMapping.map.end()) {
            return result->second->getWorldTransform();
        }
    }

    return math::mat4(1.0);
}

math::mat3x3 GltfRenderLayer::getWorldRotation(const gltf::Node* searchNode) const
{
    if (searchNode) {
        const auto& result = m_transformHierarchyMapping.map.find(searchNode);
        if (result != m_transformHierarchyMapping.map.end()) {
            return result->second->getWorldRotation();
        }
    }

    return math::mat4(1.0);
    ;
}

math::quat GltfRenderLayer::getWorldRotationQuat(const gltf::Node* searchNode) const
{
    return math::toQuat(getWorldRotation(searchNode));
}

math::mat4 GltfRenderLayer::getParentWorldTransform(const gltf::Node* searchNode) const
{
    if (searchNode) {
        for (const auto& walker : m_transformHierarchyMapping.map) {
            for (const auto& childWalker : walker.first->getChildren()) {
                if (childWalker == searchNode) {
                    return walker.second->getWorldTransform();
                }
            }
        }
    }

    return math::mat4(1.0);
}

math::mat3x3 GltfRenderLayer::getParentWorldRotation(const gltf::Node* searchNode) const
{
    if (searchNode) {
        for (const auto& walker : m_transformHierarchyMapping.map) {
            for (const auto& childWalker : walker.first->getChildren()) {
                if (childWalker == searchNode) {
                    return walker.second->getWorldRotation();
                }
            }
        }
    }

    return math::mat4(1.0);
    ;
}

math::quat GltfRenderLayer::getParentWorldRotationQuat(const gltf::Node* searchNode) const
{
    return math::toQuat(getParentWorldRotation(searchNode));
}

Camera* GltfRenderLayer::addCamera()
{
    return m_resourceManager.createWithGLContext<renderer::Camera>();
}

gltf::Image* GltfRenderLayer::pickImageForTexture(const gltf::Texture* gltfTexture, bool ddsDeclared)
{
    if (gltfTexture == nullptr) {
        return nullptr;
    }

    if (ddsDeclared) {
        const auto& oDds = gltfTexture->getTextureDds();
        if (oDds.has_value() && oDds.value().getSource() != nullptr && oDds.value().getSource()->isLoaded()) {
            return oDds.value().getSource();
        }
    }

    const auto& oBasisU = gltfTexture->getBasisU();
    if (oBasisU.has_value() && oBasisU.value().getSource() != nullptr && oBasisU.value().getSource()->isLoaded()) {
        return oBasisU.value().getSource();
    }

    const auto& oWebP = gltfTexture->getWebP();
    if (oWebP.has_value() && oWebP.value().getSource() != nullptr && oWebP.value().getSource()->isLoaded()) {
        return oWebP.value().getSource();
    }

    return gltfTexture->getSource();
}
