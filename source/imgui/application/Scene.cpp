// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "core/LoadProgress.h"
#include "imgui/AppSettings.h"
#include "imgui/ElementInspector.h"
#include "imgui/StatisticsPanel.h"
#include "imgui/GltfValidator.h"
#include "imgui/MemoryInfo.h"
#include "imgui/RenderQuality.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <portable-file-dialogs.h>
#include <imgui_internal.h>
#include <future>
#include <thread>
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
                                std::vector<ux3d::imgui::SceneEnvironmentState>& sceneEnvironments, bool texturesOnly, bool withoutTextures)
{
    // Menu commands only enqueue. The next outer frame owns the load, so neither
    // ImGui nor the model/renderer is ever re-entered from an unfinished menu frame.
    if (frameInProgress || loadingActive) {
        queuedLoadPath = filePath;
        queuedLoadWithoutTextures = withoutTextures;
        return true;
    }
    const auto wholeLoadStart = ux3d::core::loading::Clock::now();
    bool loadSucceeded = false;
    if (!texturesOnly) pendingLoadFirstFrame = false;
    loadingActive = true;
    ImGuiContext* mainContext = ImGui::GetCurrentContext();
    const ImGuiStyle loadingStyle = ImGui::GetStyle();
    // A fresh model load gets the dedicated first-screen layout and its own context:
    // keeping the progress text out of the main dynamic font atlas prevents warming
    // glyphs that would change atlas UV rounding in deterministic UI captures.
    // A texture-only refresh keeps the running UI context and paints over the frame
    // that is already on screen, so the user never leaves the current layout.
    ImGuiContext* loadingContext = nullptr;
    if (!texturesOnly) {
        ImGui_ImplGlfw_Shutdown();
        loadingContext = ImGui::CreateContext();
        ImGui::SetCurrentContext(loadingContext);
        ImGui::GetStyle() = loadingStyle;
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 410");
    }
    struct LoadGuard {
        bool& active;
        GLFWwindow* window;
        ImGuiContext* mainContext;
        ImGuiContext* loadingContext;
        bool vsync;
        ux3d::core::loading::Clock::time_point start;
        bool& succeeded;
        bool logTiming;
        ~LoadGuard() {
            ux3d::core::loading::state.yield = {};
            glfwSwapInterval(vsync ? 1 : 0);
            if (loadingContext != nullptr) {
                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext(loadingContext);
                ImGui::SetCurrentContext(mainContext);
                ImGui_ImplGlfw_InitForOpenGL(window, true);
                ImGui::GetIO().ClearInputKeys();
                ImGui::GetIO().ClearInputMouse();
            }
            active = false;
            auto& progress = ux3d::core::loading::state;
            progress.wallMilliseconds = std::chrono::duration<double, std::milli>(ux3d::core::loading::Clock::now()-start).count();
            if (succeeded && logTiming) {
                ux3d::core::logInfo("[Loading] finished in %.3f s (images %.3f s, tangents %.3f s, GL submission %.3f s, progress UI %.3f s)",
                    progress.wallMilliseconds/1000, progress.milliseconds[ux3d::core::loading::Images]/1000,
                    progress.milliseconds[ux3d::core::loading::Tangents]/1000,
                    progress.milliseconds[ux3d::core::loading::Upload]/1000, progress.uiMilliseconds/1000);
            }
        }
    } guard{loadingActive, window, mainContext, loadingContext, s_vsyncEnabled, wholeLoadStart,
        loadSucceeded, !ux3d::imgui::RenderQuality::legacyCaptureUi()};
    // Loading redraws must not pace imports/uploads at the monitor's refresh rate.
    glfwSwapInterval(0);
    try {
    logMessage("Opening file: " + filePath);
    ux3d::core::loading::reset();
    auto& progress = ux3d::core::loading::state;
    progress.imagesOmitted = withoutTextures;
    if (!texturesOnly && gltfLayer) {
        if (commandGenerator) commandGenerator->getCommandMapping().clearCommandCache();
        gltfLayer->releaseModelTextures();
    }
    auto memory = ux3d::imgui::queryMemoryInfo();
    progress.memoryAware = true;
    progress.textureRefresh = texturesOnly;
    // The old GL textures are replaced while this refresh runs. Count their payload as
    // reclaimable; otherwise the estimate sees a full VRAM budget and needlessly
    // shrinks the newly requested, larger textures.
    if (texturesOnly && gltfLayer != nullptr) {
        uint64_t reclaimable = 0;
        for (const auto& entry : gltfLayer->m_textureMapping.map) {
            if (entry.second != nullptr) reclaimable += entry.second->residentBytes;
        }
        if (memory.gpuKnown) memory.availableVram += reclaimable;
        memory.availableRam += reclaimable;
    }
    const uint64_t ramBudget = memory.availableRam ? memory.availableRam/3 : 512ull*1024*1024;
    progress.textureBudget = memory.gpuKnown ? std::min(ramBudget, memory.availableVram*7/10) : ramBudget;
    if (progress.textureBudget == 0) progress.textureBudget = 16ull*1024*1024;
    GLint maxTexture = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
    progress.maxTextureSize = static_cast<uint32_t>(std::max(64, maxTexture));
    loadedTextureCap = ux3d::imgui::RenderQuality::textureMaxSize();
    progress.requestedTextureLimit = loadedTextureCap;
    progress.unlimitedTextures = ux3d::imgui::RenderQuality::unlimitedTextures();
    if (loadedTextureCap) progress.maxTextureSize = std::min(progress.maxTextureSize.load(), loadedTextureCap);
    progress.availableRam = memory.availableRam;
    progress.availableVram = memory.availableVram;
    if (s_decodeBudgetMiB < 0) {
        ux3d::imageformats::setDecodeMemoryBudget(std::min<uint64_t>(4096ull*1024*1024,
            memory.availableRam ? std::max<uint64_t>(32ull*1024*1024, memory.availableRam/4) : 512ull*1024*1024));
    }
    logMessage(std::string("[Loading] VRAM source: ") + memory.gpuSource +
        "; free " + std::to_string(memory.availableVram/(1024*1024)) + " MiB; RAM free " +
        std::to_string(memory.availableRam/(1024*1024)) + " MiB");
    const auto loadingStart = ux3d::core::loading::Clock::now();
    auto lastProgress = loadingStart - std::chrono::seconds(1);
    auto lastEvents = lastProgress;
    auto lastMemory = lastProgress;
    auto pump = [&]() {
        const auto now = ux3d::core::loading::Clock::now();
        // Poll independently of presentation: responsive window/menu input does not
        // require submitting a full GL frame for each upload checkpoint.
        if (now-lastEvents >= std::chrono::milliseconds(8)) {
            glfwPollEvents();
            lastEvents = now;
        }
        if (now-lastProgress < std::chrono::milliseconds(50)) return;
        lastProgress = now;
        const auto uiStart = ux3d::core::loading::Clock::now();
        // ImGui's renderer restores program/VAO/buffer/texture state. Preserve the
        // framebuffer and viewport too, as this can run between GL upload items.
        GLint drawFbo = 0, readFbo = 0, viewport[4]{};
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
        glGetIntegerv(GL_VIEWPORT, viewport);
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        const int stage = progress.stage;
        const unsigned total = progress.total, completed = progress.completed;
        const float fraction = total ? std::min(1.0f, float(completed)/total) : 0.0f;
        if (texturesOnly) {
            // Keep the last presented frame visible and only replace the status bar
            // strip. The user stays in the current layout while images re-upload.
            const auto* vp = ImGui::GetMainViewport();
            ImDrawList* background = ImGui::GetBackgroundDrawList();
            const ImVec2 imageMin = vp->Pos;
            const ImVec2 imageMax(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y);
            if (textureRefreshSnapshot != 0) {
                // glCopyTexImage2D stores the default framebuffer bottom-up.
                background->AddImage(static_cast<ImTextureID>(textureRefreshSnapshot), imageMin, imageMax,
                                     ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
            } else {
                background->AddRectFilled(imageMin, imageMax, IM_COL32(26, 26, 30, 255));
            }
            const float barHeight = ImGui::GetFrameHeight() + 8.0f;
            ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - barHeight));
            ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, barHeight));
            ImGui::SetNextWindowViewport(vp->ID);
            const ImGuiWindowFlags barFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 2.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.24f, 1.0f));
            ImGui::Begin("##TextureRefreshStatus", nullptr, barFlags);
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Refreshing textures for %s",
                        ux3d::imgui::RenderQuality::profileName(ux3d::imgui::RenderQuality::selectedProfile()));
            ImGui::SameLine();
            char countText[32];
            std::snprintf(countText, sizeof(countText), "%u/%u", completed, total);
            ImGui::ProgressBar(fraction, ImVec2(200.0f, 0.0f), countText);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", ux3d::core::loading::names[stage]);
            if (progress.textureLimit == 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("Unlimited");
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        } else {
            bool open = false, openWithoutTextures = false;
            if (ImGui::BeginMainMenuBar()) {
                if (ImGui::BeginMenu("File")) {
                    open = ImGui::MenuItem("Open ... (queue after this load)", "Ctrl+O");
                    openWithoutTextures = ImGui::MenuItem("Open without texture ... (queue after this load)");
                    if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(window, GLFW_TRUE);
                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }
            if (open || openWithoutTextures || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))) {
                const auto paths = pfd::open_file("Queue glTF / GLB", "", {"glTF files", "*.gltf *.glb"}).result();
                if (!paths.empty()) { queuedLoadPath = paths[0]; queuedLoadWithoutTextures = openWithoutTextures; }
            }
            const auto* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x*.5f, vp->WorkPos.y + vp->WorkSize.y*.5f), ImGuiCond_Always, ImVec2(.5f,.5f));
            ImGui::SetNextWindowSize(ImVec2(std::min(580.0f, vp->WorkSize.x-20), 0));
            ImGui::Begin("Loading model", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
            if (!queuedLoadTestUsed && stage == ux3d::core::loading::Images) {
                if (const char* path = std::getenv("GLIDER_TEST_QUEUE_LOAD")) {
                    queuedLoadPath = path;
                    queuedLoadWithoutTextures = false;
                    queuedLoadTestUsed = true;
                    logMessage("[Loading] queued a model during image decode: " + queuedLoadPath);
                }
            }
            ImGui::TextWrapped("%s", filePath.c_str());
            if (withoutTextures) ImGui::TextDisabled("Textures intentionally omitted; material values retained.");
            ImGui::Text("%s: %u / %u", ux3d::core::loading::names[stage], completed, total);
            ImGui::ProgressBar(fraction, ImVec2(-1, 0));
            ImGui::Text("Elapsed: %.1f seconds", std::chrono::duration<double>(now-loadingStart).count());
            if (progress.reductions) {
                if (progress.textureLimit) {
                    ImGui::TextWrapped("Texture quality: %u textures reduced (maximum %u). Source files are unchanged; higher presets restore resolution when memory permits.", progress.reductions.load(), progress.textureLimit.load());
                } else {
                    ImGui::TextWrapped("Texture quality: %u textures reduced by allocation fallbacks. The active preset is unlimited; source files are unchanged.", progress.reductions.load());
                }
            }
            if (!queuedLoadPath.empty()) ImGui::TextWrapped("Next: %s", queuedLoadPath.c_str());
            ImGui::End();
        }
        ImGui::Render();
        int w = 0, h = 0; glfwGetFramebufferSize(window, &w, &h);
        glBindFramebuffer(GL_FRAMEBUFFER, 0); glViewport(0,0,w,h);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(.10f,.10f,.12f,1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!loadingCapturePath.empty() && stage == ux3d::core::loading::Images && w > 0 && h > 0) {
            std::vector<uint8_t> pixels(size_t(w)*h*4);
            glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            saveImage(loadingCapturePath.c_str(), w, h, pixels.data());
            loadingCapturePath.clear();
        }
        glfwSwapBuffers(window);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo); glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        ++progress.progressFrames;
        if (now-lastMemory >= std::chrono::milliseconds(250)) {
            const auto sampled = ux3d::imgui::queryMemoryInfo();
            progress.peakGpuUsage = std::max(progress.peakGpuUsage.load(), sampled.gpuUsage);
            lastMemory = now;
        }
        progress.uiMilliseconds += std::chrono::duration<double, std::milli>(ux3d::core::loading::Clock::now()-uiStart).count();
    };
    progress.yield = pump;
    pump();
    ux3d::imgui::StatisticsPanel::invalidate();
    // A2.1: the buffer belongs to the loaded file's decal materials.
    if (!texturesOnly) releaseDecalBuffer();
    ux3d::imgui::AppSettings::instance().setCurrentModelPath(filePath);
    ux3d::gltf::ImportOptions importOptions;
    importOptions.loadImages = !withoutTextures;
    if (withoutTextures) logMessage("[Loading] opening without textures: image reads, decode and GPU texture uploads skipped");
    auto& startupProfile = ux3d::imgui::profiling::startupProfile();
    const auto loadStart = ux3d::imgui::profiling::Clock::now();
    ux3d::filesystem::File file = ux3d::imgui::StdFileSystem::instance().toFile(filePath.c_str());

    ux3d::imgui::ElementInspector::clearPreviewCache();
    if (!texturesOnly && gltfLayer != nullptr) {
        for (auto& viewport : viewports) viewport->settings.materialExtensions.disabled.clear();
        gltfLayer->materialExtensionOptions.disabled.clear();
        gltfLayer->setIsolation(nullptr, false); // the isolated node pointer is about to dangle
    }
    if (!texturesOnly) {
        gltfModel.reset();
        activeScene = nullptr;
        selection = ux3d::imgui::SelectionState();
        animPlayer.reset();
    }
    // A2.2: the cached MikkTSpace tangents belong to the previous model's primitives.
    if (!texturesOnly && gltfLayer != nullptr) {
        gltfLayer->clearGeneratedTangents();
    }
    s_materialEditTarget = nullptr;
    ux3d::imageformats::resetDecodeBudgetStats();
    auto parsing = std::async(std::launch::async, [&]() {
        if (texturesOnly) {
            const auto decoded = gltfModel.loadBinaryDataToMemory({}, {}, true);
            return decoded ? ux3d::gltf::GltfFromFileResult(ux3d::gltf::GltfFromFileResult::Success)
                : ux3d::gltf::GltfFromFileResult(ux3d::gltf::GltfFromFileError::ReadFileFailure);
        }
        const auto imported = ux3d::gltf::GltfParsing::fromFile(gltfModel, file, importOptions);
        if (imported == ux3d::gltf::GltfFromFileResult::Success && gltfLayer && !withoutTextures)
            gltfLayer->prepareGeneratedTangents(gltfModel);
        return imported;
    });
    while (parsing.wait_for(std::chrono::milliseconds(4)) != std::future_status::ready) pump();
    const auto result = parsing.get();
    if (glfwWindowShouldClose(window)) return false;
    if (texturesOnly && result == ux3d::gltf::GltfFromFileResult::Success) {
        ux3d::core::loading::begin(ux3d::core::loading::Upload, static_cast<uint32_t>(gltfModel.getImages().size()));
        if (gltfLayer) gltfLayer->updateMaterialStates(gltfModel);
        revisions.mark(ux3d::imgui::SceneChange::ViewCamera);
        for (auto& vp : viewports) vp->antiAliasing.resetHistory();
        logMessage("[Loading] texture quality refreshed from retained source images");
        loadSucceeded = true;
        return true;
    }
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
        if (const char* fault = std::getenv("GLIDER_TEST_LOAD_FAILURE"); fault && std::string(fault) == "upload") throw std::bad_alloc();
        ux3d::core::loading::begin(ux3d::core::loading::Upload,
            static_cast<uint32_t>(gltfModel.getAccessors().size() + (withoutTextures ? 0 : gltfModel.getImages().size())));
        if (gltfLayer != nullptr) {
            gltfLayer->updateGltf(gltfModel);
            if (activeScene != nullptr) {
                gltfLayer->updateScene(gltfModel, *activeScene);
                gltfLayer->applyAnimationTime(0.0f);
            }
        }
        startupProfile.rendererUploadMs = ux3d::imgui::profiling::elapsedMs(uploadStart);
        startupProfile.residentModelTextureBytes = 0;
        startupProfile.encodedImageBytes = startupProfile.decodedImageBytes = 0;
        if (gltfLayer) for (const auto& entry : gltfLayer->m_textureMapping.map) {
            if (entry.second) startupProfile.residentModelTextureBytes += entry.second->residentBytes;
        }
        for (const auto& image : gltfModel.getImages()) {
            startupProfile.encodedImageBytes += image.getEncodedBytes()->size();
            startupProfile.decodedImageBytes += image.getData().getPixels().size();
        }
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
        // A newly loaded model snaps auto-exposure to its own metered level. The first load
        // keeps the startup behaviour (--exposure with --auto-exposure adapts from that EV).
        for (auto& vp : viewports) {
            if (vp->hasCachedImage) vp->antiAliasing.resetExposure();
        }

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
        ux3d::core::loading::begin(ux3d::core::loading::Validation, 1);
        auto validation = std::async(std::launch::async, [&]() {
            return ux3d::imgui::GltfValidator::instance().validate(filePath, gltfModel);
        });
        while (validation.wait_for(std::chrono::milliseconds(4)) != std::future_status::ready) pump();
        auto valReport = validation.get();
        progress.completed = 1;
        { std::lock_guard lock(progress.mutex);
          progress.milliseconds[ux3d::core::loading::Validation] = ux3d::imgui::profiling::elapsedMs(validationStart); }
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
        loadSucceeded = true;
        pendingLoadFirstFrame = true;
        pendingLoadStarted = wholeLoadStart;
        return true;
    } else {
        logMessage("Failed to load: " + filePath, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        throw std::runtime_error("model import did not complete");
    }
    } catch (const std::exception& error) {
        logMessage(std::string("[Loading] load stopped safely: ") + error.what(), ImVec4(1,.5f,.2f,1));
        // Never leave stale node/material pointers from the old or partial scene.
        selection = ux3d::imgui::SelectionState();
        activeScene = nullptr;
        animPlayer.reset();
        for (auto& viewport : viewports) {
            viewport->isAttachedToCamera = false;
            viewport->attachedCameraNode = nullptr;
            viewport->antiAliasing.resetHistory();
        }
        try {
            if (!viewports.empty()) newEmptyGltf(gltfModel, activeScene, selection, currentFilePath,
                viewports.front()->cameraActor, gltfLayer, animPlayer, revisions);
        } catch (...) {
            // Even a minimal replacement can fail if the driver lost its context.
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        return false;
    }
}

