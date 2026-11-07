// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/MaterialInspector.h"
#include "imgui/ElementInspector.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/ImageViewer.h"
#include "imgui/PropertyTree.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/TextureUtils.h"
#include "imgui/UiProbe.h"
#include "imgui/ViewportInstance.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/Theme.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include "gltf/extensions/MsftExtensions.h"
#include <cctype>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

namespace ux3d::imgui {

namespace {

std::string makeProbeTag(const char* prefix, const char* label)
{
    std::string tag = prefix;
    bool separator = false;
    for (const char* p = label; *p != '\0'; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (std::isalnum(c)) {
            tag.push_back(static_cast<char>(std::tolower(c)));
            separator = false;
        } else if (!separator) {
            tag.push_back('.');
            separator = true;
        }
    }
    while (!tag.empty() && tag.back() == '.') {
        tag.pop_back();
    }
    return tag;
}

struct TexCoordAccessorUsage {
    ux3d::gltf::Accessor* accessor = nullptr;
    const ux3d::gltf::Mesh* mesh = nullptr;
    int primitiveIndex = -1;
    int usageCount = 0;
};

bool primitiveUsesMaterial(const ux3d::gltf::Primitive& primitive, const ux3d::gltf::Material* material)
{
    if (primitive.getMaterial() == material) {
        return true;
    }
    return primitive.hasMaterialVariantsMapping()
        && primitive.getMaterialVariantsMapping()->hasMappingForMaterial(material);
}

TexCoordAccessorUsage findTexCoordAccessor(ux3d::gltf::Gltf& gltfModel,
                                           const ux3d::gltf::Material* material,
                                           uint32_t texCoord)
{
    TexCoordAccessorUsage usage;
    const std::string semantic = "TEXCOORD_" + std::to_string(texCoord);
    for (auto& mesh : gltfModel.getElements<ux3d::gltf::Mesh>()) {
        int primitiveIndex = 0;
        for (auto& primitive : mesh.getPrimitives()) {
            if (primitiveUsesMaterial(primitive, material)) {
                for (auto& attribute : primitive.getAttributes()) {
                    if (attribute.getAccessor() == nullptr || attribute.getName() != semantic.c_str()) {
                        continue;
                    }
                    if (usage.accessor == nullptr) {
                        usage.accessor = attribute.getAccessor();
                        usage.mesh = &mesh;
                        usage.primitiveIndex = primitiveIndex;
                    }
                    ++usage.usageCount;
                }
            }
            ++primitiveIndex;
        }
    }
    return usage;
}

std::string materialDisplayName(const ux3d::gltf::Material* material)
{
    if (material == nullptr) {
        return "Material";
    }
    return material->hasName() ? material->getName().c_str() : ("Material " + std::to_string(material->m_ID));
}

} // namespace

bool MaterialInspector::renderTextureSlot(const char* label,
                                          std::optional<ux3d::gltf::TextureInfo>& texInfo,
                                          ux3d::gltf::Gltf& gltfModel,
                                          ux3d::gltf::Material* material,
                                          SelectionState& selection,
                                          ux3d::renderer::GltfRenderLayer* gltfLayer,
                                          bool showScale,
                                          bool showStrength)
{
    bool modified = false;
    ImGui::PushID(label);

    const bool hasTexture = texInfo.has_value() && (texInfo->getTexture() != nullptr);
    ux3d::gltf::Texture* texture = hasTexture ? texInfo->getTexture() : nullptr;
    ux3d::gltf::Image* source = nullptr;
    if (texture != nullptr) {
        source = ux3d::renderer::GltfRenderLayer::pickImageForTexture(
            texture, gltfModel.isExtensionDeclared("MSFT_texture_dds"));
        if (source == nullptr) {
            source = texture->getSource();
        }
    }

    // An empty slot is one row; a slot with a texture is a group (texture, UV set and its factors).
    if (!hasTexture) {
        prop::textRow(ICON_MDI_TEXTURE, label, nullptr, "None");
        ImGui::PopID();
        return false;
    }
    if (!prop::group(ICON_MDI_TEXTURE, label, true)) {
        ImGui::PopID();
        return false;
    }

    constexpr float previewSize = 64.0f;
    const std::string previewProbe = makeProbeTag("material.texture.", label);
    const unsigned int previewId = source != nullptr
        ? ElementInspector::getImagePreviewTextureId(source, gltfLayer, showScale)
        : 0;

    prop::row(ICON_MDI_IMAGE_OUTLINE, "Texture",
              "Click the preview to open the image in the Image Viewer; click the name to inspect the texture.");
    const ImVec2 previewMin = ImGui::GetCursorScreenPos();
    const ImVec2 previewMax(previewMin.x + previewSize, previewMin.y + previewSize);
    ImGui::InvisibleButton("##TexturePreview", ImVec2(previewSize, previewSize));
    const bool previewHovered = ImGui::IsItemHovered();
    const bool previewClicked = ImGui::IsItemClicked();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(previewMin, previewMax, IM_COL32(18, 18, 22, 255), 3.0f);
    if (previewId != 0) {
        ImVec2 imageMin = previewMin;
        ImVec2 imageMax = previewMax;
        uint32_t width = 0;
        uint32_t height = 0;
        if (source != nullptr) {
            const auto& decoded = source->getData();
            width = decoded.getWidth();
            height = decoded.getHeight();
            if (width == 0 || height == 0) {
                const auto resolution = source->getResolution();
                if (resolution.size() >= 2) {
                    width = resolution[0];
                    height = resolution[1];
                }
            }
        }
        if (width > 0 && height > 0) {
            const float fit = std::min(previewSize / static_cast<float>(width),
                                       previewSize / static_cast<float>(height));
            const float imageWidth = static_cast<float>(width) * fit;
            const float imageHeight = static_cast<float>(height) * fit;
            imageMin = ImVec2(previewMin.x + (previewSize - imageWidth) * 0.5f,
                              previewMin.y + (previewSize - imageHeight) * 0.5f);
            imageMax = ImVec2(imageMin.x + imageWidth, imageMin.y + imageHeight);
        }
        drawList->AddImage(static_cast<ImTextureID>(previewId), imageMin, imageMax);
    } else {
        const char* placeholder = ICON_MDI_IMAGE_BROKEN;
        const ImVec2 textSize = ImGui::CalcTextSize(placeholder);
        drawList->AddText(ImVec2(previewMin.x + (previewSize - textSize.x) * 0.5f,
                                 previewMin.y + (previewSize - textSize.y) * 0.5f),
                          IM_COL32(125, 125, 135, 255), placeholder);
    }
    drawList->AddRect(previewMin, previewMax,
                      previewHovered ? IM_COL32(33, 171, 107, 255) : IM_COL32(70, 70, 78, 255),
                      3.0f, 0, previewHovered ? 2.0f : 1.0f);

    if (previewHovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (source != nullptr) {
            ImGui::SetTooltip("Open %s in the Image Viewer", getImageDisplayName(source).c_str());
        } else {
            ImGui::SetTooltip("Inspect %s", getTextureDisplayName(texture, source).c_str());
        }
    }
    if (previewClicked) {
        // The preview opens the full-resolution Image Viewer; the name next to it inspects the texture.
        // A normal slot's image is signed-square decoded only in a packaged (ASOBO_asset_optimized) file,
        // as in the Image / Texture Inspector, so both routes open the same view.
        if (source != nullptr) {
            ImageViewer::open(gltfModel, source, showScale && gltfModel.isExtensionDeclared("ASOBO_asset_optimized"),
                              previewId);
        } else {
            selection.selectTextureFromMaterial(texture, material);
        }
    }
    UiProbe::item(previewProbe.c_str(), 0);

    ImGui::SameLine();
    ImGui::BeginGroup();
    std::string textureLabel = std::string(ICON_MDI_TEXTURE " ") + getTextureDisplayName(texture, source);
    textureLabel += "##InspectTexture";
    const float buttonWidth = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button(textureLabel.c_str(), ImVec2(buttonWidth > 0.0f ? buttonWidth : 0.0f, 0.0f))) {
        selection.selectTextureFromMaterial(texture, material);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("Inspect %s", getTextureDisplayName(texture, source).c_str());
    }

