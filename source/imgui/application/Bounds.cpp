// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "gltf/Accessor.h"
#include "gltf/GlTypes.h"
#include "gltf/extensions/AsoboExtensions.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>

namespace ux3d::imgui::application {

ux3d::gltf::Extends Application::getNodeExtends(ux3d::gltf::Node* node, const ux3d::gltf::Gltf* gltfModel,
                                                const ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (node == nullptr || gltfModel == nullptr) {
        return ux3d::gltf::Extends();
    }
    ux3d::math::dmat4 parentTransform = (gltfLayer != nullptr)
                                            ? ux3d::math::dmat4(gltfLayer->getParentWorldTransform(node))
                                            : ux3d::math::identity<ux3d::math::dmat4>();
    return ux3d::gltf::utils::getExtends({node}, gltfModel, parentTransform);
}

void Application::accumulateNodeOBB(const ux3d::gltf::Node* node, const glm::mat4& invRootWorld,
                                    const ux3d::renderer::GltfRenderLayer* gltfLayer, NodeOBB& obb)
{
    if (node == nullptr) return;

    glm::mat4 nodeWorld =
        gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
    glm::mat4 toRootLocal = invRootWorld * nodeWorld;

    const auto* mesh = node->getMesh();
    if (mesh != nullptr) {
        for (const auto& primitive : mesh->getPrimitives()) {
            for (const auto& attribute : primitive.getAttributes()) {
                if (attribute.getName() != "POSITION") continue;
                const auto* accessor = attribute.getAccessor();
                if (accessor == nullptr) continue;
                if (accessor->getMin().size() < 3 || accessor->getMax().size() < 3) continue;

                glm::vec3 pMin(static_cast<float>(accessor->getMin()[0]), static_cast<float>(accessor->getMin()[1]),
                               static_cast<float>(accessor->getMin()[2]));
                glm::vec3 pMax(static_cast<float>(accessor->getMax()[0]), static_cast<float>(accessor->getMax()[1]),
                               static_cast<float>(accessor->getMax()[2]));

                if (accessor->isNormalized()) {
                    float minVal = std::abs(static_cast<float>(
                        ux3d::gltf::GlTypes::minimumValueFromComponentType(accessor->getComponentType())));
                    float maxVal = std::abs(static_cast<float>(
                        ux3d::gltf::GlTypes::maximumValueFromComponentType(accessor->getComponentType())));
                    if (minVal != 0.0f) pMin /= minVal;
                    if (maxVal != 0.0f) pMax /= maxVal;
                }

                glm::vec3 corners[8] = {{pMin.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMin.z}, {pMax.x, pMin.y, pMax.z},
                                        {pMin.x, pMin.y, pMax.z}, {pMin.x, pMax.y, pMin.z}, {pMax.x, pMax.y, pMin.z},
                                        {pMax.x, pMax.y, pMax.z}, {pMin.x, pMax.y, pMax.z}};

                for (const auto& c : corners) {
                    glm::vec4 pt = toRootLocal * glm::vec4(c, 1.0f);
                    obb.localMin = glm::min(obb.localMin, glm::vec3(pt));
                    obb.localMax = glm::max(obb.localMax, glm::vec3(pt));
                    obb.valid = true;
                }
            }
        }
    }

    for (const auto* child : node->getChildren()) {
        accumulateNodeOBB(child, invRootWorld, gltfLayer, obb);
    }
}

NodeOBB Application::getNodeOBB(const ux3d::gltf::Node* selectedNode, const ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    NodeOBB obb;
    if (selectedNode == nullptr) return obb;

    obb.modelMatrix = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(selectedNode))
                                : glm::mat4(selectedNode->getTransformMatrix());
    float det = glm::determinant(obb.modelMatrix);
    glm::mat4 invRootWorld = (std::abs(det) > 1e-7f) ? glm::inverse(obb.modelMatrix) : glm::mat4(1.0f);

    accumulateNodeOBB(selectedNode, invRootWorld, gltfLayer, obb);

    if (obb.valid) {
        for (int i = 0; i < 3; ++i) {
            if (obb.localMax[i] - obb.localMin[i] < 1e-5f) {
                obb.localMin[i] -= 0.001f;
                obb.localMax[i] += 0.001f;
            }
        }
    }

    return obb;
}

