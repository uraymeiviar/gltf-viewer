// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/AppSettings.h"
#include "imgui/ElementInspector.h"
#include "imgui/StatisticsPanel.h"
#include "imgui/GltfValidator.h"
#include "gltf/GltfParsing.h"
#include "gltf/ImportOptions.h"
#include "gltf/ExportOptions.h"
#include "gltf/Material.h"
#include <algorithm>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>

namespace ux3d::imgui::application {

ux3d::imgui::SceneEnvironmentState&
Application::getSceneEnvironment(std::vector<ux3d::imgui::SceneEnvironmentState>& sceneEnvironments,
                                 const ux3d::gltf::Gltf& gltfModel, const ux3d::gltf::Scene* scene)
{
    const auto& scenes = gltfModel.getElements<ux3d::gltf::Scene>();
    const size_t needed = std::max<size_t>(1, scenes.size());
    if (sceneEnvironments.size() < needed) {
        sceneEnvironments.resize(needed);
    }
    if (scene != nullptr) {
        size_t idx = 0;
        for (const auto& s : scenes) {
            if (&s == scene) {
                return sceneEnvironments[idx];
            }
            ++idx;
        }
    }
    return sceneEnvironments[0];
}

bool Application::loadGltfModel(const std::string& filePath, ux3d::gltf::Gltf& gltfModel,
                                ux3d::gltf::Scene*& activeScene, ux3d::imgui::SelectionState& selection,
                                std::string& currentFilePath,
                                std::vector<std::unique_ptr<ux3d::imgui::ViewportInstance>>& viewports,
                                ux3d::renderer::GltfRenderLayer* gltfLayer, ux3d::imgui::AnimationPlayer& animPlayer,
                                ux3d::imgui::SceneRevisions& revisions,
                                std::vector<ux3d::imgui::SceneEnvironmentState>& sceneEnvironments)
{
    logMessage("Opening file: " + filePath);
    ux3d::imgui::StatisticsPanel::invalidate();
    ux3d::imgui::AppSettings::instance().setCurrentModelPath(filePath);
    ux3d::gltf::ImportOptions importOptions;
    auto& startupProfile = ux3d::imgui::profiling::startupProfile();
    const auto loadStart = ux3d::imgui::profiling::Clock::now();
    ux3d::filesystem::File file = ux3d::imgui::StdFileSystem::instance().toFile(filePath.c_str());

    ux3d::imgui::ElementInspector::clearPreviewCache();
    if (gltfLayer != nullptr) {
        gltfLayer->setIsolation(nullptr, false); // the isolated node pointer is about to dangle
    }
    gltfModel.reset();
    ux3d::imageformats::resetDecodeBudgetStats();
    auto result = ux3d::gltf::GltfParsing::fromFile(gltfModel, file, importOptions);
    startupProfile.loadDecodeMs = ux3d::imgui::profiling::elapsedMs(loadStart);
    {
        const auto budgetStats = ux3d::imageformats::getDecodeBudgetStats();
        startupProfile.decodeBudgetBytes = budgetStats.budgetBytes;
        startupProfile.decodeBudgetWaits = budgetStats.waits;
        startupProfile.decodePeakInFlightBytes = budgetStats.peakInFlightBytes;
        startupProfile.decodePeakInFlightCount = budgetStats.peakInFlightCount;
    }
    startupProfile.peakWorkingSetAfterDecode = ux3d::imgui::processPeakWorkingSetBytes();
    if (result.derived == ux3d::gltf::GltfFromFileResult::Success) {
        currentFilePath = filePath;
        activeScene = gltfModel.getDefaultScene();
        if (!activeScene && !gltfModel.getElements<ux3d::gltf::Scene>().empty()) {
            activeScene = &gltfModel.getElements<ux3d::gltf::Scene>().front();
        }
        selection = ux3d::imgui::SelectionState();
        const size_t sceneCount = gltfModel.getElements<ux3d::gltf::Scene>().size();
        sceneEnvironments.assign(std::max<size_t>(1, sceneCount), ux3d::imgui::SceneEnvironmentState{});

        const auto uploadStart = ux3d::imgui::profiling::Clock::now();
        if (gltfLayer != nullptr) {
            gltfLayer->updateGltf(gltfModel);
            if (activeScene != nullptr) {
                gltfLayer->updateScene(gltfModel, *activeScene);
                gltfLayer->applyAnimationTime(0.0f);
            }
        }
        startupProfile.rendererUploadMs = ux3d::imgui::profiling::elapsedMs(uploadStart);
        startupProfile.peakWorkingSetAfterUpload = ux3d::imgui::processPeakWorkingSetBytes();

        animPlayer.setModel(gltfModel, gltfLayer);

        size_t nodeCount = gltfModel.getElements<ux3d::gltf::Node>().size();
        size_t meshCount = gltfModel.getElements<ux3d::gltf::Mesh>().size();
        size_t matCount = gltfModel.getElements<ux3d::gltf::Material>().size();
        startupProfile.nodeCount = nodeCount;
        startupProfile.meshCount = meshCount;
        startupProfile.materialCount = matCount;
        startupProfile.imageCount = gltfModel.getElements<ux3d::gltf::Image>().size();
        startupProfile.textureCount = gltfModel.getElements<ux3d::gltf::Texture>().size();
        std::error_code sizeError;
        const auto modelSize = std::filesystem::file_size(std::filesystem::path(filePath), sizeError);
        if (!sizeError) {
            startupProfile.modelBytes = static_cast<uint64_t>(modelSize);
        }
        char infoBuf[256];
        std::snprintf(infoBuf, sizeof(infoBuf), "Model Stats: %zu nodes, %zu meshes, %zu materials", nodeCount,
                      meshCount, matCount);
        logMessage(infoBuf, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));

        if (activeScene != nullptr && !activeScene->getNodes().empty()) {
            for (auto& vp : viewports) {
                if (vp->isAttachedToCamera) {
                    bool found = false;
                    for (const auto& node : gltfModel.getNodes()) {
                        if (node.m_ID == vp->attachedCameraNodeId) {
                            vp->attachedCameraNode = &node;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        vp->isAttachedToCamera = false;
                        vp->attachedCameraNode = nullptr;
                    }
                }
                vp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                vp->antiAliasing.resetHistory();
            }
            ux3d::gltf::Extends extends = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
            char boundsBuf[256];
            std::snprintf(boundsBuf, sizeof(boundsBuf), "Scene Bounds: [%.2f, %.2f, %.2f] to [%.2f, %.2f, %.2f]",
                          extends.min.x, extends.min.y, extends.min.z, extends.max.x, extends.max.y, extends.max.z);
            logMessage(boundsBuf, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
        }

        revisions.mark(ux3d::imgui::SceneChange::Topology);
        if (gltfLayer != nullptr) {
            // The renderer was synchronized above; the next frame need not
            // repeat it unless something else changes (P4.3).
            revisions.noteSynced(ux3d::imgui::SceneChange::Topology);
        }
        logMessage("Loaded successfully: " + filePath, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));

        // Auto-run glTF 2.0 validation
        const auto validationStart = ux3d::imgui::profiling::Clock::now();
        auto valReport = ux3d::imgui::GltfValidator::instance().validate(filePath, gltfModel);
        startupProfile.validationMs = ux3d::imgui::profiling::elapsedMs(validationStart);
        startupProfile.validationReportedMs = valReport.durationMs;
        if (!s_validationReportPath.empty()) {
            ux3d::imgui::GltfValidator::writeReportJson(valReport, s_validationReportPath);
        }
        startupProfile.validationReadMs = valReport.timing.readMs;
        startupProfile.validationParseMs = valReport.timing.parseMs;
        startupProfile.validationSchemaMs = valReport.timing.schemaMs;
        startupProfile.validationSemanticMs = valReport.timing.semanticMs;
        startupProfile.validationBytesRead = valReport.timing.bytesRead;
        if (valReport.errorCount > 0) {
            char valBuf[256];
            std::snprintf(valBuf, sizeof(valBuf), "Validation: %d %s, %d %s (%.1f ms)", valReport.errorCount,
                          valReport.errorCount == 1 ? "error" : "errors", valReport.warningCount,
                          valReport.warningCount == 1 ? "warning" : "warnings", valReport.durationMs);
            logMessage(valBuf, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        } else if (valReport.warningCount > 0) {
            char valBuf[256];
            std::snprintf(valBuf, sizeof(valBuf), "Validation: %d %s (%.1f ms)", valReport.warningCount,
                          valReport.warningCount == 1 ? "warning" : "warnings", valReport.durationMs);
            logMessage(valBuf, ImVec4(1.0f, 0.85f, 0.25f, 1.0f));
        } else {
            char valBuf[256];
            std::snprintf(valBuf, sizeof(valBuf), "Validation: PASSED (glTF 2.0 Valid, %.1f ms)", valReport.durationMs);
            logMessage(valBuf, ImVec4(0.2f, 1.0f, 0.5f, 1.0f));
        }

        if (filePath != "resources/sampleGltf/HelloCube.glb" && !filePath.empty()) {
            addRecentFile(s_recentFiles, filePath);
        }
        return true;
    } else {
        logMessage("Failed to load: " + filePath, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return false;
    }
}

void Application::newEmptyGltf(ux3d::gltf::Gltf& gltfModel, ux3d::gltf::Scene*& activeScene,
                               ux3d::imgui::SelectionState& selection, std::string& currentFilePath,
                               ux3d::editor::CameraActor& cameraActor, ux3d::renderer::GltfRenderLayer* gltfLayer,
                               ux3d::imgui::AnimationPlayer& animPlayer, ux3d::imgui::SceneRevisions& revisions)
{
    if (gltfLayer != nullptr) {
        gltfLayer->setIsolation(nullptr, false);
    }
    gltfModel.reset();
    ux3d::imgui::StatisticsPanel::invalidate();
    auto* scene = gltfModel.create<ux3d::gltf::Scene>();
    if (scene != nullptr) {
        scene->setName("Scene");
        gltfModel.setDefaultScene(*scene);
    }
    activeScene = scene;
    selection = ux3d::imgui::SelectionState();
    currentFilePath = "untitled.glb";
    if (gltfLayer != nullptr) {
        gltfLayer->updateGltf(gltfModel);
        if (activeScene != nullptr) {
            gltfLayer->updateScene(gltfModel, *activeScene);
        }
    }
    animPlayer.reset();
    animPlayer.setModel(gltfModel, gltfLayer);
    if (activeScene != nullptr) {
        cameraActor.fitCameraToScene(*activeScene, &gltfModel);
    }
    revisions.mark(ux3d::imgui::SceneChange::Topology);
    if (gltfLayer != nullptr) {
        revisions.noteSynced(ux3d::imgui::SceneChange::Topology);
    }
    logMessage("Created new empty glTF", ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
}

bool Application::saveGltfModel(const std::string& filePath, const ux3d::gltf::Gltf& gltfModel,
                                std::string& currentFilePath)
{
    logMessage("Saving file: " + filePath);
    ux3d::gltf::ExportOptions exportOptions;
    bool isGlb = (filePath.size() >= 4 && filePath.substr(filePath.size() - 4) == ".glb");
    exportOptions.fileContainer = isGlb ? ux3d::gltf::FileContainer::Glb : ux3d::gltf::FileContainer::Gltf;
    exportOptions.conversionUsage = ux3d::gltf::ConversionUsage::ValidGltf;

    ux3d::filesystem::File file = ux3d::imgui::StdFileSystem::instance().toFile(filePath.c_str());
    auto result = ux3d::gltf::GltfParsing::toFile(gltfModel, file, exportOptions);
    if (result.derived == ux3d::gltf::GltfToFileResult::Success) {
        currentFilePath = filePath;
        ux3d::imgui::StatisticsPanel::invalidate();
        logMessage("Saved successfully: " + filePath, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));
        addRecentFile(s_recentFiles, filePath);
        return true;
    } else {
        logMessage("Failed to save: " + filePath, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        return false;
    }
}

ux3d::filesystem::Path Application::resolveRendererResourcesDir()
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    if (fs.isFile(fs.toPath("resources/shaders/primitive.vert")) || fs.isFile(fs.toPath("shaders/primitive.vert"))) {
        return fs.toPath("resources");
    }

    auto exeDir = fs.getExecutableDirPath();
    if (fs.isFile(exeDir / "resources" / "shaders" / "primitive.vert")) {
        return exeDir / "resources";
    }
    if (fs.isFile(exeDir / "OpenGL-Renderer" / "resources" / "shaders" / "primitive.vert")) {
        return exeDir / "OpenGL-Renderer" / "resources";
    }

    auto compileTimePath = fs.toPath(UX3D_RENDERER_RESOURCES_DIR);
    if (fs.isFile(compileTimePath / "shaders" / "primitive.vert")) {
        return compileTimePath;
    }

    auto cwdPath = fs.toPath(".");
    if (fs.isFile(cwdPath / "resources" / "shaders" / "primitive.vert")) {
        return cwdPath / "resources";
    }
    if (fs.isFile(cwdPath / "OpenGL-Renderer" / "resources" / "shaders" / "primitive.vert")) {
        return cwdPath / "OpenGL-Renderer" / "resources";
    }

    return fs.toPath("resources");
}

ux3d::filesystem::Path Application::resolveEnvironmentPath(const ux3d::filesystem::Path& rendererRes)
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    const std::string fileName = ux3d::imgui::kDefaultEnvironmentFile;
    auto env1 = rendererRes / "environments" / fileName.c_str();
    if (fs.isFile(env1)) return env1;

    auto env2 = fs.toPath(("resources/environments/" + fileName).c_str());
    if (fs.isFile(env2)) return env2;

    auto env3 = fs.toPath(("environments/" + fileName).c_str());
    if (fs.isFile(env3)) return env3;

    auto exeDir = fs.getExecutableDirPath();
    auto env4 = exeDir / "resources" / "environments" / fileName.c_str();
    if (fs.isFile(env4)) return env4;

    return env1;
}

ux3d::filesystem::Path Application::resolveHdrPath(const std::string& hdrPath,
                                                   const ux3d::filesystem::Path& rendererRes)
{
    if (hdrPath.empty() || hdrPath == ux3d::imgui::kDefaultEnvironmentFile) {
        return resolveEnvironmentPath(rendererRes);
    }
    return ux3d::imgui::StdFileSystem::instance().toPath(hdrPath.c_str());
}

std::string Application::resolveSampleModelPath()
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    auto p1 = fs.toPath("resources/sampleGltf/HelloCube.glb");
    if (fs.isFile(p1)) return p1.toString().c_str();

    auto p2 = fs.toPath("sampleGltf/HelloCube.glb");
    if (fs.isFile(p2)) return p2.toString().c_str();

    auto exeDir = fs.getExecutableDirPath();
    auto p3 = exeDir / "resources" / "sampleGltf" / "HelloCube.glb";
    if (fs.isFile(p3)) return p3.toString().c_str();

    return "resources/sampleGltf/HelloCube.glb";
}

std::vector<const ux3d::gltf::Node*> Application::collectSceneCameraNodes(const ux3d::gltf::Scene* scene)
{
    std::vector<const ux3d::gltf::Node*> cameraNodes;
    if (scene != nullptr) {
        std::function<void(const ux3d::gltf::Node*)> collect = [&](const ux3d::gltf::Node* n) {
            if (!n) return;
            if (n->getCamera() != nullptr) {
                cameraNodes.push_back(n);
            }
            for (const auto* ch : n->getChildren()) {
                collect(ch);
            }
        };
        for (const auto* rootNode : scene->getNodes()) {
            collect(rootNode);
        }
    }
    return cameraNodes;
}

void Application::synchronizeScene(FrameContext& frame)
{
    const auto dirtySyncStart = ux3d::imgui::profiling::Clock::now();
    if (animationPlayer.isPlaying()) {
        sceneRevisions.mark(ux3d::imgui::SceneChange::AnimationTime);
    }

    // Kinds in kRendererSyncChanges run the full rebuild below. Selection and
    // ViewCamera need no renderer work (P4.2); AnimationTime alone only
    // re-applies the animation pose (P4.3), since the player already
    // evaluated the clips and no model data changed.
    // A model load synchronizes the renderer itself and notes its Topology
    // revision as synced, so that revision alone does not rebuild again.
    const uint32_t pendingChanges = sceneRevisions.pendingMask();
    const bool rendererSyncPending =
        s_forceFullSync ? pendingChanges != 0
                        : (sceneRevisions.unsyncedPendingMask() & ux3d::imgui::kRendererSyncChanges) != 0;
    const bool animationApplyPending =
        !rendererSyncPending &&
        (pendingChanges & ux3d::imgui::sceneChangeBit(ux3d::imgui::SceneChange::AnimationTime)) != 0;
    if (pendingChanges != 0 && !rendererSyncPending) {
        ++skippedSceneSyncCount;
    }
    if (animationApplyPending && gltfLayer != nullptr) {
        ++animationOnlySyncCount;
        const auto stageStart = ux3d::imgui::profiling::Clock::now();
        gltfLayer->applyAnimationTime(animationPlayer.getCurrentTime());
        if (s_profilingEnabled) {
            ++ux3d::imgui::profiling::frameCounters().applyAnimationCalls;
            const auto& animationProfile = gltfLayer->getLastAnimationProfile();
            frame.sample.stages.push_back({"animation.channels", animationProfile.channelsMs});
            frame.sample.stages.push_back({"animation.transforms", animationProfile.transformsMs});
            frame.sample.stages.push_back({"animation.skins", animationProfile.skinsMs});
        }
        frame.markStage("scene.applyAnimation", stageStart);
    }
    if (rendererSyncPending && gltfLayer != nullptr) {
        ux3d::imgui::profiling::countDirtySync();
        ++sceneSyncCount;
        auto stageStart = ux3d::imgui::profiling::Clock::now();
        gltfLayer->updateGltf(gltfModel);
        if (s_profilingEnabled) {
            ++ux3d::imgui::profiling::frameCounters().updateGltfCalls;
            ++ux3d::imgui::profiling::frameCounters().cacheRebuilds;
        }
        frame.markStage("scene.updateGltf", stageStart);

        if (activeScene != nullptr) {
            stageStart = ux3d::imgui::profiling::Clock::now();
            gltfLayer->updateScene(gltfModel, *activeScene);
            if (s_profilingEnabled) {
                ++ux3d::imgui::profiling::frameCounters().updateSceneCalls;
                ++ux3d::imgui::profiling::frameCounters().cacheRebuilds;
            }
            frame.markStage("scene.updateScene", stageStart);
        }

        stageStart = ux3d::imgui::profiling::Clock::now();
        gltfLayer->applyAnimationTime(animationPlayer.getCurrentTime());
        if (s_profilingEnabled) {
            ++ux3d::imgui::profiling::frameCounters().applyAnimationCalls;
            const auto& animationProfile = gltfLayer->getLastAnimationProfile();
            frame.sample.stages.push_back({"animation.channels", animationProfile.channelsMs});
            frame.sample.stages.push_back({"animation.transforms", animationProfile.transformsMs});
            frame.sample.stages.push_back({"animation.skins", animationProfile.skinsMs});
        }
        frame.markStage("scene.applyAnimation", stageStart);
    }
    frame.markStage("scene.dirtySync", dirtySyncStart);
    if (s_profilingEnabled) {
        // Traversals happen in the sync point and in the player's own applies.
        static ux3d::renderer::TransformHierarchyNode::TraverseStats s_lastTraverse;
        const auto& traverse = ux3d::renderer::TransformHierarchyNode::stats();
        ux3d::imgui::profiling::frameCounters().transformNodesVisited = traverse.visited - s_lastTraverse.visited;
        ux3d::imgui::profiling::frameCounters().transformNodesRecomputed =
            traverse.recomputed - s_lastTraverse.recomputed;
        s_lastTraverse = traverse;
    }

    const uint32_t frameSceneChanges = sceneRevisions.consumePending();
    if (s_profilingEnabled) {
        ux3d::imgui::profiling::frameCounters().sceneChangeMask = frameSceneChanges;
        ux3d::imgui::profiling::frameCounters().revisionMarks = sceneRevisions.markCount() - lastRevisionMarkCount;
    }
    lastRevisionMarkCount = sceneRevisions.markCount();
    lastFrameSceneChanges = frameSceneChanges;
}

} // namespace ux3d::imgui::application
