#include "imgui/ResourcesPanel.h"
#include "imgui/TextureUtils.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include <imgui.h>
#include <string>

namespace ux3d::imgui {

void ResourcesPanel::render(ux3d::gltf::Gltf& gltfModel,
                            SelectionState& selection,
                            ux3d::renderer::GltfRenderLayer* gltfLayer,
                            bool* isSceneDirty)
{
    // Search / Filter box
    static char filterBuf[128] = "";
    ImGui::InputTextWithHint("##ResourceFilter", "Filter resources...", filterBuf, sizeof(filterBuf));
    std::string filterStr(filterBuf);

    auto matchesFilter = [&filterStr](const std::string& name) {
        if (filterStr.empty()) return true;
        std::string lowerName = name;
        std::string lowerFilter = filterStr;
        for (char& c : lowerName) c = static_cast<char>(std::tolower(c));
        for (char& c : lowerFilter) c = static_cast<char>(std::tolower(c));
        return lowerName.find(lowerFilter) != std::string::npos;
    };

    ImGui::Separator();

    // -----------------------------------------------------------------
    // Materials
    // -----------------------------------------------------------------
    auto& materials = gltfModel.getElements<ux3d::gltf::Material>();
    std::string matHeader = "Materials (" + std::to_string(materials.size()) + ")";
    if (ImGui::TreeNodeEx(matHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& mat : materials) {
            std::string name = mat.hasName() ? mat.getName().c_str() : ("Material " + std::to_string(mat.m_ID));
            if (!matchesFilter(name)) continue;

            bool isSelected = (selection.type == SelectionType::Material && selection.selectedMaterial == &mat);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mat.m_ID + 100000)), flags, "%s", name.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectMaterial(&mat);
            }
        }
        ImGui::TreePop();
    }

    // -----------------------------------------------------------------
    // Meshes
    // -----------------------------------------------------------------
    auto& meshes = gltfModel.getElements<ux3d::gltf::Mesh>();
    std::string meshHeader = "Meshes (" + std::to_string(meshes.size()) + ")";
    if (ImGui::TreeNodeEx(meshHeader.c_str())) {
        for (auto& mesh : meshes) {
            std::string name = mesh.hasName() ? mesh.getName().c_str() : ("Mesh " + std::to_string(mesh.m_ID));
            name += " [" + std::to_string(mesh.getPrimitives().size()) + " prims]";
            if (!matchesFilter(name)) continue;

            bool isSelected = (selection.type == SelectionType::Mesh && selection.selectedMesh == &mesh);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh.m_ID + 200000)), flags, "%s", name.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectMesh(&mesh);
            }
        }
        ImGui::TreePop();
    }

    // -----------------------------------------------------------------
    // Textures & Images
    // -----------------------------------------------------------------
    auto& textures = gltfModel.getElements<ux3d::gltf::Texture>();
    std::string texHeader = "Textures (" + std::to_string(textures.size()) + ")";
    if (ImGui::TreeNodeEx(texHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& tex : textures) {
            std::string name = getTextureDisplayName(&tex);
            if (!matchesFilter(name)) continue;

            bool isSelected = (selection.type == SelectionType::Texture && selection.selectedTexture == &tex);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(tex.m_ID + 300000)), flags, "%s", name.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectTexture(&tex);
            }
        }
        ImGui::TreePop();
    }

    auto& images = gltfModel.getElements<ux3d::gltf::Image>();
    std::string imgHeader = "Images (" + std::to_string(images.size()) + ")";
    if (ImGui::TreeNodeEx(imgHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& img : images) {
            std::string name = getImageDisplayName(&img);
            if (!matchesFilter(name)) continue;

            bool isSelected = (selection.type == SelectionType::Image && selection.selectedImage == &img);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(img.m_ID + 400000)), flags, "%s", name.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectImage(&img);
            }
        }
        ImGui::TreePop();
    }

    // -----------------------------------------------------------------
    // Animations
    // -----------------------------------------------------------------
    auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    std::string animHeader = "Animations (" + std::to_string(animations.size()) + ")";
    if (ImGui::TreeNodeEx(animHeader.c_str())) {
        for (auto& anim : animations) {
            std::string name = anim.hasName() ? anim.getName().c_str() : ("Animation " + std::to_string(anim.m_ID));
            name += " [" + std::to_string(anim.getChannels().size()) + " channels]";
            if (!matchesFilter(name)) continue;

            bool isSelected = (selection.type == SelectionType::Animation && selection.selectedAnimation == &anim);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(anim.m_ID + 500000)), flags, "%s", name.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectAnimation(&anim);
            }
        }
        ImGui::TreePop();
    }

    // -----------------------------------------------------------------
    // Cameras & Lights
    // -----------------------------------------------------------------
    auto& cameras = gltfModel.getElements<ux3d::gltf::Camera>();
    std::string camHeader = "Cameras (" + std::to_string(cameras.size()) + ")";
    if (ImGui::TreeNodeEx(camHeader.c_str())) {
        for (auto& cam : cameras) {
            std::string name = cam.hasName() ? cam.getName().c_str() : ("Camera " + std::to_string(cam.m_ID));
            if (!matchesFilter(name)) continue;

            bool isSelected = (selection.type == SelectionType::Camera && selection.selectedCamera == &cam);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(cam.m_ID + 600000)), flags, "%s", name.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectCamera(&cam);
            }
        }
        ImGui::TreePop();
    }

    auto& lights = gltfModel.getElements<ux3d::gltf::Light>();
    std::string lightHeader = "Lights (" + std::to_string(lights.size()) + ")";
    if (ImGui::TreeNodeEx(lightHeader.c_str())) {
        for (auto& light : lights) {
            std::string name = light.hasName() ? light.getName().c_str() : ("Light " + std::to_string(light.m_ID));
            if (!matchesFilter(name)) continue;

            bool isSelected = (selection.type == SelectionType::Light && selection.selectedLight == &light);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(light.m_ID + 700000)), flags, "%s", name.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectLight(&light);
            }
        }
        ImGui::TreePop();
    }

    // -----------------------------------------------------------------
    // Skins
    // -----------------------------------------------------------------
    auto& skins = gltfModel.getElements<ux3d::gltf::Skin>();
    std::string skinHeader = "Skins (" + std::to_string(skins.size()) + ")";
    if (ImGui::TreeNodeEx(skinHeader.c_str())) {
        for (auto& skin : skins) {
            std::string name = skin.hasName() ? skin.getName().c_str() : ("Skin " + std::to_string(skin.m_ID));
            name += " [" + std::to_string(skin.getJoints().size()) + " joints]";
            if (!matchesFilter(name)) continue;

            bool isSelected = (selection.type == SelectionType::Skin && selection.selectedSkin == &skin);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(skin.m_ID + 800000)), flags, "%s", name.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectSkin(&skin);
            }
        }
        ImGui::TreePop();
    }

    // -----------------------------------------------------------------
    // Material Variants (KHR_materials_variants)
    // -----------------------------------------------------------------
    if (gltfModel.hasMaterialVariants()) {
        auto& variants = gltfModel.getMaterialVariants();
        std::string varHeader = "Material Variants (" + std::to_string(variants.size()) + ")";
        if (ImGui::TreeNodeEx(varHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            // Default / Original variant option
            bool isDefaultSelected = gltfLayer && !gltfLayer->getMaterialVariant().has_value();
            ImGuiTreeNodeFlags defFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isDefaultSelected) defFlags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(999999)), defFlags, "Default / Original");
            if (ImGui::IsItemClicked()) {
                if (gltfLayer) {
                    gltfLayer->setMaterialVariant(std::nullopt);
                    if (isSceneDirty) *isSceneDirty = true;
                }
            }

            for (auto& var : variants) {
                std::string name = var.hasName() ? var.getName().c_str() : ("Variant " + std::to_string(var.m_ID));
                if (!matchesFilter(name)) continue;

                bool isSelected = (selection.type == SelectionType::MaterialVariant && selection.selectedVariant == &var) ||
                                  (gltfLayer && gltfLayer->getMaterialVariant().has_value() && gltfLayer->getMaterialVariant().value() == var.m_ID);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(var.m_ID + 900000)), flags, "%s", name.c_str());
                if (ImGui::IsItemClicked()) {
                    selection.selectVariant(&var);
                    if (gltfLayer) {
                        gltfLayer->setMaterialVariant(var.m_ID);
                        if (isSceneDirty) *isSceneDirty = true;
                    }
                }
            }
            ImGui::TreePop();
        }
    }
}

} // namespace ux3d::imgui
