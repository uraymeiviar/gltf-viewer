// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ResourcesPanel.h"
#include "imgui/AppSettings.h"
#include "imgui/TextureUtils.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/SelfCheckResult.h"
#include "imgui/UiProbe.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include <imgui.h>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
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

struct ResourceItemCache {
    std::string label;
    std::string lowerLabel;
    std::string lowerName;
};

struct ResourceCategoryCache {
    bool valid = false;
    const ux3d::gltf::Gltf* model = nullptr;
    uint64_t modelRevision = 0;
    uint64_t nameRevision = 0;
    uint32_t optionsMask = 0;
    size_t itemCount = 0;
    std::string header;
    std::vector<ResourceItemCache> items;
    bool filterValid = false;
    std::string filter;
    std::vector<uint8_t> matched;
    std::vector<size_t> visibleIndices;
    size_t matchCount = 0;
    bool fixedHeightRows = true;
};

static std::unordered_map<const void*, ResourceCategoryCache> s_resourceCaches;

// core::LinkedList::at(n) walks from begin() on every call. Visible indices are
// ascending, so one cursor per frame resolves them in a single forward walk and
// holds no pointer past the frame.
template <typename TList>
class SourceCursor {
public:
    explicit SourceCursor(TList& list) : m_list(&list), m_it(list.begin()) {}

    auto& at(size_t index)
    {
        if (index < m_position) {
            m_it = m_list->begin();
            m_position = 0;
        }
        while (m_position < index) {
            ++m_it;
            ++m_position;
            ++m_advances;
        }
        return *m_it;
    }

    size_t advances() const { return m_advances; }

private:
    TList* m_list;
    decltype(std::declval<TList&>().begin()) m_it;
    size_t m_position = 0;
    size_t m_advances = 0;
};

static uint32_t resourceOptionsMask(const ResourceDisplayOptions& opts)
{
    return (opts.showIcon ? 1u : 0u)
        | (opts.showTitle ? 2u : 0u)
        | (opts.showIndex ? 4u : 0u)
        | (opts.showName ? 8u : 0u);
}

template <typename TList, typename TGetName>
static ResourceCategoryCache& ensureResourceCategoryCache(const void* cacheKey,
                                                          const ux3d::gltf::Gltf& gltfModel,
                                                          const TList& elements,
                                                          const char* icon,
                                                          const char* categoryTitle,
                                                          const char* itemTitle,
                                                          const ResourceDisplayOptions& opts,
                                                          const std::string& lowerFilter,
                                                          TGetName getNameFn)
{
    ResourceCategoryCache& cache = s_resourceCaches[cacheKey];
    const uint32_t optionsMask = resourceOptionsMask(opts);
    const uint64_t nameRevision = ux3d::gltf::ChildOfRoot::getNameRevision();
    const bool stale = !cache.valid
        || cache.model != &gltfModel
        || cache.modelRevision != gltfModel.getRevision()
        || cache.nameRevision != nameRevision
        || cache.optionsMask != optionsMask
        || cache.itemCount != elements.size();

    if (stale) {
        cache.valid = true;
        cache.model = &gltfModel;
        cache.modelRevision = gltfModel.getRevision();
        cache.nameRevision = nameRevision;
        cache.optionsMask = optionsMask;
        cache.itemCount = elements.size();
        cache.header = formatCategoryHeader(icon, categoryTitle, elements.size(), opts);
        cache.items.clear();
        cache.items.reserve(elements.size());
        cache.fixedHeightRows = true;

        for (const auto& item : elements) {
            std::string name = getNameFn(item);
            ResourceItemCache itemCache;
            itemCache.label = formatResourceLabel(icon, itemTitle, item.m_ID, name, opts);
            itemCache.lowerLabel = toLowerString(itemCache.label);
            itemCache.lowerName = toLowerString(name);
            if (itemCache.label.find_first_of("\r\n") != std::string::npos) {
                cache.fixedHeightRows = false;
            }
            cache.items.push_back(std::move(itemCache));
        }

        cache.filterValid = false;
    }

    if (!cache.filterValid || cache.filter != lowerFilter) {
        cache.filter = lowerFilter;
        cache.filterValid = true;
        cache.matched.assign(cache.items.size(), 0);
        cache.visibleIndices.clear();
        cache.visibleIndices.reserve(cache.items.size());
        cache.matchCount = 0;

        if (lowerFilter.empty()) {
            std::fill(cache.matched.begin(), cache.matched.end(), static_cast<uint8_t>(1));
            cache.matchCount = cache.items.size();
            for (size_t i = 0; i < cache.items.size(); ++i) {
                cache.visibleIndices.push_back(i);
            }
        } else {
            for (size_t i = 0; i < cache.items.size(); ++i) {
                const auto& item = cache.items[i];
                if (item.lowerLabel.find(lowerFilter) != std::string::npos
                    || (!item.lowerName.empty() && item.lowerName.find(lowerFilter) != std::string::npos)) {
                    cache.matched[i] = 1;
                    cache.visibleIndices.push_back(i);
                    ++cache.matchCount;
                }
            }
        }
    }

    return cache;
}

