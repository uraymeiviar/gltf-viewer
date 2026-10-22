// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/MaterialInspector.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/TextureUtils.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/Theme.h"
#include <imgui.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

namespace ux3d::imgui {

bool MaterialInspector::renderTextureSlot(const char* label,
                                          std::optional<ux3d::gltf::TextureInfo>& texInfo,
                                          ux3d::gltf::Gltf& gltfModel,
                                          bool showScale,
                                          bool showStrength)
{
    bool modified = false;
    ImGui::PushID(label);

    bool hasTex = texInfo.has_value() && (texInfo->getTexture() != nullptr);
    auto& textures = gltfModel.getElements<ux3d::gltf::Texture>();

    if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        // Texture selection combo
        std::string currentPreview = "None";
        if (hasTex) {
            currentPreview = getTextureDisplayName(texInfo->getTexture());
        }

        if (ImGui::BeginCombo("Texture", currentPreview.c_str())) {
            bool isNoneSelected = !hasTex;
            if (ImGui::Selectable("None", isNoneSelected)) {
                if (texInfo.has_value()) {
                    texInfo->unsetTexture();
                    modified = true;
                }
            }
            if (isNoneSelected) {
                ImGui::SetItemDefaultFocus();
            }

            for (auto& tex : textures) {
                bool isSelected = hasTex && (texInfo->getTexture() == &tex);
                std::string itemLabel = getTextureDisplayName(&tex);
                if (ImGui::Selectable(itemLabel.c_str(), isSelected)) {
                    if (!texInfo.has_value()) {
                        texInfo.emplace();
                    }
                    texInfo->setTexture(tex);
                    modified = true;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (texInfo.has_value() && texInfo->getTexture() != nullptr) {
            auto* t = texInfo->getTexture();
            if (t->getSource() != nullptr) {
                auto* src = t->getSource();
                if (!src->getUri().empty()) {
                    ImGui::TextDisabled("File: %s", src->getUri().c_str());
                }
                auto res = src->getResolution();
                if (res.size() >= 2 && res[0] > 0 && res[1] > 0) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%ux%u px)", res[0], res[1]);
                }
            }

            // TexCoord index
            int texCoord = static_cast<int>(texInfo->getTexCoordOrDefault());
            const char* coordItems[] = { "TEXCOORD_0", "TEXCOORD_1" };
            if (ImGui::Combo("TexCoord", &texCoord, coordItems, 2)) {
                texInfo->setTexCoord(static_cast<uint32_t>(texCoord));
                modified = true;
            }

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

bool MaterialInspector::render(ux3d::gltf::Material* material, ux3d::gltf::Gltf& gltfModel, SceneRevisions& revisions)
{
    if (material == nullptr) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No material selected.");
        return false;
    }

    bool modified = false;

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

    // Convert Specular Glossiness to Metallic Roughness button
    if (material->getPbrSpecularGlossiness().has_value()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.45f, 0.3f, 1.0f));
        if (ImGui::Button("Convert Specular Glossiness to Metallic Roughness")) {
            auto& sg = material->getPbrSpecularGlossiness().value();
            material->createPbrMetallicRoughness();
            auto& mr = material->getPbrMetallicRoughness().value();

            // Set base color from diffuse factor
            mr.setBaseColorFactor(sg.getDiffuseFactorOrDefault());
            if (sg.getDiffuseTextureInfo().has_value()) {
                mr.createBaseColorTextureInfo();
                if (mr.getBaseColorTextureInfo().has_value()) {
                    mr.getBaseColorTextureInfo()->setTexture(*sg.getDiffuseTextureInfo()->getTexture());
                }
            }
            // Approximation for metallic / roughness
            mr.setMetallicFactor(0.0);
            mr.setRoughnessFactor(1.0 - sg.getGlossinessFactorOrDefault());

            material->destroyPbrSpecularGlossiness();
            modified = true;
        }
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // -----------------------------------------------------------------
    // General Material Settings
    // -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("General Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        if (ImGui::ColorEdit3("Emissive Factor", emissive)) {
            material->setEmissiveFactor(ux3d::math::dvec3(emissive[0], emissive[1], emissive[2]));
            modified = true;
        }

        // Emissive Strength Extension
        if (material->getEmissiveStrength().has_value()) {
            ImGui::PushFont(getMonoFont());
            float strength = static_cast<float>(material->getEmissiveStrength()->getEmissiveStrengthOrDefault());
            if (ImGui::DragFloat("Emissive Strength", &strength, 0.1f, 0.0f, 1000.0f, "%.1f")) {
                material->getEmissiveStrength()->setEmissiveStrength(static_cast<double>(strength));
                modified = true;
            }
            ImGui::PopFont();
        }

        // Emissive Texture Slot
        if (renderTextureSlot("Emissive Texture", material->getEmissiveTextureInfo(), gltfModel)) {
            modified = true;
        }

        // Normal Texture Slot
        if (renderTextureSlot("Normal Texture", material->getNormalTextureInfo(), gltfModel, true, false)) {
            modified = true;
        }

        // Occlusion Texture Slot
        if (renderTextureSlot("Occlusion Texture", material->getOcclusionTextureInfo(), gltfModel, false, true)) {
            modified = true;
        }
    }

