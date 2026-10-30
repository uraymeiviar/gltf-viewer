// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Scene Hierarchy panel state, the label / visibility / search cache and the flattened row model. The panel UI is
// in SceneHierarchyRender.cpp, the cache-invalidation self-check in SceneHierarchySelfCheck.cpp.

#include "SceneHierarchyInternal.h"

namespace ux3d::imgui {

char s_searchBuffer[128] = "";

PendingScroll s_pendingScroll;
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

std::string toLowerString(const std::string& str)
{
    std::string lower = str;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

std::string formatLabel(const char* icon,
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

HierarchyCache s_hierarchyCache;

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

void ensureHierarchyCache(const ux3d::gltf::Gltf& gltfModel,
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

const HierarchyNodeCacheEntry* hierarchyEntry(const ux3d::gltf::Node* node)
{
    const auto it = s_hierarchyCache.nodes.find(node);
    return it != s_hierarchyCache.nodes.end() ? &it->second : nullptr;
}

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
                   bool selectionChanged)
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

} // namespace ux3d::imgui