SelfCheckResult ResourcesPanel::runCacheInvalidationSelfCheck(bool forceFailure)
{
    using ux3d::gltf::Gltf;
    using ux3d::gltf::Material;
    using ux3d::gltf::Mesh;
    using ux3d::gltf::Node;

    int checks = 0;
    std::vector<std::string> failures;
    auto check = [&](bool condition, const std::string& message) {
        ++checks;
        if (!condition && failures.size() < 20) {
            failures.push_back(message);
        }
    };

    struct TestModel {
        std::unique_ptr<Gltf> model;
        Node* node = nullptr;
        Mesh* mesh = nullptr;
        Material* material = nullptr;
    };

    auto makeModel = [](const char* nodeName, const char* meshName, const char* materialName) {
        TestModel result;
        result.model = std::make_unique<Gltf>();
        result.node = result.model->create<Node>();
        result.mesh = result.model->create<Mesh>();
        result.material = result.model->create<Material>();
        result.node->setName(nodeName);
        result.mesh->setName(meshName);
        result.material->setName(materialName);
        return result;
    };

    auto getName = [](auto& item) -> std::string {
        return item.hasName() ? item.getName().c_str() : "";
    };

    auto ensureNodes = [&](Gltf& model, const ResourceDisplayOptions& options, const std::string& filter) -> ResourceCategoryCache& {
        auto& elements = model.getElements<Node>();
        return ensureResourceCategoryCache(
            static_cast<const void*>(&elements), model, elements,
            ICON_MDI_SOURCE_BRANCH, "Nodes", "Node", options, filter, getName);
    };
    auto ensureMeshes = [&](Gltf& model, const ResourceDisplayOptions& options, const std::string& filter) -> ResourceCategoryCache& {
        auto& elements = model.getElements<Mesh>();
        return ensureResourceCategoryCache(
            static_cast<const void*>(&elements), model, elements,
            ICON_MDI_CUBE_OUTLINE, "Meshes", "Mesh", options, filter, getName);
    };
    auto ensureMaterials = [&](Gltf& model, const ResourceDisplayOptions& options, const std::string& filter) -> ResourceCategoryCache& {
        auto& elements = model.getElements<Material>();
        return ensureResourceCategoryCache(
            static_cast<const void*>(&elements), model, elements,
            ICON_MDI_PALETTE_OUTLINE, "Materials", "Material", options, filter, getName);
    };

    auto expectedNodeLabel = [&](const Node* node, const ResourceDisplayOptions& options) {
        return formatResourceLabel(ICON_MDI_SOURCE_BRANCH, "Node", node->m_ID, getName(*node), options);
    };
    auto expectedMeshLabel = [&](const Mesh* mesh, const ResourceDisplayOptions& options) {
        return formatResourceLabel(ICON_MDI_CUBE_OUTLINE, "Mesh", mesh->m_ID, getName(*mesh), options);
    };
    auto expectedMaterialLabel = [&](const Material* material, const ResourceDisplayOptions& options) {
        return formatResourceLabel(ICON_MDI_PALETTE_OUTLINE, "Material", material->m_ID, getName(*material), options);
    };

    s_resourceCaches.clear();
    TestModel first = makeModel("NodeOriginal", "MeshOriginal", "MaterialOriginal");
    ResourceDisplayOptions options;

    auto& nodeCache = ensureNodes(*first.model, options, "");
    auto& meshCache = ensureMeshes(*first.model, options, "");
    auto& materialCache = ensureMaterials(*first.model, options, "");
    check(nodeCache.items.size() == 1 && nodeCache.items[0].label == expectedNodeLabel(first.node, options),
        "initial node resource label mismatch");
    check(meshCache.items.size() == 1 && meshCache.items[0].label == expectedMeshLabel(first.mesh, options),
        "initial mesh resource label mismatch");
    check(materialCache.items.size() == 1 && materialCache.items[0].label == expectedMaterialLabel(first.material, options),
        "initial material resource label mismatch");

    TestModel filtered = makeModel("NodeZero", "MeshZero", "MaterialZero");
    filtered.model->create<Mesh>()->setName("MeshMiddle");
    Mesh* filteredTarget = filtered.model->create<Mesh>();
    filteredTarget->setName("MeshTarget\ncontinued");
    auto& filteredMeshes = filtered.model->getElements<Mesh>();
    auto& filteredCache = ensureMeshes(*filtered.model, options, "meshtarget");
    check(filteredCache.visibleIndices.size() == 1 && filteredCache.visibleIndices[0] == 2,
        "filtered resources row did not retain its original source index");
    check(!filteredCache.fixedHeightRows,
        "multiline resource label was incorrectly classified as fixed-height");
    SelectionState filteredSelection;
    if (filteredCache.visibleIndices.size() == 1) {
        filteredSelection.selectMesh(&filteredMeshes.at(filteredCache.visibleIndices[0]));
    }
    check(filteredSelection.selectedMesh == filteredTarget,
        "filtered resource selection resolved to a filtered-list position instead of the source item");
    auto& noMatchCache = ensureMeshes(*filtered.model, options, "no-such-mesh");
    check(noMatchCache.visibleIndices.empty() && noMatchCache.matchCount == 0,
        "no-match resource filter retained visible source indices");

    // Rename each category after its cache exists. The old term must stop
    // matching and the new term must match through the same cache object.
    first.node->setName("NodeRenamed");
    nodeCache = ensureNodes(*first.model, options, "nodeoriginal");
    check(nodeCache.matchCount == 0, "old node resource name still matches search");
    nodeCache = ensureNodes(*first.model, options, "noderena");
    check(nodeCache.matchCount == 1, "new node resource name does not match search");
    nodeCache = ensureNodes(*first.model, options, "");
    check(nodeCache.items.size() == 1 && nodeCache.items[0].label == expectedNodeLabel(first.node, options),
        "node resource rename label remained stale");

    first.mesh->setName("MeshRenamed");
    meshCache = ensureMeshes(*first.model, options, "meshoriginal");
    check(meshCache.matchCount == 0, "old mesh resource name still matches search");
    meshCache = ensureMeshes(*first.model, options, "meshrena");
    check(meshCache.matchCount == 1, "new mesh resource name does not match search");
    meshCache = ensureMeshes(*first.model, options, "");
    check(meshCache.items.size() == 1 && meshCache.items[0].label == expectedMeshLabel(first.mesh, options),
        "mesh resource rename label remained stale");

    first.material->setName("MaterialRenamed");
    materialCache = ensureMaterials(*first.model, options, "materialoriginal");
    check(materialCache.matchCount == 0, "old material resource name still matches search");
    materialCache = ensureMaterials(*first.model, options, "materialrena");
    check(materialCache.matchCount == 1, "new material resource name does not match search");
    materialCache = ensureMaterials(*first.model, options, "");
    check(materialCache.items.size() == 1 && materialCache.items[0].label == expectedMaterialLabel(first.material, options),
        "material resource rename label remained stale");

    // Display-option changes after cache construction.
    const std::string labeledMesh = meshCache.items.empty() ? std::string() : meshCache.items[0].label;
    ResourceDisplayOptions compact = options;
    compact.showIndex = false;
    compact.showName = true;
    meshCache = ensureMeshes(*first.model, compact, "");
    check(meshCache.items.size() == 1 && meshCache.items[0].label == expectedMeshLabel(first.mesh, compact),
        "resource display-option label mismatch");
    check(meshCache.items.size() == 1 && meshCache.items[0].label != labeledMesh,
        "resource display-option label did not change");

    // Model A -> B -> A, with model A reset while B is the current model.
    TestModel second = makeModel("NodeB", "MeshB", "MaterialB");
    auto& secondNodeCache = ensureNodes(*second.model, options, "nodeb");
    auto& secondMeshCache = ensureMeshes(*second.model, options, "meshb");
    auto& secondMaterialCache = ensureMaterials(*second.model, options, "materialb");
    check(secondNodeCache.matchCount == 1, "model B node search mismatch");
    check(secondMeshCache.matchCount == 1, "model B mesh search mismatch");
    check(secondMaterialCache.matchCount == 1, "model B material search mismatch");

    first.model->reset();
    Node* reloadedNode = first.model->create<Node>();
    Mesh* reloadedMesh = first.model->create<Mesh>();
    Material* reloadedMaterial = first.model->create<Material>();
    reloadedNode->setName("ReloadedNode");
    reloadedMesh->setName("ReloadedMesh");
    reloadedMaterial->setName("ReloadedMaterial");
    nodeCache = ensureNodes(*first.model, options, "reloadednode");
    meshCache = ensureMeshes(*first.model, options, "reloadedmesh");
    materialCache = ensureMaterials(*first.model, options, "reloadedmaterial");
    check(nodeCache.matchCount == 1
        && nodeCache.items.size() == 1
        && nodeCache.items[0].label == expectedNodeLabel(reloadedNode, options),
        "model A->B->A node cache retained stale data");
    check(meshCache.matchCount == 1
        && meshCache.items.size() == 1
        && meshCache.items[0].label == expectedMeshLabel(reloadedMesh, options),
        "model A->B->A mesh cache retained stale data");
    check(materialCache.matchCount == 1
        && materialCache.items.size() == 1
        && materialCache.items[0].label == expectedMaterialLabel(reloadedMaterial, options),
        "model A->B->A material cache retained stale data");

    {
        Gltf cursorModel;
        for (int i = 0; i < 1000; ++i) {
            cursorModel.create<Mesh>()->setName(("CursorMesh" + std::to_string(i)).c_str());
        }
        auto& cursorMeshes = cursorModel.getElements<Mesh>();
        SourceCursor cursor(cursorMeshes);
        bool identity = true;
        const size_t sparse[] = { 0, 3, 3, 250, 251, 999 };
        for (size_t index : sparse) {
            identity = identity && (&cursor.at(index) == &cursorMeshes.at(index));
        }
        check(identity, "source cursor disagrees with LinkedList::at on ascending indices");
        check(cursor.advances() == 999, "source cursor advance count is not linear");
        identity = (&cursor.at(10) == &cursorMeshes.at(10));
        check(identity, "source cursor did not restart on backward access");
        size_t before = cursor.advances();
        for (size_t i = 0; i < 1000; ++i) {
            cursor.at(i);
        }
        check(cursor.advances() - before == 999, "full ascending scan is not one forward walk");
    }

    if (forceFailure) {
        check(false, "forced resources cache self-check failure");
    }

    std::ostringstream report;
    report << "P2.2 resources cache invalidation self-check\n";
    report << "Result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";
    report << "Checks: " << checks << "\n";
    report << "Coverage: warm node/mesh/material rename and search, display options, "
              "model A->B->A with reloaded model A\n";
    for (const auto& failure : failures) {
        report << "FAIL: " << failure << "\n";
    }

    s_resourceCaches.clear();
    return SelfCheckResult{ failures.empty(), report.str() };
}

