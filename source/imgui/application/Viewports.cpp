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
    if (gltfLayer != nullptr) {
        const auto desiredEnvPath = resolveHdrPath(activeEnvironment.currentHdrPath, rendererResourcesDir);
        if (gltfLayer->getEnvironmentMapPath().toString() != desiredEnvPath.toString()) {
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
        const float renderScale = ux3d::imgui::RenderQuality::settings().renderScale;
        int vpW = static_cast<int>(viewportSize.x * renderScale);
        int vpH = static_cast<int>(viewportSize.y * renderScale);
        if (vpW < 1) vpW = 1;
        if (vpH < 1) vpH = 1;

        const int postAA = vp->settings.effectivePostAAIdx();
        if (postAA != 2 && vp->overlayFbo.isValid()) vp->overlayFbo = ux3d::imgui::Framebuffer();
        auto aaMode = static_cast<ux3d::imgui::AntiAliasingMode>(postAA == 0 ? 0 : postAA + 3);
        int desiredSamples = ux3d::imgui::AntiAliasing::getSampleCount(
            static_cast<ux3d::imgui::AntiAliasingMode>(vp->settings.effectiveMsaaIdx()));

        if (vp->sceneFbo.getWidth() != vpW || vp->sceneFbo.getHeight() != vpH ||
            vp->sceneFbo.getSamples() != desiredSamples) {
            vp->sceneFbo.resize(vpW, vpH, desiredSamples);
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
        const bool redrawScene = s_disableRedrawSkip || !keyUnchanged || pickingRendered || taaAccumulating;
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
            if (activeEnvironment.showBackground) {
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            } else {
                glClearColor(activeEnvironment.clearColor.r, activeEnvironment.clearColor.g,
                             activeEnvironment.clearColor.b, activeEnvironment.clearColor.a);
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
            gltfLayer->enableCubeMap(activeEnvironment.showBackground);
            glRenderer->setClearColor(activeEnvironment.clearColor);
            gltfLayer->getResourceManager().setClearColor(activeEnvironment.clearColor);

            auto mode = vp->settings.renderMode;

            // A2.1: the two decal debug views show the prepass targets instead of a PBR
            // debug channel, so the forward draws stay normally shaded underneath.
            const bool decalDebugView =
                mode == ux3d::renderer::RenderMode::DecalPrepassDepth ||
                mode == ux3d::renderer::RenderMode::DecalPrepassNormal;
            const auto shadingMode = decalDebugView ? ux3d::renderer::RenderMode::None : mode;

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

            const auto createSceneStart = ux3d::imgui::profiling::Clock::now();
            gltfLayer->frustumCulling = !s_disableCulling;
            gltfLayer->asoboOptions = vp->settings.asobo;
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
            std::vector<std::unique_ptr<ux3d::renderer::Material>> wireMaterials;
            const auto wireMaterial = [&wireMaterials](ux3d::renderer::Material* material) {
                auto copy = std::make_unique<ux3d::renderer::Material>(*material);
                copy->fragmentShader = "wireframe_material.frag";
                auto* result = copy.get();
                wireMaterials.push_back(std::move(copy));
                return result;
            };

            auto cubeTuple = gltfLayer->getResourceManager().getCubeMapTuple();
            auto* cubeMesh = std::get<0>(cubeTuple);
            auto* cubeMaterial = std::get<1>(cubeTuple);
            for (auto& [mesh, material] : scene.drawables) {
                if (mesh != nullptr && mesh != cubeMesh) {
                    mesh->polygonMode = (shadingMode == ux3d::renderer::RenderMode::Wireframe)
                                            ? ux3d::renderer::gl::PolygonMode::Line
                                            : ux3d::renderer::gl::PolygonMode::Fill;
                    mesh->defines.define("DEBUG", static_cast<int>(shadingMode));
                }
                if (material != nullptr && material != cubeMaterial) {
                    material->defines.define("DEBUG", static_cast<int>(shadingMode));
                    if (vp->settings.forceDoubleSided) {
                        material->isDoubleSided = true;
                    }
                    if (shadingMode == ux3d::renderer::RenderMode::Wireframe) {
                        material = wireMaterial(material);
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

            // A2.1: append the decal prepass copies of the opaque drawables. Their pass
            // has a lower id than Opaque, so the prepass targets are filled before the
            // forward opaque pass of this viewport. The pass restores sceneFbo afterwards.
            if (decalActive) {
                decalPrepassLayer->setDebugView(
                    mode == ux3d::renderer::RenderMode::DecalPrepassDepth
                        ? ux3d::renderer::DecalPrepassLayer::DebugView::Depth
                        : mode == ux3d::renderer::RenderMode::DecalPrepassNormal
                              ? ux3d::renderer::DecalPrepassLayer::DebugView::Normal
                              : ux3d::renderer::DecalPrepassLayer::DebugView::None);
                decalPrepassLayer->prePass = [this, vpW, vpH] { decalBuffer.bindPrepass(vpW, vpH); };
                decalPrepassLayer->postPass = [this, &vp] {
                    if (transmissionSource != nullptr) transmissionSource->bind();
                    else vp->sceneFbo.bind();
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
            commandGenerator->setIBLEnabled(activeEnvironment.iblEnabled);
            commandGenerator->setPunctualLightsEnabled(activeEnvironment.punctualLightsEnabled);
            commandGenerator->setAsoboNormalMapConvention(
                gltfLayer->usesDirectXNormalMaps() &&
                vp->settings.asobo.applies("ASOBO_normal_map_convention"));
            commandGenerator->setAsoboWindshieldVertexColorMask(
                vp->settings.asobo.applies("ASOBO_material_windshield_v3"));
            // A2.2: the receiver define is asset-level; this per-viewport flag says whether
            // the active viewport has a filled decal buffer to read.
            commandGenerator->setAsoboDecalReceiver(decalActive);
            commandGenerator->setDecalDepthTolerance(vp->settings.decalDepthToleranceMm * 0.001f);

            const auto commandGenerationStart = ux3d::imgui::profiling::Clock::now();
            const uint64_t programHitsBefore = commandGenerator->getCommandMapping().getProgramCacheHits();
            const uint64_t programMissesBefore = commandGenerator->getCommandMapping().getProgramCacheMisses();
            commandGenerator->update(scene);
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
            ssrCaptureDepth = scene.windshieldSsr && vp->settings.screenSpaceReflections;
            ssrUseDecalDepth = decalActive;
            ssrInteriorSky = vp->settings.interiorSkyReflection;
            commandGenerator->setSsrOptions(vp->settings.screenSpaceReflections,
                                           vp->settings.ssrQuality, vp->settings.interiorSkyReflection);
            // TAA resolves the per-frame ray phase noise into soft reflection silhouettes.
            commandGenerator->setSsrFrame(isTaa ? static_cast<float>(vp->taaStableRenders % 64u) : 0.0f);
            if (scene.asoboGlassCopy) glassCopyFbo.resize(vpW, vpH, vp->sceneFbo.getSamples(), true);
            const bool floatCopy = scene.asoboGlassCopy && glassCopyFbo.isValid() && glassCopyFbo.isLinearFloat();
            transmissionDestination = floatCopy ? &vp->sceneFbo : nullptr;
            transmissionSource = floatCopy ? &glassCopyFbo : &vp->sceneFbo;
            if (floatCopy) {
                glassCopyFbo.bind();
                glViewport(0, 0, vpW, vpH);
                glClearColor(activeEnvironment.clearColor.r, activeEnvironment.clearColor.g,
                             activeEnvironment.clearColor.b, activeEnvironment.clearColor.a);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }
            glRenderer->draw({commandGenerator.get()});
            transmissionSource = nullptr;
            transmissionDestination = nullptr;
            vp->lastExecutedPasses = glRenderer->getLastExecutedPasses();
            ++vp->renderCount;
            frame.markStage("draw.submit", drawSubmissionStart);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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

            if (isTaa) {
                // Resolve hardware samples first, then accumulate only the scene.
                // Composite into separate storage so overlays never contaminate history.
                vp->sceneFbo.unbind();
                const auto aaStart = ux3d::imgui::profiling::Clock::now();
                const GLuint sceneTexture = vp->antiAliasing.render(
                    aaMode, vp->sceneFbo.getColorTexture(), vpW, vpH, proj * view, frameSceneDirty);
                frame.markStage("aa.render", aaStart);
                vp->overlayFbo.resize(vpW, vpH, 1);
                GLuint aaFbo = vp->sceneFbo.getResolveFbo();
                for (int i = 0; i < 2; ++i) {
                    if (sceneTexture == vp->antiAliasing.getTaaTexture(i)) aaFbo = vp->antiAliasing.getTaaFbo(i);
                }
                glDisable(GL_SCISSOR_TEST);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, aaFbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, vp->overlayFbo.getFbo());
                glBlitFramebuffer(0, 0, vpW, vpH, 0, 0, vpW, vpH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                vp->overlayFbo.bind();
                glDepthMask(GL_TRUE);
                glClear(GL_DEPTH_BUFFER_BIT);

                // Rebuild opaque depth with the unjittered camera. Reusing jittered
                // scene depth would still make grid/box occlusion flicker at edges.
                if (vp->settings.showGrid || vp->settings.showSceneBoundingBox ||
                    vp->settings.showSelectedBoundingBox || vp->settings.wireframeOverlay) {
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
                    vp->overlayFbo.bind();
                }
                displayTexture = vp->overlayFbo.getColorTexture();
            }

            if (vp->settings.wireframeOverlay && shadingMode != ux3d::renderer::RenderMode::Wireframe) {
                const auto wireframeStart = ux3d::imgui::profiling::Clock::now();
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.0f, -1.0f);
                glLineWidth(1.0f);
                gltfLayer->frustumCulling = !s_disableCulling;
                gltfLayer->asoboOptions = vp->settings.asobo;
                auto [wireScene, _] = gltfLayer->createScene(activeScene);
                gltfLayer->frustumCulling = false;
                // The overlay draws lines over the finished shaded image. With
                // transmissive materials, createScene adds the transmission set-up:
                // duplicate Opaque2/Grid2/Transparent2 drawables and a TransmissiveBlit
                // pass whose post-command clears the whole framebuffer. Redrawing that
                // here wiped the shaded image, leaving only lines, so it is left out.
                for (auto it = wireScene.drawables.begin(); it != wireScene.drawables.end();) {
                    auto& [mesh, material] = *it;
                    const auto passId = (material != nullptr && material->renderPass != nullptr)
                                            ? material->renderPass->getId()
                                            : ux3d::renderer::RenderPassType::MaxRenderPasses;
                    if (passId == ux3d::renderer::RenderPassType::TransmissiveBlit ||
                        passId == ux3d::renderer::RenderPassType::Opaque2 ||
                        passId == ux3d::renderer::RenderPassType::Grid2 ||
                        passId == ux3d::renderer::RenderPassType::Transparent2) {
                        it = wireScene.drawables.erase(it);
                        continue;
                    }
                    if (mesh != nullptr && mesh != cubeMesh) {
                        mesh->polygonMode = ux3d::renderer::gl::PolygonMode::Line;
                        if (material != nullptr) {
                            material = wireMaterial(material);
                        }
                    }
                    ++it;
                }
                commandGenerator->update(wireScene);
                glRenderer->setGpuFrameContext(frame.number, vp->id, "wireframe");
                glRenderer->draw({commandGenerator.get()});
                vp->lastExecutedPasses = glRenderer->getLastExecutedPasses();
                ++vp->renderCount;
                glDisable(GL_POLYGON_OFFSET_LINE);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                frame.markStage("scene.wireframeOverlay", wireframeStart);
            }
        } else if (redrawScene) {
            currentReverseMapping.clear();
        }

        // 2. Render ground grid
        const auto overlayStart = ux3d::imgui::profiling::Clock::now();
        if (redrawScene && vp->settings.showGrid) {
            gridRenderer.draw(view, proj,
                              ux3d::math::vec3(cameraEye.x, cameraEye.y, cameraEye.z), cameraZoom,
                              ux3d::imgui::RenderQuality::settings().gridMode == ux3d::imgui::GridMode::PlaneMesh);
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

        // 4. Post-processing Anti-Aliasing pass
        if (redrawScene) {
            if (isTaa) vp->overlayFbo.unbind();
            else vp->sceneFbo.unbind();
            if (!isTaa) {
                const auto antiAliasingStart = ux3d::imgui::profiling::Clock::now();
                displayTexture = vp->antiAliasing.render(
                    aaMode, vp->sceneFbo.getColorTexture(), vpW, vpH, proj * view, frameSceneDirty);
                frame.markStage("aa.render", antiAliasingStart);
            }
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
