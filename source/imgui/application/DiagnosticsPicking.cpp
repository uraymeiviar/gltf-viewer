// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Report helpers on a viewport: the camera JSON, a node's screen rectangle (--report-node) and the GPU pick at a
// pixel (--report-pick, probe).

#include "Application.h"
#include "imgui/RenderQuality.h"
#include "imgui/GlCaps.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/stb_image_write.h"
#include "imgui/SceneHierarchyPanel.h"
#include "renderer/Skin.h"
#include "gltf/Material.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <glm/gtc/matrix_transform.hpp>

namespace ux3d::imgui::application {

nlohmann::json Application::getCameraJson(const ux3d::imgui::ViewportInstance* vp)
{
    nlohmann::json cam;
    if (vp == nullptr) {
        return cam;
    }
    auto* renCam = vp->cameraActor.getUserCamera();
    const auto eye = vp->cameraActor.getEyePosition();
    const auto target = vp->cameraActor.getLookTarget();
    const auto up = renCam != nullptr ? renCam->getUp() : ux3d::math::vec3(0, 1, 0);
    float fovY = renCam != nullptr ? glm::degrees(renCam->getYFoV()) : 45.0f;
    float zNear = renCam != nullptr ? renCam->getZNearPlane() : 0.1f;
    float zFar = renCam != nullptr ? renCam->getZFarPlane() : 100.0f;
    std::string projection = (renCam != nullptr && renCam->getType() == ux3d::renderer::Camera::Type::Orthographic) ? "orthographic" : "perspective";
    int vpW = vp->sceneFbo.getWidth();
    int vpH = vp->sceneFbo.getHeight();
    float dist = vp->cameraActor.getZoom();
    float yawDeg = glm::degrees(vp->cameraActor.getRotationX());
    float pitchDeg = glm::degrees(vp->cameraActor.getRotationY());

    char cliArgsBuf[512];
    std::snprintf(cliArgsBuf, sizeof(cliArgsBuf),
        "--camera-target %.6f,%.6f,%.6f --camera-distance %.6f --camera-yaw %.6f --camera-pitch %.6f --camera-fov %.6f --camera-projection %s",
        target.x, target.y, target.z, dist, yawDeg, pitchDeg, fovY, projection.c_str());

    cam["eye"] = { eye.x, eye.y, eye.z };
    cam["target"] = { target.x, target.y, target.z };
    cam["up"] = { up.x, up.y, up.z };
    cam["fovY"] = fovY;
    cam["near"] = zNear;
    cam["far"] = zFar;
    cam["projection"] = projection;
    cam["viewport"] = { vpW, vpH };
    cam["cliArgs"] = cliArgsBuf;
    return cam;
}

nlohmann::json Application::computeNodeScreenRect(const ux3d::gltf::Node* node, const ux3d::imgui::ViewportInstance* vp)
{
    nlohmann::json result;
    if (node == nullptr || vp == nullptr) {
        return result;
    }

    const NodeOBB& obb = getCachedNodeOBB(node, gltfLayer.get());
    int vpW = vp->sceneFbo.getWidth();
    int vpH = vp->sceneFbo.getHeight();
    if (vpW < 1) vpW = 1;
    if (vpH < 1) vpH = 1;

    glm::mat4 view = vp->rendererCamera.getViewMatrix();
    glm::mat4 proj = vp->rendererCamera.getProjectionMatrix();
    glm::mat4 viewProj = proj * view;

    glm::vec3 worldCenter;
    if (obb.valid) {
        glm::vec3 localCenter = (obb.localMin + obb.localMax) * 0.5f;
        worldCenter = glm::vec3(obb.modelMatrix * glm::vec4(localCenter, 1.0f));
    } else {
        glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
        worldCenter = glm::vec3(nodeWorld[3]);
    }

    result["index"] = node->m_ID;
    result["name"] = node->hasName() ? node->getName() : ("Node " + std::to_string(node->m_ID));
    result["visible"] = node->getVisible();
    result["center"] = { worldCenter.x, worldCenter.y, worldCenter.z };

    if (!obb.valid) {
        glm::vec4 clip = viewProj * glm::vec4(worldCenter, 1.0f);
        bool behindCamera = clip.w <= 0.001f;
        float x = 0.0f, y = 0.0f;
        bool onScreen = false;
        if (!behindCamera) {
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;
            x = (ndcX * 0.5f + 0.5f) * static_cast<float>(vpW);
            y = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(vpH);
            onScreen = (x >= 0.0f && x <= static_cast<float>(vpW) && y >= 0.0f && y <= static_cast<float>(vpH));
        }
        result["screenRect"] = { x, y, x, y };
        result["onScreen"] = onScreen;
        result["clipped"] = !onScreen;
        result["behindCamera"] = behindCamera;
        return result;
    }

    glm::vec3 corners[8] = {
        { obb.localMin.x, obb.localMin.y, obb.localMin.z },
        { obb.localMax.x, obb.localMin.y, obb.localMin.z },
        { obb.localMax.x, obb.localMin.y, obb.localMax.z },
        { obb.localMin.x, obb.localMin.y, obb.localMax.z },
        { obb.localMin.x, obb.localMax.y, obb.localMin.z },
        { obb.localMax.x, obb.localMax.y, obb.localMin.z },
        { obb.localMax.x, obb.localMax.y, obb.localMax.z },
        { obb.localMin.x, obb.localMax.y, obb.localMax.z }
    };

    glm::vec4 worldCorners[8];
    glm::vec4 clipCorners[8];
    int behindCount = 0;
    for (int i = 0; i < 8; ++i) {
        worldCorners[i] = obb.modelMatrix * glm::vec4(corners[i], 1.0f);
        clipCorners[i] = viewProj * worldCorners[i];
        if (clipCorners[i].w <= 0.001f) {
            ++behindCount;
        }
    }

    bool allBehind = (behindCount == 8);
    result["behindCamera"] = allBehind;

    if (allBehind) {
        result["screenRect"] = { 0.0f, 0.0f, 0.0f, 0.0f };
        result["onScreen"] = false;
        result["clipped"] = true;
        return result;
    }

    static const int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };

