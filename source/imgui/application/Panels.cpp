// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/SceneHierarchyPanel.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/SceneInspector.h"
#include "imgui/ElementInspector.h"
#include "imgui/StatisticsPanel.h"
#include "imgui/ValidationPanel.h"
#include "imgui/SearchPathsPanel.h"

namespace ux3d::imgui::application {

void Application::drawPanels(FrameContext& frame)
{
    // -------------------------------------------------------------
    // Hierarchy Panel
    // -------------------------------------------------------------
    const auto panelsStart = ux3d::imgui::profiling::Clock::now();
    if (showHierarchy) {
        ux3d::editor::CameraActor dummyActor;
        ux3d::imgui::AntiAliasing dummyAA;
        ux3d::imgui::SceneHierarchyPanel::render(gltfModel, activeScene, selection, gltfLayer.get(),
                                                 activeViewport ? activeViewport->cameraActor : dummyActor,
                                                 activeViewport ? activeViewport->antiAliasing : dummyAA,
                                                 sceneRevisions, &showHierarchy, &viewports, &activeViewport,
                                                 [this](auto* vp) { showViewportMenuFor(vp); });
    }

    // -------------------------------------------------------------
    // Resources Panel
    // -------------------------------------------------------------
    if (showResources) {
        ux3d::imgui::ResourcesPanel::render(gltfModel, selection, gltfLayer.get(), &sceneRevisions, &showResources);
    }

    // -------------------------------------------------------------
    // Inspector Panel
    // -------------------------------------------------------------
    if (showInspector) {
        if (ImGui::Begin(WND_INSPECTOR, &showInspector)) {
            if (selection.type == ux3d::imgui::SelectionType::Scene) {
                auto* inspectedScene = selection.selectedScene != nullptr ? selection.selectedScene : activeScene;
                ux3d::imgui::SceneInspector::render(getSceneEnvironment(sceneEnvironments, gltfModel, inspectedScene),
                                                    commandGenerator.get(), gltfLayer.get(), sceneRevisions,
                                                    inspectedScene);
            } else {
                ux3d::imgui::ElementInspector::render(selection, gltfModel, sceneRevisions, &animationPlayer,
                                                      gltfLayer.get(), &viewports, commandGenerator.get());
            }
        }
        ImGui::End();
    }

    // -------------------------------------------------------------
    // Statistics Panel
    // -------------------------------------------------------------
    if (showStatistics) {
        if (ImGui::Begin(WND_STATISTICS, &showStatistics)) {
            ux3d::imgui::RenderBufferStatistics buffers;
            buffers.decalBufferActive = decalBuffer.isValid();
            buffers.decalBufferWidth = decalBuffer.getWidth();
            buffers.decalBufferHeight = decalBuffer.getHeight();
            buffers.decalBufferBytes = decalBuffer.getMemoryBytes();
            buffers.decalBufferAllocations = decalBuffer.getAllocationCount();
            buffers.decalBufferResizes = decalBuffer.getResizeCount();
            buffers.decalBufferReleases = decalBuffer.getReleaseCount();
            ux3d::imgui::StatisticsPanel::render(gltfModel, currentFilePath, buffers);
        }
        ImGui::End();
    }

    // -------------------------------------------------------------
    // Validation Panel
    // -------------------------------------------------------------
    if (showValidation) {
        ux3d::editor::CameraActor dummyActor;
        ux3d::imgui::ValidationPanel::render(&showValidation, currentFilePath, gltfModel, selection,
                                             activeViewport ? activeViewport->cameraActor : dummyActor,
                                             gltfLayer.get());
    }

    // -------------------------------------------------------------
    // Search Paths Window
    // -------------------------------------------------------------
    if (showSearchPaths) {
        ux3d::imgui::renderSearchPathsWindow(&showSearchPaths, [&]() {
            if (!currentFilePath.empty()) {
                loadGltfModel(currentFilePath, gltfModel, activeScene, selection, currentFilePath, viewports,
                              gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            }
        });
    }

    drawLogger();
    // Demo Window (for debug/testing)
    if (showDemoWindow) {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }

    // -------------------------------------------------------------
    // Application Status Bar (Bottom)
    // -------------------------------------------------------------
    frame.markStage("ui.panels", panelsStart);
}

} // namespace ux3d::imgui::application