    std::string sourceLine;
    if (source != nullptr) {
        if (!source->getUri().empty()) {
            const std::string uri = source->getUri().c_str();
            sourceLine = uri.rfind("data:", 0) == 0 ? "Embedded image" : getFilenameFromUri(uri);
        }
        const auto resolution = source->getResolution();
        if (resolution.size() >= 2 && resolution[0] > 0 && resolution[1] > 0) {
            if (!sourceLine.empty()) {
                sourceLine += "  ";
            }
            sourceLine += std::to_string(resolution[0]) + " x " + std::to_string(resolution[1]) + " px";
        }
    }
    if (!sourceLine.empty()) {
        ImGui::TextDisabled("%s", sourceLine.c_str());
        prop::tooltip(sourceLine.c_str());
    }
    ImGui::EndGroup();

    // The set the renderer samples: texCoord, or KHR_texture_transform's texCoord override.
    const uint32_t texCoord = texInfo->getEffectiveTexCoord();
    const std::string texCoordLabel = "TEXCOORD_" + std::to_string(texCoord);
    const TexCoordAccessorUsage usage = findTexCoordAccessor(gltfModel, material, texCoord);
    const bool canInspectTexCoord = usage.accessor != nullptr;
    prop::row(ICON_MDI_VECTOR_SQUARE, "UV Set", "The texture coordinates the slot reads. Click to inspect the accessor.");
    if (!canInspectTexCoord) {
        ImGui::BeginDisabled();
    }
    const std::string texCoordButton = ICON_MDI_VECTOR_SQUARE " " + texCoordLabel + "##InspectTexCoord";
    // With a source image, a UV layout button follows the TEXCOORD button.
    const float uvButtonWidth = source != nullptr ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0f;
    const float texCoordWidth = ImGui::GetContentRegionAvail().x - uvButtonWidth;
    if (ImGui::Button(texCoordButton.c_str(), ImVec2(texCoordWidth > 1.0f ? texCoordWidth : -FLT_MIN, 0.0f)) && canInspectTexCoord) {
        selection.selectAccessorFromMaterial(usage.accessor, material);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        if (canInspectTexCoord) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            std::string tooltip = "Inspect " + texCoordLabel + " (Accessor " + std::to_string(usage.accessor->m_ID) + ")";
            if (usage.mesh != nullptr) {
                tooltip += "\nMesh " + std::to_string(usage.mesh->m_ID);
                if (usage.mesh->hasName()) {
                    tooltip += " \"" + std::string(usage.mesh->getName().c_str()) + "\"";
                }
                tooltip += ", Primitive " + std::to_string(usage.primitiveIndex);
            }
            if (usage.usageCount > 1) {
                tooltip += "\nUsed by " + std::to_string(usage.usageCount) + " primitives.";
            }
            ImGui::SetTooltip("%s", tooltip.c_str());
        } else {
            ImGui::SetTooltip("No primitive using this material defines %s.", texCoordLabel.c_str());
        }
    }
    UiProbe::item(makeProbeTag("material.texcoord.", label).c_str(), 0);
    if (!canInspectTexCoord) {
        ImGui::EndDisabled();
    }
    if (source != nullptr) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_VECTOR_TRIANGLE "##UvLayout", ImVec2(ImGui::GetFrameHeight(), 0.0f))) {
            ImageViewer::openUvLayout(gltfModel, gltfLayer, source,
                                      showScale && gltfModel.isExtensionDeclared("ASOBO_asset_optimized"),
                                      ImageViewer::uvLayoutRequest(material, label, *texInfo), previewId);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("Show the UV layout of this material on the texture");
        }
        UiProbe::item(makeProbeTag("material.uvlayout.", label).c_str(), 0);
    }

    // Normal map scale
    if (showScale) {
        prop::row(ICON_MDI_ARROW_EXPAND_VERTICAL, "Scale", "Normal scale; Flip Normal Y negates it.");
        const char* flipLabel = "Flip Normal Y";
        const float flipWidth = ImGui::CalcTextSize(flipLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetNextItemWidth(prop::valueWidth(flipWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::PushFont(getMonoFont());
        float scale = static_cast<float>(texInfo->getScale());
        if (ImGui::DragFloat("##Scale", &scale, 0.02f, -2.0f, 2.0f, "%.2f")) {
            texInfo->setScale(static_cast<double>(scale));
            modified = true;
        }
        ImGui::PopFont();
        ImGui::SameLine();
        if (ImGui::Button(flipLabel)) {
            scale = -scale;
            texInfo->setScale(static_cast<double>(scale));
            modified = true;
        }
    }

    // Occlusion strength
    if (showStrength) {
        prop::row(ICON_MDI_CONTRAST_CIRCLE, "Strength", "Occlusion strength.");
        ImGui::PushFont(getMonoFont());
        float strength = static_cast<float>(texInfo->getStrength());
        if (ImGui::SliderFloat("##Strength", &strength, 0.0f, 2.0f, "%.2f")) {
            texInfo->setStrength(static_cast<double>(strength));
            modified = true;
        }
        ImGui::PopFont();
    }

    prop::endGroup();
    ImGui::PopID();
    return modified;
}

bool MaterialInspector::render(ux3d::gltf::Material* material,
                               ux3d::gltf::Gltf& gltfModel,
                               SceneRevisions& revisions,
                               SelectionState& selection,
                               ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (material == nullptr) {
        prop::textRow(ICON_MDI_PALETTE_SWATCH, "Material", nullptr, "No material selected.");
        return false;
    }

    bool modified = false;
    // A Khronos material extension: a group whose value cell holds the display
    // switch. Call extensionEnd() after it, also when it returns false (and endGroup() when it returns true).
    auto extensionBegin = [&](const char* extension, const char* icon, const char* label, bool hasBody) -> bool {
        ImGui::PushID(extension);
        const std::string tip = std::string(extension) + (hasBody ? "" : " (no parameters)");
        // Always a group (also without parameters): its probe tag must be read while the value cell is current.
        const bool open = prop::groupValue(icon, label, true, tip.c_str());
        UiProbe::item((std::string("khr.header.") + extension).c_str(), material->m_ID);
        auto* viewport = ExtensionInspector::instance().getActiveViewport();
        bool applied = viewport == nullptr || viewport->settings.materialExtensions.applies(material->m_ID, extension);
        ImGui::BeginDisabled(viewport == nullptr);
        if (ImGui::Checkbox("##viewportToggle", &applied)) {
            viewport->settings.materialExtensions.setApplied(material->m_ID, extension, applied);
            revisions.mark(SceneChange::ViewportSettings);
        }
        UiProbe::item((std::string("khr.display.") + extension).c_str(), material->m_ID, false, applied);
        ImGui::EndDisabled();
        const char* displayTip = "Display only: turns this extension off for this material in the active viewport; the file is unchanged.";
        prop::tooltip(displayTip);
        ImGui::SameLine();
        ImGui::TextDisabled("Display");
        prop::tooltip(displayTip);
        return open;
    };
    auto extensionEnd = []() { ImGui::PopID(); };

    prop::textRow(ICON_MDI_PALETTE_SWATCH, "Material", nullptr, "ID %u", material->m_ID);
    prop::textRow(ICON_MDI_TAG_OUTLINE, "Name", "The name in the glTF (read only).", "%s",
                  material->hasName() ? material->getName().c_str() : "(unnamed)");

    // -----------------------------------------------------------------
    // General Material Settings
    // -----------------------------------------------------------------
    const bool generalOpen = prop::group(ICON_MDI_TUNE, "General Properties", true);
    UiProbe::item("material.section.general", material->m_ID);
    if (generalOpen) {
        bool doubleSided = material->isDoubleSidedOrDefault();
        prop::row(ICON_MDI_FLIP_TO_BACK, "Double Sided", "Back faces are rendered (no back-face culling).");
        if (ImGui::Checkbox("##doubleSided", &doubleSided)) {
            material->setDoubleSided(doubleSided);
            modified = true;
        }

        int alphaMode = 0;
        if (material->getAlphaModeOrDefault() == ux3d::gltf::AlphaMode::Mask) alphaMode = 1;
        else if (material->getAlphaModeOrDefault() == ux3d::gltf::AlphaMode::Blend) alphaMode = 2;

        const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
        prop::row(ICON_MDI_CHECKERBOARD, "Alpha Mode");
        if (ImGui::Combo("##alphaMode", &alphaMode, alphaModes, 3)) {
            if (alphaMode == 0) material->setAlphaMode(ux3d::gltf::AlphaMode::Opaque);
            else if (alphaMode == 1) material->setAlphaMode(ux3d::gltf::AlphaMode::Mask);
            else if (alphaMode == 2) material->setAlphaMode(ux3d::gltf::AlphaMode::Blend);
            modified = true;
        }

        // Alpha Cutoff (only visible in Mask mode)
        if (alphaMode == 1) {
            prop::row(ICON_MDI_CONTENT_CUT, "Alpha Cutoff", "Mask mode: alpha below this is cut away.");
            ImGui::PushFont(getMonoFont());
            float cutoff = static_cast<float>(material->getAlphaCutoffOrDefault());
            if (ImGui::SliderFloat("##alphaCutoff", &cutoff, 0.0f, 1.0f, "%.2f")) {
                material->setAlphaCutoff(static_cast<double>(cutoff));
                modified = true;
            }
            ImGui::PopFont();
        }

        auto emissiveD = material->getEmissiveFactorOrDefault();
        float emissive[3] = { static_cast<float>(emissiveD.r), static_cast<float>(emissiveD.g), static_cast<float>(emissiveD.b) };
        prop::row(ICON_MDI_LIGHTBULB_ON_OUTLINE, "Emissive Factor");
        if (ImGui::ColorEdit3("##emissiveFactor", emissive, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float)) {
            material->setEmissiveFactor(ux3d::math::dvec3(emissive[0], emissive[1], emissive[2]));
            modified = true;
        }
        float maxEmissive = std::max({ emissive[0], emissive[1], emissive[2] });
        if (maxEmissive > 1.0f) {
            float intensity = maxEmissive;
            prop::row(ICON_MDI_BRIGHTNESS_6, "Emissive Intensity", "The largest emissive component; editing scales all three.");
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat("##emissiveIntensity", &intensity, std::max(1.0f, intensity * 0.01f), 0.0f, 200000.0f, "%.1f")) {
                if (maxEmissive > 1e-6f) {
                    float s = intensity / maxEmissive;
                    emissive[0] *= s;
                    emissive[1] *= s;
                    emissive[2] *= s;
                    material->setEmissiveFactor(ux3d::math::dvec3(emissive[0], emissive[1], emissive[2]));
                    modified = true;
                }
            }
            ImGui::PopFont();
        }

        // Emissive Strength Extension
        if (material->getEmissiveStrength().has_value()) {
            if (extensionBegin("KHR_materials_emissive_strength", ICON_MDI_BRIGHTNESS_7, "Emissive Strength", true)) {
                prop::row(ICON_MDI_BRIGHTNESS_7, "Emissive Strength");
                ImGui::PushFont(getMonoFont());
                float strength = static_cast<float>(material->getEmissiveStrength()->getEmissiveStrengthOrDefault());
                if (ImGui::DragFloat("##emissiveStrength", &strength, 0.1f, 0.0f, 1000.0f, "%.1f")) {
                    material->getEmissiveStrength()->setEmissiveStrength(static_cast<double>(strength));
                    modified = true;
                }
                ImGui::PopFont();
                prop::endGroup();
            }
            extensionEnd();
        }

        if (renderTextureSlot("Emissive Texture", material->getEmissiveTextureInfo(), gltfModel, material, selection, gltfLayer)) {
            modified = true;
        }
        if (renderTextureSlot("Normal Texture", material->getNormalTextureInfo(), gltfModel, material, selection, gltfLayer, true, false)) {
            modified = true;
        }
        if (renderTextureSlot("Occlusion Texture", material->getOcclusionTextureInfo(), gltfModel, material, selection, gltfLayer, false, true)) {
            modified = true;
        }
        prop::endGroup();
    }

    // -----------------------------------------------------------------
    // PBR Metallic Roughness
    // -----------------------------------------------------------------
    if (material->getPbrMetallicRoughness().has_value()) {
        const bool pbrOpen = prop::group(ICON_MDI_SPHERE, "PBR Metallic Roughness", true);
        UiProbe::item("material.section.pbr", material->m_ID);
        if (pbrOpen) {
            auto& mr = material->getPbrMetallicRoughness().value();

            auto baseD = mr.getBaseColorFactorOrDefault();
            float baseColor[4] = {
                static_cast<float>(baseD.r),
                static_cast<float>(baseD.g),
                static_cast<float>(baseD.b),
                static_cast<float>(baseD.a)
            };
            prop::row(ICON_MDI_PALETTE, "Base Color Factor");
            if (ImGui::ColorEdit4("##baseColorFactor", baseColor)) {
                mr.setBaseColorFactor(ux3d::math::dvec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]));
                modified = true;
            }
            UiProbe::item("material.base.color.factor", material->m_ID);

            if (renderTextureSlot("Base Color Texture", mr.getBaseColorTextureInfo(), gltfModel, material, selection, gltfLayer)) {
                modified = true;
            }

            float metallic = static_cast<float>(mr.getMetallicFactorOrDefault());
            prop::row(ICON_MDI_GRADIENT_HORIZONTAL, "Metallic Factor");
            ImGui::PushFont(getMonoFont());
            if (ImGui::SliderFloat("##metallicFactor", &metallic, 0.0f, 1.0f, "%.2f")) {
                mr.setMetallicFactor(static_cast<double>(metallic));
                modified = true;
            }

            float roughness = static_cast<float>(mr.getRoughnessFactorOrDefault());
            ImGui::PopFont();
            prop::row(ICON_MDI_GRADIENT_VERTICAL, "Roughness Factor");
            ImGui::PushFont(getMonoFont());
            if (ImGui::SliderFloat("##roughnessFactor", &roughness, 0.0f, 1.0f, "%.2f")) {
                mr.setRoughnessFactor(static_cast<double>(roughness));
                modified = true;
            }
            ImGui::PopFont();

            if (renderTextureSlot("Metallic-Roughness Texture", mr.getMetallicRoughnessTextureInfo(), gltfModel, material, selection, gltfLayer)) {
                modified = true;
            }
            prop::endGroup();
        }
    }

    // -----------------------------------------------------------------
    // PBR Specular Glossiness (Legacy Extension)
    // -----------------------------------------------------------------
    if (material->getPbrSpecularGlossiness().has_value()) {
        if (extensionBegin("KHR_materials_pbrSpecularGlossiness", ICON_MDI_SPHERE, "PBR Specular Glossiness", true)) {
            auto& sg = material->getPbrSpecularGlossiness().value();

            auto diffD = sg.getDiffuseFactorOrDefault();
            float diffColor[4] = { static_cast<float>(diffD.r), static_cast<float>(diffD.g), static_cast<float>(diffD.b), static_cast<float>(diffD.a) };
            prop::row(ICON_MDI_PALETTE, "Diffuse Factor");
            if (ImGui::ColorEdit4("##diffuseFactor", diffColor)) {
                sg.setDiffuseFactor(ux3d::math::dvec4(diffColor[0], diffColor[1], diffColor[2], diffColor[3]));
                modified = true;
            }

            if (renderTextureSlot("Diffuse Texture", sg.getDiffuseTextureInfo(), gltfModel, material, selection, gltfLayer)) {
                modified = true;
            }

            auto specD = sg.getSpecularFactorOrDefault();
            float specColor[3] = { static_cast<float>(specD.r), static_cast<float>(specD.g), static_cast<float>(specD.b) };
            prop::row(ICON_MDI_SHIMMER, "Specular Factor");
            if (ImGui::ColorEdit3("##specularGlossiness", specColor)) {
                sg.setSpecularFactor(ux3d::math::dvec3(specColor[0], specColor[1], specColor[2]));
                modified = true;
            }

            float gloss = static_cast<float>(sg.getGlossinessFactorOrDefault());
            prop::row(ICON_MDI_GRADIENT_VERTICAL, "Glossiness Factor");
            if (ImGui::SliderFloat("##glossinessFactor", &gloss, 0.0f, 1.0f, "%.2f")) {
                sg.setGlossinessFactor(static_cast<double>(gloss));
                modified = true;
            }

            if (renderTextureSlot("Specular-Glossiness Texture", sg.getSpecularGlossinessTextureInfo(), gltfModel, material, selection, gltfLayer)) {
                modified = true;
            }
            prop::endGroup();
        }
        extensionEnd();
    }

    // -----------------------------------------------------------------
    // MSFS & Vendor Extensions
    // -----------------------------------------------------------------
    bool hasVendorExts = !material->getUnknownExtensions().empty();
    std::string vendorHeader = "MSFS & Vendor Extensions (" + std::to_string(material->getUnknownExtensions().size()) + ")";
    if (prop::group(ICON_MDI_PUZZLE, vendorHeader.c_str(), hasVendorExts)) {
        if (hasVendorExts) {
            if (ExtensionInspector::instance().renderExtensions(material, gltfModel, revisions)) {
                modified = true;
            }
        } else {
            prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Extensions", "No vendor extensions defined on this material.", "None");
        }
        prop::endGroup();
    }

    // -----------------------------------------------------------------
    // Khronos Material Extensions
    // -----------------------------------------------------------------
    bool hasKhronosExt = material->isUnlit() ||
                         material->getClearcoat().has_value() ||
                         material->getTransmission().has_value() ||
                         material->getVolume().has_value() ||
                         material->getIor().has_value() ||
                         material->getSpecular().has_value() ||
                         material->getSheen().has_value() ||
                         material->getIridescence().has_value() ||
                         material->getAnisotropy().has_value() ||
                         material->getDispersion().has_value() ||
                         material->getDiffuseTransmission().has_value() ||
                         material->getDiffuseRoughness().has_value();

    std::string khronosHeader = "Khronos Extensions" + std::string(hasKhronosExt ? " (Active)" : "");
    if (prop::group(ICON_MDI_PUZZLE_OUTLINE, khronosHeader.c_str(), hasKhronosExt)) {
        // KHR_materials_unlit
        if (material->isUnlit()) {
            if (extensionBegin("KHR_materials_unlit", ICON_MDI_LIGHTBULB_OUTLINE, "Unlit", false)) {
                prop::endGroup();
            }
            extensionEnd();
        }

        // KHR_materials_clearcoat
        if (material->getClearcoat().has_value()) {
            if (extensionBegin("KHR_materials_clearcoat", ICON_MDI_LAYERS_OUTLINE, "Clearcoat", true)) {
                auto& cc = material->getClearcoat().value();
                float ccFactor = static_cast<float>(cc.getClearcoatFactorOrDefault());
                prop::row(ICON_MDI_LAYERS_OUTLINE, "Clearcoat Factor");
                if (ImGui::SliderFloat("##clearcoatFactor", &ccFactor, 0.0f, 1.0f, "%.2f")) {
                    cc.setClearcoatFactor(static_cast<double>(ccFactor));
                    modified = true;
                }
                UiProbe::item("khr.KHR_materials_clearcoat.clearcoatFactor", material->m_ID);
                float ccRoughness = static_cast<float>(cc.getClearcoatRoughnessFactorOrDefault());
                prop::row(ICON_MDI_GRADIENT_VERTICAL, "Clearcoat Roughness");
                if (ImGui::SliderFloat("##clearcoatRoughness", &ccRoughness, 0.0f, 1.0f, "%.2f")) {
                    cc.setClearcoatRoughnessFactor(static_cast<double>(ccRoughness));
                    modified = true;
                }
                if (renderTextureSlot("Clearcoat Texture", cc.getClearcoatTexture(), gltfModel, material, selection, gltfLayer)) modified = true;
                if (renderTextureSlot("Clearcoat Roughness Texture", cc.getClearcoatRoughnessTexture(), gltfModel, material, selection, gltfLayer)) modified = true;
                if (renderTextureSlot("Clearcoat Normal Texture", cc.getClearcoatNormalTexture(), gltfModel, material, selection, gltfLayer, true, false)) modified = true;
                prop::endGroup();
            }
            extensionEnd();
        }

        // KHR_materials_transmission
        if (material->getTransmission().has_value()) {
            if (extensionBegin("KHR_materials_transmission", ICON_MDI_GLASS_FRAGILE, "Transmission", true)) {
                auto& trans = material->getTransmission().value();
                float transFactor = static_cast<float>(trans.getTransmissionFactorOrDefault());
                prop::row(ICON_MDI_GLASS_FRAGILE, "Transmission Factor");
                if (ImGui::SliderFloat("##transmissionFactor", &transFactor, 0.0f, 1.0f, "%.2f")) {
                    trans.setTransmissionFactor(static_cast<double>(transFactor));
                    modified = true;
                }
                UiProbe::item("khr.KHR_materials_transmission.transmissionFactor", material->m_ID);
                if (renderTextureSlot("Transmission Texture", trans.getTransmissionTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                prop::endGroup();
            }
            extensionEnd();
        }

        // KHR_materials_volume
        if (material->getVolume().has_value()) {
            if (extensionBegin("KHR_materials_volume", ICON_MDI_CUBE_OUTLINE, "Volume", true)) {
                auto& vol = material->getVolume().value();
                float thick = static_cast<float>(vol.getThicknessFactorOrDefault());
                prop::row(ICON_MDI_ARROW_EXPAND_VERTICAL, "Thickness Factor");
                if (ImGui::DragFloat("##thicknessFactor", &thick, 0.05f, 0.0f, 100.0f, "%.2f")) {
                    vol.setThicknessFactor(static_cast<double>(thick));
                    modified = true;
                }
                float attDist = static_cast<float>(vol.getAttenuationDistance().value_or(100.0));
                prop::row(ICON_MDI_RULER, "Attenuation Distance");
                if (ImGui::DragFloat("##attenuationDistance", &attDist, 0.1f, 0.001f, 1000.0f, "%.2f")) {
                    vol.setAttenuationDistance(static_cast<double>(attDist));
                    modified = true;
                }
                auto attColorD = vol.getAttenuationColorOrDefault();
                float attColor[3] = { static_cast<float>(attColorD.r), static_cast<float>(attColorD.g), static_cast<float>(attColorD.b) };
                prop::row(ICON_MDI_PALETTE, "Attenuation Color");
                if (ImGui::ColorEdit3("##attenuationColor", attColor)) {
                    vol.setAttenuationColor(ux3d::math::dvec3(attColor[0], attColor[1], attColor[2]));
                    modified = true;
                }
                if (renderTextureSlot("Thickness Texture", vol.getThicknessTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                prop::endGroup();
            }
            extensionEnd();
        }

        // KHR_materials_ior
        if (material->getIor().has_value()) {
            if (extensionBegin("KHR_materials_ior", ICON_MDI_DIAMOND_STONE, "IOR", true)) {
                auto& ior = material->getIor().value();
                float iorVal = static_cast<float>(ior.getIorOrDefault());
                const char* iorTip = "Index of refraction. Common presets: Air 1.0, Water 1.33, Glass 1.5, Diamond 2.4";
                prop::row(ICON_MDI_DIAMOND_STONE, "IOR", iorTip);
                if (ImGui::DragFloat("##ior", &iorVal, 0.01f, 1.0f, 3.0f, "%.3f")) {
                    ior.setIor(static_cast<double>(iorVal));
                    modified = true;
                }
                prop::tooltip(iorTip);
                prop::endGroup();
            }
            extensionEnd();
        }

        // KHR_materials_dispersion: 20 / Abbe number; only volumetric refraction (transmission + volume) disperses.
        if (material->getDispersion().has_value()) {
            if (extensionBegin("KHR_materials_dispersion", ICON_MDI_SHIMMER, "Dispersion", true)) {
                auto& disp = material->getDispersion().value();
                float dispersion = static_cast<float>(disp.getDispersionOrDefault());
                const char* dispTip = "20 / Abbe number (0 = no dispersion). Splits the refracted image into colours.";
                prop::row(ICON_MDI_SHIMMER, "Dispersion", dispTip);
                if (ImGui::DragFloat("##dispersion", &dispersion, 0.01f, 0.0f, 20.0f, "%.3f")) {
                    disp.setDispersion(static_cast<double>(std::max(dispersion, 0.0f)));
                    modified = true;
                }
                UiProbe::item("khr.KHR_materials_dispersion.dispersion", material->m_ID);
                prop::tooltip(dispTip);
                if (disp.getDispersionOrDefault() > 0.0) {
                    prop::textRow(ICON_MDI_NUMERIC, "Abbe Number", nullptr, "%.1f", 20.0 / disp.getDispersionOrDefault());
                }
                if (!material->getTransmission().has_value() || !material->getVolume().has_value()) {
                    prop::colorTextRow(ICON_MDI_ALERT_OUTLINE, "Effect", "Needs KHR_materials_transmission and KHR_materials_volume.",
                                       ImVec4(0.95f, 0.70f, 0.30f, 1.0f), "No effect: needs transmission and volume");
                }
                prop::endGroup();
            }
            extensionEnd();
        }

        // KHR_materials_specular
        if (material->getSpecular().has_value()) {
            if (extensionBegin("KHR_materials_specular", ICON_MDI_SHIMMER, "Specular", true)) {
                auto& spec = material->getSpecular().value();
                float specFactor = static_cast<float>(spec.getSpecularFactorOrDefault());
                prop::row(ICON_MDI_SHIMMER, "Specular Factor");
                if (ImGui::SliderFloat("##specularFactor", &specFactor, 0.0f, 1.0f, "%.2f")) {
                    spec.setSpecularFactor(static_cast<double>(specFactor));
                    modified = true;
                }
                auto specColorD = spec.getSpecularColorFactorOrDefault();
                float specColor[3] = { static_cast<float>(specColorD.r), static_cast<float>(specColorD.g), static_cast<float>(specColorD.b) };
                prop::row(ICON_MDI_PALETTE, "Specular Color Factor");
                if (ImGui::ColorEdit3("##specularColorFactor", specColor)) {
                    spec.setSpecularColorFactor(ux3d::math::dvec3(specColor[0], specColor[1], specColor[2]));
                    modified = true;
                }
                if (renderTextureSlot("Specular Texture", spec.getSpecularTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                if (renderTextureSlot("Specular Color Texture", spec.getSpecularColorTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                prop::endGroup();
            }
            extensionEnd();
        }

        // MSFT_lod / MSFT_packing_*: read-only summaries (kept as vendor JSON).
        if (const auto lod = ux3d::gltf::msft::lod(*material); lod.present) {
            if (prop::group(ICON_MDI_STAIRS_DOWN, "Levels of Detail", true, "MSFT_lod (this material is LOD0, rendered).")) {
                for (size_t i = 0; i < lod.ids.size(); ++i) {
                    const std::string key = "LOD" + std::to_string(i + 1);
                    prop::textRow(ICON_MDI_STAIRS, key.c_str(), nullptr, "material %d%s", lod.ids[i],
                                  i + 1 < lod.screenCoverage.size() ? (", screen coverage " + std::to_string(lod.screenCoverage[i + 1]).substr(0, 6)).c_str() : "");
                }
                prop::endGroup();
            }
        }
        int packingIndex = 0;
        for (const std::string& line : ux3d::gltf::msft::packingSummary(*material)) {
            ImGui::PushID(packingIndex++);
            prop::textRow(ICON_MDI_PACKAGE_VARIANT, "Packing", line.c_str(), "%s", line.c_str());
            ImGui::PopID();
        }

        // KHR_materials_diffuse_transmission: the diffuse lobe's share transmitted through the (thin) surface.
        if (material->getDiffuseTransmission().has_value()) {
            if (extensionBegin(ux3d::gltf::DiffuseTransmission::ExtensionName, ICON_MDI_LIGHTBULB_ON_OUTLINE, "Diffuse Transmission", true)) {
                auto& dt = material->getDiffuseTransmission().value();
                float dtFactor = static_cast<float>(dt.getDiffuseTransmissionFactorOrDefault());
                const char* dtTip = "Share of the diffuse light that passes through the surface (0 = opaque Lambert, 1 = all "
                                    "diffuse light is transmitted). Light from behind shows on the front.";
                prop::row(ICON_MDI_LIGHTBULB_ON_OUTLINE, "Diffuse Transmission Factor", dtTip);
                if (ImGui::SliderFloat("##diffuseTransmissionFactor", &dtFactor, 0.0f, 1.0f, "%.3f")) {
                    dt.setDiffuseTransmissionFactor(static_cast<double>(std::clamp(dtFactor, 0.0f, 1.0f)));
                    modified = true;
                }
                UiProbe::item("khr.KHR_materials_diffuse_transmission.diffuseTransmissionFactor", material->m_ID);
                prop::tooltip(dtTip);
                auto dtColorD = dt.getDiffuseTransmissionColorFactorOrDefault();
                float dtColor[3] = { static_cast<float>(dtColorD.r), static_cast<float>(dtColorD.g), static_cast<float>(dtColorD.b) };
                prop::row(ICON_MDI_PALETTE, "Diffuse Transmission Color");
                if (ImGui::ColorEdit3("##diffuseTransmissionColor", dtColor)) {
                    dt.setDiffuseTransmissionColorFactor(ux3d::math::dvec3(std::clamp(dtColor[0], 0.0f, 1.0f),
                        std::clamp(dtColor[1], 0.0f, 1.0f), std::clamp(dtColor[2], 0.0f, 1.0f)));
                    modified = true;
                }
                UiProbe::item("khr.KHR_materials_diffuse_transmission.diffuseTransmissionColorFactor", material->m_ID);
                if (renderTextureSlot("Diffuse Transmission Texture (A)", dt.getDiffuseTransmissionTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                if (renderTextureSlot("Diffuse Transmission Color Texture", dt.getDiffuseTransmissionColorTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                if (material->getTransmission().has_value()) {
                    prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Note", "KHR_materials_transmission overrides it by the transmission factor.",
                                  "Overridden by transmission");
                }
                prop::endGroup();
            }
            extensionEnd();
        }

        // KHR_materials_diffuse_roughness: Oren-Nayar (EON) roughness of the diffuse lobe.
        if (material->getDiffuseRoughness().has_value()) {
            if (extensionBegin(ux3d::gltf::DiffuseRoughness::ExtensionName, ICON_MDI_GRADIENT_VERTICAL, "Diffuse Roughness", true)) {
                auto& dr = material->getDiffuseRoughness().value();
                float drFactor = static_cast<float>(dr.getDiffuseRoughnessFactorOrDefault());
                const char* drTip = "0 = Lambert; higher values flatten the diffuse shading (energy-preserving Oren-Nayar, "
                                    "sun and punctual lights; the IBL diffuse stays Lambert).";
                prop::row(ICON_MDI_GRADIENT_VERTICAL, "Diffuse Roughness Factor", drTip);
                if (ImGui::SliderFloat("##diffuseRoughnessFactor", &drFactor, 0.0f, 1.0f, "%.3f")) {
                    dr.setDiffuseRoughnessFactor(static_cast<double>(std::clamp(drFactor, 0.0f, 1.0f)));
                    modified = true;
                }
                UiProbe::item("khr.KHR_materials_diffuse_roughness.diffuseRoughnessFactor", material->m_ID);
                prop::tooltip(drTip);
                if (renderTextureSlot("Diffuse Roughness Texture (R)", dr.getDiffuseRoughnessTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                prop::endGroup();
            }
            extensionEnd();
        }

        // KHR_materials_sheen
        if (material->getSheen().has_value()) {
            if (extensionBegin("KHR_materials_sheen", ICON_MDI_SHIMMER, "Sheen", true)) {
                auto& sheen = material->getSheen().value();
                auto sheenColorD = sheen.getSheenColorFactorOrDefault();
                float sheenColor[3] = { static_cast<float>(sheenColorD.r), static_cast<float>(sheenColorD.g), static_cast<float>(sheenColorD.b) };
                prop::row(ICON_MDI_PALETTE, "Sheen Color Factor");
                if (ImGui::ColorEdit3("##sheenColorFactor", sheenColor)) {
                    sheen.setSheenColorFactor(ux3d::math::dvec3(sheenColor[0], sheenColor[1], sheenColor[2]));
                    modified = true;
                }
                float sheenRough = static_cast<float>(sheen.getSheenRoughnessFactorOrDefault());
                prop::row(ICON_MDI_GRADIENT_VERTICAL, "Sheen Roughness");
                if (ImGui::SliderFloat("##sheenRoughness", &sheenRough, 0.0f, 1.0f, "%.2f")) {
                    sheen.setSheenRoughnessFactor(static_cast<double>(sheenRough));
                    modified = true;
                }
                if (renderTextureSlot("Sheen Color Texture", sheen.getSheenColorTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                if (renderTextureSlot("Sheen Roughness Texture", sheen.getSheenRoughnessTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                prop::endGroup();
            }
            extensionEnd();
        }
        if (material->getIridescence().has_value()) {
            if (extensionBegin("KHR_materials_iridescence", ICON_MDI_SHIMMER, "Iridescence", true)) {
                auto& iri = material->getIridescence().value();
                float factor = static_cast<float>(iri.getIridescenceFactorOrDefault());
                prop::row(ICON_MDI_SHIMMER, "Iridescence Factor");
                if (ImGui::SliderFloat("##iridescenceFactor", &factor, 0.0f, 1.0f)) {
                    iri.setIridescenceFactor(factor);
                    modified = true;
                }
                float ior = static_cast<float>(iri.getIridescenceIorOrDefault());
                prop::row(ICON_MDI_DIAMOND_STONE, "Iridescence IOR");
                if (ImGui::DragFloat("##iridescenceIor", &ior, 0.01f, 1.0f, 3.0f)) {
                    iri.setIridescenceIor(ior);
                    modified = true;
                }
                float lo = static_cast<float>(iri.getIridescenceThicknessMinimumOrDefault());
                float hi = static_cast<float>(iri.getIridescenceThicknessMaximumOrDefault());
                prop::row(ICON_MDI_ARROW_COLLAPSE_ALL, "Minimum Thickness", "Nanometres.");
                if (ImGui::DragFloat("##minimumThickness", &lo, 1.0f, 0.0f, hi)) {
                    iri.setIridescenceThicknessMinimum(lo);
                    modified = true;
                }
                prop::row(ICON_MDI_ARROW_EXPAND_ALL, "Maximum Thickness", "Nanometres.");
                if (ImGui::DragFloat("##maximumThickness", &hi, 1.0f, lo, 10000.0f)) {
                    iri.setIridescenceThicknessMaximum(hi);
                    modified = true;
                }
                if (renderTextureSlot("Iridescence Texture", iri.getIridescenceTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                if (renderTextureSlot("Iridescence Thickness Texture", iri.getIridescenceThicknessTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                prop::endGroup();
            }
            extensionEnd();
        }
        // KHR_materials_anisotropy (A7.1)
        if (material->getAnisotropy().has_value()) {
            if (extensionBegin("KHR_materials_anisotropy", ICON_MDI_ROTATE_3D_VARIANT, "Anisotropy", true)) {
                auto& aniso = material->getAnisotropy().value();
                float strength = static_cast<float>(aniso.getAnisotropyStrengthOrDefault());
                prop::row(ICON_MDI_BRIGHTNESS_6, "Anisotropy Strength");
                if (ImGui::SliderFloat("##anisotropyStrength", &strength, 0.0f, 1.0f, "%.3f")) {
                    aniso.setAnisotropyStrength(static_cast<double>(std::clamp(strength, 0.0f, 1.0f)));
                    modified = true;
                }
                UiProbe::item("khr.anisotropy.strength", material->m_ID);
                float degrees = static_cast<float>(aniso.getAnisotropyRotationOrDefault() * 180.0 / 3.14159265358979323846);
                const char* rotTip = "Counter-clockwise from the tangent; stored in radians.";
                prop::row(ICON_MDI_ROTATE_3D_VARIANT, "Anisotropy Rotation", rotTip);
                if (ImGui::DragFloat("##anisotropyRotation", &degrees, 0.5f, -360.0f, 360.0f, "%.1f deg")) {
                    aniso.setAnisotropyRotation(static_cast<double>(degrees) * 3.14159265358979323846 / 180.0);
                    modified = true;
                }
                prop::tooltip(rotTip);
                if (renderTextureSlot("Anisotropy Texture", aniso.getAnisotropyTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
                prop::endGroup();
            }
            extensionEnd();
        }
        prop::endGroup();
    }

    if (modified) {
        revisions.mark(SceneChange::Material);
    }

    return modified;
}

} // namespace ux3d::imgui
