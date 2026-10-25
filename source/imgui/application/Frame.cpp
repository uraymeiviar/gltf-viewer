// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/ObjectActions.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <ImGuizmo.h>

namespace ux3d::imgui::application {

void Application::runFrame()
{
    auto& io = ImGui::GetIO();
    frameInProgress = true;
    if (!s_idleStatsPath.empty() && idleFrameStartsMs.size() < 200000) {
        idleFrameStartsMs.push_back(wallClockMs());
    }
    const uint64_t sceneRendersAtFrameStart = s_sceneRenderTotal;

    FrameContext frame;
    frame.start = ux3d::imgui::profiling::Clock::now();
    frame.profiling = s_profilingEnabled;
    frame.number = frameCount + 1;
    frame.sample.frame = frame.number;
    if (frame.number > 1) {
        frame.sample.frameIntervalMs = ux3d::imgui::profiling::elapsedMs(previousFrameStart, frame.start);
    }
    if (frame.number == 1) {
        s_captureEpochSeconds = glfwGetTime();
    }
    previousFrameStart = frame.start;
    ux3d::imgui::profiling::resetFrameCounters();

    double currentTime = glfwGetTime();
    float deltaTime = static_cast<float>(currentTime - lastFrameTime);
    lastFrameTime = currentTime;
    if (deltaTime > 0.1f) deltaTime = 0.1f;
    if (s_fixedFrameTime >= 0.0f) deltaTime = s_fixedFrameTime;

    // Update animation player
    const auto animationUpdateStart = ux3d::imgui::profiling::Clock::now();
    animationPlayer.update(deltaTime, gltfLayer.get());
    frame.markStage("animation.update", animationUpdateStart);

    processInput(deltaTime);
    // Start ImGui frame
    const auto uiNewFrameStart = ux3d::imgui::profiling::Clock::now();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    if (uiScenario.active()) {
        uiScenario.beginFrame(ImGui::GetIO());
    }
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
    // Input or window events processed this frame (mouse, keys, text, focus).
    const bool inputThisFrame = ImGui::GetCurrentContext()->InputEventsTrail.Size > 0;
    frame.markStage("ui.newFrame", uiNewFrameStart);

    drawMenus(frame);
    driveMaterialEditCheck();
    synchronizeScene(frame);
    drawViewports(frame);
    drawPanels(frame);
    const auto statusBarStart = ux3d::imgui::profiling::Clock::now();
    ux3d::imgui::StatusBar::render(gltfModel, gltfLayer.get(), animationPlayer, sceneRevisions, showLogger,
                                   showValidation, s_consoleLogs.counts(),
                                   activeViewport ? &activeViewport->antiAliasing : nullptr);
    frame.markStage("ui.statusBar", statusBarStart);

    // Rendering main window
    const auto uiRenderStart = ux3d::imgui::profiling::Clock::now();
    ImGui::Render();
    frame.markStage("ui.render", uiRenderStart);
    int displayW = 0, displayH = 0;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const auto uiSubmitStart = ux3d::imgui::profiling::Clock::now();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    frame.markStage("ui.submit", uiSubmitStart);
    if (const ImDrawData* drawData = ImGui::GetDrawData(); drawData != nullptr) {
        ux3d::imgui::profiling::countUiDrawData(static_cast<uint64_t>(drawData->CmdListsCount),
                                                static_cast<uint64_t>(drawData->TotalVtxCount),
                                                static_cast<uint64_t>(drawData->TotalIdxCount));
    }

    finishFrame(frame, displayW, displayH);
    glfwSwapBuffers(window);
    // P5.4: a frame is quiet when no input arrived, no viewport redrew, nothing is
    // pending for the next frame, and nothing animates. ImGui gets a few quiet
    // frames to settle hover and layout before the loop starts waiting.
    bool turntableActive = false;
    for (const auto& vp : viewports) {
        turntableActive = turntableActive || (vp->isOpen && vp->settings.turntable);
    }
    const bool frameActive = inputThisFrame || s_sceneRenderTotal != sceneRendersAtFrameStart ||
                             sceneRevisions.hasPending() || animationPlayer.isPlaying() || turntableActive ||
                             io.WantTextInput || ImGui::IsAnyMouseDown();
    idleQuietFrames = frameActive ? 0 : idleQuietFrames + 1;
    frameInProgress = false;
}

void Application::driveMaterialEditCheck()
{
    if (s_materialEditEmissiveMaterial.empty() || gltfLayer == nullptr) {
        return;
    }
    if (s_materialEditTarget == nullptr) {
        for (ux3d::gltf::Material& material : gltfModel.getMaterials()) {
            if (material.getName() == s_materialEditEmissiveMaterial) {
                s_materialEditTarget = &material;
                break;
            }
        }
        if (s_materialEditTarget == nullptr) {
            std::fprintf(stderr, "--material-edit-emissive: material '%s' not found\n",
                         s_materialEditEmissiveMaterial.c_str());
            s_materialEditEmissiveMaterial.clear();
            return;
        }
    }

    // One material-only edit per frame, like dragging the emissive colour in the Inspector.
    const float phase = static_cast<float>(frameCount % 64u) / 63.0f;
    const float value = 0.1f + 0.9f * phase;
    s_materialEditTarget->setEmissiveFactor(ux3d::math::dvec3(value, value * 0.5, 1.0 - value));
    sceneRevisions.mark(ux3d::imgui::SceneChange::Material);
}

void Application::processInput(float deltaTime)
{
    auto& io = ImGui::GetIO();
    // Turntable Auto-Rotate
    for (auto& vp : viewports) {
        if (vp->isOpen && vp->settings.turntable && !ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            vp->cameraActor.rotateCamera(static_cast<int32_t>(vp->settings.turntableSpeed * deltaTime * 20.0f), 0);
        }
    }

    // Keyboard shortcuts
    if (!io.WantTextInput && activeViewport != nullptr) {
        if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
            if (activeScene != nullptr) activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
            activeViewport->antiAliasing.resetHistory();
        }

        // Numpad Camera Presets (Gestaltor style)
        if (!activeViewport->isAttachedToCamera) {
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad1)) {
                activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Back
                                                                        : ux3d::editor::CameraOrientation::Front);
                activeViewport->antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad3)) {
                activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Left
                                                                        : ux3d::editor::CameraOrientation::Right);
                activeViewport->antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad7)) {
                activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Bottom
                                                                        : ux3d::editor::CameraOrientation::Top);
                activeViewport->antiAliasing.resetHistory();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Keypad5)) {
                activeViewport->cameraActor.changeUserCameraType(
                    (activeViewport->rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic)
                        ? ux3d::renderer::Camera::Type::Perspective
                        : ux3d::renderer::Camera::Type::Orthographic);
                activeViewport->antiAliasing.resetHistory();
            }
        }

        // Object shortcuts, matching the Object menu items. Skipped while a menu or
        // popup is open, so menu navigation does not trigger scene actions.
        if (GImGui->OpenPopupStack.Size == 0) {
            ux3d::imgui::ObjectActionContext objectShortcutContext{gltfModel,
                                                                   activeScene,
                                                                   selection,
                                                                   gltfLayer.get(),
                                                                   &activeViewport->cameraActor,
                                                                   &activeViewport->antiAliasing,
                                                                   sceneRevisions};
            if (ImGui::IsKeyPressed(ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal)) {
                ux3d::imgui::focusNode(objectShortcutContext, ux3d::imgui::selectedNode(objectShortcutContext));
            }
            if (ImGui::IsKeyPressed(ImGuiKey_H)) {
                ux3d::imgui::toggleNodeVisibility(objectShortcutContext,
                                                  ux3d::imgui::selectedNode(objectShortcutContext));
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Slash)) {
                if (io.KeyShift) {
                    if (!ux3d::imgui::isolationActive(objectShortcutContext)) {
                        ux3d::imgui::setIsolation(objectShortcutContext,
                                                  ux3d::imgui::selectedNode(objectShortcutContext), true);
                    }
                } else if (ux3d::imgui::isolationActive(objectShortcutContext)) {
                    ux3d::imgui::exitIsolation(objectShortcutContext);
                } else {
                    ux3d::imgui::setIsolation(objectShortcutContext, ux3d::imgui::selectedNode(objectShortcutContext),
                                              false);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
                ux3d::imgui::selectParent(objectShortcutContext, ux3d::imgui::selectedNode(objectShortcutContext));
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
                ux3d::imgui::selectFirstChild(objectShortcutContext, ux3d::imgui::selectedNode(objectShortcutContext));
            }
        }

        // Ctrl+Alt+1: Default Shaded, Ctrl+Alt+2: Wireframe
        if (io.KeyCtrl && io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_1)) {
                activeViewport->settings.renderMode = ux3d::renderer::RenderMode::None;
                activeViewport->settings.renderModeIdx = 0;
                sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
            } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
                activeViewport->settings.renderMode = ux3d::renderer::RenderMode::Wireframe;
                activeViewport->settings.renderModeIdx = 1;
                sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
            }
        }
    }

    // Animation shortcuts: Alt+A toggles play/stop; Shift+Left/Right rewinds and
    // fast-forwards. Skipped while typing or while a menu/popup is open.
    if (!io.WantTextInput && GImGui->OpenPopupStack.Size == 0) {
        if (io.KeyAlt && !io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A)) {
            if (animationPlayer.getMaxDuration() > 0.0f) {
                animationPlayer.setPlaying(!animationPlayer.isPlaying());
            }
        }
        if (io.KeyShift && !io.KeyAlt && !io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                animationPlayer.setCurrentTime(0.0f, gltfLayer.get());
                if (activeViewport != nullptr) activeViewport->antiAliasing.resetHistory();
            } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                animationPlayer.setCurrentTime(animationPlayer.getMaxDuration(), gltfLayer.get());
                if (activeViewport != nullptr) activeViewport->antiAliasing.resetHistory();
            }
        }
    }
}

} // namespace ux3d::imgui::application
