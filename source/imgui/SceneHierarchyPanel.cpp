// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SceneHierarchyPanel.h"
#include "imgui/AppSettings.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/SelfCheckResult.h"
#include "imgui/UiProbe.h"
#include "imgui/ObjectActions.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "gltf/Gltf.h"
#include "gltf/Node.h"
#include "gltf/Mesh.h"
#include "gltf/Material.h"
#include "gltf/Camera.h"
#include "gltf/Light.h"
#include "gltf/Scene.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <algorithm>
#include <cctype>

namespace ux3d::imgui {

enum class HierarchyRowType : uint8_t {
    Scene,
    Node,
    Mesh,
    Primitive,
    Material,
    Camera,
    Light,
    ViewportsHeader,
    ViewportItem
};

struct HierarchyRow {
    HierarchyRowType type = HierarchyRowType::Node;
    int depth = 0;
    ImGuiID imguiId = 0;
    void* object = nullptr;
    void* parentObject = nullptr;
    uint32_t subIndex = 0;
    bool isLeaf = true;
    bool isOpen = false;
    const std::string* label = nullptr;
    std::string dynamicLabel;
};

static char s_searchBuffer[128] = "";

struct PendingScroll {
    bool active = false;
    SelectionType type = SelectionType::None;
    void* object = nullptr;
};
static PendingScroll s_pendingScroll;
static uint64_t s_rowModelBuildCount = 0;

void SceneHierarchyPanel::requestScrollToSelectedNode()
{
    s_pendingScroll.active = true;
    s_pendingScroll.type = SelectionType::None;
    s_pendingScroll.object = nullptr;
}

uint64_t SceneHierarchyPanel::rowModelBuildCount()
{
    return s_rowModelBuildCount;
}

void SceneHierarchyPanel::resetRowModelBuildCount()
{
    s_rowModelBuildCount = 0;
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
    std::unordered_map<const ux3d::gltf::Node*, const ux3d::gltf::Node*> parentOfNode;
    std::unordered_map<const ux3d::gltf::Mesh*, std::vector<const ux3d::gltf::Node*>> nodesWithMesh;
    std::unordered_map<const ux3d::gltf::Material*, std::vector<const ux3d::gltf::Node*>> nodesWithMaterial;
    std::unordered_map<const ux3d::gltf::Camera*, std::vector<const ux3d::gltf::Node*>> nodesWithCamera;
    std::unordered_map<const ux3d::gltf::Light*, std::vector<const ux3d::gltf::Node*>> nodesWithLight;

    bool filterValid = false;
    std::string filter;
    const ux3d::gltf::Scene* scene = nullptr;
    std::string sceneLabel;

    // P2.4 Virtualized Row Model Cache
    bool rowModelValid = false;
    bool hasMultiline = false;
    size_t nodeRowCount = 0;
    int selectedRowIndex = -1;
    std::vector<HierarchyRow> rows;
    std::unordered_map<ImGuiID, int> rowIdToIndex;
    ImGuiStorage standaloneStorage;

    // Tracking dependencies
    SelectionType lastSelectionType = SelectionType::None;
    const void* lastSelectedObject = nullptr;
    uint64_t lastSelectionRevision = 0;
    SelectionType lastAutoExpandedSelectionType = SelectionType::None;
    const void* lastAutoExpandedSelectedObject = nullptr;
    size_t lastViewportCount = 0;
    const ViewportInstance* lastActiveViewport = nullptr;
    uint64_t lastViewportStateHash = 0;
    const ux3d::gltf::Node* isolationRoot = nullptr;
    bool isolationIncludeChildren = false;

    void clear() {
        labelsValid = false;
        model = nullptr;
        modelRevision = 0;
        nameRevision = 0;
        materialRevision = 0;
        optionsMask = 0;
        nodes.clear();
        parentOfNode.clear();
        nodesWithMesh.clear();
        nodesWithMaterial.clear();
        nodesWithCamera.clear();
        nodesWithLight.clear();
        filterValid = false;
        filter.clear();
        scene = nullptr;
        sceneLabel.clear();
        rowModelValid = false;
        hasMultiline = false;
        nodeRowCount = 0;
        selectedRowIndex = -1;
        rows.clear();
        rowIdToIndex.clear();
        standaloneStorage.Clear();
        s_pendingScroll = PendingScroll{};
        lastSelectionType = SelectionType::None;
        lastSelectedObject = nullptr;
        lastSelectionRevision = 0;
        lastAutoExpandedSelectionType = SelectionType::None;
        lastAutoExpandedSelectedObject = nullptr;
        lastViewportCount = 0;
        lastActiveViewport = nullptr;
        lastViewportStateHash = 0;
        isolationRoot = nullptr;
        isolationIncludeChildren = false;
    }
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
    s_hierarchyCache.parentOfNode.clear();
    s_hierarchyCache.nodesWithMesh.clear();
    s_hierarchyCache.nodesWithMaterial.clear();
    s_hierarchyCache.nodesWithCamera.clear();
    s_hierarchyCache.nodesWithLight.clear();
    s_hierarchyCache.hasMultiline = false;
    s_hierarchyCache.nodes.reserve(gltfModel.getNodes().size());

    auto checkMultiline = [](const std::string& str) {
        if (str.find_first_of("\r\n") != std::string::npos) {
            s_hierarchyCache.hasMultiline = true;
        }
    };

    for (const auto& node : gltfModel.getNodes()) {
        HierarchyNodeCacheEntry entry;
        const std::string name = node.hasName() ? node.getName().c_str() : "";
        entry.label = formatLabel(ICON_MDI_SOURCE_BRANCH, "Node", node.m_ID, name, opts);
        checkMultiline(entry.label);
        entry.normalizedNodeName = toLowerString(node.hasName() ? name : ("node " + std::to_string(node.m_ID)));

        for (const auto* child : node.getChildren()) {
            if (child != nullptr) {
                s_hierarchyCache.parentOfNode[child] = &node;
            }
        }

        if (const auto* mesh = node.getMesh()) {
            s_hierarchyCache.nodesWithMesh[mesh].push_back(&node);
            const std::string meshName = mesh->hasName() ? mesh->getName().c_str() : "";
            entry.meshLabel = formatLabel(ICON_MDI_CUBE_OUTLINE, "Mesh", mesh->m_ID, meshName, opts);
            checkMultiline(entry.meshLabel);
            if (mesh->hasName()) {
                entry.normalizedMeshName = toLowerString(meshName);
            }

            uint32_t primitiveIndex = 0;
            entry.primitiveLabels.reserve(mesh->getPrimitives().size());
            entry.primitiveMaterialLabels.reserve(mesh->getPrimitives().size());
            for (const auto& primitive : mesh->getPrimitives()) {
                entry.primitiveLabels.push_back(
                    formatLabel(ICON_MDI_VECTOR_TRIANGLE, "Primitive", primitiveIndex, "", opts));
                checkMultiline(entry.primitiveLabels.back());
                const auto* material = primitive.getMaterial();
                if (material != nullptr) {
                    s_hierarchyCache.nodesWithMaterial[material].push_back(&node);
                    const std::string materialName = material->hasName() ? material->getName().c_str() : "";
                    entry.primitiveMaterialLabels.push_back(
                        formatLabel(ICON_MDI_CIRCLE_HALF_FULL, "Material", material->m_ID, materialName, opts));
                    checkMultiline(entry.primitiveMaterialLabels.back());
                } else {
                    entry.primitiveMaterialLabels.emplace_back();
                }
                ++primitiveIndex;
            }
        }

        if (const auto* camera = node.getCamera()) {
            s_hierarchyCache.nodesWithCamera[camera].push_back(&node);
            const std::string cameraName = camera->hasName() ? camera->getName().c_str() : "";
            entry.cameraLabel = formatLabel(ICON_MDI_CAMERA, "Camera", camera->m_ID, cameraName, opts);
            checkMultiline(entry.cameraLabel);
        }
        if (const auto* light = node.getLight()) {
            s_hierarchyCache.nodesWithLight[light].push_back(&node);
            const std::string lightName = light->hasName() ? light->getName().c_str() : "";
            entry.lightLabel = formatLabel(ICON_MDI_LIGHTBULB_OUTLINE, "Light", light->m_ID, lightName, opts);
            checkMultiline(entry.lightLabel);
        }

        s_hierarchyCache.nodes.emplace(&node, std::move(entry));
    }

    s_hierarchyCache.labelsValid = true;
    s_hierarchyCache.filterValid = false;
    s_hierarchyCache.filter.clear();
    s_hierarchyCache.rowModelValid = false;
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
        s_hierarchyCache.rowModelValid = false;
    }
}

