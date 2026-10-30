// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Private to the SceneHierarchy*.cpp files: the row and cache types, the panel state they share (defined in
// SceneHierarchyPanel.cpp) and the cache / row-model functions the render and self-check files call.

#pragma once

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

struct PendingScroll {
    bool active = false;
    SelectionType type = SelectionType::None;
    void* object = nullptr;
};
// Panel state (SceneHierarchyPanel.cpp).
extern char s_searchBuffer[128];
extern PendingScroll s_pendingScroll;

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

extern HierarchyCache s_hierarchyCache;

// Label, cache and row-model helpers (SceneHierarchyPanel.cpp).
std::string toLowerString(const std::string& str);

std::string formatLabel(const char* icon,
                        const char* title,
                        uint32_t id,
                        const std::string& name,
                        const HierarchyDisplayOptions& opts);

void ensureHierarchyCache(const ux3d::gltf::Gltf& gltfModel,
                          const ux3d::gltf::Scene* activeScene,
                          const HierarchyDisplayOptions& opts,
                          const std::string& lowerFilter);

const HierarchyNodeCacheEntry* hierarchyEntry(const ux3d::gltf::Node* node);

void buildRowModel(const ux3d::gltf::Gltf& gltfModel,
                   const ux3d::gltf::Scene* activeScene,
                   const SelectionState& selection,
                   const std::string& lowerFilter,
                   const std::vector<std::unique_ptr<ViewportInstance>>* viewports,
                   const ViewportInstance* activeViewport,
                   ImGuiStorage& storage,
                   ImGuiID baseSeed,
                   const ux3d::gltf::Node* isolationRoot,
                   bool isolationIncludeChildren,
                   bool selectionChanged = true);

} // namespace ux3d::imgui
