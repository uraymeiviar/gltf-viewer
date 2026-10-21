// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SceneHierarchyPanel.h"
#include "imgui/AppSettings.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/SelfCheckResult.h"
#include "gltf/Gltf.h"
#include "gltf/Node.h"
#include "gltf/Mesh.h"
#include "gltf/Material.h"
#include "gltf/Camera.h"
#include "gltf/Light.h"
#include "gltf/Scene.h"
#include <imgui.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <cctype>

namespace ux3d::imgui {

static char s_searchBuffer[128] = "";
static bool s_scrollToSelected = false;

void SceneHierarchyPanel::requestScrollToSelectedNode()
{
    s_scrollToSelected = true;
}

void SceneHierarchyPanel::resetSearch()
{
    s_searchBuffer[0] = '\0';
}

void SceneHierarchyPanel::setSearchFilter(const std::string& filter)
{
    std::snprintf(s_searchBuffer, sizeof(s_searchBuffer), "%s", filter.c_str());
}

static std::string toLowerString(const std::string& str)
{
    std::string lower = str;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

static bool isAncestorOf(const ux3d::gltf::Node* parent, const ux3d::gltf::Node* target)
{
    if (parent == nullptr || target == nullptr) return false;
    for (const auto* child : parent->getChildren()) {
        if (child == target || isAncestorOf(child, target)) return true;
    }
    return false;
}

static std::string formatLabel(const char* icon,
                               const char* title,
                               uint32_t id,
                               const std::string& name,
                               const HierarchyDisplayOptions& opts)
{
    std::string res;
    if (opts.showIcon && icon && icon[0] != '\0') {
        res += icon;
        res += " ";
    }
    if (opts.showTitle && title && title[0] != '\0') {
        res += title;
        res += " ";
    }
    if (opts.showIndex) {
        res += std::to_string(id);
        res += " ";
    }
    if (opts.showName && !name.empty()) {
        res += "\"" + name + "\"";
    }

    // Trim trailing whitespace
    while (!res.empty() && (res.back() == ' ' || res.back() == '\t')) {
        res.pop_back();
    }

    // Fallback if all options were unchecked so item remains clickable and visible
    if (res.empty()) {
        if (!name.empty()) return "\"" + name + "\"";
        if (title) return std::string(title) + " " + std::to_string(id);
        return std::to_string(id);
    }
    return res;
}

struct HierarchyNodeCacheEntry {
    std::string label;
    std::string meshLabel;
    std::string cameraLabel;
    std::string lightLabel;
    std::vector<std::string> primitiveLabels;
    std::vector<std::string> primitiveMaterialLabels;
    std::string normalizedNodeName;
    std::string normalizedMeshName;
    bool selfMatches = true;
    bool subtreeMatches = true;
};

struct HierarchyCache {
    bool labelsValid = false;
    const ux3d::gltf::Gltf* model = nullptr;
    uint64_t modelRevision = 0;
    uint64_t nameRevision = 0;
    uint64_t materialRevision = 0;
    uint32_t optionsMask = 0;
    std::unordered_map<const ux3d::gltf::Node*, HierarchyNodeCacheEntry> nodes;

    bool filterValid = false;
    std::string filter;
    const ux3d::gltf::Scene* scene = nullptr;
};

static HierarchyCache s_hierarchyCache;

static uint32_t hierarchyOptionsMask(const HierarchyDisplayOptions& opts)
{
    return (opts.showIcon ? 1u : 0u)
        | (opts.showTitle ? 2u : 0u)
        | (opts.showIndex ? 4u : 0u)
        | (opts.showName ? 8u : 0u);
}

static void buildHierarchyLabels(const ux3d::gltf::Gltf& gltfModel,
                                 const HierarchyDisplayOptions& opts)
{
    s_hierarchyCache.nodes.clear();
    s_hierarchyCache.nodes.reserve(gltfModel.getNodes().size());

    for (const auto& node : gltfModel.getNodes()) {
        HierarchyNodeCacheEntry entry;
        const std::string name = node.hasName() ? node.getName().c_str() : "";
        entry.label = formatLabel(ICON_MDI_SOURCE_BRANCH, "Node", node.m_ID, name, opts);
        entry.normalizedNodeName = toLowerString(node.hasName() ? name : ("node " + std::to_string(node.m_ID)));

        if (const auto* mesh = node.getMesh()) {
            const std::string meshName = mesh->hasName() ? mesh->getName().c_str() : "";
            entry.meshLabel = formatLabel(ICON_MDI_CUBE_OUTLINE, "Mesh", mesh->m_ID, meshName, opts);
            if (mesh->hasName()) {
                entry.normalizedMeshName = toLowerString(meshName);
            }

            uint32_t primitiveIndex = 0;
            entry.primitiveLabels.reserve(mesh->getPrimitives().size());
            entry.primitiveMaterialLabels.reserve(mesh->getPrimitives().size());
            for (const auto& primitive : mesh->getPrimitives()) {
                entry.primitiveLabels.push_back(
                    formatLabel(ICON_MDI_VECTOR_TRIANGLE, "Primitive", primitiveIndex, "", opts));
                const auto* material = primitive.getMaterial();
                if (material != nullptr) {
                    const std::string materialName = material->hasName() ? material->getName().c_str() : "";
                    entry.primitiveMaterialLabels.push_back(
                        formatLabel(ICON_MDI_CIRCLE_HALF_FULL, "Material", material->m_ID, materialName, opts));
                } else {
                    entry.primitiveMaterialLabels.emplace_back();
                }
                ++primitiveIndex;
            }
        }

        if (const auto* camera = node.getCamera()) {
            const std::string cameraName = camera->hasName() ? camera->getName().c_str() : "";
            entry.cameraLabel = formatLabel(ICON_MDI_CAMERA, "Camera", camera->m_ID, cameraName, opts);
        }
        if (const auto* light = node.getLight()) {
            const std::string lightName = light->hasName() ? light->getName().c_str() : "";
            entry.lightLabel = formatLabel(ICON_MDI_LIGHTBULB_OUTLINE, "Light", light->m_ID, lightName, opts);
        }

        s_hierarchyCache.nodes.emplace(&node, std::move(entry));
    }

    s_hierarchyCache.labelsValid = true;
    s_hierarchyCache.filterValid = false;
    s_hierarchyCache.filter.clear();
}

static bool updateSubtreeMatches(const ux3d::gltf::Node* node, bool hasFilter)
{
    if (node == nullptr) return false;
    const auto it = s_hierarchyCache.nodes.find(node);
    if (it == s_hierarchyCache.nodes.end()) return false;

    bool matches = !hasFilter || it->second.selfMatches;
    for (const auto* child : node->getChildren()) {
        if (updateSubtreeMatches(child, hasFilter)) {
            matches = true;
        }
    }
    it->second.subtreeMatches = matches;
    return matches;
}

static void ensureHierarchyCache(const ux3d::gltf::Gltf& gltfModel,
                                 const ux3d::gltf::Scene* activeScene,
                                 const HierarchyDisplayOptions& opts,
                                 const std::string& lowerFilter)
{
    const uint32_t optionsMask = hierarchyOptionsMask(opts);
    const uint64_t nameRevision = ux3d::gltf::ChildOfRoot::getNameRevision();
    const uint64_t materialRevision = ux3d::gltf::Primitive::getMaterialRevision();
    const bool labelsStale = !s_hierarchyCache.labelsValid
        || s_hierarchyCache.model != &gltfModel
        || s_hierarchyCache.modelRevision != gltfModel.getRevision()
        || s_hierarchyCache.nameRevision != nameRevision
        || s_hierarchyCache.materialRevision != materialRevision
        || s_hierarchyCache.optionsMask != optionsMask;

    if (labelsStale) {
        s_hierarchyCache.model = &gltfModel;
        s_hierarchyCache.modelRevision = gltfModel.getRevision();
        s_hierarchyCache.nameRevision = nameRevision;
        s_hierarchyCache.materialRevision = materialRevision;
        s_hierarchyCache.optionsMask = optionsMask;
        buildHierarchyLabels(gltfModel, opts);
    }

    if (!s_hierarchyCache.filterValid
        || s_hierarchyCache.filter != lowerFilter
        || s_hierarchyCache.scene != activeScene) {
        const bool hasFilter = !lowerFilter.empty();
        for (auto& [node, entry] : s_hierarchyCache.nodes) {
            (void)node;
            entry.selfMatches = !hasFilter
                || entry.normalizedNodeName.find(lowerFilter) != std::string::npos
                || (!entry.normalizedMeshName.empty()
                    && entry.normalizedMeshName.find(lowerFilter) != std::string::npos);
            entry.subtreeMatches = !hasFilter;
        }

        if (hasFilter && activeScene != nullptr) {
            for (const auto* rootNode : activeScene->getNodes()) {
                updateSubtreeMatches(rootNode, true);
            }
        }

        s_hierarchyCache.filter = lowerFilter;
        s_hierarchyCache.scene = activeScene;
        s_hierarchyCache.filterValid = true;
    }
}

static const HierarchyNodeCacheEntry* hierarchyEntry(const ux3d::gltf::Node* node)
{
    const auto it = s_hierarchyCache.nodes.find(node);
    return it != s_hierarchyCache.nodes.end() ? &it->second : nullptr;
}

SelfCheckResult SceneHierarchyPanel::runCacheInvalidationSelfCheck(bool forceFailure)
{
    using ux3d::gltf::Gltf;
    using ux3d::gltf::Material;
    using ux3d::gltf::Mesh;
    using ux3d::gltf::Node;
    using ux3d::gltf::Primitive;
    using ux3d::gltf::Scene;

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
        Scene* scene = nullptr;
        Node* node = nullptr;
        Mesh* mesh = nullptr;
        Material* material = nullptr;
        Primitive* primitive = nullptr;
    };

    auto makeModel = [](const char* nodeName, const char* meshName, const char* materialName) {
        TestModel result;
        result.model = std::make_unique<Gltf>();
        result.scene = result.model->create<Scene>();
        result.node = result.model->create<Node>();
        result.mesh = result.model->create<Mesh>();
        result.material = result.model->create<Material>();
        result.node->setName(nodeName);
        result.mesh->setName(meshName);
        result.material->setName(materialName);
        result.primitive = result.mesh->createPrimitive();
        result.primitive->setMaterial(*result.material);
        result.node->setMesh(*result.mesh);
        result.scene->addNode(*result.node, *result.model);
        return result;
    };

    auto nodeLabel = [](const Node* node, const HierarchyDisplayOptions& options) {
        const std::string name = node->hasName() ? node->getName().c_str() : "";
        return formatLabel(ICON_MDI_SOURCE_BRANCH, "Node", node->m_ID, name, options);
    };
    auto meshLabel = [](const Mesh* mesh, const HierarchyDisplayOptions& options) {
        const std::string name = mesh->hasName() ? mesh->getName().c_str() : "";
        return formatLabel(ICON_MDI_CUBE_OUTLINE, "Mesh", mesh->m_ID, name, options);
    };
    auto materialLabel = [](const Material* material, const HierarchyDisplayOptions& options) {
        const std::string name = material->hasName() ? material->getName().c_str() : "";
        return formatLabel(ICON_MDI_CIRCLE_HALF_FULL, "Material", material->m_ID, name, options);
    };
    auto materialLabelAt = [](const HierarchyNodeCacheEntry* cached, size_t index) {
        if (cached == nullptr || index >= cached->primitiveMaterialLabels.size()) {
            return std::string();
        }
        return cached->primitiveMaterialLabels[index];
    };

    s_hierarchyCache = HierarchyCache{};
    TestModel first = makeModel("NodeOriginal", "MeshOriginal", "MaterialOriginal");
    Material* materialB = first.model->create<Material>();
    materialB->setName("MaterialB");
    HierarchyDisplayOptions options;
    ensureHierarchyCache(*first.model, first.scene, options, "");

    const HierarchyNodeCacheEntry* entry = hierarchyEntry(first.node);
    check(entry != nullptr, "initial node cache entry missing");
    check(entry != nullptr && entry->label == nodeLabel(first.node, options), "initial node label mismatch");
    check(entry != nullptr && entry->meshLabel == meshLabel(first.mesh, options), "initial mesh label mismatch");
    check(entry != nullptr && entry->primitiveMaterialLabels.size() == 1,
        "initial primitive material label count mismatch");
    check(materialLabelAt(entry, 0) == materialLabel(first.material, options),
        "initial material label mismatch");

    // Material A -> B, B -> A, and unassigned -> assigned after the cache exists.
    first.primitive->setMaterial(*materialB);
    ensureHierarchyCache(*first.model, first.scene, options, "");
    entry = hierarchyEntry(first.node);
    check(materialLabelAt(entry, 0) == materialLabel(materialB, options),
        "A->B material label remained stale");
    check(first.primitive->getMaterial() == materialB, "A->B live material pointer mismatch");
    SelectionState selection;
    selection.selectMaterial(first.primitive->getMaterial());
    check(selection.type == SelectionType::Material && selection.selectedMaterial == materialB,
        "A->B selected material identity mismatch");

    first.primitive->setMaterial(*first.material);
    ensureHierarchyCache(*first.model, first.scene, options, "");
    entry = hierarchyEntry(first.node);
    check(materialLabelAt(entry, 0) == materialLabel(first.material, options),
        "B->A material label remained stale");
    check(first.primitive->getMaterial() == first.material, "B->A live material pointer mismatch");
    selection.selectMaterial(first.primitive->getMaterial());
    check(selection.type == SelectionType::Material && selection.selectedMaterial == first.material,
        "B->A selected material identity mismatch");

    first.primitive->unsetMaterial();
    ensureHierarchyCache(*first.model, first.scene, options, "");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr
        && entry->primitiveMaterialLabels.size() == 1
        && entry->primitiveMaterialLabels[0].empty(),
        "unassigned material label was not cleared");
    check(first.primitive->getMaterial() == nullptr, "unassigned live material pointer mismatch");