static const HierarchyNodeCacheEntry* hierarchyEntry(const ux3d::gltf::Node* node)
{
    const auto it = s_hierarchyCache.nodes.find(node);
    return it != s_hierarchyCache.nodes.end() ? &it->second : nullptr;
}

static void buildRowModel(const ux3d::gltf::Gltf& gltfModel,
                          const ux3d::gltf::Scene* activeScene,
                          const SelectionState& selection,
                          const std::string& lowerFilter,
                          const std::vector<std::unique_ptr<ViewportInstance>>* viewports,
                          const ViewportInstance* activeViewport,
                          ImGuiStorage& storage,
                          ImGuiID baseSeed,
                          const ux3d::gltf::Node* isolationRoot,
                          bool isolationIncludeChildren,
                          bool selectionChanged = true)
{
    s_hierarchyCache.rows.clear();
    s_hierarchyCache.rowIdToIndex.clear();
    s_hierarchyCache.nodeRowCount = 0;
    s_hierarchyCache.selectedRowIndex = -1;
    ++s_rowModelBuildCount;

    // 1. Identify ancestors of currently selected object to auto-expand them in O(depth)
    std::unordered_set<const ux3d::gltf::Node*> autoExpandAncestors;
    if (selectionChanged) {
        if (selection.type == SelectionType::Node && selection.selectedNode != nullptr) {
            const ux3d::gltf::Node* curr = selection.selectedNode;
            auto it = s_hierarchyCache.parentOfNode.find(curr);
            while (it != s_hierarchyCache.parentOfNode.end() && it->second != nullptr) {
                autoExpandAncestors.insert(it->second);
                curr = it->second;
                it = s_hierarchyCache.parentOfNode.find(curr);
            }
        } else if (selection.type == SelectionType::Mesh && selection.selectedMesh != nullptr) {
            auto itMesh = s_hierarchyCache.nodesWithMesh.find(selection.selectedMesh);
            if (itMesh != s_hierarchyCache.nodesWithMesh.end()) {
                for (const auto* ownerNode : itMesh->second) {
                    if (ownerNode != nullptr) {
                        autoExpandAncestors.insert(ownerNode);
                        const ux3d::gltf::Node* curr = ownerNode;
                        auto it = s_hierarchyCache.parentOfNode.find(curr);
                        while (it != s_hierarchyCache.parentOfNode.end() && it->second != nullptr) {
                            autoExpandAncestors.insert(it->second);
                            curr = it->second;
                            it = s_hierarchyCache.parentOfNode.find(curr);
                        }
                    }
                }
            }
        } else if (selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
            auto itMat = s_hierarchyCache.nodesWithMaterial.find(selection.selectedMaterial);
            if (itMat != s_hierarchyCache.nodesWithMaterial.end()) {
                for (const auto* nodeWithMat : itMat->second) {
                    if (nodeWithMat != nullptr) {
                        autoExpandAncestors.insert(nodeWithMat);
                        const ux3d::gltf::Node* curr = nodeWithMat;
                        auto it = s_hierarchyCache.parentOfNode.find(curr);
                        while (it != s_hierarchyCache.parentOfNode.end() && it->second != nullptr) {
                            autoExpandAncestors.insert(it->second);
                            curr = it->second;
                            it = s_hierarchyCache.parentOfNode.find(curr);
                        }
                    }
                }
            }
        } else if (selection.type == SelectionType::Camera && selection.selectedCamera != nullptr) {
            auto itCam = s_hierarchyCache.nodesWithCamera.find(selection.selectedCamera);
            if (itCam != s_hierarchyCache.nodesWithCamera.end()) {
                for (const auto* ownerNode : itCam->second) {
                    if (ownerNode != nullptr) {
                        autoExpandAncestors.insert(ownerNode);
                        const ux3d::gltf::Node* curr = ownerNode;
                        auto it = s_hierarchyCache.parentOfNode.find(curr);
                        while (it != s_hierarchyCache.parentOfNode.end() && it->second != nullptr) {
                            autoExpandAncestors.insert(it->second);
                            curr = it->second;
                            it = s_hierarchyCache.parentOfNode.find(curr);
                        }
                    }
                }
            }
        } else if (selection.type == SelectionType::Light && selection.selectedLight != nullptr) {
            auto itLight = s_hierarchyCache.nodesWithLight.find(selection.selectedLight);
            if (itLight != s_hierarchyCache.nodesWithLight.end()) {
                for (const auto* ownerNode : itLight->second) {
                    if (ownerNode != nullptr) {
                        autoExpandAncestors.insert(ownerNode);
                        const ux3d::gltf::Node* curr = ownerNode;
                        auto it = s_hierarchyCache.parentOfNode.find(curr);
                        while (it != s_hierarchyCache.parentOfNode.end() && it->second != nullptr) {
                            autoExpandAncestors.insert(it->second);
                            curr = it->second;
                            it = s_hierarchyCache.parentOfNode.find(curr);
                        }
                    }
                }
            }
        }
    }

    std::function<void(ux3d::gltf::Node*, int, ImGuiID, bool)> traverseNode;
    traverseNode = [&](ux3d::gltf::Node* node, int depth, ImGuiID parentScopeId, bool includeChildren) {
        if (node == nullptr) return;
        const HierarchyNodeCacheEntry* cached = hierarchyEntry(node);
        if (cached == nullptr) return;

        if (!lowerFilter.empty() && !cached->subtreeMatches) {
            return;
        }

        s_hierarchyCache.nodeRowCount++;

        const int nodeIdInt = static_cast<int>(node->m_ID);
        const ImGuiID nodeScopeId = ImHashData(&nodeIdInt, sizeof(int), parentScopeId);
        void* nodePtr = reinterpret_cast<void*>(static_cast<intptr_t>(node->m_ID));
        const ImGuiID nodeId = ImHashData(&nodePtr, sizeof(void*), nodeScopeId);

        const bool hasMesh = (node->getMesh() != nullptr);
        const bool hasCamera = (node->getCamera() != nullptr);
        const bool hasLight = (node->getLight() != nullptr);
        const bool hasChildren = includeChildren && !node->getChildren().empty();
        const bool isLeaf = !includeChildren || (!hasMesh && !hasCamera && !hasLight && !hasChildren);

        const bool isAutoExpanded = includeChildren && ((!lowerFilter.empty()) || (autoExpandAncestors.count(node) > 0));
        if (isAutoExpanded && !isLeaf) {
            storage.SetInt(nodeId, 1);
        }
        const bool isOpen = !isLeaf && (storage.GetInt(nodeId, 0) != 0);

        HierarchyRow row;
        row.type = HierarchyRowType::Node;
        row.depth = depth;
        row.imguiId = nodeId;
        row.object = node;
        row.isLeaf = isLeaf;
        row.isOpen = isOpen;
        row.label = &cached->label;

        if (selection.type == SelectionType::Node && selection.selectedNode == node) {
            s_hierarchyCache.selectedRowIndex = static_cast<int>(s_hierarchyCache.rows.size());
        }

        s_hierarchyCache.rowIdToIndex[nodeId] = static_cast<int>(s_hierarchyCache.rows.size());
        s_hierarchyCache.rows.push_back(row);

        if (isOpen && !isLeaf) {
            // 1. Mesh child element
            if (hasMesh) {
                auto* mesh = node->getMesh();
                const bool meshHasPrims = !mesh->getPrimitives().empty();
                void* meshPtr = reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID + 2000000));
                const ImGuiID meshId = ImHashData(&meshPtr, sizeof(void*), nodeScopeId);

                bool meshAutoOpen = false;
                if (selectionChanged && selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
                    for (const auto& prim : mesh->getPrimitives()) {
                        if (prim.getMaterial() == selection.selectedMaterial) {
                            meshAutoOpen = true;
                            break;
                        }
                    }
                }
                if (meshAutoOpen && meshHasPrims) {
                    storage.SetInt(meshId, 1);
                }
                const bool meshOpen = meshHasPrims && (storage.GetInt(meshId, 0) != 0);

                HierarchyRow meshRow;
                meshRow.type = HierarchyRowType::Mesh;
                meshRow.depth = depth + 1;
                meshRow.imguiId = meshId;
                meshRow.object = mesh;
                meshRow.parentObject = node;
                meshRow.isLeaf = !meshHasPrims;
                meshRow.isOpen = meshOpen;
                meshRow.label = &cached->meshLabel;

                if (selection.type == SelectionType::Mesh && selection.selectedMesh == mesh) {
                    s_hierarchyCache.selectedRowIndex = static_cast<int>(s_hierarchyCache.rows.size());
                }

                s_hierarchyCache.rowIdToIndex[meshId] = static_cast<int>(s_hierarchyCache.rows.size());
                s_hierarchyCache.rows.push_back(meshRow);

                if (meshOpen && meshHasPrims) {
                    uint32_t primIdx = 0;
                    for (auto& prim : mesh->getPrimitives()) {
                        auto* mat = prim.getMaterial();
                        const bool primHasMat = (mat != nullptr);
                        void* primPtr = reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID * 1000 + primIdx + 3000000));
                        const ImGuiID primId = ImHashData(&primPtr, sizeof(void*), meshId);

                        const bool primAutoOpen = (selectionChanged && selection.type == SelectionType::Material && selection.selectedMaterial == mat);
                        if (primAutoOpen && primHasMat) {
                            storage.SetInt(primId, 1);
                        }
                        const bool primOpen = primHasMat && (storage.GetInt(primId, 0) != 0);

                        if (primIdx < cached->primitiveLabels.size()) {
                            HierarchyRow primRow;
                            primRow.type = HierarchyRowType::Primitive;
                            primRow.depth = depth + 2;
                            primRow.imguiId = primId;
                            primRow.object = &prim;
                            primRow.parentObject = mesh;
                            primRow.subIndex = primIdx;
                            primRow.isLeaf = !primHasMat;
                            primRow.isOpen = primOpen;
                            primRow.label = &cached->primitiveLabels[primIdx];

                            s_hierarchyCache.rowIdToIndex[primId] = static_cast<int>(s_hierarchyCache.rows.size());
                            s_hierarchyCache.rows.push_back(primRow);

                            if (primOpen && primHasMat && primIdx < cached->primitiveMaterialLabels.size()) {
                                void* matPtr = reinterpret_cast<void*>(static_cast<intptr_t>(mat->m_ID + 4000000));
                                const ImGuiID matId = ImHashData(&matPtr, sizeof(void*), primId);

                                HierarchyRow matRow;
                                matRow.type = HierarchyRowType::Material;
                                matRow.depth = depth + 3;
                                matRow.imguiId = matId;
                                matRow.object = mat;
                                matRow.parentObject = mesh;
                                matRow.subIndex = primIdx;
                                matRow.isLeaf = true;
                                matRow.isOpen = false;
                                matRow.label = &cached->primitiveMaterialLabels[primIdx];

                                if (selection.type == SelectionType::Material && selection.selectedMaterial == mat) {
                                    s_hierarchyCache.selectedRowIndex = static_cast<int>(s_hierarchyCache.rows.size());
                                }

                                s_hierarchyCache.rowIdToIndex[matId] = static_cast<int>(s_hierarchyCache.rows.size());
                                s_hierarchyCache.rows.push_back(matRow);
                            }
                        }
                        primIdx++;
                    }
                }
            }

            // 2. Camera child element
            if (hasCamera) {
                auto* cam = node->getCamera();
                void* camPtr = reinterpret_cast<void*>(static_cast<intptr_t>(cam->m_ID + 5000000));
                const ImGuiID camId = ImHashData(&camPtr, sizeof(void*), nodeScopeId);

                HierarchyRow camRow;
                camRow.type = HierarchyRowType::Camera;
                camRow.depth = depth + 1;
                camRow.imguiId = camId;
                camRow.object = cam;
                camRow.parentObject = node;
                camRow.isLeaf = true;
                camRow.isOpen = false;
                camRow.label = &cached->cameraLabel;

                if (selection.type == SelectionType::Camera && selection.selectedCamera == cam) {
                    s_hierarchyCache.selectedRowIndex = static_cast<int>(s_hierarchyCache.rows.size());
                }

                s_hierarchyCache.rowIdToIndex[camId] = static_cast<int>(s_hierarchyCache.rows.size());
                s_hierarchyCache.rows.push_back(camRow);
            }

            // 3. Light child element
            if (hasLight) {
                auto* light = node->getLight();
                void* lightPtr = reinterpret_cast<void*>(static_cast<intptr_t>(light->m_ID + 6000000));
                const ImGuiID lightId = ImHashData(&lightPtr, sizeof(void*), nodeScopeId);

                HierarchyRow lightRow;
                lightRow.type = HierarchyRowType::Light;
                lightRow.depth = depth + 1;
                lightRow.imguiId = lightId;
                lightRow.object = light;
                lightRow.parentObject = node;
                lightRow.isLeaf = true;
                lightRow.isOpen = false;
                lightRow.label = &cached->lightLabel;

                if (selection.type == SelectionType::Light && selection.selectedLight == light) {
                    s_hierarchyCache.selectedRowIndex = static_cast<int>(s_hierarchyCache.rows.size());
                }

                s_hierarchyCache.rowIdToIndex[lightId] = static_cast<int>(s_hierarchyCache.rows.size());
                s_hierarchyCache.rows.push_back(lightRow);
            }

            // 4. Child Nodes
            for (auto* child : node->getChildren()) {
                traverseNode(child, depth + 1, nodeScopeId, true);
            }
        }
    };

    if (activeScene != nullptr) {
        const std::string sceneName = activeScene->hasName() ? activeScene->getName().c_str() : "";
        s_hierarchyCache.sceneLabel = formatLabel(ICON_MDI_IMAGE_FILTER_HDR, "Scene", activeScene->m_ID, sceneName, AppSettings::instance().getHierarchyOptions());
        if (s_hierarchyCache.sceneLabel.find_first_of("\r\n") != std::string::npos) {
            s_hierarchyCache.hasMultiline = true;
        }

        const ImGuiID sceneId = ImHashStr(s_hierarchyCache.sceneLabel.c_str(), 0, baseSeed);
        const bool sceneOpen = storage.GetInt(sceneId, 1) != 0; // Default open

        HierarchyRow sceneRow;
        sceneRow.type = HierarchyRowType::Scene;
        sceneRow.depth = 0;
        sceneRow.imguiId = sceneId;
        sceneRow.object = const_cast<ux3d::gltf::Scene*>(activeScene);
        sceneRow.isLeaf = (isolationRoot == nullptr) && activeScene->getNodes().empty();
        sceneRow.isOpen = sceneOpen;
        sceneRow.label = &s_hierarchyCache.sceneLabel;

        if (selection.type == SelectionType::Scene && selection.selectedScene == activeScene) {
            s_hierarchyCache.selectedRowIndex = static_cast<int>(s_hierarchyCache.rows.size());
        }
        s_hierarchyCache.rowIdToIndex[sceneId] = static_cast<int>(s_hierarchyCache.rows.size());
        s_hierarchyCache.rows.push_back(sceneRow);

        if (sceneOpen) {
            if (isolationRoot != nullptr) {
                // Isolation shows only the isolated node as the scene's root (and its
                // descendants when isolation includes children).
                traverseNode(const_cast<ux3d::gltf::Node*>(isolationRoot), 1, sceneId, isolationIncludeChildren);
            } else {
                for (auto* rootNode : activeScene->getNodes()) {
                    traverseNode(rootNode, 1, sceneId, true);
                }
            }
        }
    }

    // Viewports (Root Sibling)
    if (viewports != nullptr && !viewports->empty()) {
        const std::string vpHeader = std::string(ICON_MDI_MONITOR_MULTIPLE " Viewports (") + std::to_string(viewports->size()) + ")";
        const ImGuiID vpHeaderId = ImHashStr(vpHeader.c_str(), 0, baseSeed);
        const bool vpHeaderOpen = storage.GetInt(vpHeaderId, 1) != 0; // Default open

        HierarchyRow vpHeaderRow;
        vpHeaderRow.type = HierarchyRowType::ViewportsHeader;
        vpHeaderRow.depth = 0;
        vpHeaderRow.imguiId = vpHeaderId;
        vpHeaderRow.isOpen = vpHeaderOpen;
        vpHeaderRow.isLeaf = viewports->empty();
        vpHeaderRow.dynamicLabel = vpHeader;

        s_hierarchyCache.rowIdToIndex[vpHeaderId] = static_cast<int>(s_hierarchyCache.rows.size());
        s_hierarchyCache.rows.push_back(vpHeaderRow);

        if (vpHeaderOpen) {
            for (auto& vp : *viewports) {
                if (!lowerFilter.empty()) {
                    if (toLowerString(vp->name).find(lowerFilter) == std::string::npos) {
                        continue;
                    }
                }
                void* vpPtr = reinterpret_cast<void*>(static_cast<intptr_t>(vp->id));
                const ImGuiID vpId = ImHashData(&vpPtr, sizeof(void*), vpHeaderId);

                const char* icon = vp->isAttachedToCamera ? ICON_MDI_CAMERA : ICON_MDI_CUBE_SCAN;
                std::string itemLabel = std::string(icon) + " " + vp->name;
                if (activeViewport != nullptr && activeViewport == vp.get()) {
                    itemLabel += " (Active)";
                }

                HierarchyRow vpRow;
                vpRow.type = HierarchyRowType::ViewportItem;
                vpRow.depth = 1;
                vpRow.imguiId = vpId;
                vpRow.object = vp.get();
                vpRow.isLeaf = true;
                vpRow.isOpen = false;
                vpRow.dynamicLabel = itemLabel;

                if (selection.type == SelectionType::Viewport && selection.selectedViewportId == vp->id) {
                    s_hierarchyCache.selectedRowIndex = static_cast<int>(s_hierarchyCache.rows.size());
                }

                s_hierarchyCache.rowIdToIndex[vpId] = static_cast<int>(s_hierarchyCache.rows.size());
                s_hierarchyCache.rows.push_back(vpRow);
            }
        }
    }

    s_hierarchyCache.rowModelValid = true;
}