const NodeOBB& Application::getCachedNodeOBB(const ux3d::gltf::Node* node,
                                             const ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto& cache = s_boundsCache;
    const bool hit = gltfLayer != nullptr && cache.obbValid && cache.obbNode == node && cache.obbLayer == gltfLayer &&
                     cache.obbPoseGeneration == gltfLayer->getPoseGeneration();
    if (hit) {
        ux3d::imgui::profiling::countBoundsCacheHit();
        ++s_boundsHitTotal;
        return cache.obb;
    }
    ux3d::imgui::profiling::countBoundsComputation();
    ++s_boundsComputedTotal;
    cache.obb = getNodeOBB(node, gltfLayer);
    cache.obbNode = node;
    cache.obbLayer = gltfLayer;
    cache.obbPoseGeneration = gltfLayer != nullptr ? gltfLayer->getPoseGeneration() : 0;
    cache.obbValid = gltfLayer != nullptr; // without a layer the inputs have no generation; never reuse
    return cache.obb;
}

const ux3d::gltf::Extends& Application::getCachedSceneExtents(const ux3d::gltf::Scene* scene,
                                                              const ux3d::gltf::Gltf& gltfModel,
                                                              const ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto& cache = s_boundsCache;
    const bool hit = gltfLayer != nullptr && cache.extentsValid && cache.extentsScene == scene &&
                     cache.extentsLayer == gltfLayer && cache.extentsSceneGeneration == gltfLayer->getSceneGeneration();
    if (hit) {
        ux3d::imgui::profiling::countBoundsCacheHit();
        ++s_boundsHitTotal;
        return cache.extents;
    }
    ux3d::imgui::profiling::countBoundsComputation();
    ++s_boundsComputedTotal;
    cache.extents = ux3d::gltf::utils::getExtends(scene->getNodes(), &gltfModel);
    cache.extentsScene = scene;
    cache.extentsLayer = gltfLayer;
    cache.extentsSceneGeneration = gltfLayer != nullptr ? gltfLayer->getSceneGeneration() : 0;
    cache.extentsValid = gltfLayer != nullptr;
    return cache.extents;
}

namespace {

// A3.6: lowest world Y over the visible nodes' POSITION boxes (accessor min/max, eight corners through the
// node's static world matrix). Primitives of invisible / environment-occluder materials (MSFS collision and
// cabin volumes) are not part of the model's base.
struct ModelBaseScan {
    const ux3d::gltf::Gltf* gltf = nullptr;
    std::unordered_map<const ux3d::gltf::Material*, bool> hidden; // material -> never rendered
    double lowest = std::numeric_limits<double>::max();
    bool found = false;

    bool materialHidden(const ux3d::gltf::Material* material)
    {
        if (material == nullptr) return false;
        const auto it = hidden.find(material);
        if (it != hidden.end()) return it->second;
        const auto extensions =
            ux3d::gltf::asobo::parseExtensions(*gltf, *material, ux3d::gltf::asobo::Scope::Material);
        bool result = false;
        if (const auto* invisible = extensions.find("ASOBO_material_invisible")) {
            result = invisible->boolean("enabled").value_or(true);
        }
        if (const auto* occluder = extensions.find("ASOBO_material_environment_occluder")) {
            result = result || occluder->boolean("enabled").value_or(true);
        }
        hidden.emplace(material, result);
        return result;
    }

    void visit(const ux3d::gltf::Node* node, const ux3d::math::dmat4& parent)
    {
        if (node == nullptr || !node->getVisible()) return;
        const ux3d::math::dmat4 world = parent * node->getTransformMatrix();
        if (const auto* mesh = node->getMesh()) {
            for (const auto& primitive : mesh->getPrimitives()) {
                if (materialHidden(primitive.getMaterial())) continue;
                for (const auto& attribute : primitive.getAttributes()) {
                    if (attribute.getName() != "POSITION") continue;
                    const auto* accessor = attribute.getAccessor();
                    if (accessor == nullptr || accessor->getMin().size() < 3 || accessor->getMax().size() < 3) continue;
                    ux3d::math::dvec3 lo(accessor->getMin()[0], accessor->getMin()[1], accessor->getMin()[2]);
                    ux3d::math::dvec3 hi(accessor->getMax()[0], accessor->getMax()[1], accessor->getMax()[2]);
                    if (accessor->isNormalized()) {
                        const double minValue = std::abs(ux3d::gltf::GlTypes::minimumValueFromComponentType(accessor->getComponentType()));
                        const double maxValue = std::abs(ux3d::gltf::GlTypes::maximumValueFromComponentType(accessor->getComponentType()));
                        lo /= minValue != 0.0 ? minValue : maxValue;
                        if (maxValue != 0.0) hi /= maxValue;
                    }
                    for (int corner = 0; corner < 8; ++corner) {
                        const ux3d::math::dvec4 point(corner & 1 ? hi.x : lo.x, corner & 2 ? hi.y : lo.y,
                                                      corner & 4 ? hi.z : lo.z, 1.0);
                        lowest = std::min(lowest, (world * point).y);
                        found = true;
                    }
                }
            }
        }
        for (const auto* child : node->getChildren()) {
            visit(child, world);
        }
    }
};

} // namespace

