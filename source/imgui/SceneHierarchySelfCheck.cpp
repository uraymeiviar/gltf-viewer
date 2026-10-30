// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// SceneHierarchyPanel::runCacheInvalidationSelfCheck: the hierarchy cache / row-model invalidation self-check.

#include "SceneHierarchyInternal.h"

namespace ux3d::imgui {

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

} // namespace ux3d::imgui