void ResourcesPanel::render(ux3d::gltf::Gltf& gltfModel,
                            SelectionState& selection,
                            ux3d::renderer::GltfRenderLayer* gltfLayer,
                            SceneRevisions* revisions,
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
    ux3d::imgui::UiProbe::item("res.search", 0);

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
        ResourceCategoryCache& cache = ensureResourceCategoryCache(
            static_cast<const void*>(&elements), gltfModel, elements, icon, categoryTitle, itemTitle,
            opts, lowerFilter, getNameFn);

        size_t totalCount = cache.items.size();
        if (totalCount == 0) {
            if (lowerFilter.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                ImGui::TreeNodeEx(cache.header.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                ImGui::PopStyleColor();
            }
            return;
        }

        if (!lowerFilter.empty() && cache.matchCount == 0) {
            return; // Hide category if nothing matches search
        }

        bool shouldAutoOpen = (!lowerFilter.empty() && cache.matchCount > 0);
        if (shouldAutoOpen) {
            ImGui::SetNextItemOpen(true);
        }

        ImGuiTreeNodeFlags catFlags = 0;
        if (defaultOpen && lowerFilter.empty()) catFlags |= ImGuiTreeNodeFlags_DefaultOpen;

        const bool categoryOpen = ImGui::TreeNodeEx(cache.header.c_str(), catFlags);
        ux3d::imgui::UiProbe::item(categoryTitle, -1);
        if (categoryOpen) {
            SourceCursor sourceCursor(elements);
            auto renderItem = [&](size_t itemIndex) {
                auto& item = sourceCursor.at(itemIndex);
                const ResourceItemCache& itemCache = cache.items[itemIndex];

                bool isSelected = (selection.type == selType && selectedPtr == &item);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(item.m_ID + idOffset)), flags, "%s", itemCache.label.c_str());
                ux3d::imgui::UiProbe::item(categoryTitle, static_cast<long long>(item.m_ID), false, isSelected);
                if (ImGui::IsItemClicked()) {
                    selectFn(item);
                }
            };

            if (cache.fixedHeightRows) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(cache.visibleIndices.size()));
                while (clipper.Step()) {
                    for (int visibleIndex = clipper.DisplayStart; visibleIndex < clipper.DisplayEnd; ++visibleIndex) {
                        renderItem(cache.visibleIndices[static_cast<size_t>(visibleIndex)]);
                    }
                }
            } else {
                for (size_t itemIndex : cache.visibleIndices) {
                    renderItem(itemIndex);
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
                   [&](auto& n) { selection.selectNode(&n); if (revisions) revisions->mark(SceneChange::Selection); },
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
        ResourceCategoryCache& variantCache = ensureResourceCategoryCache(
            static_cast<const void*>(&variants), gltfModel, variants,
            ICON_MDI_PALETTE_SWATCH_OUTLINE, "Material Variants", "Variant", opts, lowerFilter,
            [](auto& v) -> std::string { return v.hasName() ? v.getName().c_str() : ""; });

        const bool variantsOpen = ImGui::TreeNodeEx(variantCache.header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        ux3d::imgui::UiProbe::item("Material Variants", -1);
        if (variantsOpen) {
            // Default / Original variant option
            bool isDefaultSelected = gltfLayer && !gltfLayer->getMaterialVariant().has_value();
            ImGuiTreeNodeFlags defFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isDefaultSelected) defFlags |= ImGuiTreeNodeFlags_Selected;

            std::string defLabel = opts.showIcon ? (std::string(ICON_MDI_PALETTE_SWATCH_OUTLINE) + " Default / Original") : "Default / Original";
            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(999999)), defFlags, "%s", defLabel.c_str());
            if (ImGui::IsItemClicked()) {
                if (gltfLayer) {
                    gltfLayer->setMaterialVariant(std::nullopt);
                    if (revisions) revisions->mark(SceneChange::Material);
                }
            }

            SourceCursor variantCursor(variants);
            auto renderVariant = [&](size_t variantIndex) {
                auto& var = variantCursor.at(variantIndex);
                const ResourceItemCache& variantItem = variantCache.items[variantIndex];

                bool isSelected = (selection.type == SelectionType::MaterialVariant && selection.selectedVariant == &var) ||
                                  (gltfLayer && gltfLayer->getMaterialVariant().has_value() && gltfLayer->getMaterialVariant().value() == var.m_ID);
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(var.m_ID + 900000)), flags, "%s", variantItem.label.c_str());
                ux3d::imgui::UiProbe::item("Material Variants", static_cast<long long>(var.m_ID));
                if (ImGui::IsItemClicked()) {
                    selection.selectVariant(&var);
                    if (gltfLayer) {
                        gltfLayer->setMaterialVariant(var.m_ID);
                        if (revisions) revisions->mark(SceneChange::Material);
                    }
                }
            };

            if (variantCache.fixedHeightRows) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(variantCache.visibleIndices.size()));
                while (clipper.Step()) {
                    for (int visibleIndex = clipper.DisplayStart; visibleIndex < clipper.DisplayEnd; ++visibleIndex) {
                        renderVariant(variantCache.visibleIndices[static_cast<size_t>(visibleIndex)]);
                    }
                }
            } else {
                for (size_t variantIndex : variantCache.visibleIndices) {
                    renderVariant(variantIndex);
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
                   [&](auto& t) -> std::string {
                       // Same image the renderer uses: MSFT_texture_dds, then the core source.
                       const ux3d::gltf::Image* src = ux3d::renderer::GltfRenderLayer::pickImageForTexture(
                           &t, gltfModel.isExtensionDeclared("MSFT_texture_dds"));
                       if (src != nullptr) {
                           if (!src->getUri().empty()) {
                               std::string fn = getFilenameFromUri(src->getUri().c_str());
                               if (!fn.empty()) return fn;
                           }
                           if (src->hasName() && !src->getName().empty()) {
                               return src->getName().c_str();
                           }
                       }
                       if (t.hasName() && !t.getName().empty()) return t.getName().c_str();
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