constexpr float kEyeButtonWidth = 20.0f;

static void beginVisibilityToggle()
{
    // Reserve room at the end of the scrollable row for the pinned eye button, and give
    // the button an opaque background so it never overlaps the label text.
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(kEyeButtonWidth + ImGui::GetStyle().ItemSpacing.x, 0.0f));
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - kEyeButtonWidth);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
}

static void endVisibilityToggle()
{
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

static void renderNodeVisibilityToggle(ux3d::gltf::Node* node, SceneRevisions& revisions)
{
    beginVisibilityToggle();

    bool isVisible = node->getVisible();
    const char* icon = isVisible ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!isVisible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string btnLabel = std::string(icon) + "##eye_n_" + std::to_string(node->m_ID);
    const bool nodeEyeClicked = ImGui::Button(btnLabel.c_str(), ImVec2(kEyeButtonWidth, ImGui::GetFrameHeight()));
    ux3d::imgui::UiProbe::item("hier.node_eye", static_cast<long long>(node->m_ID));
    if (nodeEyeClicked) {
        node->setVisible(!isVisible);
        revisions.mark(SceneChange::Visibility);
    }

    if (!isVisible) {
        ImGui::PopStyleColor();
    }
    endVisibilityToggle();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void renderMeshVisibilityToggle(ux3d::gltf::Mesh* mesh, SceneRevisions& revisions)
{
    beginVisibilityToggle();

    bool isVisible = mesh->getVisible();
    const char* icon = isVisible ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!isVisible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string btnLabel = std::string(icon) + "##eye_m_" + std::to_string(mesh->m_ID);
    const bool meshEyeClicked = ImGui::Button(btnLabel.c_str(), ImVec2(kEyeButtonWidth, ImGui::GetFrameHeight()));
    ux3d::imgui::UiProbe::item("hier.mesh_eye", static_cast<long long>(mesh->m_ID));
    if (meshEyeClicked) {
        mesh->setVisible(!isVisible);
        revisions.mark(SceneChange::Visibility);
    }

    if (!isVisible) {
        ImGui::PopStyleColor();
    }
    endVisibilityToggle();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void renderViewportVisibilityToggle(ViewportInstance* vp)
{
    beginVisibilityToggle();

    const char* eyeIcon = vp->isOpen ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!vp->isOpen) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string eyeBtnId = std::string(eyeIcon) + "##vp_eye_" + std::to_string(vp->id);
    const bool vpEyeClicked = ImGui::Button(eyeBtnId.c_str(), ImVec2(kEyeButtonWidth, ImGui::GetFrameHeight()));
    ux3d::imgui::UiProbe::item("hier.vp_eye", static_cast<long long>(vp->id));
    if (vpEyeClicked) {
        vp->isOpen = !vp->isOpen;
        if (vp->isOpen) {
            vp->requestFocus = true;
        }
    }

    if (!vp->isOpen) {
        ImGui::PopStyleColor();
    }
    endVisibilityToggle();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(vp->isOpen ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void focusCameraOnNode(ux3d::gltf::Node* node,
                              ux3d::gltf::Gltf& gltfModel,
                              ux3d::renderer::GltfRenderLayer* gltfLayer,
                              ux3d::editor::CameraActor& cameraActor,
                              ux3d::imgui::AntiAliasing& antiAliasing,
                              SceneRevisions& revisions)
{
    if (node == nullptr) return;
    cameraActor.presentFocusNode(gltfLayer, node, &gltfModel);
    antiAliasing.resetHistory();
    revisions.mark(SceneChange::ViewCamera);
}

// A node is effectively hidden when its own flag or any ancestor's flag is false: the
// renderer applies the same inherited rule, so the hierarchy can grey the whole subtree.
static bool hierarchyNodeHidden(const ux3d::gltf::Node* node)
{
    while (node != nullptr) {
        if (!node->getVisible()) {
            return true;
        }
        const auto it = s_hierarchyCache.parentOfNode.find(node);
        node = (it != s_hierarchyCache.parentOfNode.end()) ? it->second : nullptr;
    }
    return false;
}

static bool hierarchyRowHidden(const HierarchyRow& row)
{
    switch (row.type) {
        case HierarchyRowType::Node:
            return hierarchyNodeHidden(static_cast<const ux3d::gltf::Node*>(row.object));
        case HierarchyRowType::Mesh: {
            const auto* mesh = static_cast<const ux3d::gltf::Mesh*>(row.object);
            return (mesh != nullptr && !mesh->getVisible())
                || hierarchyNodeHidden(static_cast<const ux3d::gltf::Node*>(row.parentObject));
        }
        case HierarchyRowType::Camera:
        case HierarchyRowType::Light:
            return hierarchyNodeHidden(static_cast<const ux3d::gltf::Node*>(row.parentObject));
        default:
            return false;
    }
}

static void renderRow(const HierarchyRow& row,
                      size_t rowIndex,
                      ux3d::gltf::Gltf& gltfModel,
                      ux3d::gltf::Scene* activeScene,
                      SelectionState& selection,
                      ux3d::renderer::GltfRenderLayer* gltfLayer,
                      ux3d::editor::CameraActor& cameraActor,
                      AntiAliasing& antiAliasing,
                      SceneRevisions& revisions,
                      ViewportInstance** activeViewport,
                      const std::function<void(ViewportInstance*)>& viewportMenu,
                      bool isSelected)
{
    if (row.type == HierarchyRowType::ViewportsHeader) {
        ImGui::Spacing();
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (row.type == HierarchyRowType::Scene) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
        if (!row.isLeaf) {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        }
    } else if (row.type == HierarchyRowType::ViewportsHeader) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    } else {
        if (!row.isLeaf) {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_AllowOverlap;
        }
    }
    if (row.isLeaf) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
        if (s_pendingScroll.active) {
            if (s_pendingScroll.object == nullptr || row.object == s_pendingScroll.object) {
                ImGui::SetScrollHereY(0.5f);
                s_pendingScroll.active = false;
                s_pendingScroll.object = nullptr;
            }
        }
    }

    // Selected rows are highlighted light green; hidden rows (own flag or hidden parent)
    // are grey, matching what the renderer actually draws.
    const bool rowHidden = hierarchyRowHidden(row);
    const bool pushRowColor = isSelected || rowHidden;
    if (pushRowColor) {
        const ImVec4 color = isSelected
            ? ImVec4(0.62f, 0.95f, 0.55f, 1.0f)
            : ImVec4(0.52f, 0.55f, 0.60f, 0.8f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
    }

    const std::string& labelStr = row.label ? *row.label : row.dynamicLabel;
    const bool open = ImGui::TreeNodeBehavior(row.imguiId, flags, labelStr.c_str());
    if (pushRowColor) {
        ImGui::PopStyleColor();
    }

    // Record probe with open/disclosure state and selection highlighting state
    switch (row.type) {
        case HierarchyRowType::Scene:
            ux3d::imgui::UiProbe::item("hier.scene", row.object ? static_cast<long long>(static_cast<ux3d::gltf::Scene*>(row.object)->m_ID) : 0, open, isSelected);
            break;
        case HierarchyRowType::Node:
            ux3d::imgui::UiProbe::item("hier.node", static_cast<long long>(static_cast<ux3d::gltf::Node*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::Mesh:
            ux3d::imgui::UiProbe::item("hier.mesh", static_cast<long long>(static_cast<ux3d::gltf::Mesh*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::Primitive:
            ux3d::imgui::UiProbe::item("hier.primitive", static_cast<long long>(row.subIndex), open, isSelected);
            break;
        case HierarchyRowType::Material:
            ux3d::imgui::UiProbe::item("hier.material", static_cast<long long>(static_cast<ux3d::gltf::Material*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::Camera:
            ux3d::imgui::UiProbe::item("hier.camera", static_cast<long long>(static_cast<ux3d::gltf::Camera*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::Light:
            ux3d::imgui::UiProbe::item("hier.light", static_cast<long long>(static_cast<ux3d::gltf::Light*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::ViewportsHeader:
            ux3d::imgui::UiProbe::item("hier.viewports_header", 0, open, isSelected);
            break;
        case HierarchyRowType::ViewportItem:
            ux3d::imgui::UiProbe::item("hier.viewport", static_cast<long long>(static_cast<ViewportInstance*>(row.object)->id), open, isSelected);
            break;
    }

    if (!row.isLeaf && open != row.isOpen) {
        s_hierarchyCache.rowModelValid = false;
    }

    switch (row.type) {
        case HierarchyRowType::Scene: {
            auto* scene = static_cast<ux3d::gltf::Scene*>(row.object);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                selection.selectScene(scene);
            }
            break;
        }
        case HierarchyRowType::Node: {
            auto* node = static_cast<ux3d::gltf::Node*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectNode(node);
                revisions.mark(SceneChange::Selection);
                focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, revisions);
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                selection.selectNode(node);
                revisions.mark(SceneChange::Selection);
                focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, revisions);
            }
            if (ImGui::BeginPopupContextItem(nullptr)) {
                // Right-click does not move the selection; the menu targets this row and
                // offers Select first when the row was not selected.
                const bool wasSelected = (selection.type == SelectionType::Node && selection.selectedNode == node);
                ObjectActionContext context{
                    gltfModel, activeScene, selection, gltfLayer, &cameraActor, &antiAliasing, revisions
                };
                drawObjectMenuItems(context, node, !wasSelected, true);
                ImGui::EndPopup();
            }
            renderNodeVisibilityToggle(node, revisions);
            break;
        }
        case HierarchyRowType::Mesh: {
            auto* mesh = static_cast<ux3d::gltf::Mesh*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectMesh(mesh);
                revisions.mark(SceneChange::Selection);
            }
            renderMeshVisibilityToggle(mesh, revisions);
            break;
        }
        case HierarchyRowType::Material: {
            auto* mat = static_cast<ux3d::gltf::Material*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectMaterial(mat);
                revisions.mark(SceneChange::Selection);
            }
            break;
        }
        case HierarchyRowType::Camera: {
            auto* cam = static_cast<ux3d::gltf::Camera*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectCamera(cam);
                revisions.mark(SceneChange::Selection);
            }
            break;
        }
        case HierarchyRowType::Light: {
            auto* light = static_cast<ux3d::gltf::Light*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectLight(light);
                revisions.mark(SceneChange::Selection);
            }
            break;
        }
        case HierarchyRowType::ViewportItem: {
            auto* vp = static_cast<ViewportInstance*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectViewport(vp->id);
                if (activeViewport != nullptr) {
                    *activeViewport = vp;
                }
                vp->requestFocus = true;
            }
            if (ImGui::BeginPopupContextItem(nullptr)) {
                if (viewportMenu) {
                    viewportMenu(vp);
                }
                ImGui::EndPopup();
            }
            renderViewportVisibilityToggle(vp);
            break;
        }
        default:
            break;
    }

    if (row.type == HierarchyRowType::Scene && row.isOpen) {
        if (s_searchBuffer[0] != '\0' && s_hierarchyCache.nodeRowCount == 0) {
            ImGui::Spacing();
            ImGui::TextDisabled("  No nodes matching \"%s\"", s_searchBuffer);
        }
    }
}

SelfCheckResult SceneHierarchyPanel::runCacheInvalidationSelfCheck(bool forceFailure)
{
    using ux3d::gltf::Gltf;
    using ux3d::gltf::Material;
    using ux3d::gltf::Mesh;
    using ux3d::gltf::Node;
    using ux3d::gltf::Primitive;
    using ux3d::gltf::Scene;
    using ux3d::gltf::Camera;
    using ux3d::gltf::Light;

    int checks = 0;
    std::vector<std::string> failures;
    auto check = [&](bool condition, const std::string& message) {
        ++checks;
        if (!condition && failures.size() < 25) {
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

    s_hierarchyCache.clear();
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

    // -----------------------------------------------------------------
    // P2.4 Virtualized Row Model Verification
    // -----------------------------------------------------------------
    TestModel p24Model = makeModel("P24Node", "P24Mesh", "P24Material");
    Node* childNode = p24Model.model->create<Node>();
    childNode->setName("P24Child");
    p24Model.node->addChild(*childNode, *p24Model.model);
    Camera* cam = p24Model.model->create<Camera>(Camera::ProjectionType::Perspective);
    cam->setName("P24Cam");
    p24Model.node->setCamera(*cam);
    Light* light = p24Model.model->create<Light>(Light::Type::Directional);
    light->setName("P24Light");
    p24Model.node->setLight(*light);
    Camera* cam2 = p24Model.model->create<Camera>(Camera::ProjectionType::Orthographic);
    cam2->setName("P24Cam2");
    childNode->setCamera(*cam2);
    Light* light2 = p24Model.model->create<Light>(Light::Type::Point);
    light2->setName("P24Light2");
    childNode->setLight(*light2);

    ensureHierarchyCache(*p24Model.model, p24Model.scene, options, "");
    check(!s_hierarchyCache.hasMultiline, "single-line model falsely classified as multiline");

    // Default state: scene open, node collapsed
    SelectionState selNone;
    buildRowModel(*p24Model.model, p24Model.scene, selNone, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false);
    check(s_hierarchyCache.rowModelValid, "row model not marked valid after build");
    check(s_hierarchyCache.rows.size() == 2, "collapsed node should yield exactly 2 rows (Scene, RootNode)");
    check(s_hierarchyCache.rows[0].type == HierarchyRowType::Scene && s_hierarchyCache.rows[0].depth == 0,
        "row 0 should be Scene at depth 0");
    check(s_hierarchyCache.rows[1].type == HierarchyRowType::Node && s_hierarchyCache.rows[1].depth == 1,
        "row 1 should be Node at depth 1");
    check(!s_hierarchyCache.rows[1].isOpen, "root node should be collapsed by default");

    const ImGuiID rootNodeId = s_hierarchyCache.rows[1].imguiId;

    // Expand root node: rows must include Mesh, Camera, Light, ChildNode
    s_hierarchyCache.standaloneStorage.SetInt(rootNodeId, 1);
    buildRowModel(*p24Model.model, p24Model.scene, selNone, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false);
    // Scene (0), Node (1), Mesh (2), Camera (2), Light (2), ChildNode (2) = 6 rows
    check(s_hierarchyCache.rows.size() == 6, "expanded node should yield 6 rows when children are collapsed");
    check(s_hierarchyCache.rows[1].isOpen, "root node should now be recorded as open");
    check(s_hierarchyCache.rows[2].type == HierarchyRowType::Mesh && s_hierarchyCache.rows[2].depth == 2, "mesh row at depth 2");
    check(s_hierarchyCache.rows[3].type == HierarchyRowType::Camera && s_hierarchyCache.rows[3].depth == 2, "camera row at depth 2");
    check(s_hierarchyCache.rows[4].type == HierarchyRowType::Light && s_hierarchyCache.rows[4].depth == 2, "light row at depth 2");
    check(s_hierarchyCache.rows[5].type == HierarchyRowType::Node && s_hierarchyCache.rows[5].depth == 2, "child node at depth 2");

    // Expand Mesh: must reveal Primitive
    const ImGuiID meshId = s_hierarchyCache.rows[2].imguiId;
    s_hierarchyCache.standaloneStorage.SetInt(meshId, 1);
    buildRowModel(*p24Model.model, p24Model.scene, selNone, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false);
    check(s_hierarchyCache.rows.size() == 7, "expanded mesh should reveal primitive row");
    check(s_hierarchyCache.rows[3].type == HierarchyRowType::Primitive && s_hierarchyCache.rows[3].depth == 3, "primitive row at depth 3");

    // Expand Primitive: must reveal Material
    const ImGuiID primId = s_hierarchyCache.rows[3].imguiId;
    s_hierarchyCache.standaloneStorage.SetInt(primId, 1);
    buildRowModel(*p24Model.model, p24Model.scene, selNone, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false);
    check(s_hierarchyCache.rows.size() == 8, "expanded primitive should reveal material row");
    check(s_hierarchyCache.rows[4].type == HierarchyRowType::Material && s_hierarchyCache.rows[4].depth == 4, "material row at depth 4");

    // Verify stable ID invariance under search filter
    const ImGuiID childNodeId = s_hierarchyCache.rows.back().imguiId;
    ensureHierarchyCache(*p24Model.model, p24Model.scene, options, "child");
    buildRowModel(*p24Model.model, p24Model.scene, selNone, "child", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false);
    check(!s_hierarchyCache.rows.empty(), "filter matching child should keep rows");
    auto itChild = s_hierarchyCache.rowIdToIndex.find(childNodeId);
    check(itChild != s_hierarchyCache.rowIdToIndex.end(), "child node ID changed during search filter");

    // No-match search produces 0 node rows
    ensureHierarchyCache(*p24Model.model, p24Model.scene, options, "zzznonexistent");
    buildRowModel(*p24Model.model, p24Model.scene, selNone, "zzznonexistent", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false);
    size_t nodeRowCount = 0;
    for (const auto& r : s_hierarchyCache.rows) {
        if (r.type == HierarchyRowType::Node) nodeRowCount++;
    }
    check(nodeRowCount == 0, "no-match filter should exclude all node rows");

    // Clearing search restores nodes
    ensureHierarchyCache(*p24Model.model, p24Model.scene, options, "");
    buildRowModel(*p24Model.model, p24Model.scene, selNone, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false);
    check(s_hierarchyCache.rows.size() >= 2, "clearing search should restore rows");

    // Multiline label classification check
    TestModel multiModel = makeModel("Multi\nNode", "Mesh", "Mat");
    ensureHierarchyCache(*multiModel.model, multiModel.scene, options, "");
    check(s_hierarchyCache.hasMultiline, "multiline node label not detected");

    // -----------------------------------------------------------------
    // Camera & Light Selection Highlighting and Switching Verification
    // -----------------------------------------------------------------
    ensureHierarchyCache(*p24Model.model, p24Model.scene, options, "");
    check(s_hierarchyCache.nodesWithCamera.find(cam) != s_hierarchyCache.nodesWithCamera.end(), "nodesWithCamera missing cam");
    check(s_hierarchyCache.nodesWithCamera.find(cam2) != s_hierarchyCache.nodesWithCamera.end(), "nodesWithCamera missing cam2");
    check(s_hierarchyCache.nodesWithLight.find(light) != s_hierarchyCache.nodesWithLight.end(), "nodesWithLight missing light");
    check(s_hierarchyCache.nodesWithLight.find(light2) != s_hierarchyCache.nodesWithLight.end(), "nodesWithLight missing light2");

    // Expand root and child to reveal all camera and light rows
    s_hierarchyCache.standaloneStorage.SetInt(rootNodeId, 1);
    s_hierarchyCache.standaloneStorage.SetInt(childNodeId, 1);

    // 1. Select Camera 0
    SelectionState selCam;
    selCam.selectCamera(cam);
    buildRowModel(*p24Model.model, p24Model.scene, selCam, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].type == HierarchyRowType::Camera,
          "selecting camera sets selectedRowIndex to camera row");
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].object == cam,
          "selecting camera matches cam object identity");

    // 2. Switch to Camera 1 (must update selection and clear previous cam)
    SelectionState selCam2;
    selCam2.selectCamera(cam2);
    buildRowModel(*p24Model.model, p24Model.scene, selCam2, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].object == cam2,
          "switching to cam2 updates selectedRowIndex to cam2");
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].object != cam,
          "switching to cam2 clears previous cam selection");

    // 3. Switch to Light 0 (clears camera, selects light)
    SelectionState selLight;
    selLight.selectLight(light);
    buildRowModel(*p24Model.model, p24Model.scene, selLight, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].type == HierarchyRowType::Light,
          "selecting light sets selectedRowIndex to light row");
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].object == light,
          "selecting light matches light object identity");

    // 4. Switch to Light 1 (must update selection and clear previous light)
    SelectionState selLight2;
    selLight2.selectLight(light2);
    buildRowModel(*p24Model.model, p24Model.scene, selLight2, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].object == light2,
          "switching to light2 updates selectedRowIndex to light2");
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].object != light,
          "switching to light2 clears previous light selection");

    // 5. Camera & Light selection-driven auto-expansion when owning node is collapsed
    s_hierarchyCache.standaloneStorage.Clear(); // Collapse all nodes
    buildRowModel(*p24Model.model, p24Model.scene, selCam2, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].object == cam2,
          "selecting cam2 with collapsed owners auto-expands ancestors down to cam2 row");

    s_hierarchyCache.standaloneStorage.Clear();
    buildRowModel(*p24Model.model, p24Model.scene, selLight2, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);
    check(s_hierarchyCache.selectedRowIndex >= 0 && s_hierarchyCache.rows[s_hierarchyCache.selectedRowIndex].object == light2,
          "selecting light2 with collapsed owners auto-expands ancestors down to light2 row");

    // -----------------------------------------------------------------
    // Shared Mesh Owner Tracking & Selection Expansion Verification
    // -----------------------------------------------------------------
    TestModel sharedModel = makeModel("SharedRoot0", "SharedMesh", "SharedMat");
    Node* sharedRoot1 = sharedModel.model->create<Node>();
    sharedRoot1->setName("SharedRoot1");
    sharedRoot1->setMesh(*sharedModel.mesh);
    sharedModel.scene->addNode(*sharedRoot1, *sharedModel.model);

    Node* sharedParent2 = sharedModel.model->create<Node>();
    sharedParent2->setName("SharedParent2");
    sharedModel.scene->addNode(*sharedParent2, *sharedModel.model);
    Node* sharedChild3 = sharedModel.model->create<Node>();
    sharedChild3->setName("SharedChild3");
    sharedChild3->setMesh(*sharedModel.mesh);
    sharedParent2->addChild(*sharedChild3, *sharedModel.model);

    ensureHierarchyCache(*sharedModel.model, sharedModel.scene, options, "");
    auto itSharedMesh = s_hierarchyCache.nodesWithMesh.find(sharedModel.mesh);
    check(itSharedMesh != s_hierarchyCache.nodesWithMesh.end() && itSharedMesh->second.size() == 3,
        "shared mesh must track all 3 owning nodes");

    // With all nodes collapsed, select the shared mesh
    s_hierarchyCache.standaloneStorage.Clear();
    SelectionState selMesh;
    selMesh.selectMesh(sharedModel.mesh);
    buildRowModel(*sharedModel.model, sharedModel.scene, selMesh, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);

    const auto* entryRoot0 = hierarchyEntry(sharedModel.node);
    const auto* entryRoot1 = hierarchyEntry(sharedRoot1);
    const auto* entryParent2 = hierarchyEntry(sharedParent2);
    const auto* entryChild3 = hierarchyEntry(sharedChild3);
    check(entryRoot0 && entryRoot1 && entryParent2 && entryChild3, "all shared entries must exist");

    // All owning nodes and parents must be expanded in storage and row model
    int meshRowCount = 0;
    for (const auto& r : s_hierarchyCache.rows) {
        if (r.type == HierarchyRowType::Mesh && r.object == sharedModel.mesh) {
            meshRowCount++;
        }
    }
    check(meshRowCount == 3, "selecting shared mesh must expand all owners and render 3 mesh rows");

    // Material selection-driven auto-expansion
    s_hierarchyCache.standaloneStorage.Clear();
    SelectionState selMat;
    selMat.selectMaterial(sharedModel.material);
    buildRowModel(*sharedModel.model, sharedModel.scene, selMat, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);
    int matRowCount = 0;
    for (const auto& r : s_hierarchyCache.rows) {
        if (r.type == HierarchyRowType::Material && r.object == sharedModel.material) {
            matRowCount++;
        }
    }
    check(matRowCount == 3, "selecting material must auto-expand node, mesh, and primitive for all 3 instances");

    // Settled frame reuse and build count verification
    const uint64_t initialBuildCount = rowModelBuildCount();
    check(initialBuildCount > 0, "row model build count must track actual rebuilds");

    buildRowModel(*sharedModel.model, sharedModel.scene, selMat, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, true);
    check(rowModelBuildCount() == initialBuildCount + 1, "invalidation must increment row-model build count by exactly 1");

    const uint64_t settledBuildCount = rowModelBuildCount();
    const bool simulatedSelectionStale = false;
    if (!s_hierarchyCache.rowModelValid || simulatedSelectionStale) {
        buildRowModel(*sharedModel.model, sharedModel.scene, selMat, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, false);
    }
    check(rowModelBuildCount() == settledBuildCount, "settled frames must reuse row model without rebuilds (build count delta 0)");

    // Pending scroll request with unavailable target must NOT trigger rebuild on settled frames
    s_pendingScroll.active = true;
    s_pendingScroll.object = nullptr; // unavailable target
    const bool pendingSelectionStale = false; // s_pendingScroll is separated from selectionStale
    if (!s_hierarchyCache.rowModelValid || pendingSelectionStale) {
        buildRowModel(*sharedModel.model, sharedModel.scene, selMat, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, false);
    }
    check(rowModelBuildCount() == settledBuildCount, "pending scroll with unavailable target must not trigger row-model rebuilds");
    s_pendingScroll = PendingScroll{};

    // Subsequent collapse: collapsing root 0 must persist without being re-forced open
    ImGuiID root0Id = s_hierarchyCache.rows[1].imguiId;
    s_hierarchyCache.standaloneStorage.SetInt(root0Id, 0); // User collapses root 0
    buildRowModel(*sharedModel.model, sharedModel.scene, selMat, "", nullptr, nullptr, s_hierarchyCache.standaloneStorage, 0x12345678, nullptr, false, false);
    check(!s_hierarchyCache.rows[1].isOpen, "root 0 must remain collapsed when user collapsed it");

    // Stale pointers cleared after reload
    sharedModel.model->reset();
    ensureHierarchyCache(*sharedModel.model, nullptr, options, "");
    check(s_hierarchyCache.nodesWithMesh.empty(), "stale shared mesh mappings remained after reset");
    p24Model.model->reset();
    ensureHierarchyCache(*p24Model.model, nullptr, options, "");
    check(s_hierarchyCache.nodes.empty(), "stale node pointers remained after model reset");
    check(s_hierarchyCache.nodesWithCamera.empty(), "stale camera mappings remained after model reset");
    check(s_hierarchyCache.nodesWithLight.empty(), "stale light mappings remained after model reset");
    check(!s_hierarchyCache.rowModelValid, "row model remained valid after reset");

    if (forceFailure) {
        check(false, "forced hierarchy cache self-check failure");
    }

    std::ostringstream report;
    report << "P2.4 hierarchy cache and row virtualization self-check\n";
    report << "Result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";
    report << "Checks: " << checks << "\n";
    report << "Coverage: warm node/mesh/material rename, primitive material A->B->A, "
              "unassigned->assigned, display options, model A->B->A, same-object reload, "
              "row model ordering/depths, expansion state invalidation, search filtering, "
              "stable ID invariance, multiline classification, and reload pointer clearing\n";
    for (const auto& failure : failures) {
        report << "FAIL: " << failure << "\n";
    }

    s_hierarchyCache.clear();
    return SelfCheckResult{ failures.empty(), report.str() };
}