void Application::newEmptyGltf(ux3d::gltf::Gltf& gltfModel, ux3d::gltf::Scene*& activeScene,
                               ux3d::imgui::SelectionState& selection, std::string& currentFilePath,
                               ux3d::editor::CameraActor& cameraActor, ux3d::renderer::GltfRenderLayer* gltfLayer,
                               ux3d::imgui::AnimationPlayer& animPlayer, ux3d::imgui::SceneRevisions& revisions)
{
    if (gltfLayer != nullptr) {
        for (auto& viewport : viewports) viewport->settings.materialExtensions.disabled.clear();
        gltfLayer->materialExtensionOptions.disabled.clear();
        gltfLayer->setIsolation(nullptr, false);
    }
    gltfModel.reset();
    if (gltfLayer != nullptr) {
        gltfLayer->clearGeneratedTangents();
    }
    s_materialEditTarget = nullptr;
    ux3d::imgui::StatisticsPanel::invalidate();
    releaseDecalBuffer();
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
    const uint32_t rendererChanges = s_forceFullSync
        ? pendingChanges
        : (sceneRevisions.unsyncedPendingMask() & ux3d::imgui::kRendererSyncChanges);
    const bool rendererSyncPending = rendererChanges != 0;
    // A2.2: a material-only edit refreshes material state and uniforms. The full sync path
    // re-uploads every accessor (m_bufferMapping) and rebuilds the meshes, which is not
    // needed for an Inspector colour/factor change. --force-full-sync keeps the old path
    // as an A/B reference for the per-edit cost measurement.
    const uint32_t materialSyncMask =
        ux3d::imgui::sceneChangeBit(ux3d::imgui::SceneChange::Material) |
        ux3d::imgui::sceneChangeBit(ux3d::imgui::SceneChange::Extension);
    const bool materialOnlySync = rendererSyncPending && !s_forceFullSync
        && (rendererChanges & ~materialSyncMask) == 0;
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
        if (materialOnlySync) {
            const bool meshUpdateNeeded = gltfLayer->updateMaterialStates(gltfModel);
            frame.markStage("scene.updateMaterials", stageStart);
            if (meshUpdateNeeded && activeScene != nullptr) {
                // A normal-map UV set changed: the tangent buffer for the new set is
                // generated (cache key miss) and the meshes must be re-bound to it.
                stageStart = ux3d::imgui::profiling::Clock::now();
                gltfLayer->updateScene(gltfModel, *activeScene);
                if (s_profilingEnabled) {
                    ++ux3d::imgui::profiling::frameCounters().updateSceneCalls;
                    ++ux3d::imgui::profiling::frameCounters().cacheRebuilds;
                }
                frame.markStage("scene.updateScene", stageStart);
            }
        } else {
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