bool Application::getCachedModelBaseHeight(const ux3d::gltf::Scene* scene, const ux3d::gltf::Gltf& gltfModel,
                                           const ux3d::renderer::GltfRenderLayer* gltfLayer, float& height)
{
    auto& cache = s_boundsCache;
    height = 0.0f;
    if (scene == nullptr || gltfLayer == nullptr) return false;
    const uint64_t transformRevision = sceneRevisions.revision(ux3d::imgui::SceneChange::Transform);
    const uint64_t visibilityRevision = sceneRevisions.revision(ux3d::imgui::SceneChange::Visibility);
    const uint64_t materialRevision = sceneRevisions.revision(ux3d::imgui::SceneChange::Material);
    const bool hit = cache.baseValid && cache.baseScene == scene && cache.baseLayer == gltfLayer &&
                     cache.baseSceneGeneration == gltfLayer->getSceneGeneration() &&
                     cache.baseTransformRevision == transformRevision &&
                     cache.baseVisibilityRevision == visibilityRevision && cache.baseMaterialRevision == materialRevision;
    if (!hit) {
        ModelBaseScan scan;
        scan.gltf = &gltfModel;
        for (const auto* node : scene->getNodes()) {
            scan.visit(node, ux3d::math::identity<ux3d::math::dmat4>());
        }
        cache.baseScene = scene;
        cache.baseLayer = gltfLayer;
        cache.baseSceneGeneration = gltfLayer->getSceneGeneration();
        cache.baseTransformRevision = transformRevision;
        cache.baseVisibilityRevision = visibilityRevision;
        cache.baseMaterialRevision = materialRevision;
        cache.baseValid = true;
        cache.baseFound = scan.found;
        cache.baseHeight = scan.found ? static_cast<float>(scan.lowest) : 0.0f;
    }
    height = cache.baseHeight;
    return cache.baseFound;
}

void Application::focusCameraOnNode(ux3d::gltf::Node* node, ux3d::gltf::Gltf& gltfModel,
                                    ux3d::renderer::GltfRenderLayer* gltfLayer, ux3d::editor::CameraActor& cameraActor,
                                    ux3d::imgui::AntiAliasing& antiAliasing, ux3d::imgui::SceneRevisions& revisions,
                                    float margin)
{
    if (node == nullptr) return;
    // Uncached: filling the OBB cache here, before the first frame, would change when the selection box first draws.
    const NodeOBB obb = getNodeOBB(node, gltfLayer);
    if (!obb.valid) {
        std::string name = node->hasName() ? node->getName() : ("Node #" + std::to_string(node->m_ID));
        std::fprintf(stderr, "Node '%s' has empty bounds; framing world position at default distance\n", name.c_str());
        logMessage("Node '" + name + "' has empty bounds; framing world position at default distance");
    }
    cameraActor.presentFocusNode(gltfLayer, node, &gltfModel, margin);
    antiAliasing.resetHistory();
    revisions.mark(ux3d::imgui::SceneChange::ViewCamera);
}

