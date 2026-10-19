// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ResourcesPanel.h"
#include "imgui/AppSettings.h"
#include "imgui/TextureUtils.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include <imgui.h>
#include <string>
#include <algorithm>
#include <cctype>

namespace ux3d::imgui {

static char s_resourceSearchBuffer[128] = "";

void ResourcesPanel::resetSearch()
{
    s_resourceSearchBuffer[0] = '\0';
}

void ResourcesPanel::setSearchFilter(const std::string& filter)
{
    std::snprintf(s_resourceSearchBuffer, sizeof(s_resourceSearchBuffer), "%s", filter.c_str());
}

static std::string toLowerString(const std::string& str)
{
    std::string lower = str;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

static std::string formatResourceLabel(const char* icon,
                                       const char* title,
                                       uint32_t id,
                                       const std::string& name,
                                       const ResourceDisplayOptions& opts)
{
    std::string res;
    if (opts.showIcon && icon && icon[0] != '\0') {
        res += icon;
        res += " ";
    }
    bool hasTitle = opts.showTitle && title && title[0] != '\0';
    bool hasIndex = opts.showIndex;
    bool hasName = opts.showName && !name.empty();

    if (hasTitle) {
        res += title;
    }
    if (hasIndex) {
        if (hasTitle) res += " ";
        res += std::to_string(id);
    }
    if (hasName) {
        if (hasTitle || hasIndex) res += " ";
        res += "\"" + name + "\"";
    }

    if (!hasTitle && !hasIndex && !hasName) {
        if (title && title[0] != '\0') {
            res += title;
            res += " ";
            res += std::to_string(id);
        } else {
            res += std::to_string(id);
        }
    }

    return res;
}

static std::string formatCategoryHeader(const char* icon,
                                        const char* title,
                                        size_t count,
                                        const ResourceDisplayOptions& opts)
{
    std::string res;
    if (opts.showIcon && icon && icon[0] != '\0') {
        res += icon;
        res += " ";
    }
    res += title;
    res += " (" + std::to_string(count) + ")";
    return res;
}

void ResourcesPanel::render(ux3d::gltf::Gltf& gltfModel,
                            SelectionState& selection,
                            ux3d::renderer::GltfRenderLayer* gltfLayer,
                            bool* isSceneDirty,
                            bool* pOpen)
{
    static const char* const WND_RESOURCES = ICON_MDI_FOLDER_MULTIPLE_OUTLINE " Resources";

    if (pOpen != nullptr) {
        if (!ImGui::Begin(WND_RESOURCES, pOpen)) {
            ImGui::End();
            return;
        }
    }

    ResourceDisplayOptions opts = AppSettings::instance().getResourceOptions();

    // -----------------------------------------------------------------
    // Top Bar: Search Input (Left) + Options Burger Menu (Right)
    // -----------------------------------------------------------------
    float burgerBtnWidth = 26.0f;
    float clearBtnWidth = (s_resourceSearchBuffer[0] != '\0') ? 24.0f : 0.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float availWidth = ImGui::GetContentRegionAvail().x;
    float searchWidth = availWidth - burgerBtnWidth - spacing;
    if (clearBtnWidth > 0.0f) {
        searchWidth -= (clearBtnWidth + spacing);
    }
    if (searchWidth < 60.0f) searchWidth = 60.0f;

    ImGui::SetNextItemWidth(searchWidth);
    ImGui::InputTextWithHint("##ResourceSearch", ICON_MDI_MAGNIFY " Filter resources...", s_resourceSearchBuffer, sizeof(s_resourceSearchBuffer));

    if (clearBtnWidth > 0.0f) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_CLOSE "##ClearResourceSearch", ImVec2(clearBtnWidth, 0.0f))) {
            s_resourceSearchBuffer[0] = '\0';
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear filter");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_MENU "##ResourceBurger", ImVec2(burgerBtnWidth, 0.0f))) {
        ImGui::OpenPopup("ResourceBurgerMenu");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Display Options");

    if (ImGui::BeginPopup("ResourceBurgerMenu")) {
        bool changed = false;
        if (ImGui::MenuItem("Show Icon", nullptr, &opts.showIcon)) changed = true;
        if (ImGui::MenuItem("Show Title", nullptr, &opts.showTitle)) changed = true;
        if (ImGui::MenuItem("Show Index", nullptr, &opts.showIndex)) changed = true;
        if (ImGui::MenuItem("Show Name", nullptr, &opts.showName)) changed = true;

        if (changed) {
            AppSettings::instance().setResourceOptions(opts);
            AppSettings::instance().saveSettings();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // -----------------------------------------------------------------
    // Resources Tree View (In scrolling child window so top bar stays fixed)
    // -----------------------------------------------------------------
    ImGui::BeginChild("##ResourcesTreeScrollRegion", ImVec2(0, 0), false);

    std::string lowerFilter = toLowerString(s_resourceSearchBuffer);

    auto matchesFilter = [&lowerFilter](const std::string& label, const std::string& name) {
        if (lowerFilter.empty()) return true;
        if (toLowerString(label).find(lowerFilter) != std::string::npos) return true;
        if (!name.empty() && toLowerString(name).find(lowerFilter) != std::string::npos) return true;
        return false;
    };

    // Helper macro to render a simple list category
    auto renderCategory = [&](const char* icon,
                              const char* categoryTitle,
                              const char* itemTitle,
                              auto& elements,
                              SelectionType selType,
                              auto* selectedPtr,
                              auto selectFn,
                              auto getNameFn,
                              bool defaultOpen,
                              uint32_t idOffset)
    {
        size_t totalCount = elements.size();
        if (totalCount == 0) {
            if (lowerFilter.empty()) {
                std::string header = formatCategoryHeader(icon, categoryTitle, 0, opts);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                ImGui::PopStyleColor();
            }
            return;
        }

        // Count matching items
        size_t matchCount = 0;
        if (!lowerFilter.empty()) {
            for (auto& item : elements) {
                std::string name = getNameFn(item);
                std::string label = formatResourceLabel(icon, itemTitle, item.m_ID, name, opts);
                if (matchesFilter(label, name)) {
                    matchCount++;
                }
            }
            if (matchCount == 0) {
                return; // Hide category if nothing matches search
            }
        }

        bool shouldAutoOpen = (!lowerFilter.empty() && matchCount > 0);
        if (shouldAutoOpen) {
            ImGui::SetNextItemOpen(true);
        }

        std::string header = formatCategoryHeader(icon, categoryTitle, totalCount, opts);
        ImGuiTreeNodeFlags catFlags = 0;
        if (defaultOpen && lowerFilter.empty()) catFlags |= ImGuiTreeNodeFlags_DefaultOpen;

        if (ImGui::TreeNodeEx(header.c_str(), catFlags)) {
            for (auto& item : elements) {
                std::string name = getNameFn(item);
                std::string label = formatResourceLabel(icon, itemTitle, item.m_ID, name, opts);
                if (!matchesFilter(label, name)) continue;

                bool isSelected = (selection.type == selType && selectedPtr == &item);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(item.m_ID + idOffset)), flags, "%s", label.c_str());
                if (ImGui::IsItemClicked()) {
                    selectFn(item);
                }
            }
            ImGui::TreePop();
        }
    };

    // 1. Scenes
    auto& scenes = gltfModel.getElements<ux3d::gltf::Scene>();
    renderCategory(ICON_MDI_IMAGE_FILTER_HDR, "Scenes", "Scene", scenes,
                   SelectionType::Scene, selection.selectedScene,
                   [&](auto& s) { selection.selectScene(&s); },
                   [](auto& s) -> std::string { return s.hasName() ? s.getName().c_str() : ""; },
                   false, 1000000);

    // 2. Nodes
    auto& nodes = gltfModel.getElements<ux3d::gltf::Node>();
    renderCategory(ICON_MDI_SOURCE_BRANCH, "Nodes", "Node", nodes,
                   SelectionType::Node, selection.selectedNode,
                   [&](auto& n) { selection.selectNode(&n); if (isSceneDirty) *isSceneDirty = true; },
                   [](auto& n) -> std::string { return n.hasName() ? n.getName().c_str() : ""; },
                   false, 2000000);

    // 3. Cameras
    auto& cameras = gltfModel.getElements<ux3d::gltf::Camera>();
    renderCategory(ICON_MDI_CAMERA, "Cameras", "Camera", cameras,
                   SelectionType::Camera, selection.selectedCamera,
                   [&](auto& c) { selection.selectCamera(&c); },
                   [](auto& c) -> std::string { return c.hasName() ? c.getName().c_str() : ""; },
                   false, 600000);

    // 4. Lights
    auto& lights = gltfModel.getElements<ux3d::gltf::Light>();
    renderCategory(ICON_MDI_LIGHTBULB_OUTLINE, "Lights", "Light", lights,
                   SelectionType::Light, selection.selectedLight,
                   [&](auto& l) { selection.selectLight(&l); },
                   [](auto& l) -> std::string { return l.hasName() ? l.getName().c_str() : ""; },
                   false, 700000);

    // 5. Material Variants
    if (gltfModel.hasMaterialVariants()) {
        auto& variants = gltfModel.getMaterialVariants();
        size_t totalCount = variants.size();
        std::string varCatHeader = formatCategoryHeader(ICON_MDI_PALETTE_SWATCH_OUTLINE, "Material Variants", totalCount, opts);

        if (ImGui::TreeNodeEx(varCatHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            // Default / Original variant option
            bool isDefaultSelected = gltfLayer && !gltfLayer->getMaterialVariant().has_value();
            ImGuiTreeNodeFlags defFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isDefaultSelected) defFlags |= ImGuiTreeNodeFlags_Selected;

            std::string defLabel = opts.showIcon ? (std::string(ICON_MDI_PALETTE_SWATCH_OUTLINE) + " Default / Original") : "Default / Original";
            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(999999)), defFlags, "%s", defLabel.c_str());
            if (ImGui::IsItemClicked()) {
                if (gltfLayer) {
                    gltfLayer->setMaterialVariant(std::nullopt);
                    if (isSceneDirty) *isSceneDirty = true;
                }
            }

            for (auto& var : variants) {
                std::string name = var.hasName() ? var.getName().c_str() : "";
                std::string label = formatResourceLabel(ICON_MDI_PALETTE_SWATCH_OUTLINE, "Variant", var.m_ID, name, opts);
                if (!matchesFilter(label, name)) continue;

                bool isSelected = (selection.type == SelectionType::MaterialVariant && selection.selectedVariant == &var) ||
                                  (gltfLayer && gltfLayer->getMaterialVariant().has_value() && gltfLayer->getMaterialVariant().value() == var.m_ID);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(var.m_ID + 900000)), flags, "%s", label.c_str());
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
    } else if (lowerFilter.empty()) {
        std::string header = formatCategoryHeader(ICON_MDI_PALETTE_SWATCH_OUTLINE, "Material Variants", 0, opts);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
        ImGui::TreeNodeEx(header.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
        ImGui::PopStyleColor();
    }

    // 6. Meshes
    auto& meshes = gltfModel.getElements<ux3d::gltf::Mesh>();
    renderCategory(ICON_MDI_CUBE_OUTLINE, "Meshes", "Mesh", meshes,
                   SelectionType::Mesh, selection.selectedMesh,
                   [&](auto& m) { selection.selectMesh(&m); },
                   [](auto& m) -> std::string { return m.hasName() ? m.getName().c_str() : ""; },
                   false, 200000);

    // 7. Materials
    auto& materials = gltfModel.getElements<ux3d::gltf::Material>();
    renderCategory(ICON_MDI_PALETTE_OUTLINE, "Materials", "Material", materials,
                   SelectionType::Material, selection.selectedMaterial,
                   [&](auto& m) { selection.selectMaterial(&m); },
                   [](auto& m) -> std::string { return m.hasName() ? m.getName().c_str() : ""; },
                   true, 100000);

    // 8. Textures
    auto& textures = gltfModel.getElements<ux3d::gltf::Texture>();
    renderCategory(ICON_MDI_TEXTURE_BOX, "Textures", "Texture", textures,
                   SelectionType::Texture, selection.selectedTexture,
                   [&](auto& t) { selection.selectTexture(&t); },
                   [](auto& t) -> std::string {
                       if (t.hasName() && !t.getName().empty()) return t.getName().c_str();
                       if (t.getSource() != nullptr) {
                           if (!t.getSource()->getUri().empty()) {
                               std::string fn = getFilenameFromUri(t.getSource()->getUri().c_str());
                               if (!fn.empty()) return fn;
                           }
                           if (t.getSource()->hasName() && !t.getSource()->getName().empty()) {
                               return t.getSource()->getName().c_str();
                           }
                       }
                       return "";
                   },
                   true, 300000);

    // 9. Images
    auto& images = gltfModel.getElements<ux3d::gltf::Image>();
    renderCategory(ICON_MDI_IMAGE_OUTLINE, "Images", "Image", images,
                   SelectionType::Image, selection.selectedImage,
                   [&](auto& i) { selection.selectImage(&i); },
                   [](auto& i) -> std::string {
                       std::string fn;
                       if (!i.getUri().empty()) fn = getFilenameFromUri(i.getUri().c_str());
                       if (i.hasName() && !i.getName().empty()) {
                           if (!fn.empty() && fn != i.getName().c_str()) return fn + " (" + i.getName().c_str() + ")";
                           return i.getName().c_str();
                       }
                       return fn;
                   },
                   true, 400000);

    // 10. Samplers
    auto& samplers = gltfModel.getElements<ux3d::gltf::Sampler>();
    renderCategory(ICON_MDI_TUNE, "Samplers", "Sampler", samplers,
                   SelectionType::Sampler, selection.selectedSampler,
                   [&](auto& s) { selection.selectSampler(&s); },
                   [](auto& s) -> std::string { return s.hasName() ? s.getName().c_str() : ""; },
                   false, 3000000);

    // 11. Skins
    auto& skins = gltfModel.getElements<ux3d::gltf::Skin>();
    renderCategory(ICON_MDI_HUMAN, "Skins", "Skin", skins,
                   SelectionType::Skin, selection.selectedSkin,
                   [&](auto& s) { selection.selectSkin(&s); },
                   [](auto& s) -> std::string { return s.hasName() ? s.getName().c_str() : ""; },
                   false, 800000);

    // 12. Animations
    auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    renderCategory(ICON_MDI_MOVIE_PLAY_OUTLINE, "Animations", "Animation", animations,
                   SelectionType::Animation, selection.selectedAnimation,
                   [&](auto& a) { selection.selectAnimation(&a); },
                   [](auto& a) -> std::string { return a.hasName() ? a.getName().c_str() : ""; },
                   false, 500000);

    // 13. Accessors
    auto& accessors = gltfModel.getElements<ux3d::gltf::Accessor>();
    renderCategory(ICON_MDI_FILE_DOCUMENT_OUTLINE, "Accessors", "Accessor", accessors,
                   SelectionType::Accessor, selection.selectedAccessor,
                   [&](auto& a) { selection.selectAccessor(&a); },
                   [](auto& a) -> std::string { return a.hasName() ? a.getName().c_str() : ""; },
                   false, 4000000);

    // 14. BufferViews
    auto& bufferViews = gltfModel.getElements<ux3d::gltf::BufferView>();
    renderCategory(ICON_MDI_VIEW_STREAM_OUTLINE, "BufferViews", "BufferView", bufferViews,
                   SelectionType::BufferView, selection.selectedBufferView,
                   [&](auto& bv) { selection.selectBufferView(&bv); },
                   [](auto& bv) -> std::string { return bv.hasName() ? bv.getName().c_str() : ""; },
                   false, 5000000);

    // 15. Buffers
    auto& buffers = gltfModel.getElements<ux3d::gltf::Buffer>();
    renderCategory(ICON_MDI_DATABASE_OUTLINE, "Buffers", "Buffer", buffers,
                   SelectionType::Buffer, selection.selectedBuffer,
                   [&](auto& b) { selection.selectBuffer(&b); },
                   [](auto& b) -> std::string { return b.hasName() ? b.getName().c_str() : ""; },
                   false, 6000000);

    ImGui::EndChild();

    if (pOpen != nullptr) {
        ImGui::End();
    }
}

} // namespace ux3d::imgui
