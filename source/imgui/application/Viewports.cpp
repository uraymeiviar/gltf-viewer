// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/RenderQuality.h"
#include "imgui/Theme.h"
#include "imgui/ObjectActions.h"
#include "imgui/SceneHierarchyPanel.h"
#include "renderer/editor/EditorRenderLayer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace ux3d::imgui::application {

void Application::drawViewports(FrameContext& frame)
{
    auto& io = ImGui::GetIO();
    const bool frameSceneDirty = lastFrameSceneChanges != 0;
    // -------------------------------------------------------------
    // Viewport Windows (Multi-Viewport)
    // -------------------------------------------------------------
    auto& activeEnvironment = getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
    if (gltfLayer != nullptr && (ux3d::imgui::RenderQuality::allowIbl() || ux3d::imgui::RenderQuality::allowSkybox(activeEnvironment.showBackground))) {
        const auto desiredEnvPath = resolveHdrPath(activeEnvironment.currentHdrPath, rendererResourcesDir);
        // A3.1: while the sun is on (either mode) the HDR's own sun is removed from the panorama, so
        // switching the sun re-loads and re-filters the environment once (never per frame).
        const bool wantSunRemoval = activeEnvironment.sunEnabled;
        const bool sunRemovalChanged = gltfLayer->getEnvironmentSunRemoval() != wantSunRemoval;
        if (gltfLayer->getEnvironmentMapPath().toString() != desiredEnvPath.toString() || sunRemovalChanged) {
            gltfLayer->setEnvironmentSunRemoval(wantSunRemoval);
            scenarioEnvironmentPath = desiredEnvPath.toString().c_str();
            scenarioEnvironmentLoaded = false;
            if (ux3d::imgui::StdFileSystem::instance().isFile(desiredEnvPath)) {
                gltfLayer->setEnvironmentMap(desiredEnvPath);
                if (gltfLayer->filterEnvironmentMap()) {
                    scenarioEnvironmentLoaded = true;
                    sceneRevisions.mark(ux3d::imgui::SceneChange::RenderSettings);
                }
            }
        }
    }

    // Viewports drawn this frame share one scene traversal (see GltfRenderLayer).
    if (!s_disableSharedTraversal) {
        gltfLayer->beginSceneFrame();
    }
    for (auto& vp : viewports) {
        if (!vp->isOpen) {
            vp->antiAliasing.releaseTargets();
            vp->overlayFbo = ux3d::imgui::Framebuffer();
            vp->hasCachedImage = false;
            vp->sunShadowMap.release();
            vp->sunShadowActive = false;
            continue;
        }

        std::string wndTitle =
            std::string(ICON_MDI_CUBE_OUTLINE " ") + vp->name + "###Viewport_" + std::to_string(vp->id);

        ImGui::SetNextWindowDockID(s_centralDockId, ImGuiCond_Appearing);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool isVpVisible = ImGui::Begin(wndTitle.c_str(), &vp->isOpen);
        ImGui::PopStyleVar();

        if (!vp->isOpen) {
            vp->antiAliasing.releaseTargets();
            vp->overlayFbo = ux3d::imgui::Framebuffer();
            vp->hasCachedImage = false;
            vp->sunShadowMap.release();
            vp->sunShadowActive = false;
            ImGui::End();
            continue;
        }

        if (vp->requestFocus) {
            ImGui::SetWindowFocus();
            ImGuiWindow* w = ImGui::GetCurrentWindow();
            if (w != nullptr && w->DockNode != nullptr) {
                w->DockNode->SelectedTabId = w->TabId;
                if (w->DockNode->TabBar != nullptr) {
                    w->DockNode->TabBar->SelectedTabId = w->TabId;
                    w->DockNode->TabBar->NextSelectedTabId = w->TabId;
                }
            }
            activeViewport = vp.get();
            vp->requestFocus = false;
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            activeViewport = vp.get();
        }

        if (!isVpVisible) {
            ImGui::End();
            continue;
        }

        // Quick Navigation & Inspection Toolbar Strip. It keeps the default frame
        // padding, so it is exactly as tall as a collapsing header.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
        ImGui::BeginGroup();

        // 1. Perspective / Orthographic Toggle (E)
        bool isOrtho = (vp->rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic);
        const char* projIcon = isOrtho ? ICON_MDI_PERSPECTIVE_LESS : ICON_MDI_PERSPECTIVE_MORE;
        if (ImGui::SmallButton(projIcon)) {
            if (!vp->isAttachedToCamera) {
                vp->cameraActor.changeUserCameraType(isOrtho ? ux3d::renderer::Camera::Type::Perspective
                                                             : ux3d::renderer::Camera::Type::Orthographic);
                vp->antiAliasing.resetHistory();
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(isOrtho ? "Orthographic (Click to switch to Perspective)"
                                      : "Perspective (Click to switch to Orthographic)");
        }

        // 2. Toggle Floor Grid (D)
        ImGui::SameLine();
        bool gridActive = vp->settings.showGrid;
        if (gridActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
        }
        if (ImGui::SmallButton(ICON_MDI_GRID "##GridToggle")) {
            vp->settings.showGrid = !vp->settings.showGrid;
        }
        if (gridActive) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(vp->settings.showGrid ? "Floor Grid (Active - click to hide)"
                                                    : "Floor Grid (Hidden - click to show)");
        }

        // 3. Toggle Bounding Box (E)
        ImGui::SameLine();
        bool bboxActive = vp->settings.showSceneBoundingBox || vp->settings.showSelectedBoundingBox;
        if (bboxActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
        }
        if (ImGui::SmallButton(ICON_MDI_CUBE_OUTLINE "##BBoxToggle")) {
            bool next = !bboxActive;
            vp->settings.showSceneBoundingBox = next;
            vp->settings.showSelectedBoundingBox = next;
        }
        if (bboxActive) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(bboxActive ? "Bounding Boxes (Active - click to hide)"
                                         : "Bounding Boxes (Hidden - click to show)");
        }

        // 4. Toggle Orbit (Turntable)
        ImGui::SameLine();
        bool orbitActive = vp->settings.turntable;
        if (orbitActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
        }
        if (ImGui::SmallButton(ICON_MDI_ROTATE_ORBIT "##OrbitToggle")) {
            vp->settings.turntable = !vp->settings.turntable;
        }
        if (orbitActive) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(vp->settings.turntable ? "Turntable Orbit (Active - click to stop)"
                                                     : "Turntable Orbit (Stopped - click to start)");
        }

        // Material Variants (if model has variants)
        if (gltfModel.hasMaterialVariants()) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            auto& variants = gltfModel.getMaterialVariants();
            std::string curVarName = "Default";
            if (gltfLayer && gltfLayer->getMaterialVariant().has_value()) {
                uint32_t activeVarId = gltfLayer->getMaterialVariant().value();
                for (const auto& v : variants) {
                    if (v.m_ID == activeVarId) {
                        curVarName = v.hasName() ? v.getName().c_str() : ("Var " + std::to_string(v.m_ID));
                        break;
                    }
                }
            }
            std::string varComboId = "##ToolbarVariant_" + std::to_string(vp->id);
            if (ImGui::BeginCombo(varComboId.c_str(), curVarName.c_str())) {
                if (ImGui::Selectable("Default", !gltfLayer || !gltfLayer->getMaterialVariant().has_value())) {
                    if (gltfLayer) {
                        gltfLayer->setMaterialVariant(std::nullopt);
                        sceneRevisions.mark(ux3d::imgui::SceneChange::Material);
                    }
                }
                for (const auto& v : variants) {
                    bool isSel = (gltfLayer && gltfLayer->getMaterialVariant().has_value() &&
                                  gltfLayer->getMaterialVariant().value() == v.m_ID);
                    std::string vLabel = v.hasName() ? v.getName().c_str() : ("Var " + std::to_string(v.m_ID));
                    if (ImGui::Selectable(vLabel.c_str(), isSel)) {
                        if (gltfLayer) {
                            gltfLayer->setMaterialVariant(v.m_ID);
                            sceneRevisions.mark(ux3d::imgui::SceneChange::Material);
                        }
                    }
                }
                ImGui::EndCombo();
            }
        }

        // Right-corner pulldowns: [ Free Look | <camera-list> ] and [ Shading Mode ]
        std::string camPulldownLabel;
        if (vp->isAttachedToCamera) {
            std::string camName = vp->attachedCameraNodeName.empty() ? "Camera" : vp->attachedCameraNodeName;
            camPulldownLabel = std::string(ICON_MDI_CAMERA " ") + camName + " " ICON_MDI_MENU_DOWN;
        } else {
            camPulldownLabel = std::string(ICON_MDI_EYE_OUTLINE " Free Look ") + ICON_MDI_MENU_DOWN;
        }
        float camPulldownWidth = ImGui::CalcTextSize(camPulldownLabel.c_str()).x + 18.0f;

        const char* curModeLabel = getRenderModeLabel(vp->settings.renderMode);
        std::string shadingPulldownLabel = std::string(curModeLabel) + " " ICON_MDI_MENU_DOWN;
        float shadingPulldownWidth = ImGui::CalcTextSize(shadingPulldownLabel.c_str()).x + 18.0f;

        float totalRightWidth = camPulldownWidth + shadingPulldownWidth + ImGui::GetStyle().ItemSpacing.x;
        if (!ux3d::imgui::RenderQuality::legacyCaptureUi()) totalRightWidth += 109.0f;
        float rightX = ImGui::GetWindowContentRegionMax().x - totalRightWidth - 8.0f;
        if (rightX > ImGui::GetCursorPosX() + 10.0f) {
            ImGui::SameLine(rightX);
        } else {
            ImGui::SameLine();
        }

        // 1. Camera attachment pulldown [ Free Look | <camera-list> ]
        std::string camPopupId = "##CameraMenuPopup_" + std::to_string(vp->id);
        if (vp->isAttachedToCamera) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.78f, 1.0f, 1.0f));
        }
        if (ImGui::SmallButton(camPulldownLabel.c_str())) {
            ImGui::OpenPopup(camPopupId.c_str());
        }
        if (vp->isAttachedToCamera) {
            ImGui::PopStyleColor();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(vp->isAttachedToCamera ? "Camera Attached (Click to switch camera or Free Look)"
                                                     : "Free Look (Click to attach to a glTF camera)");
        }

        if (ImGui::BeginPopup(camPopupId.c_str())) {
            bool isFreeLook = !vp->isAttachedToCamera;
            if (ImGui::MenuItem(ICON_MDI_EYE_OUTLINE " Free Look", nullptr, isFreeLook)) {
                vp->isAttachedToCamera = false;
                vp->attachedCameraNode = nullptr;
                vp->attachedCameraNodeId = 0;
                vp->attachedCameraNodeName = "";
                sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
            }
            ImGui::Separator();

            auto cameraNodes = collectSceneCameraNodes(activeScene);
            if (cameraNodes.empty()) {
                ImGui::TextDisabled("  (No Cameras in Scene)");
            } else {
                for (const auto* camNode : cameraNodes) {
                    std::string cName =
                        camNode->hasName() ? camNode->getName() : ("Camera Node " + std::to_string(camNode->m_ID));
                    std::string camItemLabel = ICON_MDI_CAMERA " " + cName;
                    bool isSelected = vp->isAttachedToCamera &&
                                      (vp->attachedCameraNodeId == camNode->m_ID || vp->attachedCameraNode == camNode);
                    if (ImGui::MenuItem(camItemLabel.c_str(), nullptr, isSelected)) {
                        vp->isAttachedToCamera = true;
                        vp->attachedCameraNode = camNode;
                        vp->attachedCameraNodeId = camNode->m_ID;
                        vp->attachedCameraNodeName = cName;
                        sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
                    }
                }
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        // 2. Shading Mode pulldown
        std::string popupId = "##ShadingModePopup_" + std::to_string(vp->id);
        if (ImGui::SmallButton(shadingPulldownLabel.c_str())) {
            ImGui::OpenPopup(popupId.c_str());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Shading & Debug Render Modes");
        }

        if (ImGui::BeginPopup(popupId.c_str())) {
            drawShadingMenu(*vp);
            ImGui::EndPopup();
        }

        ImGui::EndGroup();
        ImGui::PopStyleVar();

        ImVec2 viewportPos = ImGui::GetCursorScreenPos();
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        // P7.5: the scene renders at renderScale of the panel size and the image is
        // stretched to the panel. Picking maps mouse positions through the same scale.
        if (!ux3d::imgui::RenderQuality::legacyCaptureUi()) {
            ImGui::SameLine();
            ux3d::imgui::RenderQuality::drawProfileSelector(sceneRevisions);
            viewportPos = ImGui::GetCursorScreenPos();
            viewportSize = ImGui::GetContentRegionAvail();
        }
        if (gltfModel.imagesOmitted()) {
            ImGui::TextDisabled("Opened without textures - material values only");
            viewportPos = ImGui::GetCursorScreenPos();
            viewportSize = ImGui::GetContentRegionAvail();
        }
        if (ux3d::imgui::RenderQuality::autoActive() && !ux3d::imgui::RenderQuality::qualityReached()) {
            ImGui::TextDisabled("%s", ux3d::imgui::RenderQuality::autoStatus().c_str());
            viewportPos = ImGui::GetCursorScreenPos();
            viewportSize = ImGui::GetContentRegionAvail();
        }
        const auto effectiveSettings = ux3d::imgui::RenderQuality::effectiveViewport(vp->settings);
        const float renderScale = ux3d::imgui::RenderQuality::effective().renderScale;
        int vpW = static_cast<int>(viewportSize.x * renderScale);
        int vpH = static_cast<int>(viewportSize.y * renderScale);
        if (vpW < 1) vpW = 1;
        if (vpH < 1) vpH = 1;

        const int postAA = effectiveSettings.effectivePostAAIdx();
        auto aaMode = static_cast<ux3d::imgui::AntiAliasingMode>(postAA == 0 ? 0 : postAA + 3);
        int desiredSamples = ux3d::imgui::AntiAliasing::getSampleCount(
            static_cast<ux3d::imgui::AntiAliasingMode>(effectiveSettings.effectiveMsaaIdx()));

        if (vp->sceneFbo.getWidth() != vpW || vp->sceneFbo.getHeight() != vpH ||
            vp->sceneFbo.getSamples() != desiredSamples || !vp->sceneFbo.isLinearFloat()) {
            vp->sceneFbo.resize(vpW, vpH, desiredSamples, true);
            vp->antiAliasing.resetHistory();
        }

        // P5.2: picking renders the clicked pixel into one shared 1x1 target. The
        // per-viewport full-size buffer is allocated only for --full-size-picking.
        if (s_fullSizePicking && (vp->pickingFbo.getWidth() != vpW || vp->pickingFbo.getHeight() != vpH)) {
            vp->pickingFbo.resize(vpW, vpH, 1);
        }
        ux3d::imgui::Framebuffer& pickTarget = s_fullSizePicking ? vp->pickingFbo : pickingTarget;

        pickingLayer->prePass = [&]() {
            pickTarget.bind();
            glViewport(0, 0, pickTarget.getWidth(), pickTarget.getHeight());
        };
        pickingLayer->postPass = [&]() {
            vp->sceneFbo.bind();
            glViewport(0, 0, vpW, vpH);
        };

        // P5.1: the picking buffer is read only by a left click handled later in this
        // viewport's frame, so it is rendered only when such a click is possible. The
        // test is a superset of the click handler's: same click, same image rectangle
        // (the image is drawn at viewportPos with viewportSize), and a hover test that
        // ignores active items and popups. --always-pick restores every-frame picking.
        const bool pickingRendered =
            s_alwaysPick || (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                             ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                                    ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
                             io.MousePos.x >= viewportPos.x && io.MousePos.x < viewportPos.x + viewportSize.x &&
                             io.MousePos.y >= viewportPos.y && io.MousePos.y < viewportPos.y + viewportSize.y);
        if (pickingRendered) {
            ++s_pickingRenderTotal;
        } else {
            ++s_pickingSkipTotal;
        }
        // The clicked pixel, computed exactly as the click handler computes it.
        int pickPx = static_cast<int>((io.MousePos.x - viewportPos.x) * renderScale);
        int pickPy = vpH - static_cast<int>((io.MousePos.y - viewportPos.y) * renderScale) - 1;
        pickPx = std::clamp(pickPx, 0, vpW - 1);
        pickPy = std::clamp(pickPy, 0, vpH - 1);

        // Clear picking buffer
        if (pickingRendered && pickTarget.isValid()) {
            pickTarget.bind();
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        float aspect = (vpH > 0) ? (static_cast<float>(vpW) / static_cast<float>(vpH)) : 1.0f;
        if (gltfLayer != nullptr) {
            gltfLayer->viewportAspectRatio = aspect;
        }

        glm::mat4 view(1.0f);
        glm::mat4 proj(1.0f);
        glm::vec3 cameraEye(0.0f, 0.0f, 1.0f);
        float cameraZoom = 1.0f;

        if (vp->isAttachedToCamera && vp->attachedCameraNode != nullptr) {
            gltfLayer->setActiveGltfCameraByNode(vp->attachedCameraNode);
            auto* renCam = gltfLayer->getActiveCamera();
            if (renCam != nullptr) {
                renCam->setViewportResolution(vpW, vpH);
                renCam->setAspectRatio(aspect);
                view = renCam->getViewMatrix();
                proj = renCam->getProjectionMatrix();
                cameraEye = glm::vec3(renCam->getPositon().x, renCam->getPositon().y, renCam->getPositon().z);
                cameraZoom = renCam->getZoom();
            } else {
                vp->rendererCamera.setViewportResolution(vpW, vpH);
                vp->rendererCamera.setAspectRatio(aspect);
                vp->cameraActor.updateUserCameraTransform();
                gltfLayer->setActiveCamera(&vp->rendererCamera);
                view = vp->rendererCamera.getViewMatrix();
                proj = vp->rendererCamera.getProjectionMatrix();
                cameraEye = glm::vec3(vp->cameraActor.getEyePosition().x, vp->cameraActor.getEyePosition().y,
                                      vp->cameraActor.getEyePosition().z);
                cameraZoom = vp->cameraActor.getZoom();
            }
        } else {
            vp->rendererCamera.setViewportResolution(vpW, vpH);
            vp->rendererCamera.setAspectRatio(aspect);
            vp->cameraActor.updateUserCameraTransform();
            gltfLayer->setActiveCamera(&vp->rendererCamera);
            view = vp->rendererCamera.getViewMatrix();
            proj = vp->rendererCamera.getProjectionMatrix();
            cameraEye = glm::vec3(vp->cameraActor.getEyePosition().x, vp->cameraActor.getEyePosition().y,
                                  vp->cameraActor.getEyePosition().z);
            cameraZoom = vp->cameraActor.getZoom();
        }

        // Subpixel jitter for TAA
        glm::vec2 jitter = vp->antiAliasing.getJitter(aaMode, vpW, vpH);
        glm::mat4 jitterMat = (aaMode == ux3d::imgui::AntiAliasingMode::TAA)
                                  ? glm::translate(glm::mat4(1.0f), glm::vec3(2.0f * jitter.x, 2.0f * jitter.y, 0.0f))
                                  : glm::mat4(1.0f);
        glm::mat4 jitteredProj = jitterMat * proj;
        glm::mat4 viewProj = proj * view; // Overlays always use the stable camera.

        // P5.3: redraw only when the render key changed, a click needs the picking
        // pass, the cached image is gone, or TAA is still accumulating. The key holds
        // the unjittered projection; TAA history resets force a render.
        ux3d::imgui::ViewportRenderKey renderKey;
        renderKey.layer = gltfLayer.get();
        renderKey.scene = activeScene;
        for (size_t k = 0; k < ux3d::imgui::kSceneChangeCount; ++k) {
            renderKey.revisions[k] = sceneRevisions.revision(static_cast<ux3d::imgui::SceneChange>(k));
        }
        if (gltfLayer != nullptr) {
            renderKey.layerSceneGeneration = gltfLayer->getSceneGeneration();
            renderKey.layerPoseGeneration = gltfLayer->getPoseGeneration();
        }
        renderKey.view = view;
        renderKey.projection = proj;
        renderKey.eye = cameraEye;
        renderKey.zoom = cameraZoom;
        renderKey.width = vpW;
        renderKey.height = vpH;
        renderKey.samples = vp->sceneFbo.getSamples();
        renderKey.aaMode = static_cast<int>(aaMode);
        renderKey.aaTargetGeneration = vp->antiAliasing.getTargetGeneration();
        renderKey.attached = vp->isAttachedToCamera;
        renderKey.attachedCameraNode = vp->attachedCameraNode;
        renderKey.settings = vp->settings;
        renderKey.environment = activeEnvironment;
        const bool keyUnchanged = vp->hasCachedImage && renderKey == vp->lastRenderKey;
        const bool isTaa = aaMode == ux3d::imgui::AntiAliasingMode::TAA;
        const bool taaAccumulating =
            isTaa && (!vp->antiAliasing.isHistoryValid() || vp->taaStableRenders < kTaaSettleRenders);
        const bool exposureAdapting = activeEnvironment.autoExposure && vp->antiAliasing.isExposureAdapting();
        const bool cameraJumped = !vp->hasCachedImage ||
            (glm::distance(cameraEye, vp->lastRenderKey.eye) > 0.5f * std::max(1.0f, cameraZoom));
        const bool redrawScene = s_disableRedrawSkip || !keyUnchanged || pickingRendered || taaAccumulating || exposureAdapting
            || ux3d::imgui::RenderQuality::wantsSample();
        if (redrawScene) {
            vp->taaStableRenders =
                (keyUnchanged && isTaa && vp->antiAliasing.isHistoryValid()) ? vp->taaStableRenders + 1 : 0;
            ++vp->sceneRenders;
            ++s_sceneRenderTotal;
        } else {
            ++vp->sceneSkips;
            ++s_sceneSkipTotal;
            if (isTaa) {
                vp->antiAliasing.skipFrame();
            }
        }

        // Render to offscreen Framebuffer
        if (redrawScene) {
            vp->sceneFbo.bind();
            glViewport(0, 0, vpW, vpH);
            if (ux3d::imgui::RenderQuality::allowSkybox(activeEnvironment.showBackground)) {
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            } else {
                glClearColor(activeEnvironment.clearColor.r, activeEnvironment.clearColor.g,
                             activeEnvironment.clearColor.b, 0.0f);
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        if (redrawScene && aaMode == ux3d::imgui::AntiAliasingMode::TAA) {
            if (gltfLayer->getActiveCamera()) {
                gltfLayer->getActiveCamera()->setProjection(ux3d::math::mat4x4(jitteredProj));
            }
        }

        GLuint displayTexture = vp->cachedDisplayTexture;

        // 1. Render glTF scene & background skybox
        if (redrawScene && gltfLayer != nullptr && glRenderer != nullptr && commandGenerator != nullptr) {
            const auto sceneBuildStart = ux3d::imgui::profiling::Clock::now();
            gltfLayer->enableCubeMap(ux3d::imgui::RenderQuality::allowSkybox(activeEnvironment.showBackground));
            glRenderer->setClearColor(activeEnvironment.clearColor);
            gltfLayer->getResourceManager().setClearColor(activeEnvironment.clearColor);

            auto mode = vp->settings.renderMode;
            // With no IBL, offer a useful texture-colour inspection view even for
            // models without authored lights (the Low preset).
            if (!ux3d::imgui::RenderQuality::allowIbl() && mode == ux3d::renderer::RenderMode::None)
                mode = ux3d::renderer::RenderMode::BaseColor;

            // A2.1: the two decal debug views show the prepass targets instead of a PBR
            // debug channel, so the forward draws stay normally shaded underneath.
            const bool decalDebugView =
                mode == ux3d::renderer::RenderMode::DecalPrepassDepth ||
                mode == ux3d::renderer::RenderMode::DecalPrepassNormal;
            const bool ssrDebugOverlay = mode == ux3d::renderer::RenderMode::SSR ||
                mode == ux3d::renderer::RenderMode::SSRRay;
            const auto shadingMode = decalDebugView ? ux3d::renderer::RenderMode::None : mode;
            const bool isWireframeMode = shadingMode == ux3d::renderer::RenderMode::Wireframe;
            const bool hasWireframeOverlay = vp->settings.wireframeOverlay && !isWireframeMode;
            const bool drawWireframe = isWireframeMode || hasWireframeOverlay;

            // Persistent overrides (double-sidedness, winding) are rewritten only when
            // this viewport's overrides or the layer changed (P4.5). The DEBUG define
            // and polygon mode are reset by createScene and applied per drawable below.
            ux3d::imgui::ViewportDrawOverrides drawOverrides;
            drawOverrides.forceDoubleSided = vp->settings.forceDoubleSided;
            drawOverrides.invertWinding = vp->settings.invertWinding;
            if (s_sharedOverrides.apply(*gltfLayer, drawOverrides)) {
                ++s_overrideApplyTotal;
                if (s_profilingEnabled) ++ux3d::imgui::profiling::frameCounters().overrideApplies;
            } else {
                ++s_overrideSkipTotal;
                if (s_profilingEnabled) ++ux3d::imgui::profiling::frameCounters().overrideSkips;
            }

            // A2.1: only a file with decal materials, in a viewport where ASOBO is on and
            // ASOBO_material_geometry_decal is applied, allocates the shared buffer.
            const bool decalActive = viewportNeedsDecalBuffer(*vp);
            if (decalActive) {
                // One buffer for every viewport, sized to the largest render-scale
                // resolution that needs it. Other open viewports contribute their latest
                // scene target size; this one contributes the size being rendered now.
                int bufferWidth = vpW;
                int bufferHeight = vpH;
                for (const auto& other : viewports) {
                    if (!other || !other->isOpen || other.get() == vp.get() || !viewportNeedsDecalBuffer(*other)) {
                        continue;
                    }
                    bufferWidth = std::max(bufferWidth, other->sceneFbo.getWidth());
                    bufferHeight = std::max(bufferHeight, other->sceneFbo.getHeight());
                }
                if (decalBuffer.ensure(bufferWidth, bufferHeight)) {
                    // The prepass target changed; the debug material reads it through
                    // stable texture wrappers refreshed here.
                    decalPrepassLayer->setDebugTargets(decalBuffer.getDepthTexture(), decalBuffer.getNormalTexture(),
                                                       decalBuffer.getWidth(), decalBuffer.getHeight());
                    // A2.2: the receiver programs and the decal pass read the same buffer
                    // through stable samplers; only the handles move on create/resize.
                    commandGenerator->setDecalTargets(decalBuffer.getAccumulationTexture(0),
                                                      decalBuffer.getAccumulationTexture(1),
                                                      decalBuffer.getAccumulationTexture(2),
                                                      decalBuffer.getAccumulationTexture(3),
                                                      decalBuffer.getAccumulationTexture(4),
                                                      decalBuffer.getAccumulationTexture(5));
                    decalAccumulationLayer->setG0Target(decalBuffer.getNormalTexture());
                    // A2.2: the prepass depth for the shader's tolerant decal depth test.
                    decalAccumulationLayer->setDepthTarget(decalBuffer.getDepthTexture());
                }
                // Depth, G0 and every accumulation target are cleared in the frame the
                // buffer is used, whether or not the prepass ends up drawing anything.
                decalBuffer.clearTargets();
                vp->sceneFbo.bind();
                glViewport(0, 0, vpW, vpH);
            }

            // A3.6: the ground plane is drawn in shaded views only (debug views and wireframe keep the sky), at the
            // model base, Y = 0 or a chosen height.
            const bool groundPlane = vp->settings.ground.mode == ux3d::imgui::GroundMode::Plane &&
                                     vp->settings.renderMode == ux3d::renderer::RenderMode::None && !isWireframeMode &&
                                     activeScene != nullptr && gltfLayer->getActiveCamera() != nullptr;
            float groundHeight = 0.0f;
            if (groundPlane) {
                const auto& ground = vp->settings.ground;
                if (ground.heightMode == ux3d::imgui::GroundHeightMode::ModelBase) {
                    getCachedModelBaseHeight(activeScene, gltfModel, gltfLayer.get(), groundHeight);
                } else if (ground.heightMode == ux3d::imgui::GroundHeightMode::Custom) {
                    groundHeight = ground.customHeight;
                }
            }
            vp->groundPlaneActive = groundPlane;
            vp->groundPlaneHeight = groundHeight;
            vp->groundAlbedo = groundPlane ? vp->settings.ground.averageAlbedo() : glm::vec3(0.0f);

            // A3.2: the sun's shadow map for this viewport, before the forward scene is built (the caster
            // pass builds its own unculled scene, so off-screen casters still cast). Off: no pass at all.
            {
                const auto shadowStart = ux3d::imgui::profiling::Clock::now();
                updateSunShadows(*vp, frame, view, proj, activeEnvironment, !isWireframeMode, vpW, vpH,
                                 groundPlane ? std::optional<float>(groundHeight) : std::nullopt);
                if (vp->sunShadowActive) frame.markStage("shadow.render", shadowStart);
            }

            const auto createSceneStart = ux3d::imgui::profiling::Clock::now();
            gltfLayer->frustumCulling = !s_disableCulling;
            gltfLayer->asoboOptions = vp->settings.asobo;
            gltfLayer->materialExtensionOptions = vp->settings.materialExtensions;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            gltfLayer->frustumCulling = false;
            s_culledTotal += gltfLayer->getLastSceneStats().culled;
            s_uncullableTotal += gltfLayer->getLastSceneStats().uncullable;
            frame.markStage("scene.create", createSceneStart);
            if (s_profilingEnabled) {
                const auto& sceneStats = gltfLayer->getLastSceneStats();
                ux3d::imgui::profiling::countSceneBuild(sceneStats.drawables);
                ux3d::imgui::profiling::countSceneNodesVisited(sceneStats.nodesVisited);
            }
            // Moved, not copied: reverseMapping is not used after this point (P4.4).
            currentReverseMapping = std::move(reverseMapping);

            // Keep source materials intact for shaded views and other viewports. The
            // dedicated line shader neither samples alpha nor discards MASK fragments,
            // and emits alpha 1 even for a transparent forward pass.
            const auto wireSettingsRevision = sceneRevisions.revision(ux3d::imgui::SceneChange::RenderSettings);
            if (vp->wireMaterialLayer != gltfLayer.get() ||
                vp->wireMaterialGeneration != gltfLayer->getGltfGeneration() ||
                vp->wireRenderSettingsRevision != wireSettingsRevision ||
                !(vp->wireMaterialSettings == vp->settings)) {
                vp->wireMaterials.clear();
                vp->wireMaterialLayer = gltfLayer.get();
                vp->wireMaterialGeneration = gltfLayer->getGltfGeneration();
                vp->wireRenderSettingsRevision = wireSettingsRevision;
                vp->wireMaterialSettings = vp->settings;
            }
            std::vector<std::unique_ptr<ux3d::renderer::Material>> uncachedWireMaterials;
            const auto wireMaterial = [this, &vp, &uncachedWireMaterials](ux3d::renderer::Material* material) {
                auto configureMaterial = [&vp](ux3d::renderer::Material* copy) {
                    copy->fragmentShader = "wireframe_material.frag";
                    copy->overrideBlend = true;
                    copy->customBlend = true;
                    const auto blendMode = vp->settings.wireframeBlend;
                    const auto& col = vp->settings.wireframeColor;
                    if (blendMode == ux3d::imgui::WireframeBlendMode::XOR) {
                        copy->logicOpEnabled = true;
                        copy->logicOp = GL_XOR;
                        copy->blendingEnabled = false;
                        copy->setUniform("u_WireframeColor", ux3d::math::vec4(col.r * col.a, col.g * col.a, col.b * col.a, 0.0f));
                    } else if (blendMode == ux3d::imgui::WireframeBlendMode::Invert) {
                        copy->logicOpEnabled = false;
                        copy->blendingEnabled = true;
                        copy->blendSrcFactor = GL_ONE_MINUS_DST_COLOR;
                        copy->blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
                        copy->blendEquation = GL_FUNC_ADD;
                        copy->setUniform("u_WireframeColor", ux3d::math::vec4(col.r * col.a, col.g * col.a, col.b * col.a, col.a));
                    } else if (blendMode == ux3d::imgui::WireframeBlendMode::Additive) {
                        copy->logicOpEnabled = false;
                        copy->blendingEnabled = true;
                        copy->blendSrcFactor = GL_SRC_ALPHA;
                        copy->blendDstFactor = GL_ONE;
                        copy->blendEquation = GL_FUNC_ADD;
                        copy->setUniform("u_WireframeColor", ux3d::math::vec4(col.r, col.g, col.b, col.a));
                    } else { // Normal
                        copy->logicOpEnabled = false;
                        copy->blendingEnabled = true;
                        copy->blendSrcFactor = GL_SRC_ALPHA;
                        copy->blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
                        copy->blendEquation = GL_FUNC_ADD;
                        copy->setUniform("u_WireframeColor", ux3d::math::vec4(col.r, col.g, col.b, col.a));
                    }
                };

                if (s_disableWireframeCache) {
                    auto copy = std::make_unique<ux3d::renderer::Material>(*material);
                    configureMaterial(copy.get());
                    auto* result = copy.get();
                    uncachedWireMaterials.push_back(std::move(copy));
                    if (ux3d::imgui::profiling::countingEnabled())
                        ++ux3d::imgui::profiling::frameCounters().wireframeMaterialCopies;
                    return result;
                }
                auto& copy = vp->wireMaterials[material];
                if (!copy || copy->defines.contentHash() != material->defines.contentHash()) {
                    copy = std::make_unique<ux3d::renderer::Material>(*material);
                    configureMaterial(copy.get());
                    if (ux3d::imgui::profiling::countingEnabled())
                        ++ux3d::imgui::profiling::frameCounters().wireframeMaterialCopies;
                }
                return copy.get();
            };

            auto cubeTuple = gltfLayer->getResourceManager().getCubeMapTuple();
            auto* cubeMesh = std::get<0>(cubeTuple);
            auto* cubeMaterial = std::get<1>(cubeTuple);
            for (auto& [mesh, material] : scene.drawables) {
                if (mesh != nullptr && mesh != cubeMesh) {
                    mesh->polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
                    mesh->defines.define("DEBUG", static_cast<int>(shadingMode));
                }
                if (material != nullptr && material != cubeMaterial) {
                    material->defines.define("DEBUG", static_cast<int>(shadingMode));
                    material->defines.defineIf(ssrDebugOverlay, "RAW_SSR_OVERLAY");
                    if (vp->settings.forceDoubleSided) {
                        material->isDoubleSided = true;
                    }
                }
            }

            // A2.2: the decal draws leave the forward transparent list and accumulate into
            // the shared buffer instead. Run before the prepass copies are appended; the
            // pass order (prepass, accumulation, opaque) comes from RenderPassType.
            if (decalActive) {
                decalAccumulationLayer->setAppliedDecals(
                    gltfLayer->hasGeometryDecalMaterials()
                        && vp->settings.asobo.applies("ASOBO_material_geometry_decal"),
                    gltfLayer->hasBlendGbufferMaterials()
                        && vp->settings.asobo.applies("ASOBO_material_blend_gbuffer"));
                decalAccumulationLayer->setPassCallbacks(
                    [this, vpW, vpH] { decalBuffer.bindAccumulation(vpW, vpH); },
                    [this, &vp] {
                        if (transmissionSource != nullptr) transmissionSource->bind();
                        else vp->sceneFbo.bind();
                    });
                scene = decalAccumulationLayer->applyLayer(
                    std::move(scene), [this](const ux3d::renderer::Mesh* mesh) {
                        return currentReverseMapping.find(const_cast<ux3d::renderer::Mesh*>(mesh)) !=
                               currentReverseMapping.end();
                    });
            }

            // A3.5: screen-space ambient occlusion. It needs the prepass depth and normal before the opaque pass:
            // the same prepass as the decals (into the decal buffer when decals are active, otherwise into the
            // viewport's own prepass target), then the AO passes from the prepass's post callback. Shaded view and
            // the SSAO debug view only; nothing is allocated or drawn while it is off.
            const bool ssaoDebugView = mode == ux3d::renderer::RenderMode::SSAO;
            bool ssaoActive = vp->settings.ssao.enabled && !isWireframeMode && !decalDebugView &&
                              (shadingMode == ux3d::renderer::RenderMode::None || ssaoDebugView);
            auto& ambientOcclusion = vp->ambientOcclusion;
            if (ssaoActive) {
                ssaoActive = ambientOcclusion.ensureTargets(vpW, vpH, vp->settings.ssao.halfResolution);
                if (ssaoActive && !decalActive) {
                    ssaoActive = ambientOcclusion.ensurePrepassTarget(vpW, vpH);
                } else if (ambientOcclusion.hasPrepassTarget()) {
                    ambientOcclusion.releasePrepassTarget();
                }
                vp->sceneFbo.bind();
                glViewport(0, 0, vpW, vpH);
            }
            if (!ssaoActive && ambientOcclusion.hasAnything()) {
                ambientOcclusion.release();
            }
            vp->ssaoActive = ssaoActive;
            ux3d::imgui::AmbientOcclusion::Params ssaoParams;
            if (ssaoActive) {
                const auto& ssao = vp->settings.ssao;
                ssaoParams.depthTexture = decalActive ? decalBuffer.getDepthTexture() : ambientOcclusion.getPrepassDepth();
                ssaoParams.normalTexture = decalActive ? decalBuffer.getNormalTexture() : ambientOcclusion.getPrepassNormal();
                ssaoParams.width = vpW;
                ssaoParams.height = vpH;
                // The prepass draws with the camera block of this render: the TAA-jittered projection under TAA.
                ssaoParams.projection = jitteredProj;
                ssaoParams.view = view;
                ssaoParams.orthographic = proj[3][3] == 1.0f;
                ssaoParams.radius = std::max(ssao.radius, 1.0e-4f);
                ssaoParams.intensity = std::max(ssao.intensity, 0.0f);
                ssaoParams.sliceCount = ux3d::imgui::SsaoSettings::sliceCount(ssao.quality);
                ssaoParams.stepCount = ux3d::imgui::SsaoSettings::stepCount(ssao.quality);
            }

            // A2.1: append the decal prepass copies of the opaque drawables. Their pass
            // has a lower id than Opaque, so the prepass targets are filled before the
            // forward opaque pass of this viewport. The pass restores sceneFbo afterwards.
            if (decalActive || ssaoActive) {
                decalPrepassLayer->setDebugView(
                    !decalActive ? ux3d::renderer::DecalPrepassLayer::DebugView::None
                    : mode == ux3d::renderer::RenderMode::DecalPrepassDepth
                        ? ux3d::renderer::DecalPrepassLayer::DebugView::Depth
                        : mode == ux3d::renderer::RenderMode::DecalPrepassNormal
                              ? ux3d::renderer::DecalPrepassLayer::DebugView::Normal
                              : ux3d::renderer::DecalPrepassLayer::DebugView::None);
                if (decalActive) {
                    decalPrepassLayer->prePass = [this, vpW, vpH] { decalBuffer.bindPrepass(vpW, vpH); };
                } else {
                    decalPrepassLayer->prePass = [&ambientOcclusion, vpW, vpH] { ambientOcclusion.bindPrepass(vpW, vpH); };
                }
                decalPrepassLayer->postPass = [this, &vp, ssaoActive, ssaoParams, vpW, vpH] {
                    if (ssaoActive) {
                        vp->ambientOcclusion.compute(ssaoParams);
                    }
                    if (transmissionSource != nullptr) transmissionSource->bind();
                    else vp->sceneFbo.bind();
                    if (ssaoActive) glViewport(0, 0, vpW, vpH);
                };
                decalPrepassLayer->setDebugViewport(vpW, vpH);
                scene = decalPrepassLayer->applyLayer(
                    std::move(scene), [this](const ux3d::renderer::Mesh* mesh) {
                        return currentReverseMapping.find(const_cast<ux3d::renderer::Mesh*>(mesh)) !=
                               currentReverseMapping.end();
                    });
            }

            // Apply GPU picking layer (full-size reference path only; see P5.2 below)
            if (pickingRendered && s_fullSizePicking) {
                const auto pickingBuildStart = ux3d::imgui::profiling::Clock::now();
                pickingLayer->pixelOriginX = 0;
                pickingLayer->pixelOriginY = 0;
                scene = pickingLayer->applyLayer(std::move(scene));
                frame.markStage("picking.build", pickingBuildStart);
                ux3d::imgui::profiling::countPickingBuild();
            }
            frame.markStage("scene.build", sceneBuildStart);

            // Continuously synchronize scene lighting, tone mapping, exposure, and environment parameters
            if (activeEnvironment.toneMappingIdx == 0) {
                commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::PBR_NEUTRAL);
            } else if (activeEnvironment.toneMappingIdx == 1) {
                commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_HILL);
            } else if (activeEnvironment.toneMappingIdx == 2) {
                commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_NARKOWICZ);
            }
            commandGenerator->setExposure(std::pow(2.0f, activeEnvironment.exposureEV));
            commandGenerator->setEnvironmentMapIntensity(activeEnvironment.hdrIntensity);
            commandGenerator->setEnvironmentMapRotation(activeEnvironment.hdrRotation);
            commandGenerator->setEnvironmentMapBlurNormalized(activeEnvironment.hdrBlur);
            commandGenerator->setIBLEnabled(activeEnvironment.iblEnabled && ux3d::imgui::RenderQuality::allowIbl());
            commandGenerator->setPunctualLightsEnabled(ux3d::imgui::RenderQuality::allowLights(activeEnvironment.punctualLightsEnabled));
            {
                const EffectiveSun sun = effectiveSun(activeEnvironment);
                commandGenerator->setSun(sun.enabled, sun.direction, sun.illuminance,
                                         glm::radians(std::clamp(activeEnvironment.sunAngularRadius, 0.0f, 10.0f)));
            }
            commandGenerator->setAsoboNormalMapConvention(
                gltfLayer->usesDirectXNormalMaps() &&
                vp->settings.asobo.applies("ASOBO_normal_map_convention"));
            commandGenerator->setAsoboWindshieldVertexColorMask(
                vp->settings.asobo.applies("ASOBO_material_windshield_v3"));
            commandGenerator->setWindshieldIcing(vp->settings.windshieldIcing);
            // A5.1: the detail-map define is material-level; this per-viewport flag says
            // whether the active viewport samples the detail layers.
            commandGenerator->setAsoboDetailMap(
                vp->settings.asobo.applies("ASOBO_material_detail_map"));
            // A2.2: the receiver define is asset-level; this per-viewport flag says whether
            // the active viewport has a filled decal buffer to read.
            commandGenerator->setAsoboDecalReceiver(decalActive);
            // A3.5: the SSAO result this viewport's draw fills after its prepass (zero while off).
            commandGenerator->setSsao(ssaoActive, ssaoActive ? ambientOcclusion.getResultTexture() : 0);
            // A3.7: this viewport's sky portals (none while it does not apply ASOBO_sky_portal).
            commandGenerator->setSkyPortals(scene.skyPortals, vp->settings.skyPortalStrength);
            vp->skyPortalCount = static_cast<int>(scene.skyPortals.size());
            commandGenerator->setDecalDepthTolerance(vp->settings.decalDepthToleranceMm * 0.001f);
            commandGenerator->setAsoboEmissiveActive(
                gltfLayer->declaresAsoboExtensions() &&
                vp->settings.asobo.enabled && !s_disableMsfsEmissive);
            commandGenerator->setMsfsEmissiveScale(activeEnvironment.msfsEmissiveScale);
            commandGenerator->setTimeOfDay(activeEnvironment.timeOfDay);
            commandGenerator->setAsoboMaterialEmissiveApplied(
                vp->settings.asobo.applies("ASOBO_material_emissive"));
            commandGenerator->setAsoboOcclusionStrengthApplied(
                vp->settings.asobo.applies("ASOBO_occlusion_strength"));
            commandGenerator->setAsoboUVOptions(
                vp->settings.asobo.applies("ASOBO_material_UV_options"));
            commandGenerator->setAsoboFresnelFade(
                vp->settings.asobo.applies("ASOBO_material_fresnel_fade"));
            commandGenerator->setAsoboGhostEffect(
                vp->settings.asobo.applies("ASOBO_material_ghost_effect"));
            commandGenerator->setAsoboAlphaDither(
                vp->settings.asobo.applies("ASOBO_material_alphamode_dither"));

            // A3.6: the ground plane drawable, an Opaque-pass draw whose material carries this viewport's surface
            // and fog parameters (the draw reads them when it executes, viewports render one after another).
            // It is not in the reverse mapping, so the overlay depth and the picking passes never see it.
            auto [groundMesh, groundMaterial] = gltfLayer->getResourceManager().getGroundTuple();
            if (groundPlane && groundMesh != nullptr && groundMaterial != nullptr) {
                const auto& ground = vp->settings.ground;
                const auto* camera = gltfLayer->getActiveCamera();
                const float farPlane = std::max(camera->getZFarPlane(), 1.0e-3f);
                const float visibility = ground.haze > 0.0f ? ground.haze : 0.9f * farPlane;
                const float extinction = 3.912f / std::max(visibility, 1.0e-3f); // 98% fog at the visibility distance
                const bool hasEnvironment = gltfLayer->getActiveEnvironmentMap() != nullptr;
                const bool hazeFromEnvironment =
                    hasEnvironment && ux3d::imgui::RenderQuality::allowSkybox(activeEnvironment.showBackground);
                groundMaterial->defines.defineIf(hasEnvironment, "GROUND_ENV");
                groundMaterial->setUniform("u_GroundHeight", groundHeight);
                groundMaterial->setUniform("u_GroundAlbedo", ux3d::math::vec3(ground.albedo.x, ground.albedo.y, ground.albedo.z));
                groundMaterial->setUniform("u_GroundRoughness", ground.roughness);
                groundMaterial->setUniform("u_GroundDetail", ux3d::math::vec3(ground.detail, std::max(ground.detailScale, 1.0e-4f), std::max(ground.jointSpacing, 0.0f)));
                groundMaterial->setUniform("u_GroundHaze", ux3d::math::vec2(extinction, hazeFromEnvironment ? 1.0f : 0.0f));
                groundMaterial->setUniform("u_GroundHazeColor", ux3d::math::vec3(activeEnvironment.clearColor.r,
                                                                                  activeEnvironment.clearColor.g,
                                                                                  activeEnvironment.clearColor.b));
                scene.drawables.emplace_back(groundMesh, groundMaterial);
            }

            const auto commandGenerationStart = ux3d::imgui::profiling::Clock::now();
            const uint64_t programHitsBefore = commandGenerator->getCommandMapping().getProgramCacheHits();
            const uint64_t programMissesBefore = commandGenerator->getCommandMapping().getProgramCacheMisses();
            if (isWireframeMode) {
                // Keep camera, lights and environment; only the sky is drawn into the HDR target,
                // the wire lines follow after tone mapping.
                decltype(scene) skyScene = scene;
                skyScene.drawables.clear();
                for (const auto& [mesh, material] : scene.drawables) {
                    if (mesh == cubeMesh) {
                        skyScene.drawables.emplace_back(mesh, material);
                    }
                }
                commandGenerator->update(skyScene);
            } else {
                commandGenerator->update(scene);
            }
            s_sceneInstancedDraws = commandGenerator->getCommandMapping().getInstancedDraws();
            s_sceneInstancedMembers = commandGenerator->getCommandMapping().getInstancedMembers();
            frame.markStage("commands.generate", commandGenerationStart);
            ux3d::imgui::profiling::countCommandGeneration();
            if (s_profilingEnabled) {
                auto& counters = ux3d::imgui::profiling::frameCounters();
                counters.programCacheHits +=
                    commandGenerator->getCommandMapping().getProgramCacheHits() - programHitsBefore;
                counters.programCacheMisses +=
                    commandGenerator->getCommandMapping().getProgramCacheMisses() - programMissesBefore;
            }
            const auto drawSubmissionStart = ux3d::imgui::profiling::Clock::now();
            glRenderer->setGpuFrameContext(frame.number, vp->id, "main");
            // A4: the behind pass needs HDR radiance and the exact render-scale/MSAA
            // pixel grid. The blit callback switches back to the ordinary scene target.
            commandGenerator->setAsoboGlassCopyValid(false);
            ssrCaptureDepth = scene.windshieldSsr && effectiveSettings.screenSpaceReflections;
            ssrUseDecalDepth = decalActive;
            ssrInteriorSky = vp->settings.interiorSkyReflection;
            commandGenerator->setSsrOptions(effectiveSettings.screenSpaceReflections,
                                           effectiveSettings.ssrQuality, vp->settings.interiorSkyReflection);
            // TAA resolves the per-frame ray phase noise into soft reflection silhouettes.
            commandGenerator->setSsrFrame(isTaa ? static_cast<float>(vp->taaStableRenders % 64u) : 0.0f);
            // A1.7: TAA averages a per-frame offset of the dither pattern; without TAA it stays fixed.
            commandGenerator->setAsoboDitherFrame(isTaa ? static_cast<float>(vp->taaStableRenders % 1024u) : 0.0f);
            const bool floatCopy = scene.asoboGlassCopy && vp->sceneFbo.isValid() && vp->sceneFbo.isLinearFloat();
            transmissionDestination = floatCopy ? &vp->sceneFbo : nullptr;
            transmissionSource = &vp->sceneFbo;
            GLuint exposureHdrTexture = 0;
            if (ssrDebugOverlay && activeEnvironment.autoExposure) {
                // Meter the same radiance as Shaded, including windshield reflections.
                // Only these diagnostic modes pay for the extra draw and TAA history.
                for (auto [mesh, material] : scene.drawables) {
                    if (!currentReverseMapping.contains(mesh) || material == nullptr) continue;
                    mesh->defines.define("DEBUG", static_cast<int>(ux3d::renderer::RenderMode::None));
                    material->defines.define("DEBUG", static_cast<int>(ux3d::renderer::RenderMode::None));
                    material->defines.undefine("RAW_SSR_OVERLAY");
                }
                commandGenerator->update(scene);
                glRenderer->setGpuFrameContext(frame.number, vp->id, "ssr-exposure");
                glRenderer->draw({commandGenerator.get()});
                vp->sceneFbo.unbind();
                vp->ssrExposureFbo.resize(vpW, vpH, 1, true);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->sceneFbo.getResolveFbo());
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vp->ssrExposureFbo.getFbo());
                glBlitFramebuffer(0, 0, vpW, vpH, 0, 0, vpW, vpH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                exposureHdrTexture = vp->ssrExposureFbo.getColorTexture();
                if (isTaa) {
                    if (!vp->ssrExposureAa) {
                        vp->ssrExposureAa = std::make_unique<ux3d::imgui::AntiAliasing>();
                        vp->ssrExposureAa->init();
                    }
                    exposureHdrTexture = vp->ssrExposureAa->render(
                        aaMode, exposureHdrTexture, vpW, vpH, proj * view, frameSceneDirty);
                }
                for (auto [mesh, material] : scene.drawables) {
                    if (!currentReverseMapping.contains(mesh) || material == nullptr) continue;
                    mesh->defines.define("DEBUG", static_cast<int>(shadingMode));
                    material->defines.define("DEBUG", static_cast<int>(shadingMode));
                    material->defines.define("RAW_SSR_OVERLAY");
                }
                vp->sceneFbo.bind();
                glViewport(0, 0, vpW, vpH);
                commandGenerator->update(scene);
                commandGenerator->setAsoboGlassCopyValid(false);
                glRenderer->setGpuFrameContext(frame.number, vp->id, "main");
            } else {
                vp->ssrExposureAa.reset();
                if (vp->ssrExposureFbo.isValid()) vp->ssrExposureFbo = ux3d::imgui::Framebuffer();
            }
            glRenderer->draw({commandGenerator.get()});
            transmissionSource = nullptr;
            transmissionDestination = nullptr;
            vp->lastExecutedPasses = glRenderer->getLastExecutedPasses();
            ++vp->renderCount;
            frame.markStage("draw.submit", drawSubmissionStart);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            // A3.6: the ground plane is not pickable; clicks fall through to the background.
            if (groundPlane) {
                for (auto it = scene.drawables.begin(); it != scene.drawables.end();) {
                    if (std::get<0>(*it) == groundMesh) {
                        it = scene.drawables.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            // A2.1 check: sample the forward depth and Geometry Normal colour before the
            // grid, bounding boxes and picking overlays touch the same framebuffer.
            if (!s_decalPrepassCheckPath.empty() && !s_decalCheckCaptured
                && frame.number >= SCENARIO_REPORT_FRAME && vp.get() == activeViewport
                && vp->sceneFbo.getSamples() == 1 && vp->sceneFbo.getFbo() != 0) {
                s_decalCheckWidth = vpW;
                s_decalCheckHeight = vpH;
                const size_t pixelCount = static_cast<size_t>(vpW) * static_cast<size_t>(vpH);
                s_decalCheckForwardColor.resize(pixelCount * 4u);
                s_decalCheckForwardDepth.resize(pixelCount);
                GLint previousPackAlignment = 4;
                glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->sceneFbo.getFbo());
                glReadBuffer(GL_COLOR_ATTACHMENT0);
                glReadPixels(0, 0, vpW, vpH, GL_RGBA, GL_UNSIGNED_BYTE, s_decalCheckForwardColor.data());
                glReadPixels(0, 0, vpW, vpH, GL_DEPTH_COMPONENT, GL_FLOAT, s_decalCheckForwardDepth.data());
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
                s_decalCheckCaptured = true;
            }

            // P5.2: picking draws only the clicked pixel, into the shared 1x1 target,
            // through a pixel-window projection W * P. W scales clip x/y by the viewport
            // size and translates by an exact integer, so the clicked pixel's centre maps
            // to the target's only sample and screen-space derivatives are unchanged. P is
            // the projection the scene was just drawn with (jittered under TAA).
            if (pickingRendered && !s_fullSizePicking && gltfLayer->getActiveCamera() != nullptr) {
                const auto pickingStart = ux3d::imgui::profiling::Clock::now();
                auto* pickCamera = gltfLayer->getActiveCamera();
                const auto sceneProjection = pickCamera->getProjectionMatrix();
                glm::mat4 pixelWindow(1.0f);
                pixelWindow[0][0] = static_cast<float>(vpW);
                pixelWindow[1][1] = static_cast<float>(vpH);
                pixelWindow[3][0] = static_cast<float>(vpW - 2 * pickPx - 1);
                pixelWindow[3][1] = static_cast<float>(vpH - 2 * pickPy - 1);
                pickCamera->setProjection(ux3d::math::mat4x4(pixelWindow * glm::mat4(sceneProjection)));
                if (!s_disablePickCulling) {
                    s_pickCulledTotal +=
                        gltfLayer->cullToClip(scene.drawables, currentReverseMapping,
                                              pickCamera->getProjectionMatrix() * pickCamera->getViewMatrix());
                }

                pickingLayer->pickingOnly = true;
                pickingLayer->pixelOriginX = pickPx;
                pickingLayer->pixelOriginY = pickPy;
                auto pickingScene = pickingLayer->applyLayer(std::move(scene));
                pickingLayer->pickingOnly = false;
                ux3d::imgui::profiling::countPickingBuild();
                commandGenerator->update(pickingScene);
                glRenderer->setGpuFrameContext(frame.number, vp->id, "picking");
                glRenderer->draw({commandGenerator.get()});
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                pickCamera->setProjection(sceneProjection);
                frame.markStage("picking.draw", pickingStart);
            }

            // Restore unjittered camera projection so subsequent passes and next frame are clean
            if (aaMode == ux3d::imgui::AntiAliasingMode::TAA && gltfLayer->getActiveCamera()) {
                gltfLayer->getActiveCamera()->setProjection(ux3d::math::mat4x4(proj));
            }

            // Order: scene (HDR, MSAA) -> resolve -> TAA in HDR -> [exposure metering] ->
            // [bloom] -> tone map -> FXAA (LDR) -> overlays -> ImGui.
            vp->sceneFbo.unbind();
            GLuint hdrTexture = vp->sceneFbo.getColorTexture();
            const bool isDebugView = mode != ux3d::renderer::RenderMode::None &&
                                     mode != ux3d::renderer::RenderMode::Wireframe;
            vp->antiAliasing.setRawMode(isDebugView);
            if (isTaa) {
                const auto aaStart = ux3d::imgui::profiling::Clock::now();
                hdrTexture = vp->antiAliasing.render(
                    aaMode, hdrTexture, vpW, vpH, proj * view, frameSceneDirty);
                frame.markStage("aa.render", aaStart);
            } else if (aaMode != ux3d::imgui::AntiAliasingMode::FXAA) {
                vp->antiAliasing.releaseTargets();
            }

            const int overlaySamples = (!isTaa && aaMode != ux3d::imgui::AntiAliasingMode::FXAA)
                                           ? vp->sceneFbo.getSamples()
                                           : 1;
            vp->overlayFbo.resize(vpW, vpH, overlaySamples, false);

            ux3d::imgui::HdrPostProcessParams postParams;
            postParams.rawPassThrough = isDebugView && !ssrDebugOverlay;
            postParams.ssrDebugOverlay = ssrDebugOverlay;
            postParams.exposureHdrTexture = exposureHdrTexture;
            postParams.exposureEv = activeEnvironment.exposureEV;
            postParams.toneMappingMode = activeEnvironment.toneMappingIdx;
            postParams.hasClearColor = !ux3d::imgui::RenderQuality::allowSkybox(activeEnvironment.showBackground);
            postParams.clearColor = glm::vec3(activeEnvironment.clearColor.r,
                                              activeEnvironment.clearColor.g,
                                              activeEnvironment.clearColor.b);
            postParams.bloomEnabled = ux3d::imgui::RenderQuality::allowBloom(activeEnvironment.bloomEnabled) && !ssrDebugOverlay;
            postParams.bloomIntensity = activeEnvironment.bloomIntensity;
            postParams.bloomRadius = activeEnvironment.bloomRadius;
            postParams.autoExposure = activeEnvironment.autoExposure;
            postParams.startFromManualEv = activeEnvironment.autoExposureStartFromManual;
            postParams.exposureCompensationEv = activeEnvironment.exposureCompensationEV;
            postParams.autoExposureMinEv = activeEnvironment.autoExposureMinEV;
            postParams.autoExposureMaxEv = activeEnvironment.autoExposureMaxEV;
            postParams.autoExposureSpeedUp = activeEnvironment.autoExposureSpeedUp;
            postParams.autoExposureSpeedDown = activeEnvironment.autoExposureSpeedDown;
            postParams.deltaTime = frame.deltaTime;
            postParams.snapExposure = cameraJumped && !activeEnvironment.autoExposureStartFromManual;
            vp->antiAliasing.renderPostProcess(hdrTexture, vp->overlayFbo.getFbo(), vpW, vpH, postParams);

            if (aaMode == ux3d::imgui::AntiAliasingMode::FXAA) {
                const auto aaStart = ux3d::imgui::profiling::Clock::now();
                const GLuint fxaaTex = vp->antiAliasing.render(
                    aaMode, vp->overlayFbo.getColorTexture(), vpW, vpH, proj * view, frameSceneDirty);
                frame.markStage("aa.render", aaStart);
                if (fxaaTex != 0 && vp->antiAliasing.getFxaaFbo() != 0) {
                    glDisable(GL_SCISSOR_TEST);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->antiAliasing.getFxaaFbo());
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vp->overlayFbo.getFbo());
                    glBlitFramebuffer(0, 0, vpW, vpH, 0, 0, vpW, vpH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                }
            }

            const bool hasSceneBbox =
                vp->settings.showSceneBoundingBox && activeScene != nullptr && !activeScene->getNodes().empty();
            const bool hasSelectedBbox =
                vp->settings.showSelectedBoundingBox && selection.type == ux3d::imgui::SelectionType::Node &&
                selection.selectedNode != nullptr;
            const bool hasOverlays =
                (vp->settings.showGrid && (!groundPlane || vp->settings.ground.showGrid)) || hasSceneBbox ||
                hasSelectedBbox || drawWireframe;

            if (hasOverlays) {
                // The ground plane is in the scene depth, so overlays (the grid on the ground at the plane's own
                // height, boxes) test against the opaque model only, rebuilt with the unjittered camera.
                if (isTaa || groundPlane) {
                    vp->overlayFbo.bind();
                    glDepthMask(GL_TRUE);
                    glClear(GL_DEPTH_BUFFER_BIT);
                    // Rebuild opaque depth with the unjittered camera. Reusing jittered
                    // scene depth would still make grid/box occlusion flicker at edges.
                    if (!isWireframeMode) {
                        auto [depthScene, depthMapping] = gltfLayer->createScene(activeScene);
                        for (auto it = depthScene.drawables.begin(); it != depthScene.drawables.end();) {
                            auto [mesh, material] = *it;
                            if (!material || !material->renderPass ||
                                material->renderPass->getId() != ux3d::renderer::RenderPassType::Opaque ||
                                !currentReverseMapping.contains(mesh)) {
                                it = depthScene.drawables.erase(it);
                            } else ++it;
                        }
                        auto& depthLayer = vp->overlayDepthLayer;
                        depthLayer.pickingOnly = true;
                        depthLayer.prePass = [&] { vp->overlayFbo.bind(); };
                        depthLayer.postPass = [] {};
                        depthScene = depthLayer.applyLayer(std::move(depthScene));
                        commandGenerator->update(depthScene);
                        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                        glRenderer->setGpuFrameContext(frame.number, vp->id, "overlay-depth");
                        glRenderer->draw({commandGenerator.get()});
                        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    }
                } else {
                    glDisable(GL_SCISSOR_TEST);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->sceneFbo.getFbo());
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vp->overlayFbo.getFbo());
                    glBlitFramebuffer(0, 0, vpW, vpH, 0, 0, vpW, vpH, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
                }
                vp->overlayFbo.bind();
            }

            if (drawWireframe) {
                const auto wireframeStart = ux3d::imgui::profiling::Clock::now();
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                if (hasWireframeOverlay) {
                    glEnable(GL_POLYGON_OFFSET_LINE);
                    glPolygonOffset(-1.0f, -1.0f);
                }
                glLineWidth(1.0f);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
                gltfLayer->frustumCulling = !s_disableCulling;
                gltfLayer->asoboOptions = vp->settings.asobo;
                gltfLayer->materialExtensionOptions = vp->settings.materialExtensions;
                auto [wireScene, _] = gltfLayer->createScene(activeScene);
                gltfLayer->frustumCulling = false;
                for (auto it = wireScene.drawables.begin(); it != wireScene.drawables.end();) {
                    auto& [mesh, material] = *it;
                    const auto passId = (material != nullptr && material->renderPass != nullptr)
                                            ? material->renderPass->getId()
                                            : ux3d::renderer::RenderPassType::MaxRenderPasses;
                    if (passId == ux3d::renderer::RenderPassType::TransmissiveBlit ||
                        passId == ux3d::renderer::RenderPassType::Opaque2 ||
                        passId == ux3d::renderer::RenderPassType::Grid2 ||
                        passId == ux3d::renderer::RenderPassType::Transparent2 ||
                        mesh == cubeMesh) {
                        it = wireScene.drawables.erase(it);
                        continue;
                    }
                    if (mesh != nullptr) {
                        mesh->polygonMode = ux3d::renderer::gl::PolygonMode::Line;
                        if (material != nullptr) {
                            material = wireMaterial(material);
                        }
                    }
                    ++it;
                }
                commandGenerator->update(wireScene);
                if (isWireframeMode) {
                    // Wireframe mode draws its geometry here; the earlier pass held only the sky.
                    s_sceneInstancedDraws = commandGenerator->getCommandMapping().getInstancedDraws();
                    s_sceneInstancedMembers = commandGenerator->getCommandMapping().getInstancedMembers();
                }
                glRenderer->setGpuFrameContext(frame.number, vp->id, "wireframe");
                glRenderer->draw({commandGenerator.get()});
                vp->lastExecutedPasses = glRenderer->getLastExecutedPasses();
                ++vp->renderCount;
                glDisable(GL_COLOR_LOGIC_OP);
                if (hasWireframeOverlay) {
                    glDisable(GL_POLYGON_OFFSET_LINE);
                }
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                frame.markStage(isWireframeMode ? "scene.wireframe" : "scene.wireframeOverlay", wireframeStart);
            }
        } else if (redrawScene) {
            vp->groundPlaneActive = false;
            currentReverseMapping.clear();
            vp->sceneFbo.unbind();
            vp->overlayFbo.resize(vpW, vpH, 1, false);
            vp->overlayFbo.bind();
        }

        // 2. Render ground grid
        const auto overlayStart = ux3d::imgui::profiling::Clock::now();
        // A3.6: with a ground plane the grid is an option drawn on the plane (at its height).
        if (redrawScene && vp->settings.showGrid && (!vp->groundPlaneActive || vp->settings.ground.showGrid)) {
            gridRenderer.draw(view, proj,
                              ux3d::math::vec3(cameraEye.x, cameraEye.y, cameraEye.z), cameraZoom,
                              ux3d::imgui::RenderQuality::effective().gridMode == ux3d::imgui::GridMode::PlaneMesh,
                              vp->groundPlaneActive ? vp->groundPlaneHeight : 0.0f);
            ux3d::imgui::profiling::countOverlayDraw();
        }

        // 3. Render bounding boxes
        if (redrawScene && vp->settings.showSceneBoundingBox && activeScene != nullptr &&
            !activeScene->getNodes().empty()) {
            const ux3d::gltf::Extends& ext = getCachedSceneExtents(activeScene, gltfModel, gltfLayer.get());
            bboxRenderer.draw(glm::vec3(ext.min.x, ext.min.y, ext.min.z), glm::vec3(ext.max.x, ext.max.y, ext.max.z),
                              glm::vec4(1.0f, 0.85f, 0.2f, 0.85f), viewProj);
            ux3d::imgui::profiling::countOverlayDraw();
        }
        if (redrawScene && vp->settings.showSelectedBoundingBox && selection.type == ux3d::imgui::SelectionType::Node &&
            selection.selectedNode != nullptr) {
            const NodeOBB& obb = getCachedNodeOBB(selection.selectedNode, gltfLayer.get());
            if (obb.valid) {
                bboxRenderer.drawOBB(obb.localMin, obb.localMax, obb.modelMatrix, glm::vec4(1.0f, 0.62f, 0.12f, 1.0f),
                                     viewProj);
                ux3d::imgui::profiling::countOverlayDraw();
            }
        }
        frame.markStage("overlays.gridBbox", overlayStart);

        // 4. Finalize LDR display target
        if (redrawScene) {
            vp->overlayFbo.unbind();
            displayTexture = vp->overlayFbo.getColorTexture();
            // The AA call can reallocate its targets; key on the generation after it.
            renderKey.aaTargetGeneration = vp->antiAliasing.getTargetGeneration();
            vp->lastRenderKey = renderKey;
            vp->cachedDisplayTexture = displayTexture;
            vp->hasCachedImage = displayTexture != 0;
        }

        if (vp.get() == activeViewport && displayTexture != 0) {
            const double captureElapsed = glfwGetTime() - s_captureEpochSeconds;
            if (!s_captureFirstFramePath.empty() && frame.number == 1 && !s_firstFrameCaptureAttempted) {
                s_firstFrameCaptureAttempted = true;
                const bool firstCaptureSucceeded =
                    captureViewportTexture(displayTexture, vpW, vpH, s_captureFirstFramePath, "active-viewport-texture",
                                           frame.number, true, captureElapsed, s_firstFrameCaptureRecord);
                s_firstFrameCaptureRecord.viewportId = vp->id;
                if (!firstCaptureSucceeded) {
                    s_outputFailed = true;
                }
            }

            if (!s_captureViewportPath.empty() && !s_captureAttempted) {
                const bool byFrame = s_captureFrameProvided && frame.number == s_captureFrame;
                const bool byTime = s_captureTimeProvided && captureElapsed >= s_captureTimeSeconds;
                if (byFrame || byTime) {
                    s_captureAttempted = true;
                    const bool captureSucceeded = captureViewportTexture(
                        displayTexture, vpW, vpH, s_captureViewportPath, "active-viewport-texture", frame.number, false,
                        captureElapsed, s_captureRecord);
                    s_captureRecord.viewportId = vp->id;
                    if (!captureSucceeded) {
                        s_outputFailed = true;
                    }
                }
            }
        }

        // Draw offscreen FBO texture inside ImGui viewport window
        if (displayTexture != 0) {
            ImGui::Image(static_cast<ImTextureID>(displayTexture), viewportSize, ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));

            // Right-click: Object actions while a single node is selected, otherwise
            // the same items as the Viewport menu. Drawn inside the loop, so viewport
            // creation is deferred until after it (see pendingNewViewport).
            if (ImGui::BeginPopupContextItem("##ViewportContextMenu")) {
                activeViewport = vp.get();
                if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                    ux3d::imgui::ObjectActionContext objectContext{gltfModel,       activeScene,      selection,
                                                                   gltfLayer.get(), &vp->cameraActor, &vp->antiAliasing,
                                                                   sceneRevisions};
                    ux3d::imgui::drawObjectMenuItems(objectContext, selection.selectedNode, false, true);
                } else {
                    drawViewportMenuBody();
                }
                ImGui::EndPopup();
            }
        }

        ImVec2 imagePos = ImGui::GetItemRectMin();
        ImVec2 imageSize = ImGui::GetItemRectSize();

        bool clickedCompass = false;

        // 3D Viewport Orientation Compass / Navigation Gizmo
        if (vp->settings.showOrientationGizmo) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float radius = 35.0f;
            ImVec2 center = ImVec2(imagePos.x + imageSize.x - radius - 15.0f, imagePos.y + radius + 15.0f);

            // Background subtle disc
            drawList->AddCircleFilled(center, radius + 4.0f, IM_COL32(28, 30, 38, 190));
            drawList->AddCircle(center, radius + 4.0f, IM_COL32(70, 75, 95, 180), 0, 1.5f);

            struct AxisDef {
                glm::vec3 dir;
                const char* name;
                ImU32 color;
                ImU32 dimColor;
                ux3d::editor::CameraOrientation preset;
            };

            AxisDef axisDefs[] = {{glm::vec3(1.0f, 0.0f, 0.0f), "X", IM_COL32(235, 75, 75, 255),
                                   IM_COL32(140, 40, 40, 160), ux3d::editor::CameraOrientation::Right},
                                  {glm::vec3(-1.0f, 0.0f, 0.0f), "-X", IM_COL32(180, 70, 70, 200),
                                   IM_COL32(100, 35, 35, 120), ux3d::editor::CameraOrientation::Left},
                                  {glm::vec3(0.0f, 1.0f, 0.0f), "Y", IM_COL32(75, 220, 95, 255),
                                   IM_COL32(40, 130, 50, 160), ux3d::editor::CameraOrientation::Top},
                                  {glm::vec3(0.0f, -1.0f, 0.0f), "-Y", IM_COL32(70, 170, 85, 200),
                                   IM_COL32(35, 95, 45, 120), ux3d::editor::CameraOrientation::Bottom},
                                  {glm::vec3(0.0f, 0.0f, 1.0f), "Z", IM_COL32(70, 140, 245, 255),
                                   IM_COL32(40, 80, 150, 160), ux3d::editor::CameraOrientation::Front},
                                  {glm::vec3(0.0f, 0.0f, -1.0f), "-Z", IM_COL32(65, 115, 200, 200),
                                   IM_COL32(35, 65, 120, 120), ux3d::editor::CameraOrientation::Back}};

            glm::mat3 rot = glm::mat3(view);

            struct ProjectedAxis {
                glm::vec3 v;
                AxisDef* def;
                ImVec2 pt;
            };
            std::vector<ProjectedAxis> projAxes;
            for (auto& a : axisDefs) {
                glm::vec3 v = rot * a.dir;
                ImVec2 pt = ImVec2(center.x + v.x * radius * 0.82f, center.y - v.y * radius * 0.82f);
                projAxes.push_back({v, &a, pt});
            }
            std::sort(projAxes.begin(), projAxes.end(),
                      [](const ProjectedAxis& a, const ProjectedAxis& b) { return a.v.z < b.v.z; });

            ImVec2 mousePos = io.MousePos;
            bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
            float distCenterSq =
                (mousePos.x - center.x) * (mousePos.x - center.x) + (mousePos.y - center.y) * (mousePos.y - center.y);
            if (distCenterSq <= (radius + 6.0f) * (radius + 6.0f)) {
                clickedCompass = true;
            }

            // Middle part / center hub -> click turns into isometric mode!
            float hubRadius = 7.0f;
            bool hoveredHub = (distCenterSq <= (hubRadius + 2.0f) * (hubRadius + 2.0f));

            drawList->AddCircleFilled(center, hubRadius,
                                      hoveredHub ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 165, 185, 220));
            drawList->AddCircle(center, hubRadius, IM_COL32(35, 40, 55, 255), 0, 1.2f);

            if (hoveredHub) {
                ImGui::SetTooltip("Isometric View");
                if (clicked && !vp->isAttachedToCamera) {
                    vp->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Isometric);
                    vp->antiAliasing.resetHistory();
                }
            }

            for (const auto& pa : projAxes) {
                bool isPositive = (pa.def->name[0] != '-');
                float dotRadius = isPositive ? 7.0f : 4.5f;

                if (isPositive) {
                    drawList->AddLine(center, pa.pt, pa.def->color, 2.0f);
                }

                float distSq =
                    (mousePos.x - pa.pt.x) * (mousePos.x - pa.pt.x) + (mousePos.y - pa.pt.y) * (mousePos.y - pa.pt.y);
                bool hovered = (distSq <= (dotRadius + 3.0f) * (dotRadius + 3.0f));

                ImU32 col =
                    hovered ? IM_COL32(255, 255, 255, 255) : (pa.v.z >= 0.0f ? pa.def->color : pa.def->dimColor);
                drawList->AddCircleFilled(pa.pt, dotRadius, col);

                if (isPositive) {
                    ImVec2 textSize = ImGui::CalcTextSize(pa.def->name);
                    drawList->AddText(ImVec2(pa.pt.x - textSize.x * 0.5f, pa.pt.y - textSize.y * 0.5f),
                                      IM_COL32(20, 20, 25, 255), pa.def->name);
                }

                if (hovered && clicked && !hoveredHub && !vp->isAttachedToCamera) {
                    vp->cameraActor.orientUserCamera(pa.def->preset);
                    vp->antiAliasing.resetHistory();
                }
            }
        }

        // Viewport mouse interaction & Picking
        bool isMouseInViewport = (io.MousePos.x >= imagePos.x && io.MousePos.x < imagePos.x + imageSize.x &&
                                  io.MousePos.y >= imagePos.y && io.MousePos.y < imagePos.y + imageSize.y);
        bool isViewportHovered = isMouseInViewport && ImGui::IsWindowHovered();

        if (isViewportHovered && !clickedCompass) {
            // Only allow orbit / pan / zoom if not attached to glTF camera
            if (!vp->isAttachedToCamera) {
                if (io.MouseWheel != 0.0f) {
                    vp->cameraActor.zoomCamera(io.MouseWheel * 120.0f);
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                    if (io.KeyShift) {
                        vp->cameraActor.panCamera(-static_cast<int32_t>(io.MouseDelta.x),
                                                  -static_cast<int32_t>(io.MouseDelta.y));
                    } else if (io.KeyCtrl) {
                        vp->cameraActor.zoomCamera(-io.MouseDelta.y * 10.0f);
                    } else {
                        vp->cameraActor.rotateCamera(static_cast<int32_t>(io.MouseDelta.x),
                                                     static_cast<int32_t>(io.MouseDelta.y));
                    }
                }
            }

            // Left-click 3D mesh picking
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
                float localX = (io.MousePos.x - imagePos.x) * renderScale;
                float localY = (io.MousePos.y - imagePos.y) * renderScale;
                int px = static_cast<int>(localX);
                int py = vpH - static_cast<int>(localY) - 1;

                if (px < 0) px = 0;
                if (px >= vpW) px = vpW - 1;
                if (py < 0) py = 0;
                if (py >= vpH) py = vpH - 1;

                if (!pickingRendered || (!s_fullSizePicking && (px != pickPx || py != pickPy))) {
                    // Unreachable while the scheduling test above is a superset of this
                    // handler's; counted so tests can prove it never happens.
                    ++s_pickUnservedTotal;
                    logMessage("Picking skipped: no picking pass was rendered this frame");
                } else if (pickTarget.isValid()) {
                    ++s_pickReadTotal;
                    uint8_t pixel[4] = {0, 0, 0, 0};
                    glPixelStorei(GL_PACK_ALIGNMENT, 1);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, pickTarget.getFbo());
                    glReadBuffer(GL_COLOR_ATTACHMENT0);
                    if (s_fullSizePicking) {
                        glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                    } else {
                        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                    }
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                    uint32_t pickingId = (static_cast<uint32_t>(pixel[0]) << 16) |
                                         (static_cast<uint32_t>(pixel[1]) << 8) | static_cast<uint32_t>(pixel[2]);

                    if (pickingId != 0) {
                        auto* pickedMesh = pickingLayer->getMesh(pickingId);
                        if (pickedMesh != nullptr) {
                            auto it = currentReverseMapping.find(pickedMesh);
                            if (it != currentReverseMapping.end() && it->second != nullptr) {
                                auto* pickedNode = const_cast<ux3d::gltf::Node*>(it->second);
                                selection.selectNode(pickedNode);
                                ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
                                sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
                                logMessage("Picked: " + (pickedNode->hasName()
                                                             ? pickedNode->getName()
                                                             : ("Node " + std::to_string(pickedNode->m_ID))));
                                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                    focusCameraOnNode(pickedNode, gltfModel, gltfLayer.get(), vp->cameraActor,
                                                      vp->antiAliasing, sceneRevisions);
                                }
                            } else {
                                selection = ux3d::imgui::SelectionState();
                                sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
                            }
                        } else {
                            selection = ux3d::imgui::SelectionState();
                            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
                        }
                    } else {
                        selection = ux3d::imgui::SelectionState();
                        sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
                    }
                }
            }
        }

        // 3D Coordinate Gizmo
        if (vp->settings.showNodeCoordinateGizmo && selection.type == ux3d::imgui::SelectionType::Node &&
            selection.selectedNode != nullptr && vp->sceneFbo.isValid()) {
            auto* node = selection.selectedNode;
            glm::mat4 nodeWorld =
                gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawNodeCoordinateGizmo(drawList, nodeWorld, viewProj, imagePos, imageSize,
                                    ux3d::math::vec3(cameraEye.x, cameraEye.y, cameraEye.z),
                                    vp->settings.gizmoAxisConvention);
        }

        // A3.7: sky portal gizmos: every portal while "Show sky portals" is on, otherwise the selected portal node.
        if (gltfLayer != nullptr && gltfLayer->hasSkyPortals() && vp->sceneFbo.isValid()) {
            const ux3d::gltf::Node* selectedNode =
                selection.type == ux3d::imgui::SelectionType::Node ? selection.selectedNode : nullptr;
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (const auto& [portalNode, source] : gltfLayer->getSkyPortalSources()) {
                const bool selected = portalNode == selectedNode;
                if (!selected && !vp->settings.showSkyPortals) continue;
                drawSkyPortalGizmo(drawList, glm::mat4(gltfLayer->getWorldTransform(portalNode)), source.radiusMetres,
                                   source.innerRadiusMetres, selected, viewProj, imagePos, imageSize);
            }
        }

        ImGui::End(); // End Viewport Window
    }
    gltfLayer->endSceneFrame();

    // A2.1: the buffer is shared and lives only while some open viewport needs it.
    bool anyViewportNeedsDecalBuffer = false;
    for (const auto& vp : viewports) {
        if (vp && vp->isOpen && viewportNeedsDecalBuffer(*vp)) {
            anyViewportNeedsDecalBuffer = true;
            break;
        }
    }
    if (!anyViewportNeedsDecalBuffer && decalBuffer.isValid()) {
        releaseDecalBuffer();
    }

    // Deferred viewport creation: the Viewport menu body can run inside the viewport
    // loop (right-click context menu), where changing the viewports vector would
    // invalidate the iteration.
    if (pendingNewViewport || pendingNewViewportCamera != nullptr) {
        createViewport(pendingNewViewportCamera);
        pendingNewViewport = false;
        pendingNewViewportCamera = nullptr;
    }
}