void Application::drawNodeCoordinateGizmo(ImDrawList* drawList, const glm::mat4& nodeWorld, const glm::mat4& viewProj,
                                          const ImVec2& vpPos, const ImVec2& vpSize, const glm::vec3& camEye,
                                          int axisConvention)
{
    glm::vec3 origin = glm::vec3(nodeWorld[3]);
    float distToCam = glm::length(origin - camEye);
    float axisWorldLen = std::max(0.002f, distToCam * 0.085f);

    glm::vec3 xDir, yDir, zDir;
    if (axisConvention == 0) {
        // Blender Source Convention (Z-Up):
        // Red   (X): Blender local X = glTF column 0 (width)
        // Green (Y): Blender local Y = -glTF column 2 (length towards top of panel)
        // Blue  (Z): Blender local Z = +glTF column 1 (normal pointing out of panel face)
        xDir =
            glm::length(glm::vec3(nodeWorld[0])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[0])) : glm::vec3(1, 0, 0);
        yDir = glm::length(glm::vec3(nodeWorld[2])) > 1e-5f ? -glm::normalize(glm::vec3(nodeWorld[2]))
                                                            : glm::vec3(0, 0, -1);
        zDir =
            glm::length(glm::vec3(nodeWorld[1])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[1])) : glm::vec3(0, 1, 0);
    } else {
        // glTF Native Convention (Y-Up):
        // Red   (X): glTF column 0
        // Green (Y): glTF column 1
        // Blue  (Z): glTF column 2
        xDir =
            glm::length(glm::vec3(nodeWorld[0])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[0])) : glm::vec3(1, 0, 0);
        yDir =
            glm::length(glm::vec3(nodeWorld[1])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[1])) : glm::vec3(0, 1, 0);
        zDir =
            glm::length(glm::vec3(nodeWorld[2])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[2])) : glm::vec3(0, 0, 1);
    }

    glm::vec3 xEnd = origin + xDir * axisWorldLen;
    glm::vec3 yEnd = origin + yDir * axisWorldLen;
    glm::vec3 zEnd = origin + zDir * axisWorldLen;

    auto project = [&](const glm::vec3& p, ImVec2& outPt, float& outZ) -> bool {
        glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
        if (clip.w <= 0.0001f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
        outPt.x = vpPos.x + (ndc.x * 0.5f + 0.5f) * vpSize.x;
        outPt.y = vpPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpSize.y;
        outZ = ndc.z;
        return true;
    };

    ImVec2 ptOrigin;
    float zOrigin;
    if (!project(origin, ptOrigin, zOrigin)) return;

    struct AxisItem {
        ImVec2 endPt;
        ImU32 color;
        const char* label;
        float z;
    };
    std::vector<AxisItem> axes;

    ImVec2 ptX, ptY, ptZ;
    float zX, zY, zZ;
    if (project(xEnd, ptX, zX)) axes.push_back({ptX, IM_COL32(235, 45, 45, 255), "X", zX});
    if (project(yEnd, ptY, zY)) axes.push_back({ptY, IM_COL32(45, 215, 45, 255), "Y", zY});
    if (project(zEnd, ptZ, zZ)) axes.push_back({ptZ, IM_COL32(45, 120, 245, 255), "Z", zZ});

    std::sort(axes.begin(), axes.end(), [](const AxisItem& a, const AxisItem& b) { return a.z > b.z; });

    for (const auto& axis : axes) {
        drawList->AddLine(ptOrigin, axis.endPt, axis.color, 3.0f);
        glm::vec2 dir(axis.endPt.x - ptOrigin.x, axis.endPt.y - ptOrigin.y);
        float len = glm::length(dir);
        if (len > 5.0f) {
            dir /= len;
            glm::vec2 perp(-dir.y, dir.x);
            float arrowSize = 11.0f;
            ImVec2 tip = axis.endPt;
            ImVec2 p1(axis.endPt.x - dir.x * arrowSize + perp.x * (arrowSize * 0.45f),
                      axis.endPt.y - dir.y * arrowSize + perp.y * (arrowSize * 0.45f));
            ImVec2 p2(axis.endPt.x - dir.x * arrowSize - perp.x * (arrowSize * 0.45f),
                      axis.endPt.y - dir.y * arrowSize - perp.y * (arrowSize * 0.45f));
            drawList->AddTriangleFilled(tip, p1, p2, axis.color);

            ImVec2 labelSize = ImGui::CalcTextSize(axis.label);
            ImVec2 labelPos(tip.x + dir.x * 10.0f - labelSize.x * 0.5f, tip.y + dir.y * 10.0f - labelSize.y * 0.5f);
            drawList->AddText(ImVec2(labelPos.x + 1.0f, labelPos.y + 1.0f), IM_COL32(0, 0, 0, 220), axis.label);
            drawList->AddText(labelPos, axis.color, axis.label);
        }
    }

    drawList->AddCircleFilled(ptOrigin, 4.5f, IM_COL32(255, 255, 255, 255));
    drawList->AddCircle(ptOrigin, 4.5f, IM_COL32(30, 30, 30, 255), 0, 1.5f);
}

} // namespace ux3d::imgui::application
