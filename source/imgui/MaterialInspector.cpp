// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/MaterialInspector.h"
#include "imgui/ElementInspector.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/TextureUtils.h"
#include "imgui/UiProbe.h"
#include "imgui/ViewportInstance.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/Theme.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include <imgui.h>
#include <algorithm>
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

    if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        if (!hasTexture) {
            ImGui::TextDisabled("None");
            ImGui::TreePop();
            ImGui::PopID();
            return false;
        }

        constexpr float previewSize = 64.0f;
        const std::string previewProbe = makeProbeTag("material.texture.", label);
        const unsigned int previewId = source != nullptr
            ? ElementInspector::getImagePreviewTextureId(source, gltfLayer, showScale)
            : 0;

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
            ImGui::SetTooltip("Inspect %s", getTextureDisplayName(texture, source).c_str());
        }
        if (previewClicked) {
            selection.selectTextureFromMaterial(texture, material);
        }
        UiProbe::item(previewProbe.c_str(), 0);

        ImGui::SameLine();
        ImGui::BeginGroup();
        if (hasTexture) {
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
            }

            const uint32_t texCoord = texInfo->getTexCoordOrDefault();
            const std::string texCoordLabel = "TEXCOORD_" + std::to_string(texCoord);
            const TexCoordAccessorUsage usage = findTexCoordAccessor(gltfModel, material, texCoord);
            const bool canInspectTexCoord = usage.accessor != nullptr;
            if (!canInspectTexCoord) {
                ImGui::BeginDisabled();
            }
            const std::string texCoordButton = ICON_MDI_VECTOR_SQUARE " " + texCoordLabel + "##InspectTexCoord";
            if (ImGui::Button(texCoordButton.c_str()) && canInspectTexCoord) {
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
        } else {
            ImGui::TextDisabled("None");
        }
        ImGui::EndGroup();

        if (hasTexture) {
            // Normal map scale
            if (showScale) {
                ImGui::PushFont(getMonoFont());
                float scale = static_cast<float>(texInfo->getScale());
                if (ImGui::DragFloat("Scale", &scale, 0.02f, -2.0f, 2.0f, "%.2f")) {
                    texInfo->setScale(static_cast<double>(scale));
                    modified = true;
                }
                ImGui::PopFont();
                ImGui::SameLine();
                if (ImGui::Button("Flip Normal Y")) {
                    scale = -scale;
                    texInfo->setScale(static_cast<double>(scale));
                    modified = true;
                }
            }

            // Occlusion strength
            if (showStrength) {
                ImGui::PushFont(getMonoFont());
                float strength = static_cast<float>(texInfo->getStrength());
                if (ImGui::SliderFloat("Strength", &strength, 0.0f, 2.0f, "%.2f")) {
                    texInfo->setStrength(static_cast<double>(strength));
                    modified = true;
                }
                ImGui::PopFont();
            }
        }

        ImGui::TreePop();
    }

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
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No material selected.");
        return false;
    }

    bool modified = false;
    auto extensionHeader = [&](const char* extension) {
        ImGui::PushID(extension);
        ImGui::TextUnformatted(extension);
        UiProbe::item((std::string("khr.header.") + extension).c_str(), material->m_ID);
        ImGui::SameLine();
        auto* viewport = ExtensionInspector::instance().getActiveViewport();
        bool applied = viewport == nullptr || viewport->settings.materialExtensions.applies(material->m_ID, extension);
        ImGui::BeginDisabled(viewport == nullptr);
        if (ImGui::Checkbox("##viewportToggle", &applied)) {
            viewport->settings.materialExtensions.setApplied(material->m_ID, extension, applied);
            revisions.mark(SceneChange::ViewportSettings);
        }
        UiProbe::item((std::string("khr.display.") + extension).c_str(), material->m_ID, false, applied);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Display only: turns this extension off for this material in the active viewport; the file is unchanged.");
        }
        ImGui::PopID();
    };

    // Header: Name and ID
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Material ID: %u", material->m_ID);

    char nameBuf[256] = {};
    if (material->hasName()) {
        std::strncpy(nameBuf, material->getName().c_str(), sizeof(nameBuf) - 1);
    }
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        material->setName(nameBuf);
        modified = true;
    }

    ImGui::Separator();

    // -----------------------------------------------------------------
    // General Material Settings
    // -----------------------------------------------------------------
    const bool generalOpen = ImGui::CollapsingHeader("General Properties", ImGuiTreeNodeFlags_DefaultOpen);
    UiProbe::item("material.section.general", material->m_ID);
    if (generalOpen) {
        // Double Sided
        bool doubleSided = material->isDoubleSidedOrDefault();
        if (ImGui::Checkbox("Double Sided", &doubleSided)) {
            material->setDoubleSided(doubleSided);
            modified = true;
        }

        // Alpha Mode
        int alphaMode = 0;
        if (material->getAlphaModeOrDefault() == ux3d::gltf::AlphaMode::Mask) alphaMode = 1;
        else if (material->getAlphaModeOrDefault() == ux3d::gltf::AlphaMode::Blend) alphaMode = 2;

        const char* alphaModes[] = { "Opaque", "Mask", "Blend" };
        if (ImGui::Combo("Alpha Mode", &alphaMode, alphaModes, 3)) {
            if (alphaMode == 0) material->setAlphaMode(ux3d::gltf::AlphaMode::Opaque);
            else if (alphaMode == 1) material->setAlphaMode(ux3d::gltf::AlphaMode::Mask);
            else if (alphaMode == 2) material->setAlphaMode(ux3d::gltf::AlphaMode::Blend);
            modified = true;
        }

        // Alpha Cutoff (only visible in Mask mode)
        if (alphaMode == 1) {
            ImGui::PushFont(getMonoFont());
            float cutoff = static_cast<float>(material->getAlphaCutoffOrDefault());
            if (ImGui::SliderFloat("Alpha Cutoff", &cutoff, 0.0f, 1.0f, "%.2f")) {
                material->setAlphaCutoff(static_cast<double>(cutoff));
                modified = true;
            }
            ImGui::PopFont();
        }

        // Emissive Factor
        auto emissiveD = material->getEmissiveFactorOrDefault();
        float emissive[3] = { static_cast<float>(emissiveD.r), static_cast<float>(emissiveD.g), static_cast<float>(emissiveD.b) };
        if (ImGui::ColorEdit3("Emissive Factor", emissive, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float)) {
            material->setEmissiveFactor(ux3d::math::dvec3(emissive[0], emissive[1], emissive[2]));
            modified = true;
        }
        float maxEmissive = std::max({ emissive[0], emissive[1], emissive[2] });
        if (maxEmissive > 1.0f) {
            float intensity = maxEmissive;
            ImGui::PushFont(getMonoFont());
            if (ImGui::DragFloat("Emissive Intensity", &intensity, std::max(1.0f, intensity * 0.01f), 0.0f, 200000.0f, "%.1f")) {
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
            extensionHeader("KHR_materials_emissive_strength");
            ImGui::PushFont(getMonoFont());
            float strength = static_cast<float>(material->getEmissiveStrength()->getEmissiveStrengthOrDefault());
            if (ImGui::DragFloat("Emissive Strength", &strength, 0.1f, 0.0f, 1000.0f, "%.1f")) {
                material->getEmissiveStrength()->setEmissiveStrength(static_cast<double>(strength));
                modified = true;
            }
            ImGui::PopFont();
        }

        // Emissive Texture Slot
        if (renderTextureSlot("Emissive Texture", material->getEmissiveTextureInfo(), gltfModel, material, selection, gltfLayer)) {
            modified = true;
        }

        // Normal Texture Slot
        if (renderTextureSlot("Normal Texture", material->getNormalTextureInfo(), gltfModel, material, selection, gltfLayer, true, false)) {
            modified = true;
        }

        // Occlusion Texture Slot
        if (renderTextureSlot("Occlusion Texture", material->getOcclusionTextureInfo(), gltfModel, material, selection, gltfLayer, false, true)) {
            modified = true;
        }
    }

    // -----------------------------------------------------------------
    // PBR Metallic Roughness
    // -----------------------------------------------------------------
    if (material->getPbrMetallicRoughness().has_value()) {
        const bool pbrOpen = ImGui::CollapsingHeader("PBR Metallic Roughness", ImGuiTreeNodeFlags_DefaultOpen);
        UiProbe::item("material.section.pbr", material->m_ID);
        if (pbrOpen) {
            auto& mr = material->getPbrMetallicRoughness().value();

            // Base Color Factor
            auto baseD = mr.getBaseColorFactorOrDefault();
            float baseColor[4] = {
                static_cast<float>(baseD.r),
                static_cast<float>(baseD.g),
                static_cast<float>(baseD.b),
                static_cast<float>(baseD.a)
            };
            if (ImGui::ColorEdit4("Base Color Factor", baseColor)) {
                mr.setBaseColorFactor(ux3d::math::dvec4(baseColor[0], baseColor[1], baseColor[2], baseColor[3]));
                modified = true;
            }

            UiProbe::item("material.base.color.factor", material->m_ID);

            // Base Color Texture Slot
            if (renderTextureSlot("Base Color Texture", mr.getBaseColorTextureInfo(), gltfModel, material, selection, gltfLayer)) {
                modified = true;
            }

            // Metallic Factor
            ImGui::PushFont(getMonoFont());
            float metallic = static_cast<float>(mr.getMetallicFactorOrDefault());
            if (ImGui::SliderFloat("Metallic Factor", &metallic, 0.0f, 1.0f, "%.2f")) {
                mr.setMetallicFactor(static_cast<double>(metallic));
                modified = true;
            }

            // Roughness Factor
            float roughness = static_cast<float>(mr.getRoughnessFactorOrDefault());
            if (ImGui::SliderFloat("Roughness Factor", &roughness, 0.0f, 1.0f, "%.2f")) {
                mr.setRoughnessFactor(static_cast<double>(roughness));
                modified = true;
            }
            ImGui::PopFont();

            // Metallic Roughness Texture Slot
            if (renderTextureSlot("Metallic-Roughness Texture", mr.getMetallicRoughnessTextureInfo(), gltfModel, material, selection, gltfLayer)) {
                modified = true;
            }
        }
    }

    // -----------------------------------------------------------------
    // PBR Specular Glossiness (Legacy Extension)
    // -----------------------------------------------------------------
    if (material->getPbrSpecularGlossiness().has_value()) {
        if (ImGui::CollapsingHeader("PBR Specular Glossiness", ImGuiTreeNodeFlags_DefaultOpen)) {
            extensionHeader("KHR_materials_pbrSpecularGlossiness");
            auto& sg = material->getPbrSpecularGlossiness().value();

            auto diffD = sg.getDiffuseFactorOrDefault();
            float diffColor[4] = { static_cast<float>(diffD.r), static_cast<float>(diffD.g), static_cast<float>(diffD.b), static_cast<float>(diffD.a) };
            if (ImGui::ColorEdit4("Diffuse Factor", diffColor)) {
                sg.setDiffuseFactor(ux3d::math::dvec4(diffColor[0], diffColor[1], diffColor[2], diffColor[3]));
                modified = true;
            }

            if (renderTextureSlot("Diffuse Texture", sg.getDiffuseTextureInfo(), gltfModel, material, selection, gltfLayer)) {
                modified = true;
            }

            auto specD = sg.getSpecularFactorOrDefault();
            float specColor[3] = { static_cast<float>(specD.r), static_cast<float>(specD.g), static_cast<float>(specD.b) };
            if (ImGui::ColorEdit3("Specular Factor", specColor)) {
                sg.setSpecularFactor(ux3d::math::dvec3(specColor[0], specColor[1], specColor[2]));
                modified = true;
            }

            float gloss = static_cast<float>(sg.getGlossinessFactorOrDefault());
            if (ImGui::SliderFloat("Glossiness Factor", &gloss, 0.0f, 1.0f, "%.2f")) {
                sg.setGlossinessFactor(static_cast<double>(gloss));
                modified = true;
            }

            if (renderTextureSlot("Specular-Glossiness Texture", sg.getSpecularGlossinessTextureInfo(), gltfModel, material, selection, gltfLayer)) {
                modified = true;
            }
        }
    }

    // -----------------------------------------------------------------
    // MSFS & Vendor Extensions
    // -----------------------------------------------------------------
    bool hasVendorExts = !material->getUnknownExtensions().empty();
    std::string vendorHeader = "MSFS & Vendor Extensions (" + std::to_string(material->getUnknownExtensions().size()) + ")";
    if (ImGui::CollapsingHeader(vendorHeader.c_str(), hasVendorExts ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        if (hasVendorExts) {
            if (ExtensionInspector::instance().renderExtensions(material, gltfModel, revisions)) {
                modified = true;
            }
        } else {
            ImGui::TextDisabled("No vendor extensions defined on this material.");
        }
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
                         material->getIridescence().has_value();

    std::string khronosHeader = "Khronos Extensions" + std::string(hasKhronosExt ? " (Active)" : "");
    if (ImGui::CollapsingHeader(khronosHeader.c_str(), hasKhronosExt ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        // KHR_materials_unlit
        if (material->isUnlit()) extensionHeader("KHR_materials_unlit");

        // KHR_materials_clearcoat
        if (material->getClearcoat().has_value()) {
            extensionHeader("KHR_materials_clearcoat");
            ImGui::Indent(15.0f);
            auto& cc = material->getClearcoat().value();
            float ccFactor = static_cast<float>(cc.getClearcoatFactorOrDefault());
            if (ImGui::SliderFloat("Clearcoat Factor", &ccFactor, 0.0f, 1.0f, "%.2f")) {
                cc.setClearcoatFactor(static_cast<double>(ccFactor));
                modified = true;
            }
            UiProbe::item("khr.KHR_materials_clearcoat.clearcoatFactor", material->m_ID);
            float ccRoughness = static_cast<float>(cc.getClearcoatRoughnessFactorOrDefault());
            if (ImGui::SliderFloat("Clearcoat Roughness", &ccRoughness, 0.0f, 1.0f, "%.2f")) {
                cc.setClearcoatRoughnessFactor(static_cast<double>(ccRoughness));
                modified = true;
            }
            if (renderTextureSlot("Clearcoat Texture", cc.getClearcoatTexture(), gltfModel, material, selection, gltfLayer)) modified = true;
            if (renderTextureSlot("Clearcoat Roughness Texture", cc.getClearcoatRoughnessTexture(), gltfModel, material, selection, gltfLayer)) modified = true;
            if (renderTextureSlot("Clearcoat Normal Texture", cc.getClearcoatNormalTexture(), gltfModel, material, selection, gltfLayer, true, false)) modified = true;
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_transmission
        if (material->getTransmission().has_value()) {
            extensionHeader("KHR_materials_transmission");
            ImGui::Indent(15.0f);
            auto& trans = material->getTransmission().value();
            float transFactor = static_cast<float>(trans.getTransmissionFactorOrDefault());
            if (ImGui::SliderFloat("Transmission Factor", &transFactor, 0.0f, 1.0f, "%.2f")) {
                trans.setTransmissionFactor(static_cast<double>(transFactor));
                modified = true;
            }
            UiProbe::item("khr.KHR_materials_transmission.transmissionFactor", material->m_ID);
            if (renderTextureSlot("Transmission Texture", trans.getTransmissionTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_volume
        if (material->getVolume().has_value()) {
            extensionHeader("KHR_materials_volume");
            ImGui::Indent(15.0f);
            auto& vol = material->getVolume().value();
            float thick = static_cast<float>(vol.getThicknessFactorOrDefault());
            if (ImGui::DragFloat("Thickness Factor", &thick, 0.05f, 0.0f, 100.0f, "%.2f")) {
                vol.setThicknessFactor(static_cast<double>(thick));
                modified = true;
            }
            float attDist = static_cast<float>(vol.getAttenuationDistance().value_or(100.0));
            if (ImGui::DragFloat("Attenuation Distance", &attDist, 0.1f, 0.001f, 1000.0f, "%.2f")) {
                vol.setAttenuationDistance(static_cast<double>(attDist));
                modified = true;
            }
            auto attColorD = vol.getAttenuationColorOrDefault();
            float attColor[3] = { static_cast<float>(attColorD.r), static_cast<float>(attColorD.g), static_cast<float>(attColorD.b) };
            if (ImGui::ColorEdit3("Attenuation Color", attColor)) {
                vol.setAttenuationColor(ux3d::math::dvec3(attColor[0], attColor[1], attColor[2]));
                modified = true;
            }
            if (renderTextureSlot("Thickness Texture", vol.getThicknessTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_ior
        if (material->getIor().has_value()) {
            extensionHeader("KHR_materials_ior");
            ImGui::Indent(15.0f);
            auto& ior = material->getIor().value();
            float iorVal = static_cast<float>(ior.getIorOrDefault());
            if (ImGui::DragFloat("IOR", &iorVal, 0.01f, 1.0f, 3.0f, "%.3f")) {
                ior.setIor(static_cast<double>(iorVal));
                modified = true;
            }
            ImGui::TextDisabled("Common presets: Air 1.0, Water 1.33, Glass 1.5, Diamond 2.4");
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_specular
        if (material->getSpecular().has_value()) {
            extensionHeader("KHR_materials_specular");
            ImGui::Indent(15.0f);
            auto& spec = material->getSpecular().value();
            float specFactor = static_cast<float>(spec.getSpecularFactorOrDefault());
            if (ImGui::SliderFloat("Specular Factor", &specFactor, 0.0f, 1.0f, "%.2f")) {
                spec.setSpecularFactor(static_cast<double>(specFactor));
                modified = true;
            }
            auto specColorD = spec.getSpecularColorFactorOrDefault();
            float specColor[3] = { static_cast<float>(specColorD.r), static_cast<float>(specColorD.g), static_cast<float>(specColorD.b) };
            if (ImGui::ColorEdit3("Specular Color Factor", specColor)) {
                spec.setSpecularColorFactor(ux3d::math::dvec3(specColor[0], specColor[1], specColor[2]));
                modified = true;
            }
            if (renderTextureSlot("Specular Texture", spec.getSpecularTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
            if (renderTextureSlot("Specular Color Texture", spec.getSpecularColorTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_sheen
        if (material->getSheen().has_value()) {
            extensionHeader("KHR_materials_sheen");
            ImGui::Indent(15.0f);
            auto& sheen = material->getSheen().value();
            auto sheenColorD = sheen.getSheenColorFactorOrDefault();
            float sheenColor[3] = { static_cast<float>(sheenColorD.r), static_cast<float>(sheenColorD.g), static_cast<float>(sheenColorD.b) };
            if (ImGui::ColorEdit3("Sheen Color Factor", sheenColor)) {
                sheen.setSheenColorFactor(ux3d::math::dvec3(sheenColor[0], sheenColor[1], sheenColor[2]));
                modified = true;
            }
            float sheenRough = static_cast<float>(sheen.getSheenRoughnessFactorOrDefault());
            if (ImGui::SliderFloat("Sheen Roughness", &sheenRough, 0.0f, 1.0f, "%.2f")) {
                sheen.setSheenRoughnessFactor(static_cast<double>(sheenRough));
                modified = true;
            }
            if (renderTextureSlot("Sheen Color Texture", sheen.getSheenColorTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
            if (renderTextureSlot("Sheen Roughness Texture", sheen.getSheenRoughnessTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
            ImGui::Unindent(15.0f);
        }
        if (material->getIridescence().has_value()) {
            extensionHeader("KHR_materials_iridescence");
            auto& iri = material->getIridescence().value();
            float factor = static_cast<float>(iri.getIridescenceFactorOrDefault());
            if (ImGui::SliderFloat("Iridescence Factor", &factor, 0.0f, 1.0f)) {
                iri.setIridescenceFactor(factor);
                modified = true;
            }
            float ior = static_cast<float>(iri.getIridescenceIorOrDefault());
            if (ImGui::DragFloat("Iridescence IOR", &ior, 0.01f, 1.0f, 3.0f)) {
                iri.setIridescenceIor(ior);
                modified = true;
            }
            float lo = static_cast<float>(iri.getIridescenceThicknessMinimumOrDefault());
            float hi = static_cast<float>(iri.getIridescenceThicknessMaximumOrDefault());
            if (ImGui::DragFloat("Minimum Thickness", &lo, 1.0f, 0.0f, hi)) {
                iri.setIridescenceThicknessMinimum(lo);
                modified = true;
            }
            if (ImGui::DragFloat("Maximum Thickness", &hi, 1.0f, lo, 10000.0f)) {
                iri.setIridescenceThicknessMaximum(hi);
                modified = true;
            }
            if (renderTextureSlot("Iridescence Texture", iri.getIridescenceTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
            if (renderTextureSlot("Iridescence Thickness Texture", iri.getIridescenceThicknessTextureInfo(), gltfModel, material, selection, gltfLayer)) modified = true;
        }
    }

    if (modified) {
        revisions.mark(SceneChange::Material);
    }

    return modified;
}

} // namespace ux3d::imgui