bool Application::viewportNeedsDecalBuffer(const ux3d::imgui::ViewportInstance& viewport) const
{
    // Inspection needs the decal geometry itself, even without a receiver beneath it.
    // Keeping forward draws also makes isolated decals eligible for the picking pass.
    if (gltfLayer == nullptr || gltfLayer->getIsolationRoot() != nullptr
        || viewport.settings.renderMode == ux3d::renderer::RenderMode::Wireframe) {
        return false;
    }
    // A2.1/A2.2: the buffer exists only when the file has decals of an extension the
    // viewport applies. AsoboOptions::applies already includes the master switch; checking
    // it explicitly keeps the requirement readable and guards a future change to applies().
    if (!viewport.settings.asobo.enabled) {
        return false;
    }
    return (gltfLayer->hasGeometryDecalMaterials()
               && viewport.settings.asobo.applies("ASOBO_material_geometry_decal"))
        || (gltfLayer->hasBlendGbufferMaterials()
            && viewport.settings.asobo.applies("ASOBO_material_blend_gbuffer"));
}

void Application::updateSunShadows(ux3d::imgui::ViewportInstance& vp, FrameContext& frame, const glm::mat4& view,
                                   const glm::mat4& projection, const ux3d::imgui::SceneEnvironmentState& environment,
                                   bool shadingAllowed, int vpW, int vpH, std::optional<float> groundHeight)
{
    ux3d::renderer::CommandGenerator::SunShadowParameters parameters; // disabled
    const EffectiveSun sun = effectiveSun(environment);
    const ux3d::imgui::RenderQualitySettings quality = ux3d::imgui::RenderQuality::effective();
    const bool wanted = sun.enabled && quality.shadows && shadingAllowed && gltfLayer != nullptr && commandGenerator != nullptr
        && glRenderer != nullptr && shadowCasterLayer != nullptr && sunShadowCamera != nullptr && activeScene != nullptr;
    const auto disable = [&] {
        vp.sunShadowMap.release();
        vp.sunShadowSetup = ux3d::renderer::ShadowCascadeSetup();
        vp.sunShadowActive = false;
        if (commandGenerator != nullptr) commandGenerator->setSunShadow(parameters);
    };
    if (!wanted) {
        disable();
        return;
    }

    // The viewport's unculled scene: the casters, and (with the receivers) the extent the cascades are cut to.
    gltfLayer->frustumCulling = false;
    gltfLayer->asoboOptions = vp.settings.asobo;
    gltfLayer->materialExtensionOptions = vp.settings.materialExtensions;
    auto [casterScene, casterMapping] = gltfLayer->createScene(activeScene);

    // Cascades fitted to this viewport's (unjittered) frustum, cut to the scene's extent.
    ux3d::renderer::ShadowCascadeInput input;
    input.view = view;
    input.projection = projection;
    input.directionToSun = sun.direction;
    input.cascadeCount = std::clamp(quality.shadowCascades, 2, 4);
    input.resolution = static_cast<int>(quality.shadowResolution);
    input.maxDistance = quality.shadowDistance;
    input.splitLambda = quality.shadowSplitLambda;
    {
        // Tight bounds from the drawables' static POSITION boxes (world space, current pose). Skinned, morphed
        // and quantized primitives cannot be bounded that way: the loose node extents (bounding spheres of
        // the accessor boxes) are added for them.
        ux3d::math::vec3 lo(0.0f);
        ux3d::math::vec3 hi(0.0f);
        bool bounded = false;
        const size_t unbounded = gltfLayer->boundDrawables(casterScene.drawables, casterMapping, lo, hi, bounded);
        glm::vec3 sceneMin(lo);
        glm::vec3 sceneMax(hi);
        if (unbounded > 0 || !bounded) {
            const ux3d::gltf::Extends& extents = getCachedSceneExtents(activeScene, gltfModel, gltfLayer.get());
            const glm::vec3 extentsMin(extents.min);
            const glm::vec3 extentsMax(extents.max);
            sceneMin = bounded ? glm::min(sceneMin, extentsMin) : extentsMin;
            sceneMax = bounded ? glm::max(sceneMax, extentsMax) : extentsMax;
        }
        if (groundHeight.has_value() && std::isfinite(sceneMin.x + sceneMin.y + sceneMin.z + sceneMax.x + sceneMax.y + sceneMax.z)
            && sceneMax.x >= sceneMin.x && sceneMax.y >= sceneMin.y && sceneMax.z >= sceneMin.z) {
            // A3.6: the ground plane receives the model's shadow. Add the plane to the range the cascades are cut
            // to, together with the model's footprint on it: every corner of the scene box carried along the sun's
            // rays to the plane (at most ten box diagonals away when the sun is near the horizon).
            const float height = groundHeight.value();
            const glm::vec3 toSun = glm::normalize(sun.direction);
            const float reach = 10.0f * glm::length(sceneMax - sceneMin);
            glm::vec3 lo = sceneMin;
            glm::vec3 hi = sceneMax;
            lo.y = std::min(lo.y, height);
            hi.y = std::max(hi.y, height);
            if (toSun.y > 1.0e-4f) {
                for (int corner = 0; corner < 8; ++corner) {
                    const glm::vec3 point((corner & 1) ? sceneMax.x : sceneMin.x, (corner & 2) ? sceneMax.y : sceneMin.y,
                                          (corner & 4) ? sceneMax.z : sceneMin.z);
                    const float distance = std::min((point.y - height) / toSun.y, reach);
                    if (distance <= 0.0f) continue;
                    const glm::vec3 onGround = point - toSun * distance;
                    lo = glm::min(lo, onGround);
                    hi = glm::max(hi, onGround);
                }
            }
            sceneMin = lo;
            sceneMax = hi;
        }
        if (std::isfinite(sceneMin.x + sceneMin.y + sceneMin.z + sceneMax.x + sceneMax.y + sceneMax.z)
            && sceneMax.x >= sceneMin.x && sceneMax.y >= sceneMin.y && sceneMax.z >= sceneMin.z) {
            const glm::vec3 margin(0.01f * glm::length(sceneMax - sceneMin) + 0.01f);
            input.hasSceneBounds = true;
            input.sceneMin = sceneMin - margin;
            input.sceneMax = sceneMax + margin;
        }
    }
    const ux3d::renderer::ShadowCascadeSetup setup = ux3d::renderer::fitShadowCascades(input);
    if (!setup.valid || !vp.sunShadowMap.ensure(input.resolution, setup.count)) {
        disable();
        vp.sceneFbo.bind();
        glViewport(0, 0, vpW, vpH);
        return;
    }

    // Casters: every primitive of the unculled scene that casts (ShadowCasterLayer rules); each cascade
    // then culls them against its own light volume.
    ux3d::renderer::ShadowCasterLayer::Options options;
    options.asoboEnabled = vp.settings.asobo.enabled;
    options.applyShadowOptions = vp.settings.asobo.applies("ASOBO_material_shadow_options");
    options.applyMaterialCode = vp.settings.asobo.applies("ASOBO_material_code");
    options.slopeBias = quality.shadowSlopeBias;
    options.constantBias = quality.shadowConstantBias;
    // The caster copies follow their sources when the model, a material or the render settings change.
    uint64_t sourceRevision = gltfLayer->getGltfGeneration();
    for (const auto change : { ux3d::imgui::SceneChange::Material, ux3d::imgui::SceneChange::Extension,
                               ux3d::imgui::SceneChange::RenderSettings, ux3d::imgui::SceneChange::ViewportSettings }) {
        sourceRevision = sourceRevision * 0x9E3779B97F4A7C15ULL + sceneRevisions.revision(change);
    }
    options.sourceRevision = sourceRevision;
    options.modelGeneration = gltfLayer->getSceneGeneration();
    ux3d::core::LinkedList<std::tuple<ux3d::renderer::Mesh*, ux3d::renderer::Material*>> casters;
    shadowCasterLayer->collect(casterScene.drawables,
                               [&casterMapping](const ux3d::renderer::Mesh* mesh) {
                                   return casterMapping.find(const_cast<ux3d::renderer::Mesh*>(mesh)) != casterMapping.end();
                               },
                               options, casters);

    // Frame values the caster shader reads (getBaseColor, the A1.7 dither); the dither pattern stays fixed.
    commandGenerator->setSunShadow(parameters);
    commandGenerator->setAsoboAlphaDither(vp.settings.asobo.applies("ASOBO_material_alphamode_dither"));
    commandGenerator->setAsoboDitherFrame(0.0f);
    commandGenerator->setAsoboUVOptions(vp.settings.asobo.applies("ASOBO_material_UV_options"));
    commandGenerator->setAsoboDetailMap(vp.settings.asobo.applies("ASOBO_material_detail_map"));
    commandGenerator->setAsoboWindshieldVertexColorMask(vp.settings.asobo.applies("ASOBO_material_windshield_v3"));

    int currentLayer = 0;
    shadowCasterLayer->prePass = [&vp, &currentLayer] { vp.sunShadowMap.bindLayer(currentLayer, false); };
    shadowCasterLayer->postPass = [&vp, vpW, vpH] {
        vp.sceneFbo.bind();
        glViewport(0, 0, vpW, vpH);
    };
    for (int c = 0; c < setup.count; ++c) {
        const ux3d::renderer::ShadowCascade& cascade = setup.cascades[static_cast<size_t>(c)];
        currentLayer = c;
        sunShadowCamera->setTransform(ux3d::math::mat4x4(glm::inverse(cascade.lightView)));
        sunShadowCamera->setProjection(ux3d::math::mat4x4(cascade.lightProjection));
        ux3d::renderer::RendererScene shadowScene;
        shadowScene.camera = sunShadowCamera.get();
        shadowScene.environmentMap = casterScene.environmentMap;
        shadowScene.lights = casterScene.lights;
        shadowScene.drawables = casters;
        gltfLayer->cullToClip(shadowScene.drawables, casterMapping, cascade.lightProjection * cascade.lightView);
        // Cleared here: a cascade without casters runs no pass, but must not keep an old map.
        vp.sunShadowMap.bindLayer(c, true);
        commandGenerator->update(shadowScene);
        glRenderer->setGpuFrameContext(frame.number, vp.id, "shadow");
        glRenderer->draw({ commandGenerator.get() });
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    vp.sceneFbo.bind();
    glViewport(0, 0, vpW, vpH);

    parameters.enabled = true;
    parameters.texture = vp.sunShadowMap.texture();
    parameters.cascadeCount = setup.count;
    parameters.filter = static_cast<int>(quality.shadowFilter);
    parameters.mapSize = static_cast<float>(input.resolution);
    parameters.normalBias = quality.shadowNormalBias;
    for (int c = 0; c < setup.count; ++c) {
        const auto& cascade = setup.cascades[static_cast<size_t>(c)];
        parameters.matrices[static_cast<size_t>(c)] = cascade.textureFromWorld;
        parameters.splits[c] = cascade.splitFar;
        parameters.texelSizes[c] = cascade.texelWorld;
    }
    parameters.nearDepth = setup.nearDepth;
    parameters.fadeStart = setup.fadeStart;
    parameters.fadeEnd = setup.fadeEnd;
    commandGenerator->setSunShadow(parameters);
    vp.sunShadowSetup = setup;
    vp.sunShadowActive = true;
}

nlohmann::json Application::sunShadowStateJson() const
{
    const ux3d::imgui::RenderQualitySettings configured = ux3d::imgui::RenderQuality::settings();
    const ux3d::imgui::RenderQualitySettings quality = ux3d::imgui::RenderQuality::effective();
    nlohmann::json out = {{"enabled", configured.shadows},
                          {"resolution", quality.shadowResolution},
                          {"cascadeCount", quality.shadowCascades},
                          {"maxDistance", agent::floatJson(quality.shadowDistance)},
                          {"filter", ux3d::imgui::RenderQuality::shadowFilterName(quality.shadowFilter)},
                          {"splitLambda", agent::floatJson(quality.shadowSplitLambda)},
                          {"normalBias", agent::floatJson(quality.shadowNormalBias)},
                          {"slopeBias", agent::floatJson(quality.shadowSlopeBias)},
                          {"constantBias", agent::floatJson(quality.shadowConstantBias)}};
    nlohmann::json viewportsJson = nlohmann::json::array();
    for (const auto& vp : viewports) {
        if (vp == nullptr) continue;
        nlohmann::json entry = {{"viewport", vp->id}, {"active", vp->sunShadowActive},
                                {"memoryBytes", vp->sunShadowMap.bytes()}};
        if (vp->sunShadowActive) {
            nlohmann::json cascades = nlohmann::json::array();
            for (int c = 0; c < vp->sunShadowSetup.count; ++c) {
                const auto& cascade = vp->sunShadowSetup.cascades[static_cast<size_t>(c)];
                cascades.push_back({{"near", agent::floatJson(cascade.splitNear)},
                                    {"far", agent::floatJson(cascade.splitFar)},
                                    {"radius", agent::floatJson(cascade.radius)},
                                    {"texelWorld", agent::floatJson(cascade.texelWorld)},
                                    {"depthRange", agent::floatJson(cascade.depthRange)},
                                    {"lightOrigin", {agent::floatJson(cascade.lightOrigin.x), agent::floatJson(cascade.lightOrigin.y)}}});
            }
            entry["cascades"] = cascades;
            entry["casters"] = shadowCasterLayer != nullptr ? shadowCasterLayer->getStats().casters : 0;
        }
        viewportsJson.push_back(entry);
    }
    out["viewports"] = viewportsJson;
    if (shadowCasterLayer != nullptr) {
        const auto& stats = shadowCasterLayer->getStats();
        out["excluded"] = {{"notOpaquePass", stats.notOpaquePass}, {"decals", stats.decals}, {"glass", stats.glass},
                           {"invisible", stats.invisible}, {"noCastShadow", stats.noCastShadow}};
        out["materialCopies"] = shadowCasterLayer->getMaterialCopies();
    }
    return out;
}

void Application::releaseDecalBuffer()
{
    decalBuffer.release();
    if (decalPrepassLayer != nullptr) {
        decalPrepassLayer->setDebugTargets(0, 0, 0, 0);
    }
    if (decalAccumulationLayer != nullptr) {
        decalAccumulationLayer->setG0Target(0);
        decalAccumulationLayer->setDepthTarget(0);
    }
    if (commandGenerator != nullptr) {
        commandGenerator->setDecalTargets(0, 0, 0, 0, 0, 0);
    }
}

} // namespace ux3d::imgui::application