    first.primitive->setMaterial(*materialB);
    ensureHierarchyCache(*first.model, first.scene, options, "");
    entry = hierarchyEntry(first.node);
    check(materialLabelAt(entry, 0) == materialLabel(materialB, options),
        "unassigned->assigned material label remained empty");
    check(first.primitive->getMaterial() == materialB, "unassigned->assigned live material pointer mismatch");
    selection.selectMaterial(first.primitive->getMaterial());
    check(selection.type == SelectionType::Material && selection.selectedMaterial == materialB,
        "unassigned->assigned selected material identity mismatch");

    // Node and mesh renames must refresh labels and search matches.
    first.node->setName("NodeRenamed");
    ensureHierarchyCache(*first.model, first.scene, options, "");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && entry->label == nodeLabel(first.node, options), "node rename label mismatch");
    ensureHierarchyCache(*first.model, first.scene, options, "nodeoriginal");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && !entry->selfMatches && !entry->subtreeMatches,
        "old node name still matches search");
    ensureHierarchyCache(*first.model, first.scene, options, "noderena");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && entry->selfMatches && entry->subtreeMatches,
        "new node name does not match search");

    first.mesh->setName("MeshRenamed");
    ensureHierarchyCache(*first.model, first.scene, options, "mesh");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && entry->meshLabel == meshLabel(first.mesh, options), "mesh rename label mismatch");
    ensureHierarchyCache(*first.model, first.scene, options, "meshoriginal");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && !entry->selfMatches && !entry->subtreeMatches,
        "old mesh name still matches search");
    ensureHierarchyCache(*first.model, first.scene, options, "meshrena");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && entry->selfMatches && entry->subtreeMatches,
        "new mesh name does not match search");

    // Material rename is visible in the cached primitive label.
    first.primitive->setMaterial(*first.material);
    first.material->setName("MaterialRenamed");
    ensureHierarchyCache(*first.model, first.scene, options, "");
    entry = hierarchyEntry(first.node);
    check(materialLabelAt(entry, 0) == materialLabel(first.material, options),
        "material rename label remained stale");

    // Display-option change after cache construction.
    const std::string labeledNode = entry != nullptr ? entry->label : std::string();
    HierarchyDisplayOptions namesOnly = options;
    namesOnly.showIcon = false;
    namesOnly.showTitle = false;
    namesOnly.showIndex = false;
    namesOnly.showName = true;
    ensureHierarchyCache(*first.model, first.scene, namesOnly, "");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && entry->label == nodeLabel(first.node, namesOnly),
        "display-option label mismatch");
    check(entry != nullptr && entry->label != labeledNode, "display-option label did not change");

    // Model A -> B -> A with the hierarchy cache already populated.
    TestModel second = makeModel("NodeB", "MeshB", "MaterialB");
    ensureHierarchyCache(*second.model, second.scene, options, "");
    const HierarchyNodeCacheEntry* secondEntry = hierarchyEntry(second.node);
    check(secondEntry != nullptr && secondEntry->label == nodeLabel(second.node, options),
        "model B label mismatch");
    ensureHierarchyCache(*first.model, first.scene, options, "");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && entry->label == nodeLabel(first.node, options),
        "model A->B->A label retained B");
    ensureHierarchyCache(*first.model, first.scene, options, "nodeb");
    entry = hierarchyEntry(first.node);
    check(entry != nullptr && !entry->selfMatches && !entry->subtreeMatches,
        "model A cache matched model B search");

    // Same-object reload through Gltf::reset() must rebuild even though the
    // model pointer stays the same.
    const uint64_t revisionBeforeReload = first.model->getRevision();
    first.model->reset();
    Scene* reloadedScene = first.model->create<Scene>();
    Node* reloadedNode = first.model->create<Node>();
    reloadedNode->setName("ReloadedNode");
    reloadedScene->addNode(*reloadedNode, *first.model);
    ensureHierarchyCache(*first.model, reloadedScene, options, "");
    entry = hierarchyEntry(reloadedNode);
    check(entry != nullptr && entry->label == nodeLabel(reloadedNode, options),
        "same-object reload label mismatch");
    check(first.model->getRevision() != revisionBeforeReload, "same-object reload did not bump model revision");

    if (forceFailure) {
        check(false, "forced hierarchy cache self-check failure");
    }

    std::ostringstream report;
    report << "P2.2 hierarchy cache invalidation self-check\n";
    report << "Result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";
    report << "Checks: " << checks << "\n";
    report << "Coverage: warm node/mesh/material rename, primitive material A->B->A, "
              "unassigned->assigned, display options, model A->B->A and same-object reload\n";
    for (const auto& failure : failures) {
        report << "FAIL: " << failure << "\n";
    }

    s_hierarchyCache = HierarchyCache{};
    return SelfCheckResult{ failures.empty(), report.str() };
}