    std::vector<glm::vec2> projectedPoints;
    bool anyClipped = (behindCount > 0);

    for (int i = 0; i < 8; ++i) {
        if (clipCorners[i].w > 0.001f) {
            float ndcX = clipCorners[i].x / clipCorners[i].w;
            float ndcY = clipCorners[i].y / clipCorners[i].w;
            float px = (ndcX * 0.5f + 0.5f) * static_cast<float>(vpW);
            float py = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(vpH);
            projectedPoints.push_back({ px, py });
            if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) {
                anyClipped = true;
            }
        }
    }

    constexpr float nearW = 0.001f;
    for (int e = 0; e < 12; ++e) {
        const auto& c0 = clipCorners[edges[e][0]];
        const auto& c1 = clipCorners[edges[e][1]];
        if ((c0.w > nearW && c1.w <= nearW) || (c0.w <= nearW && c1.w > nearW)) {
            float t = (nearW - c0.w) / (c1.w - c0.w);
            glm::vec4 clipped = c0 + t * (c1 - c0);
            float ndcX = clipped.x / nearW;
            float ndcY = clipped.y / nearW;
            float px = (ndcX * 0.5f + 0.5f) * static_cast<float>(vpW);
            float py = (1.0f - (ndcY * 0.5f + 0.5f)) * static_cast<float>(vpH);
            projectedPoints.push_back({ px, py });
            anyClipped = true;
        }
    }

    if (projectedPoints.empty()) {
        result["screenRect"] = { 0.0f, 0.0f, 0.0f, 0.0f };
        result["onScreen"] = false;
        result["clipped"] = true;
        return result;
    }

    float minX = projectedPoints[0].x, maxX = projectedPoints[0].x;
    float minY = projectedPoints[0].y, maxY = projectedPoints[0].y;
    for (size_t i = 1; i < projectedPoints.size(); ++i) {
        minX = std::min(minX, projectedPoints[i].x);
        maxX = std::max(maxX, projectedPoints[i].x);
        minY = std::min(minY, projectedPoints[i].y);
        maxY = std::max(maxY, projectedPoints[i].y);
    }

    bool onScreen = (maxX >= 0.0f && minX <= static_cast<float>(vpW) &&
                     maxY >= 0.0f && minY <= static_cast<float>(vpH));

    if (minX < 0.0f || maxX > static_cast<float>(vpW) ||
        minY < 0.0f || maxY > static_cast<float>(vpH)) {
        anyClipped = true;
    }

    result["screenRect"] = { minX, minY, maxX, maxY };
    result["onScreen"] = onScreen;
    result["clipped"] = anyClipped;
    return result;
}