    // -----------------------------------------------------------------
    // PBR Metallic Roughness
    // -----------------------------------------------------------------
    if (material->getPbrMetallicRoughness().has_value()) {
        if (ImGui::CollapsingHeader("PBR Metallic Roughness", ImGuiTreeNodeFlags_DefaultOpen)) {
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

            // Base Color Texture Slot
            if (renderTextureSlot("Base Color Texture", mr.getBaseColorTextureInfo(), gltfModel)) {
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
            if (renderTextureSlot("Metallic-Roughness Texture", mr.getMetallicRoughnessTextureInfo(), gltfModel)) {
                modified = true;
            }
        }
    }

    // -----------------------------------------------------------------
    // PBR Specular Glossiness (Legacy Extension)
    // -----------------------------------------------------------------
    if (material->getPbrSpecularGlossiness().has_value()) {
        if (ImGui::CollapsingHeader("PBR Specular Glossiness", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& sg = material->getPbrSpecularGlossiness().value();

            auto diffD = sg.getDiffuseFactorOrDefault();
            float diffColor[4] = { static_cast<float>(diffD.r), static_cast<float>(diffD.g), static_cast<float>(diffD.b), static_cast<float>(diffD.a) };
            if (ImGui::ColorEdit4("Diffuse Factor", diffColor)) {
                sg.setDiffuseFactor(ux3d::math::dvec4(diffColor[0], diffColor[1], diffColor[2], diffColor[3]));
                modified = true;
            }

            if (renderTextureSlot("Diffuse Texture", sg.getDiffuseTextureInfo(), gltfModel)) {
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

            if (renderTextureSlot("Specular-Glossiness Texture", sg.getSpecularGlossinessTextureInfo(), gltfModel)) {
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
            ImGui::Separator();
        } else {
            ImGui::TextDisabled("No vendor extensions defined on this material.");
        }

        // Add Extension Button
        if (ImGui::Button("+ Add MSFS Extension...")) {
            ImGui::OpenPopup("AddMsfsExtensionPopup");
        }

        if (ImGui::BeginPopup("AddMsfsExtensionPopup")) {
            ImGui::TextDisabled("Select an extension to attach:");
            ImGui::Separator();
            for (const auto& name : ExtensionInspector::instance().getAvailableExtensionNames()) {
                if (name.rfind("ASOBO_material_", 0) == 0 || name == "ASOBO_normal_map_convention" || name == "ASOBO_extra_occlusion_texture" || name == "ASOBO_occlusion_strength") {
                    std::string label = ExtensionInspector::formatExtensionLabel(name);
                    bool alreadyHas = material->getUnknownExtensions().count(name) != 0;
                    if (alreadyHas) {
                        ImGui::BeginDisabled();
                    }
                    if (ImGui::MenuItem(label.c_str())) {
                        material->setUnknownExtension(name, "{}");
                        modified = true;
                        revisions.mark(SceneChange::Material);
                    }
                    if (alreadyHas) {
                        ImGui::EndDisabled();
                    }
                }
            }
            ImGui::EndPopup();
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
                         material->getSheen().has_value();

    std::string khronosHeader = "Khronos Extensions" + std::string(hasKhronosExt ? " (Active)" : "");
    if (ImGui::CollapsingHeader(khronosHeader.c_str(), hasKhronosExt ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        // KHR_materials_unlit
        bool hasUnlit = material->isUnlit();
        if (ImGui::Checkbox("Unlit (KHR_materials_unlit)", &hasUnlit)) {
            if (hasUnlit) material->createUnlit();
            else material->destroyUnlit();
            modified = true;
        }

        // KHR_materials_clearcoat
        bool hasClearcoat = material->getClearcoat().has_value();
        if (ImGui::Checkbox("Clearcoat (KHR_materials_clearcoat)", &hasClearcoat)) {
            if (hasClearcoat) material->createClearcoat();
            else material->destroyClearcoat();
            modified = true;
        }
        if (material->getClearcoat().has_value()) {
            ImGui::Indent(15.0f);
            auto& cc = material->getClearcoat().value();
            float ccFactor = static_cast<float>(cc.getClearcoatFactorOrDefault());
            if (ImGui::SliderFloat("Clearcoat Factor", &ccFactor, 0.0f, 1.0f, "%.2f")) {
                cc.setClearcoatFactor(static_cast<double>(ccFactor));
                modified = true;
            }
            float ccRoughness = static_cast<float>(cc.getClearcoatRoughnessFactorOrDefault());
            if (ImGui::SliderFloat("Clearcoat Roughness", &ccRoughness, 0.0f, 1.0f, "%.2f")) {
                cc.setClearcoatRoughnessFactor(static_cast<double>(ccRoughness));
                modified = true;
            }
            if (renderTextureSlot("Clearcoat Texture", cc.getClearcoatTexture(), gltfModel)) modified = true;
            if (renderTextureSlot("Clearcoat Roughness Texture", cc.getClearcoatRoughnessTexture(), gltfModel)) modified = true;
            if (renderTextureSlot("Clearcoat Normal Texture", cc.getClearcoatNormalTexture(), gltfModel, true, false)) modified = true;
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_transmission
        bool hasTransmission = material->getTransmission().has_value();
        if (ImGui::Checkbox("Transmission (KHR_materials_transmission)", &hasTransmission)) {
            if (hasTransmission) material->createTransmission();
            else material->destroyTransmission();
            modified = true;
        }
        if (material->getTransmission().has_value()) {
            ImGui::Indent(15.0f);
            auto& trans = material->getTransmission().value();
            float transFactor = static_cast<float>(trans.getTransmissionFactorOrDefault());
            if (ImGui::SliderFloat("Transmission Factor", &transFactor, 0.0f, 1.0f, "%.2f")) {
                trans.setTransmissionFactor(static_cast<double>(transFactor));
                modified = true;
            }
            if (renderTextureSlot("Transmission Texture", trans.getTransmissionTextureInfo(), gltfModel)) modified = true;
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_volume
        bool hasVolume = material->getVolume().has_value();
        if (ImGui::Checkbox("Volume (KHR_materials_volume)", &hasVolume)) {
            if (hasVolume) material->createVolume();
            else material->destroyVolume();
            modified = true;
        }
        if (material->getVolume().has_value()) {
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
            if (renderTextureSlot("Thickness Texture", vol.getThicknessTextureInfo(), gltfModel)) modified = true;
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_ior
        bool hasIor = material->getIor().has_value();
        if (ImGui::Checkbox("Index of Refraction (KHR_materials_ior)", &hasIor)) {
            if (hasIor) material->createIor();
            else material->destroyIor();
            modified = true;
        }
        if (material->getIor().has_value()) {
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
        bool hasSpecular = material->getSpecular().has_value();
        if (ImGui::Checkbox("Specular (KHR_materials_specular)", &hasSpecular)) {
            if (hasSpecular) material->createSpecular();
            else material->destroySpecular();
            modified = true;
        }
        if (material->getSpecular().has_value()) {
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
            if (renderTextureSlot("Specular Texture", spec.getSpecularTextureInfo(), gltfModel)) modified = true;
            if (renderTextureSlot("Specular Color Texture", spec.getSpecularColorTextureInfo(), gltfModel)) modified = true;
            ImGui::Unindent(15.0f);
        }

        // KHR_materials_sheen
        bool hasSheen = material->getSheen().has_value();
        if (ImGui::Checkbox("Sheen (KHR_materials_sheen)", &hasSheen)) {
            if (hasSheen) material->createSheen();
            else material->destroySheen();
            modified = true;
        }
        if (material->getSheen().has_value()) {
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
            if (renderTextureSlot("Sheen Color Texture", sheen.getSheenColorTextureInfo(), gltfModel)) modified = true;
            if (renderTextureSlot("Sheen Roughness Texture", sheen.getSheenRoughnessTextureInfo(), gltfModel)) modified = true;
            ImGui::Unindent(15.0f);
        }
    }

    if (modified) {
        revisions.mark(SceneChange::Material);
    }

    return modified;
}

} // namespace ux3d::imgui