static void renderNodeVisibilityToggle(ux3d::gltf::Node* node, bool& sceneDirty)
{
    float buttonWidth = 20.0f;
    float eyePos = ImGui::GetWindowContentRegionMax().x - buttonWidth;
    ImGui::SameLine(eyePos);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    bool isVisible = node->getVisible();
    const char* icon = isVisible ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!isVisible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string btnLabel = std::string(icon) + "##eye_n_" + std::to_string(node->m_ID);
    if (ImGui::Button(btnLabel.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
        node->setVisible(!isVisible);
        sceneDirty = true;
    }

    if (!isVisible) {
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void renderMeshVisibilityToggle(ux3d::gltf::Mesh* mesh, bool& sceneDirty)
{
    float buttonWidth = 20.0f;
    float eyePos = ImGui::GetWindowContentRegionMax().x - buttonWidth;
    ImGui::SameLine(eyePos);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    bool isVisible = mesh->getVisible();
    const char* icon = isVisible ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!isVisible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string btnLabel = std::string(icon) + "##eye_m_" + std::to_string(mesh->m_ID);
    if (ImGui::Button(btnLabel.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
        mesh->setVisible(!isVisible);
        sceneDirty = true;
    }

    if (!isVisible) {
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void focusCameraOnNode(ux3d::gltf::Node* node,
                              ux3d::gltf::Gltf& gltfModel,
                              ux3d::renderer::GltfRenderLayer* gltfLayer,
                              ux3d::editor::CameraActor& cameraActor,
                              ux3d::imgui::AntiAliasing& antiAliasing,
                              bool& sceneDirty)
{
    if (node == nullptr) return;
    cameraActor.presentFocusNode(gltfLayer, node, &gltfModel);
    antiAliasing.resetHistory();
    sceneDirty = true;
}

static void renderNodeRecursive(ux3d::gltf::Node* node,
                                ux3d::gltf::Gltf& gltfModel,
                                SelectionState& selection,
                                ux3d::renderer::GltfRenderLayer* gltfLayer,
                                ux3d::editor::CameraActor& cameraActor,
                                AntiAliasing& antiAliasing,
                                bool& sceneDirty,
                                const std::string& lowerFilter)
{
    if (node == nullptr) return;

    const HierarchyNodeCacheEntry* cached = hierarchyEntry(node);
    if (cached == nullptr) return;

    // Filter check: skip if neither this node nor its descendants match
    if (!lowerFilter.empty() && !cached->subtreeMatches) {
        return;
    }

    ImGui::PushID(static_cast<int>(node->m_ID));

    // Auto-expand if search query matches a descendant
    if (!lowerFilter.empty()) {
        ImGui::SetNextItemOpen(true);
    }

    // Auto-expand parents if selected element is a descendant
    if (selection.type == SelectionType::Node && selection.selectedNode != nullptr) {
        if (isAncestorOf(node, selection.selectedNode)) {
            ImGui::SetNextItemOpen(true);
        }
    } else if (selection.type == SelectionType::Mesh && selection.selectedMesh != nullptr) {
        if (node->getMesh() == selection.selectedMesh) {
            ImGui::SetNextItemOpen(true);
        }
    } else if (selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
        if (node->getMesh() != nullptr) {
            for (const auto& prim : node->getMesh()->getPrimitives()) {
                if (prim.getMaterial() == selection.selectedMaterial) {
                    ImGui::SetNextItemOpen(true);
                    break;
                }
            }
        }
    }

    bool hasMesh = (node->getMesh() != nullptr);
    bool hasCamera = (node->getCamera() != nullptr);
    bool hasLight = (node->getLight() != nullptr);
    bool hasChildren = !node->getChildren().empty();
    bool isLeaf = (!hasMesh && !hasCamera && !hasLight && !hasChildren);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
    bool isSelected = (selection.type == SelectionType::Node && selection.selectedNode == node);
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
        if (s_scrollToSelected) {
            ImGui::SetScrollHereY(0.5f);
            s_scrollToSelected = false;
        }
    }
    if (isLeaf) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node->m_ID)), flags, "%s", cached->label.c_str());

    // Single-click selection & focus
    if (ImGui::IsItemClicked()) {
        selection.selectNode(node);
        focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, sceneDirty);
    }
    // Double-click focus
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        selection.selectNode(node);
        focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, sceneDirty);
    }

    // Right-aligned eye toggle button on the node row
    renderNodeVisibilityToggle(node, sceneDirty);

    // If open and has children elements
    if (open && !isLeaf) {
        // 1. Mesh child element
        if (hasMesh) {
            auto* mesh = node->getMesh();
            bool meshHasPrims = !mesh->getPrimitives().empty();
            ImGuiTreeNodeFlags meshFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
            if (selection.type == SelectionType::Mesh && selection.selectedMesh == mesh) {
                meshFlags |= ImGuiTreeNodeFlags_Selected;
            }
            if (!meshHasPrims) {
                meshFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }

            // Auto-expand mesh if selected material belongs to one of its primitives
            if (selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
                for (const auto& prim : mesh->getPrimitives()) {
                    if (prim.getMaterial() == selection.selectedMaterial) {
                        ImGui::SetNextItemOpen(true);
                        break;
                    }
                }
            }

            bool meshOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID + 2000000)), meshFlags, "%s", cached->meshLabel.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectMesh(mesh);
                sceneDirty = true;
            }

            // Right-aligned eye toggle button for mesh
            renderMeshVisibilityToggle(mesh, sceneDirty);

            if (meshOpen && meshHasPrims) {
                uint32_t primIdx = 0;
                for (auto& prim : mesh->getPrimitives()) {
                    auto* mat = prim.getMaterial();
                    bool primHasMat = (mat != nullptr);

                    if (primIdx >= cached->primitiveLabels.size()) {
                        break;
                    }
                    const std::string& primLabel = cached->primitiveLabels[primIdx];
                    ImGuiTreeNodeFlags primFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

                    if (!primHasMat) {
                        primFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    }

                    if (selection.type == SelectionType::Material && selection.selectedMaterial == mat) {
                        ImGui::SetNextItemOpen(true);
                    }

                    bool primOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID * 1000 + primIdx + 3000000)), primFlags, "%s", primLabel.c_str());

                    if (primOpen && primHasMat) {
                        const std::string& matLabel = cached->primitiveMaterialLabels[primIdx];

                        ImGuiTreeNodeFlags matFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (selection.type == SelectionType::Material && selection.selectedMaterial == mat) {
                            matFlags |= ImGuiTreeNodeFlags_Selected;
                        }

                        ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mat->m_ID + 4000000)), matFlags, "%s", matLabel.c_str());
                        if (ImGui::IsItemClicked()) {
                            selection.selectMaterial(mat);
                            sceneDirty = true;
                        }

                        ImGui::TreePop(); // pop primitive
                    }
                    primIdx++;
                }
                ImGui::TreePop(); // pop mesh
            }
        }

        // 2. Camera child element
        if (hasCamera) {
            auto* cam = node->getCamera();

            ImGuiTreeNodeFlags camFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selection.type == SelectionType::Camera && selection.selectedCamera == cam) {
                camFlags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(cam->m_ID + 5000000)), camFlags, "%s", cached->cameraLabel.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectCamera(cam);
                sceneDirty = true;
            }
        }

        // 3. Light child element
        if (hasLight) {
            auto* light = node->getLight();

            ImGuiTreeNodeFlags lightFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selection.type == SelectionType::Light && selection.selectedLight == light) {
                lightFlags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(light->m_ID + 6000000)), lightFlags, "%s", cached->lightLabel.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectLight(light);
                sceneDirty = true;
            }
        }

        // 4. Child Nodes
        for (auto* child : node->getChildren()) {
            renderNodeRecursive(child, gltfModel, selection, gltfLayer, cameraActor, antiAliasing, sceneDirty, lowerFilter);
        }

        ImGui::TreePop(); // pop node
    }

    ImGui::PopID();
}