nlohmann::json Application::performPickAtPixel(int px, int py, ux3d::imgui::ViewportInstance* vp)
{
    if (vp == nullptr || gltfLayer == nullptr || activeScene == nullptr) {
        return nullptr;
    }
    int vpW = vp->sceneFbo.getWidth();
    int vpH = vp->sceneFbo.getHeight();
    if (vpW < 1 || vpH < 1) {
        return nullptr;
    }

    if (px < 0) px = 0;
    if (px >= vpW) px = vpW - 1;
    if (py < 0) py = 0;
    if (py >= vpH) py = vpH - 1;

    int glPy = vpH - py - 1;

    if (!pickingTarget.isValid() || pickingTarget.getWidth() != 1 || pickingTarget.getHeight() != 1) {
        pickingTarget.resize(1, 1, 1);
    }

    auto* pickCamera = gltfLayer->getActiveCamera();
    if (pickCamera == nullptr) {
        return nullptr;
    }

    const auto sceneProj = pickCamera->getProjectionMatrix();

    auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
    currentReverseMapping = std::move(reverseMapping);

    glm::mat4 pixelWindow(1.0f);
    pixelWindow[0][0] = static_cast<float>(vpW);
    pixelWindow[1][1] = static_cast<float>(vpH);
    pixelWindow[3][0] = static_cast<float>(vpW - 2 * px - 1);
    pixelWindow[3][1] = static_cast<float>(vpH - 2 * glPy - 1);
    pickCamera->setProjection(ux3d::math::mat4x4(pixelWindow * glm::mat4(sceneProj)));

    if (!s_disablePickCulling) {
        gltfLayer->cullToClip(scene.drawables, currentReverseMapping,
                              pickCamera->getProjectionMatrix() * pickCamera->getViewMatrix());
    }

    pickingTarget.bind();
    glViewport(0, 0, 1, 1);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pickingLayer->prePass = [&]() {
        pickingTarget.bind();
        glViewport(0, 0, 1, 1);
    };
    pickingLayer->postPass = [&]() {
        pickingTarget.bind();
        glViewport(0, 0, 1, 1);
    };

    pickingLayer->pickingOnly = true;
    pickingLayer->pixelOriginX = px;
    pickingLayer->pixelOriginY = glPy;
    auto pickingScene = pickingLayer->applyLayer(std::move(scene));
    pickingLayer->pickingOnly = false;

    commandGenerator->update(pickingScene);
    glRenderer->draw({ commandGenerator.get() });
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    pickCamera->setProjection(sceneProj);

    uint8_t pixel[4] = {0, 0, 0, 0};
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, pickingTarget.getFbo());
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    uint32_t pickingId = (static_cast<uint32_t>(pixel[0]) << 16) |
                         (static_cast<uint32_t>(pixel[1]) << 8) |
                         static_cast<uint32_t>(pixel[2]);

    if (pickingId == 0) {
        return nullptr;
    }

    auto* pickedMesh = pickingLayer->getMesh(pickingId);
    if (pickedMesh == nullptr) {
        return nullptr;
    }

    const ux3d::gltf::Node* pickedNode = nullptr;
    auto nodeIt = currentReverseMapping.find(pickedMesh);
    if (nodeIt != currentReverseMapping.end()) {
        pickedNode = nodeIt->second;
    }

    const ux3d::gltf::Mesh* gltfMesh = nullptr;
    const ux3d::gltf::Primitive* gltfPrim = nullptr;
    int primIdx = 0;

    for (const auto& [key, rendererMesh] : gltfLayer->m_meshMapping.map) {
        if (rendererMesh == pickedMesh) {
            if (!pickedNode) pickedNode = std::get<0>(key);
            gltfMesh = std::get<1>(key);
            gltfPrim = std::get<2>(key);
            break;
        }
    }

    if (gltfMesh != nullptr && gltfPrim != nullptr) {
        int idx = 0;
        for (const auto& p : gltfMesh->getPrimitives()) {
            if (&p == gltfPrim) {
                primIdx = idx;
                break;
            }
            idx++;
        }
    }

    const ux3d::gltf::Material* gltfMat = gltfPrim ? gltfPrim->getMaterial() : nullptr;

    nlohmann::json pickResult;
    pickResult["x"] = px;
    pickResult["y"] = py;
    if (pickedNode != nullptr) {
        pickResult["nodeIndex"] = pickedNode->m_ID;
        std::string nName = pickedNode->hasName() ? pickedNode->getName() : ("Node " + std::to_string(pickedNode->m_ID));
        pickResult["nodeName"] = nName;
        pickResult["node"] = nName;
    } else {
        pickResult["nodeIndex"] = -1;
        pickResult["nodeName"] = "";
        pickResult["node"] = "";
    }

    if (gltfMesh != nullptr) {
        pickResult["mesh"] = gltfMesh->hasName() ? gltfMesh->getName() : ("Mesh " + std::to_string(gltfMesh->m_ID));
    } else {
        pickResult["mesh"] = "";
    }
    pickResult["primitive"] = primIdx;

    if (gltfMat != nullptr) {
        pickResult["material"] = gltfMat->hasName() ? gltfMat->getName() : ("Material " + std::to_string(gltfMat->m_ID));
    } else {
        pickResult["material"] = "";
    }

    return pickResult;
}

} // namespace ux3d::imgui::application