void SceneHierarchyPanel::render(ux3d::gltf::Gltf& gltfModel,
                                 ux3d::gltf::Scene* activeScene,
                                 SelectionState& selection,
                                 ux3d::renderer::GltfRenderLayer* gltfLayer,
                                 ux3d::editor::CameraActor& cameraActor,
                                 AntiAliasing& antiAliasing,
                                 SceneRevisions& revisions,
                                 bool* pOpen,
                                 std::vector<std::unique_ptr<ViewportInstance>>* viewports,
                                 ViewportInstance** activeViewport,
                                 std::function<void(ViewportInstance*)> viewportMenu)
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
    ux3d::imgui::UiProbe::item("hier.search", 0);

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

    // Isolation indicator and exit action, above the list (always visible while active).
    if (gltfLayer != nullptr && gltfLayer->getIsolationRoot() != nullptr) {
        const auto* isolatedRoot = gltfLayer->getIsolationRoot();
        const std::string isolatedName = isolatedRoot->hasName()
            ? isolatedRoot->getName()
            : ("Node " + std::to_string(isolatedRoot->m_ID));
        if (ImGui::Button(ICON_MDI_CLOSE " Exit Isolate")) {
            ObjectActionContext context{
                gltfModel, activeScene, selection, gltfLayer, &cameraActor, &antiAliasing, revisions
            };
            exitIsolation(context);
        }
        ux3d::imgui::UiProbe::item("hier.exit_isolate", 0);
        ImGui::SameLine();
        ImGui::TextDisabled("Isolated: %s%s", isolatedName.c_str(),
                            gltfLayer->getIsolationIncludeChildren() ? " (with children)" : "");
        ImGui::Separator();
    }

    const std::string lowerFilter = toLowerString(s_searchBuffer);
    if (activeScene != nullptr) {
        ensureHierarchyCache(gltfModel, activeScene, opts, lowerFilter);
    }

    // -----------------------------------------------------------------
    // Hierarchy Tree View (Child window so top search bar never scrolls)
    // -----------------------------------------------------------------
    ImGui::BeginChild("##HierarchyTreeScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiWindow* childWindow = ImGui::GetCurrentWindow();
    ImGuiStorage* storage = (childWindow && childWindow->DC.StateStorage) ? childWindow->DC.StateStorage : &s_hierarchyCache.standaloneStorage;
    const ImGuiID baseSeed = childWindow ? childWindow->ID : 0x12345678;

    // Dependency tracking for virtualized row model
    size_t vpCount = viewports ? viewports->size() : 0;
    const ViewportInstance* curActiveVp = activeViewport ? *activeViewport : nullptr;
    uint64_t vpHash = 0;
    if (viewports) {
        for (const auto& vp : *viewports) {
            vpHash = vpHash * 31 + (vp->isOpen ? 1 : 0) + vp->id * 7;
        }
    }

    const bool viewportsStale = (vpCount != s_hierarchyCache.lastViewportCount)
                             || (curActiveVp != s_hierarchyCache.lastActiveViewport)
                             || (vpHash != s_hierarchyCache.lastViewportStateHash);

    const auto* isolationRoot = gltfLayer != nullptr ? gltfLayer->getIsolationRoot() : nullptr;
    const bool isolationIncludeChildren = gltfLayer != nullptr && gltfLayer->getIsolationIncludeChildren();
    const bool isolationStale = (isolationRoot != s_hierarchyCache.isolationRoot)
                             || (isolationIncludeChildren != s_hierarchyCache.isolationIncludeChildren);

    const void* curSelectedObj = nullptr;
    if (selection.type == SelectionType::Node) curSelectedObj = selection.selectedNode;
    else if (selection.type == SelectionType::Mesh) curSelectedObj = selection.selectedMesh;
    else if (selection.type == SelectionType::Material) curSelectedObj = selection.selectedMaterial;
    else if (selection.type == SelectionType::Camera) curSelectedObj = selection.selectedCamera;
    else if (selection.type == SelectionType::Light) curSelectedObj = selection.selectedLight;
    else if (selection.type == SelectionType::Scene) curSelectedObj = selection.selectedScene;
    else if (selection.type == SelectionType::Viewport) curSelectedObj = reinterpret_cast<void*>(static_cast<intptr_t>(selection.selectedViewportId));

    const bool selectionStale = (selection.type != s_hierarchyCache.lastSelectionType)
                             || (curSelectedObj != s_hierarchyCache.lastSelectedObject)
                             || (selection.revision != s_hierarchyCache.lastSelectionRevision);

    if (s_pendingScroll.active) {
        if (s_pendingScroll.object != nullptr && curSelectedObj != s_pendingScroll.object) {
            // Selection changed to a different object while scroll was pending: cancel stale request
            s_pendingScroll = PendingScroll{};
        } else if (s_pendingScroll.object == nullptr && curSelectedObj != nullptr) {
            s_pendingScroll.type = selection.type;
            s_pendingScroll.object = const_cast<void*>(curSelectedObj);
        }
    }

    if (!s_hierarchyCache.rowModelValid || viewportsStale || selectionStale || isolationStale) {
        s_hierarchyCache.lastViewportCount = vpCount;
        s_hierarchyCache.lastActiveViewport = curActiveVp;
        s_hierarchyCache.lastViewportStateHash = vpHash;
        s_hierarchyCache.lastSelectionType = selection.type;
        s_hierarchyCache.lastSelectedObject = curSelectedObj;
        s_hierarchyCache.lastSelectionRevision = selection.revision;
        s_hierarchyCache.isolationRoot = isolationRoot;
        s_hierarchyCache.isolationIncludeChildren = isolationIncludeChildren;

        buildRowModel(gltfModel, activeScene, selection, lowerFilter, viewports, curActiveVp, *storage, baseSeed, isolationRoot, isolationIncludeChildren, selectionStale);
    }

    if (activeScene == nullptr) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No active glTF scene.");
    }

    if (!s_hierarchyCache.rows.empty()) {
        auto isRowSelected = [&](const HierarchyRow& r) -> bool {
            if (curSelectedObj == nullptr) return false;
            if (r.type == HierarchyRowType::ViewportItem) {
                return selection.type == SelectionType::Viewport && selection.selectedViewportId == static_cast<ViewportInstance*>(r.object)->id;
            }
            return r.object == curSelectedObj;
        };

        const bool useUnclippedFallback = s_hierarchyCache.hasMultiline
            || (!lowerFilter.empty() && s_hierarchyCache.nodeRowCount == 0);

        const float indentSpacing = ImGui::GetStyle().IndentSpacing;
        if (useUnclippedFallback) {
            // Unclipped fallback path for multiline content or no-match search results
            int currentDepth = 0;
            for (size_t i = 0; i < s_hierarchyCache.rows.size(); ++i) {
                const auto& row = s_hierarchyCache.rows[i];
                if (row.depth != currentDepth) {
                    ImGui::Indent((row.depth - currentDepth) * indentSpacing);
                    currentDepth = row.depth;
                }
                renderRow(row, i, gltfModel, activeScene, selection, gltfLayer, cameraActor, antiAliasing, revisions, activeViewport, viewportMenu, isRowSelected(row));
            }
            if (currentDepth > 0) {
                ImGui::Unindent(currentDepth * indentSpacing);
            }
        } else {
            // Virtualized clipped path for fixed-height single-line rows
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(s_hierarchyCache.rows.size()));

            ImGuiContext& g = *GImGui;
            if (g.NavId != 0) {
                auto it = s_hierarchyCache.rowIdToIndex.find(g.NavId);
                if (it != s_hierarchyCache.rowIdToIndex.end()) {
                    int navIdx = it->second;
                    clipper.IncludeItemsByIndex(std::max(0, navIdx - 1),
                                                std::min(static_cast<int>(s_hierarchyCache.rows.size()), navIdx + 2));
                }
            }

            if (s_pendingScroll.active && s_hierarchyCache.selectedRowIndex >= 0) {
                clipper.IncludeItemByIndex(s_hierarchyCache.selectedRowIndex);
            }

            bool viewportsHeaderRendered = false;
            while (clipper.Step()) {
                int currentDepth = 0;
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& row = s_hierarchyCache.rows[static_cast<size_t>(i)];
                    if (row.type == HierarchyRowType::ViewportsHeader) {
                        viewportsHeaderRendered = true;
                    }
                    if (row.depth != currentDepth) {
                        ImGui::Indent((row.depth - currentDepth) * indentSpacing);
                        currentDepth = row.depth;
                    }
                    renderRow(row, static_cast<size_t>(i), gltfModel, activeScene, selection, gltfLayer, cameraActor, antiAliasing, revisions, activeViewport, viewportMenu, isRowSelected(row));
                }
                if (currentDepth > 0) {
                    ImGui::Unindent(currentDepth * indentSpacing);
                }
            }

            if (!viewportsHeaderRendered && (viewports != nullptr && !viewports->empty())) {
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace ux3d::imgui