void SceneHierarchyPanel::render(ux3d::gltf::Gltf& gltfModel,
                                 ux3d::gltf::Scene* activeScene,
                                 SelectionState& selection,
                                 ux3d::renderer::GltfRenderLayer* gltfLayer,
                                 ux3d::editor::CameraActor& cameraActor,
                                 AntiAliasing& antiAliasing,
                                 bool& isSceneDirty,
                                 bool* pOpen,
                                 std::vector<std::unique_ptr<ViewportInstance>>* viewports,
                                 ViewportInstance** activeViewport)
{
    static const char* const WND_HIERARCHY = ICON_MDI_FILE_TREE " Scene Hierarchy";

    if (!ImGui::Begin(WND_HIERARCHY, pOpen)) {
        ImGui::End();
        return;
    }

    HierarchyDisplayOptions opts = AppSettings::instance().getHierarchyOptions();

    // -----------------------------------------------------------------
    // Top Bar: Search Input (Left) + Options Burger Menu (Right)
    // -----------------------------------------------------------------
    float burgerBtnWidth = 26.0f;
    float clearBtnWidth = (s_searchBuffer[0] != '\0') ? 24.0f : 0.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float availWidth = ImGui::GetContentRegionAvail().x;
    float searchWidth = availWidth - burgerBtnWidth - spacing;
    if (clearBtnWidth > 0.0f) {
        searchWidth -= (clearBtnWidth + spacing);
    }
    if (searchWidth < 60.0f) searchWidth = 60.0f;

    ImGui::SetNextItemWidth(searchWidth);
    ImGui::InputTextWithHint("##HierarchySearch", ICON_MDI_MAGNIFY " Filter nodes...", s_searchBuffer, sizeof(s_searchBuffer));

    if (clearBtnWidth > 0.0f) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_CLOSE "##ClearSearch", ImVec2(clearBtnWidth, 0.0f))) {
            s_searchBuffer[0] = '\0';
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear filter");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_MENU "##HierarchyBurger", ImVec2(burgerBtnWidth, 0.0f))) {
        ImGui::OpenPopup("HierarchyBurgerMenu");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Display Options");

    if (ImGui::BeginPopup("HierarchyBurgerMenu")) {
        bool changed = false;
        if (ImGui::MenuItem("Show Icon", nullptr, &opts.showIcon)) changed = true;
        if (ImGui::MenuItem("Show Title", nullptr, &opts.showTitle)) changed = true;
        if (ImGui::MenuItem("Show Index", nullptr, &opts.showIndex)) changed = true;
        if (ImGui::MenuItem("Show Name", nullptr, &opts.showName)) changed = true;

        if (changed) {
            AppSettings::instance().setHierarchyOptions(opts);
            AppSettings::instance().saveSettings();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    const std::string lowerFilter = toLowerString(s_searchBuffer);
    if (activeScene != nullptr) {
        ensureHierarchyCache(gltfModel, activeScene, opts, lowerFilter);
    }

    // -----------------------------------------------------------------
    // Hierarchy Tree View (Child window so top search bar never scrolls)
    // -----------------------------------------------------------------
    ImGui::BeginChild("##HierarchyTreeScrollRegion", ImVec2(0, 0), false);

    if (activeScene != nullptr) {
        std::string sceneName = activeScene->hasName() ? activeScene->getName().c_str() : "";
        std::string sceneLabel = formatLabel(ICON_MDI_IMAGE_FILTER_HDR, "Scene", activeScene->m_ID, sceneName, opts);

        if (ImGui::TreeNodeEx(sceneLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            size_t renderedCount = 0;
            for (auto* rootNode : activeScene->getNodes()) {
                const HierarchyNodeCacheEntry* rootEntry = hierarchyEntry(rootNode);
                if (lowerFilter.empty() || (rootEntry != nullptr && rootEntry->subtreeMatches)) {
                    renderNodeRecursive(rootNode, gltfModel, selection, gltfLayer, cameraActor, antiAliasing, isSceneDirty, lowerFilter);
                    renderedCount++;
                }
            }

            if (!lowerFilter.empty() && renderedCount == 0) {
                ImGui::Spacing();
                ImGui::TextDisabled("  No nodes matching \"%s\"", s_searchBuffer);
            }

            ImGui::TreePop();
        }
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No active glTF scene.");
    }

    // -----------------------------------------------------------------
    // Viewport List (Root Sibling in Scene Hierarchy)
    // -----------------------------------------------------------------
    if (viewports != nullptr && !viewports->empty()) {
        ImGui::Spacing();

        std::string vpHeader = std::string(ICON_MDI_MONITOR_MULTIPLE " Viewports (") + std::to_string(viewports->size()) + ")";
        if (ImGui::TreeNodeEx(vpHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& vp : *viewports) {
                if (!lowerFilter.empty()) {
                    if (toLowerString(vp->name).find(lowerFilter) == std::string::npos) {
                        continue;
                    }
                }

                bool isSelected = (selection.type == SelectionType::Viewport && selection.selectedViewportId == vp->id);
                ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (isSelected) nodeFlags |= ImGuiTreeNodeFlags_Selected;

                const char* icon = vp->isAttachedToCamera ? ICON_MDI_CAMERA : ICON_MDI_CUBE_SCAN;
                std::string itemLabel = std::string(icon) + " " + vp->name;
                if (activeViewport != nullptr && *activeViewport == vp.get()) {
                    itemLabel += " (Active)";
                }

                float buttonWidth = 20.0f;
                float eyePos = ImGui::GetWindowContentRegionMax().x - buttonWidth;

                ImGui::TreeNodeEx((void*)(intptr_t)vp->id, nodeFlags, "%s", itemLabel.c_str());

                if (ImGui::IsItemClicked()) {
                    selection.selectViewport(vp->id);
                    if (activeViewport != nullptr) {
                        *activeViewport = vp.get();
                    }
                    vp->requestFocus = true;
                }

                // Eye toggle button on the right
                ImGui::SameLine(eyePos);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

                const char* eyeIcon = vp->isOpen ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
                if (!vp->isOpen) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                }

                std::string eyeBtnId = std::string(eyeIcon) + "##vp_eye_" + std::to_string(vp->id);
                if (ImGui::Button(eyeBtnId.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
                    vp->isOpen = !vp->isOpen;
                    if (vp->isOpen) {
                        vp->requestFocus = true;
                    }
                }

                if (!vp->isOpen) {
                    ImGui::PopStyleColor();
                }
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);

                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(vp->isOpen ? "Visible (click to hide)" : "Hidden (click to show)");
                }
            }
            ImGui::TreePop();
        }
    }

    ImGui::EndChild();

    ImGui::End();
}

} // namespace ux3d::imgui
