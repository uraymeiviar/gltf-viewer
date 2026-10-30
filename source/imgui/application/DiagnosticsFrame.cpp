// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// End-of-frame diagnostics: finishFrame (frame dumps, viewport captures, profile / benchmark reports, the agent
// pump and UI scripts) and the scenario report writer.

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
namespace {
std::string effectiveAAName(const ViewportSettingsState& settings)
{
    const char* msaaNames[] = { "none", "msaa2", "msaa4", "msaa8" };
    const char* postNames[] = { "none", "fxaa", "taa" };
    const int msaa = settings.effectiveMsaaIdx();
    const int post = settings.effectivePostAAIdx();
    if (msaa == 0) return postNames[post];
    if (post == 0) return msaaNames[msaa];
    return std::string(msaaNames[msaa]) + "+" + postNames[post];
}
}

bool Application::writeScenarioReport(const std::string& reportPath, const std::string& scenarioName, uint64_t frame,
                                      GLFWwindow* window, const std::string& modelPath, bool modelLoaded,
                                      ux3d::gltf::Gltf& gltfModel, ux3d::gltf::Scene* activeScene,
                                      const ux3d::imgui::ViewportInstance& viewport,
                                      const ux3d::imgui::SceneEnvironmentState& environment,
                                      const std::string& environmentPath, bool environmentLoaded,
                                      const ux3d::imgui::AnimationPlayer& animationPlayer,
                                      const ux3d::imgui::SelectionState& selection)
{
    std::error_code directoryError;
    const std::filesystem::path reportFilePath = std::filesystem::u8path(reportPath);
    if (reportFilePath.has_parent_path()) {
        std::filesystem::create_directories(reportFilePath.parent_path(), directoryError);
    }

    std::ofstream out(reportFilePath, std::ios::binary);
    if (!out.is_open()) return false;

    int clientWidth = 0;
    int clientHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    float contentScaleX = 1.0f;
    float contentScaleY = 1.0f;
    glfwGetWindowSize(window, &clientWidth, &clientHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glfwGetWindowContentScale(window, &contentScaleX, &contentScaleY);

    uint64_t modelHash = 0;
    uint64_t modelBytes = 0;
    const bool modelHashed = hashFileFnv1a64(modelPath, modelHash, modelBytes);

    uint64_t environmentHash = 0;
    uint64_t environmentBytes = 0;
    const bool environmentHashed = hashFileFnv1a64(environmentPath, environmentHash, environmentBytes);

    const ux3d::imgui::ViewportSettingsState& settings = viewport.settings;
    const ux3d::renderer::Camera& camera = viewport.rendererCamera;

    ux3d::gltf::Extends sceneExtends;
    bool hasSceneExtends = false;
    if (activeScene != nullptr && !activeScene->getNodes().empty()) {
        sceneExtends = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
        hasSceneExtends = true;
    }

    const char* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glslVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));

    out << std::fixed << std::setprecision(9);
    out << "{\n";
    out << "  \"schema\": \"glider.scenario.v1\",\n";
    out << "  \"scenarioName\": " << jsonEscape(scenarioName) << ",\n";
    out << "  \"frame\": " << frame << ",\n";
    out << "  \"isolated\": " << (s_isolatedState ? "true" : "false") << ",\n";
    out << "  \"layout\": " << jsonEscape(s_isolatedState ? "default-forced" : "persisted") << ",\n";
    out << "  \"vsync\": " << (s_vsyncEnabled ? "true" : "false") << ",\n";

    out << "  \"application\": {\n";
    out << "    \"name\": \"Glider\",\n";
    out << "    \"build\": " << jsonEscape(std::string(__DATE__) + " " + __TIME__) << ",\n";
    out << "    \"openglVendor\": " << jsonEscape(glVendor != nullptr ? glVendor : "") << ",\n";
    out << "    \"openglRenderer\": " << jsonEscape(glRenderer != nullptr ? glRenderer : "") << ",\n";
    out << "    \"openglVersion\": " << jsonEscape(glVersion != nullptr ? glVersion : "") << ",\n";
    out << "    \"glslVersion\": " << jsonEscape(glslVersion != nullptr ? glslVersion : "") << "\n";
    out << "  },\n";

    out << "  \"window\": {\n";
    out << "    \"clientWidth\": " << clientWidth << ",\n";
    out << "    \"clientHeight\": " << clientHeight << ",\n";
    out << "    \"framebufferWidth\": " << framebufferWidth << ",\n";
    out << "    \"framebufferHeight\": " << framebufferHeight << ",\n";
    out << "    \"contentScaleX\": ";
    writeJsonNumber(out, contentScaleX);
    out << ",\n";
    out << "    \"contentScaleY\": ";
    writeJsonNumber(out, contentScaleY);
    out << "\n";
    out << "  },\n";

    out << "  \"viewport\": {\n";
    out << "    \"id\": " << viewport.id << ",\n";
    out << "    \"name\": " << jsonEscape(viewport.name) << ",\n";
    out << "    \"width\": " << viewport.sceneFbo.getWidth() << ",\n";
    out << "    \"height\": " << viewport.sceneFbo.getHeight() << ",\n";
    out << "    \"samples\": " << viewport.sceneFbo.getSamples() << ",\n";
    out << "    \"pickingWidth\": " << viewport.pickingFbo.getWidth() << ",\n";
    out << "    \"pickingHeight\": " << viewport.pickingFbo.getHeight() << "\n";
    out << "  },\n";

    out << "  \"model\": {\n";
    out << "    \"path\": " << jsonEscape(modelPath) << ",\n";
    out << "    \"loaded\": " << (modelLoaded ? "true" : "false") << ",\n";
    out << "    \"hashScope\": \"container-file\",\n";
    out << "    \"hashAlgorithm\": \"fnv1a64\",\n";
    out << "    \"bytes\": " << modelBytes << ",\n";
    out << "    \"hash\": ";
    if (modelHashed) {
        out << "\"0x" << std::hex << std::setw(16) << std::setfill('0') << modelHash << std::dec << std::setfill(' ')
            << "\"";
    } else {
        out << "null";
    }
    out << "\n";
    out << "  },\n";

    out << "  \"scene\": {\n";
    out << "    \"nodes\": " << gltfModel.getElements<ux3d::gltf::Node>().size() << ",\n";
    out << "    \"meshes\": " << gltfModel.getElements<ux3d::gltf::Mesh>().size() << ",\n";
    out << "    \"materials\": " << gltfModel.getElements<ux3d::gltf::Material>().size() << ",\n";
    out << "    \"boundsMin\": ";
    if (hasSceneExtends)
        writeJsonVec3(out, ux3d::math::vec3(sceneExtends.min));
    else
        out << "null";
    out << ",\n";
    out << "    \"boundsMax\": ";
    if (hasSceneExtends)
        writeJsonVec3(out, ux3d::math::vec3(sceneExtends.max));
    else
        out << "null";
    out << "\n";
    out << "  },\n";

    out << "  \"camera\": {\n";
    out << "    \"type\": " << jsonEscape(cameraTypeName(camera.getType())) << ",\n";
    out << "    \"eye\": ";
    writeJsonVec3(out, viewport.cameraActor.getEyePosition());
    out << ",\n";
    out << "    \"target\": ";
    writeJsonVec3(out, viewport.cameraActor.getLookTarget());
    out << ",\n";
    out << "    \"up\": ";
    writeJsonVec3(out, ux3d::math::vec3(camera.getUp()));
    out << ",\n";
    out << "    \"yawRadians\": ";
    writeJsonNumber(out, viewport.cameraActor.getRotationX());
    out << ",\n";
    out << "    \"pitchRadians\": ";
    writeJsonNumber(out, viewport.cameraActor.getRotationY());
    out << ",\n";
    out << "    \"zoom\": ";
    writeJsonNumber(out, viewport.cameraActor.getZoom());
    out << ",\n";
    out << "    \"yfovRadians\": ";
    writeJsonNumber(out, camera.getYFoV());
    out << ",\n";
    out << "    \"aspectRatio\": ";
    writeJsonNumber(out, camera.getAspectRatio());
    out << ",\n";
    out << "    \"zNear\": ";
    writeJsonNumber(out, camera.getZNearPlane());
    out << ",\n";
    out << "    \"zFar\": ";
    writeJsonNumber(out, camera.getZFarPlane());
    out << ",\n";
    out << "    \"orthographicScaling\": ";
    writeJsonNumber(out, camera.getOrthographicScaling());
    out << ",\n";
    out << "    \"matrixOrder\": \"column-major\",\n";
    out << "    \"view\": ";
    writeJsonMat4(out, camera.getViewMatrix());
    out << ",\n";
    out << "    \"projection\": ";
    writeJsonMat4(out, camera.getProjectionMatrix());
    out << "\n";
    out << "  },\n";

    out << "  \"environment\": {\n";
    out << "    \"path\": " << jsonEscape(environmentPath) << ",\n";
    out << "    \"loaded\": " << (environmentLoaded ? "true" : "false") << ",\n";
    out << "    \"bytes\": " << environmentBytes << ",\n";
    out << "    \"hash\": ";
    if (environmentHashed) {
        out << "\"0x" << std::hex << std::setw(16) << std::setfill('0') << environmentHash << std::dec
            << std::setfill(' ') << "\"";
    } else {
        out << "null";
    }
    out << ",\n";
    out << "    \"uiPath\": " << jsonEscape(environment.currentHdrPath) << ",\n";
    out << "    \"iblEnabled\": " << (environment.iblEnabled ? "true" : "false") << ",\n";
    out << "    \"rotation\": ";
    writeJsonNumber(out, environment.hdrRotation);
    out << ",\n";
    out << "    \"intensity\": ";
    writeJsonNumber(out, environment.hdrIntensity);
    out << ",\n";
    out << "    \"blur\": ";
    writeJsonNumber(out, environment.hdrBlur);
    out << ",\n";
    // A3.1: the effective sun and the sun found in the HDR (the same object get_state returns).
    out << "    \"sun\": " << sunStateJson(environment).dump() << "\n";
    out << "  },\n";

    out << "  \"settings\": {\n";
    out << "    \"antiAliasing\": " << jsonEscape(effectiveAAName(settings)) << ",\n";
    out << "    \"antiAliasingIndex\": " << settings.antiAliasingIdx << ",\n";
    out << "    \"msaaSamples\": " << (settings.effectiveMsaaIdx() == 0 ? 1 : 1 << settings.effectiveMsaaIdx()) << ",\n";
    out << "    \"postAAIndex\": " << settings.effectivePostAAIdx() << ",\n";
    out << "    \"renderModeIndex\": " << settings.renderModeIdx << ",\n";
    out << "    \"toneMappingIndex\": " << ux3d::imgui::effectiveToneMapping(environment) << ",\n";
    out << "    \"exposureEV\": ";
    writeJsonNumber(out, environment.exposureEV);
    out << ",\n";
    out << "    \"showBackground\": " << (environment.showBackground ? "true" : "false") << ",\n";
    out << "    \"showGrid\": " << (settings.showGrid ? "true" : "false") << ",\n";
    out << "    \"forceDoubleSided\": " << (settings.forceDoubleSided ? "true" : "false") << ",\n";
    out << "    \"invertWinding\": " << (settings.invertWinding ? "true" : "false") << ",\n";
    out << "    \"wireframeOverlay\": " << (settings.wireframeOverlay ? "true" : "false") << ",\n";
    out << "    \"wireframeColor\": [";
    writeJsonNumber(out, settings.wireframeColor.r);
    out << ", ";
    writeJsonNumber(out, settings.wireframeColor.g);
    out << ", ";
    writeJsonNumber(out, settings.wireframeColor.b);
    out << ", ";
    writeJsonNumber(out, settings.wireframeColor.a);
    out << "],\n";
    out << "    \"wireframeBlend\": " << static_cast<int>(settings.wireframeBlend) << ",\n";
    out << "    \"showSceneBoundingBox\": " << (settings.showSceneBoundingBox ? "true" : "false") << ",\n";
    out << "    \"showSelectedBoundingBox\": " << (settings.showSelectedBoundingBox ? "true" : "false") << ",\n";
    out << "    \"showNodeCoordinateGizmo\": " << (settings.showNodeCoordinateGizmo ? "true" : "false") << ",\n";
    out << "    \"showOrientationGizmo\": " << (settings.showOrientationGizmo ? "true" : "false") << ",\n";
    out << "    \"gizmoAxisConvention\": " << settings.gizmoAxisConvention << ",\n";
    out << "    \"turntable\": " << (settings.turntable ? "true" : "false") << ",\n";
    out << "    \"turntableSpeed\": ";
    writeJsonNumber(out, settings.turntableSpeed);
    out << ",\n";
    out << "    \"punctualLightsEnabled\": " << (environment.punctualLightsEnabled ? "true" : "false") << ",\n";
    out << "    \"clearColor\": [";
    writeJsonNumber(out, environment.clearColor.r);
    out << ", ";
    writeJsonNumber(out, environment.clearColor.g);
    out << ", ";
    writeJsonNumber(out, environment.clearColor.b);
    out << ", ";
    writeJsonNumber(out, environment.clearColor.a);
    out << "]\n";
    out << "  },\n";

    out << "  \"animation\": {\n";
    out << "    \"time\": ";
    writeJsonNumber(out, animationPlayer.getCurrentTime());
    out << ",\n";
    out << "    \"maxDuration\": ";
    writeJsonNumber(out, animationPlayer.getMaxDuration());
    out << ",\n";
    out << "    \"playing\": " << (animationPlayer.isPlaying() ? "true" : "false") << ",\n";
    out << "    \"activeIndex\": " << animationPlayer.getActiveAnimationIndex() << ",\n";
    out << "    \"looping\": " << (animationPlayer.isLooping() ? "true" : "false") << ",\n";
    out << "    \"loopCount\": " << animationPlayer.getLoopCount() << ",\n";
    out << "    \"playbackSpeed\": ";
    writeJsonNumber(out, animationPlayer.getPlaybackSpeed());
    out << "\n";
    out << "  },\n";

    out << "  \"selection\": {\n";
    out << "    \"type\": " << jsonEscape(selectionTypeName(selection.type)) << ",\n";
    out << "    \"viewportId\": " << selection.selectedViewportId << ",\n";
    out << "    \"nodeId\": ";
    if (selection.selectedNode != nullptr)
        out << selection.selectedNode->m_ID;
    else
        out << "null";
    out << ",\n";
    out << "    \"nodeName\": ";
    if (selection.selectedNode != nullptr && selection.selectedNode->hasName()) {
        out << jsonEscape(selection.selectedNode->getName());
    } else {
        out << "null";
    }
    out << "\n";
    out << "  }\n";
    out << "}\n";

    out.flush();
    return out.good();
}

void Application::finishFrame(FrameContext& frame, int displayW, int displayH)
{
    auto& activeEnvironment = getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
    frame.sample.cpuFrameMs = ux3d::imgui::profiling::elapsedMs(frame.start);
    frame.sample.firstFrame = (frame.number == 1);
    if (s_profilingEnabled) {
        frame.sample.counters = ux3d::imgui::profiling::frameCounters();
    }
    if (frame.sample.firstFrame && (s_profilingEnabled || s_benchmarkEnabled)) {
        s_firstFrameSample = frame.sample;
        s_haveFirstFrameSample = true;
    }
    if (s_profilingEnabled) {
        s_frameSamples.push_back(frame.sample);
        if (s_frameSamples.size() > ux3d::imgui::profiling::kMaxRetainedFrameSamples) {
            s_frameSamples.pop_front();
            ++s_frameSamplesDropped;
        }
    }
    if (s_benchmarkEnabled && frame.number > s_benchmarkWarmupFrames &&
        s_benchmarkSamples.size() < s_benchmarkMeasuredFrames) {
        s_benchmarkSamples.push_back(frame.sample);
    }

    ++frameCount;
    const bool profileCaptureReady = s_captureViewportPath.empty() || s_captureAttempted;

    // A2.1: one-shot diagnostic readback of the prepass depth/G0 against the forward
    // depth and Geometry Normal debug view. It runs after the warm-up frames and exits.
    if (!s_decalPrepassCheckPath.empty() && !s_decalPrepassCheckAttempted
        && frameCount >= SCENARIO_REPORT_FRAME && activeViewport != nullptr && s_decalCheckCaptured) {
        s_decalPrepassCheckAttempted = true;
        if (!writeDecalPrepassCheck(s_decalPrepassCheckPath, *activeViewport)) {
            s_outputFailed = true;
        }
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    } else if (!s_decalPrepassCheckPath.empty() && !s_decalPrepassCheckAttempted && frameCount >= 64) {
        // The active viewport never rendered into a single-sample target (for example it
        // was closed or stayed inactive). Do not wait forever for a diagnostic.
        s_decalPrepassCheckAttempted = true;
        s_outputFailed = true;
        std::fprintf(stderr, "Decal prepass check could not capture the active viewport\n");
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Automation runs read GL errors instead of letting them go unnoticed.
    if (uiScenario.active() || !s_decalPrepassCheckPath.empty() || !s_profileReportPath.empty() || controlPipe != nullptr) {
        drainGLErrors("frame");
    }
    const bool profileReportReady = !s_profileReportPath.empty() && !s_profileReportAttempted &&
                                    frameCount >= SCENARIO_REPORT_FRAME && activeViewport != nullptr &&
                                    profileCaptureReady;

    if (!profileReportReady && !s_profileReportPath.empty() && !s_profileReportAttempted &&
        !s_profileReportDeferredLogged && frameCount >= SCENARIO_REPORT_FRAME && !s_captureViewportPath.empty() &&
        !s_captureAttempted) {
        std::printf("Deferring profile report until the requested viewport capture completes\n");
        s_profileReportDeferredLogged = true;
    }

    if (profileReportReady) {
        std::printf("GL automation errors: %llu\n", static_cast<unsigned long long>(s_glErrorTotal));
        s_profileReportAttempted = true;
        ux3d::imgui::profiling::ProfileReport report;
        report.scenarioName = scenarioName;
        report.modelPath = currentFilePath.empty() ? modelToLoad : currentFilePath;
        report.build = std::string(__DATE__) + " " + __TIME__;
        const auto* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* glRendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        report.glVersion = glVersion != nullptr ? glVersion : "";
        report.glMajor = ux3d::imgui::glCaps().major;
        report.glMinor = ux3d::imgui::glCaps().minor;
        report.glCompute = ux3d::imgui::glCaps().compute;
        report.glMaxComputeWorkGroupInvocations = ux3d::imgui::glCaps().maxComputeWorkGroupInvocations;
        report.gpuVendor = glVendor != nullptr ? glVendor : "";
        report.gpuRenderer = glRendererName != nullptr ? glRendererName : "";
        report.isolated = s_isolatedState;
        report.instrumentationEnabled = s_profilingEnabled;
        report.vsyncRequested = s_vsyncEnabled;
        glfwGetWindowSize(window, &report.windowWidth, &report.windowHeight);
        report.viewportWidth = activeViewport->sceneFbo.getWidth();
        report.viewportHeight = activeViewport->sceneFbo.getHeight();
        report.viewportSamples = activeViewport->sceneFbo.getSamples();
        report.antiAliasing = effectiveAAName(ux3d::imgui::RenderQuality::effectiveViewport(activeViewport->settings));
        report.startup = ux3d::imgui::profiling::startupProfile();
        report.reportFrame = frameCount;
        report.frameSampleLimit = ux3d::imgui::profiling::kMaxRetainedFrameSamples;
        report.frameSamplesDropped = s_frameSamplesDropped;
        report.frames.assign(s_frameSamples.begin(), s_frameSamples.end());
        fillGpuReport(report.gpu, glRenderer.get());
        report.captureRequested = !s_captureViewportPath.empty();
        report.hasFirstFrameCapture = s_firstFrameCaptureAttempted;
        report.firstFrameCapture = s_firstFrameCaptureRecord;
        report.capture = s_captureRecord;
        report.camera = getCameraJson(activeViewport);
        report.sun = sunStateJson(activeEnvironment);
        report.ambient = ambientStateJson(activeEnvironment);
        report.timeOfDay = timeOfDayStateJson(activeEnvironment);
        if (activeEnvironment.environmentSource != 0) report.sky = skyStateJson(activeEnvironment);
        // A6.4-A6.6: exposure, moon, stars (only with the procedural sky, so HDR reports are unchanged).
        if (activeEnvironment.environmentSource != 0) report.night = nightStateJson(activeEnvironment);
        resolveAllViewportSettings();
        report.ssao = ssaoStateJson();
        report.quality = qualityStateJson();
        report.gltfExtensions = ux3d::imgui::GltfExtensionSettings::instance().stateJson();
        report.effects = effectsStateJson(activeEnvironment);
        report.skyOcclusion = skyOcclusionStateJson(activeEnvironment);
        // A6.8: the ocean's settings, cascades, memory, precompute and live GPU cost (only while the ground is the ocean).
        if (activeViewport != nullptr && activeViewport->settings.ground.mode == ux3d::imgui::GroundMode::Ocean) {
            report.ground = groundStateJson(*activeViewport);
        }
        if (!reportNodeArgs.empty()) {
            for (const auto& spec : reportNodeArgs) {
                auto* node = findNode(spec);
                if (node != nullptr) {
                    report.nodes.push_back(computeNodeScreenRect(node, activeViewport));
                } else {
                    nlohmann::json missing;
                    missing["index"] = -1;
                    missing["name"] = spec;
                    missing["visible"] = false;
                    missing["screenRect"] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    missing["onScreen"] = false;
                    missing["clipped"] = true;
                    missing["behindCamera"] = false;
                    missing["center"] = { 0.0f, 0.0f, 0.0f };
                    report.nodes.push_back(missing);
                }
            }
        }
        if (!reportPickArgs.empty()) {
            for (const auto& coord : reportPickArgs) {
                report.picks.push_back(performPickAtPixel(coord.first, coord.second, activeViewport));
            }
        }
        if (ux3d::imgui::profiling::writeProfileReport(s_profileReportPath, report)) {
            std::printf("Wrote profile report to %s (finalized at frame %llu)\n", s_profileReportPath.c_str(),
                        static_cast<unsigned long long>(frameCount));
        } else {
            s_outputFailed = true;
            std::fprintf(stderr, "Failed to write profile report to %s\n", s_profileReportPath.c_str());
        }
        if (s_exitAfterProfile) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
    if (!scenarioReportPath.empty() && frameCount == SCENARIO_REPORT_FRAME && activeViewport != nullptr) {
        const std::string reportModelPath = currentFilePath.empty() ? modelToLoad : currentFilePath;
        if (writeScenarioReport(scenarioReportPath, scenarioName, frameCount, window, reportModelPath, modelLoaded,
                                gltfModel, activeScene, *activeViewport, activeEnvironment, scenarioEnvironmentPath,
                                scenarioEnvironmentLoaded, animationPlayer, selection)) {
            std::printf("Wrote scenario report to %s\n", scenarioReportPath.c_str());
        } else {
            s_outputFailed = true;
            std::fprintf(stderr, "Failed to write scenario report to %s\n", scenarioReportPath.c_str());
        }
    }

    if (s_benchmarkEnabled && !s_benchmarkReportAttempted && s_benchmarkSamples.size() >= s_benchmarkMeasuredFrames) {
        ux3d::imgui::profiling::BenchmarkReport report;
        report.scenarioName = scenarioName;
        report.modelPath = currentFilePath.empty() ? modelToLoad : currentFilePath;
        report.build = std::string(__DATE__) + " " + __TIME__;
        const auto* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const auto* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const auto* glRendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        report.glVersion = glVersion != nullptr ? glVersion : "";
        report.glMajor = ux3d::imgui::glCaps().major;
        report.glMinor = ux3d::imgui::glCaps().minor;
        report.glCompute = ux3d::imgui::glCaps().compute;
        report.glMaxComputeWorkGroupInvocations = ux3d::imgui::glCaps().maxComputeWorkGroupInvocations;
        report.gpuVendor = glVendor != nullptr ? glVendor : "";
        report.gpuRenderer = glRendererName != nullptr ? glRendererName : "";
        report.isolated = s_isolatedState;
        report.vsyncRequested = s_vsyncEnabled;
        report.detailedInstrumentation = s_profilingEnabled;
        glfwGetWindowSize(window, &report.windowWidth, &report.windowHeight);
        if (activeViewport != nullptr) {
            report.viewportWidth = activeViewport->sceneFbo.getWidth();
            report.viewportHeight = activeViewport->sceneFbo.getHeight();
            report.viewportSamples = activeViewport->sceneFbo.getSamples();
            report.antiAliasing = effectiveAAName(activeViewport->settings);
        }
        report.warmupFrames = s_benchmarkWarmupFrames;
        report.measuredFramesRequested = s_benchmarkMeasuredFrames;
        report.hasFirstFrame = s_haveFirstFrameSample;
        report.firstFrame = s_firstFrameSample;
        report.measuredFrames = s_benchmarkSamples;
        fillGpuReport(report.gpu, glRenderer.get());

        s_benchmarkReportAttempted = true;
        s_benchmarkReportWritten = ux3d::imgui::profiling::writeBenchmarkReport(s_benchmarkOutputPath, report);
        if (s_benchmarkReportWritten) {
            std::printf("Wrote benchmark report to %s (%zu measured frames, warmup %llu, vsync %s)\n",
                        s_benchmarkOutputPath.c_str(), s_benchmarkSamples.size(),
                        static_cast<unsigned long long>(s_benchmarkWarmupFrames), s_vsyncEnabled ? "on" : "off");
        } else {
            s_outputFailed = true;
            std::fprintf(stderr, "Benchmark report write failed after attempt: %s\n", s_benchmarkOutputPath.c_str());
        }
        if (s_exitAfterBenchmark) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    if (dumpFrameCountdown > 0) {
        dumpFrameCountdown--;
        if (dumpFrameCountdown == 0 && !dumpFramePath.empty()) {
            std::vector<uint8_t> pixels(displayW * displayH * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, displayW, displayH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            if (saveImage(dumpFramePath.c_str(), displayW, displayH, pixels.data())) {
                std::printf("Dumped window frame to %s\n", dumpFramePath.c_str());
            } else {
                s_outputFailed = true;
                std::fprintf(stderr, "Failed to dump window frame to %s\n", dumpFramePath.c_str());
            }
            if (!dumpNextFramePath.empty()) {
                dumpFramePath = std::move(dumpNextFramePath);
                dumpNextFramePath.clear();
                dumpFrameCountdown = 1;
            } else if (exitAfterDump) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }
    }

    // Agent channel: advance the running multi-frame job, then run queued pipe requests.
    pumpAgentCommands();

    if (uiScenario.active()) {
        ux3d::imgui::UiScenarioFrame uiFrame;
        uiFrame.selection = describeUiSelection(selection);
        if (activeViewport != nullptr) {
            const auto target = activeViewport->cameraActor.getLookTarget();
            char cameraText[160];
            std::snprintf(cameraText, sizeof(cameraText), "target=%.3f,%.3f,%.3f zoom=%.3f", target.x, target.y,
                          target.z, activeViewport->cameraActor.getZoom());
            uiFrame.camera = cameraText;
        }
        uiFrame.logEntries = s_consoleLogs.size();
        uiFrame.logErrors = s_consoleLogs.counts().errors;
        uiFrame.logWarnings = s_consoleLogs.counts().warnings;
        uiFrame.logInfo = s_consoleLogs.counts().info;
        uiFrame.decalBufferActive = decalBuffer.isValid();
        uiFrame.decalBufferWidth = decalBuffer.getWidth();
        uiFrame.decalBufferHeight = decalBuffer.getHeight();
        uiFrame.decalBufferBytes = decalBuffer.getMemoryBytes();
        uiFrame.decalBufferAllocations = decalBuffer.getAllocationCount();
        uiFrame.decalBufferResizes = decalBuffer.getResizeCount();
        uiFrame.decalBufferReleases = decalBuffer.getReleaseCount();
        uiFrame.glErrors = s_glErrorTotal;
        uiFrame.hierarchyBuildCount = ux3d::imgui::SceneHierarchyPanel::rowModelBuildCount();
        {
            std::string revisionsJson =
                "{\"syncs\":" + std::to_string(sceneSyncCount) +
                ",\"skippedSyncs\":" + std::to_string(skippedSceneSyncCount) +
                ",\"animationApplies\":" + std::to_string(animationOnlySyncCount) +
                ",\"boundsComputed\":" + std::to_string(s_boundsComputedTotal) +
                ",\"boundsHits\":" + std::to_string(s_boundsHitTotal) +
                ",\"overrideApplies\":" + std::to_string(s_overrideApplyTotal) +
                ",\"overrideSkips\":" + std::to_string(s_overrideSkipTotal) +
                ",\"programCacheHits\":" + std::to_string(commandGenerator->getCommandMapping().getProgramCacheHits()) +
                ",\"programCacheMisses\":" +
                std::to_string(commandGenerator->getCommandMapping().getProgramCacheMisses()) +
                ",\"commandCacheHits\":" + std::to_string(commandGenerator->getCommandMapping().getCommandCacheHits()) +
                ",\"commandCacheMisses\":" +
                std::to_string(commandGenerator->getCommandMapping().getCommandCacheMisses()) +
                ",\"transformVisited\":" + std::to_string(ux3d::renderer::TransformHierarchyNode::stats().visited) +
                ",\"transformRecomputed\":" +
                std::to_string(ux3d::renderer::TransformHierarchyNode::stats().recomputed) +
                ",\"weightPushes\":" + std::to_string(ux3d::renderer::TransformHierarchyNode::stats().weightPushes) +
                ",\"jointUploads\":" + std::to_string(ux3d::renderer::Skin::s_jointUploadStats.uploads) +
                ",\"jointUploadsSuppressed\":" + std::to_string(ux3d::renderer::Skin::s_jointUploadStats.suppressed) +
                ",\"pickingRenders\":" + std::to_string(s_pickingRenderTotal) +
                ",\"pickingSkips\":" + std::to_string(s_pickingSkipTotal) +
                ",\"pickReads\":" + std::to_string(s_pickReadTotal) +
                ",\"pickUnserved\":" + std::to_string(s_pickUnservedTotal) +
                ",\"pickCulled\":" + std::to_string(s_pickCulledTotal) +
                ",\"traversalReuses\":" + std::to_string(gltfLayer->getTraversalReuses()) +
                ",\"instancedDraws\":" + std::to_string(s_sceneInstancedDraws) +
                ",\"instancedMembers\":" + std::to_string(s_sceneInstancedMembers) +
                ",\"sceneRenders\":" + std::to_string(s_sceneRenderTotal) +
                ",\"sceneSkips\":" + std::to_string(s_sceneSkipTotal) + ",\"culled\":" + std::to_string(s_culledTotal) +
                ",\"uncullable\":" + std::to_string(s_uncullableTotal) +
                ",\"marks\":" + std::to_string(sceneRevisions.markCount()) + ",\"lastConsumed\":\"" +
                ux3d::imgui::SceneRevisions::maskToString(lastFrameSceneChanges) + "\"" + ",\"pending\":\"" +
                ux3d::imgui::SceneRevisions::maskToString(sceneRevisions.pendingMask()) + "\"";
            for (size_t i = 0; i < ux3d::imgui::kSceneChangeCount; ++i) {
                const auto change = static_cast<ux3d::imgui::SceneChange>(i);
                revisionsJson += ",\"";
                revisionsJson += ux3d::imgui::sceneChangeName(change);
                revisionsJson += "\":" + std::to_string(sceneRevisions.revision(change));
            }
            revisionsJson += "}";
            uiFrame.revisionsJson = revisionsJson;
        }
        uiFrame.expectedCopyText = [&]() {
            std::string expected;
            for (const auto& entry : s_consoleLogs.entries()) {
                expected += entry.text;
                expected += "\n";
            }
            return expected;
        };
        uiFrame.appendLog = [&](const std::string& text, int severity) {
            const auto level = severity == 2   ? ux3d::core::LogLevel::Error
                               : severity == 1 ? ux3d::core::LogLevel::Warning
                                               : ux3d::core::LogLevel::Info;
            const ImVec4 color = severity == 2   ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                                 : severity == 1 ? ImVec4(1.0f, 0.85f, 0.3f, 1.0f)
                                                 : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            s_consoleLogs.append({text, color, level});
        };
        uiFrame.saveFrame = [&](const std::string& path) {
            std::vector<uint8_t> pixels(static_cast<size_t>(displayW) * displayH * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadBuffer(GL_BACK);
            glReadPixels(0, 0, displayW, displayH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            return saveImage(path.c_str(), displayW, displayH, pixels.data());
        };
        uiFrame.loadModel = [&](const std::string& path) {
            const bool loaded = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                                              gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments);
            updateWindowTitle(window, currentFilePath);
            return loaded;
        };
        uiFrame.loadModelWithoutTextures = [&](const std::string& path) {
            const bool loaded = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports,
                gltfLayer.get(), animationPlayer, sceneRevisions, sceneEnvironments, false, true);
            updateWindowTitle(window, currentFilePath);
            return loaded;
        };
        uiFrame.pickNode = [&](int id) {
            for (const auto& node : gltfModel.getNodes()) {
                if (static_cast<int>(node.m_ID) == id) {
                    auto* pickedNode = const_cast<ux3d::gltf::Node*>(&node);
                    selection.selectNode(pickedNode);
                    ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
                    sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
                    return true;
                }
            }
            return false;
        };
        for (const auto& vp : viewports) {
            if (!vp) continue;
            ux3d::imgui::UiScenarioFrame::ViewportInfo vinfo;
            vinfo.id = vp->id;
            vinfo.name = vp->name;
            vinfo.open = vp->isOpen;
            vinfo.renderW = vp->sceneFbo.getWidth();
            vinfo.renderH = vp->sceneFbo.getHeight();
            vinfo.aaMode = vp->settings.antiAliasingIdx;
            vinfo.aaW = vp->antiAliasing.getWidth();
            vinfo.aaH = vp->antiAliasing.getHeight();
            vinfo.aaTargets = vp->antiAliasing.getOwnedTargetCount();
            vinfo.aaFbos = vp->antiAliasing.getOwnedFramebufferCount();
            vinfo.aaBytes = vp->antiAliasing.getEstimatedLiveBytes();
            vinfo.aaHistoryValid = vp->antiAliasing.isHistoryValid();
            vinfo.aaAllocs = vp->antiAliasing.getTargetAllocationCount();
            vinfo.aaDeletes = vp->antiAliasing.getTargetDeletionCount();
            vinfo.targetGeneration = vp->antiAliasing.getTargetGeneration();
            vinfo.lastTaaHistoryValid = vp->antiAliasing.getLastTaaHistoryValid();
            vinfo.lastTaaBlendFactor = vp->antiAliasing.getLastTaaBlendFactor();
            vinfo.lastTaaTargetGeneration = vp->antiAliasing.getLastTaaTargetGeneration();
            vinfo.wireframe = vp->settings.wireframeOverlay;
            vinfo.showGrid = vp->settings.showGrid;
            vinfo.showSceneBoundingBox = vp->settings.showSceneBoundingBox;
            vinfo.forceDoubleSided = vp->settings.forceDoubleSided;
            vinfo.invertWinding = vp->settings.invertWinding;
            vinfo.activePasses = vp->lastExecutedPasses;
            vinfo.renderCount = vp->renderCount;
            uiFrame.viewports.push_back(vinfo);
        }
        uiFrame.setAaMode = [&](const std::string& modeStr, uint32_t vpId) {
            // Settings scopes: AA is renderer-wide; the viewport argument only has to name a viewport.
            if (vpId != 0 && findViewportById(vpId) == nullptr) return false;
            std::string m = toLowerAscii(modeStr);
            static const char* const names[] = {"none", "msaa2", "msaa4", "msaa8", "fxaa", "taa"};
            int mode = -1;
            for (int k = 0; k < 6; ++k) {
                if (m == names[k] || m == std::to_string(k)) mode = k;
            }
            if (mode < 0) return false;
            auto& quality = ux3d::imgui::RenderQuality::settings();
            quality.msaaIdx = mode >= 1 && mode <= 3 ? mode : 0;
            quality.postAAIdx = mode >= 4 ? mode - 3 : 0;
            ux3d::imgui::RenderQuality::pinAA();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
            return true;
        };
        uiFrame.createViewport = [&]() {
            uint32_t nextId = 1;
            for (const auto& vp : viewports) {
                if (vp && vp->id >= nextId) nextId = vp->id + 1;
            }
            std::string vpName = "Viewport " + std::to_string(nextId);
            auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
            if (activeViewport != nullptr) {
                newVp->settings = activeViewport->settings; // AA is renderer-wide (settings scopes)
                if (activeScene != nullptr) {
                    newVp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                }
            }
            newVp->requestFocus = true;
            activeViewport = newVp.get();
            selection.selectViewport(newVp->id);
            viewports.push_back(std::move(newVp));
            return true;
        };
        uiFrame.setAnimationPlaying = [&](bool play) {
            if (play && animationPlayer.getMaxDuration() <= 0.0f) return false;
            animationPlayer.setPlaying(play);
            return true;
        };
        uiFrame.focusViewport = [&](uint32_t id) {
            for (const auto& vp : viewports) {
                if (vp && vp->id == id && vp->isOpen) {
                    vp->requestFocus = true;
                    return true;
                }
            }
            return false;
        };
        uiFrame.splitViewport = [&](uint32_t vpId) {
            ImGuiWindow* first = ImGui::FindWindowByName("###Viewport_1");
            if (vpId <= 1 || first == nullptr || first->DockId == 0) return false;
            bool exists = false;
            for (const auto& vp : viewports)
                exists = exists || (vp && vp->id == vpId && vp->isOpen);
            if (!exists) return false;
            ImGuiID left = 0;
            ImGuiID right = 0;
            ImGui::DockBuilderSplitNode(first->DockId, ImGuiDir_Right, 0.5f, &right, &left);
            ImGui::DockBuilderDockWindow("###Viewport_1", left);
            ImGui::DockBuilderDockWindow(("###Viewport_" + std::to_string(vpId)).c_str(), right);
            ImGui::DockBuilderFinish(first->DockId);
            return true;
        };
        uiFrame.toggleViewport = [&](uint32_t vpId) {
            for (const auto& vp : viewports) {
                if (vp && vp->id == vpId) {
                    vp->isOpen = !vp->isOpen;
                    if (vp->isOpen) vp->requestFocus = true;
                    return true;
                }
            }
            return false;
        };
        uiFrame.setViewportSetting = [&](uint32_t vpId, const std::string& key, bool val) {
            ux3d::imgui::ViewportInstance* targetVp = activeViewport;
            if (vpId != 0) {
                for (const auto& vp : viewports) {
                    if (vp && vp->id == vpId) {
                        targetVp = vp.get();
                        break;
                    }
                }
            }
            if (!targetVp) return false;
            std::string k = toLowerAscii(key);
            if (k == "wireframe" || k == "wireframeoverlay")
                targetVp->settings.wireframeOverlay = val;
            else if (k == "grid" || k == "showgrid")
                targetVp->settings.showGrid = val;
            else if (k == "bbox" || k == "boundingbox" || k == "showsceneboundingbox")
                targetVp->settings.showSceneBoundingBox = val;
            else if (k == "doublesided" || k == "forcedoublesided")
                targetVp->settings.forceDoubleSided = val;
            else if (k == "invertwinding")
                targetVp->settings.invertWinding = val;
            else if (k == "invisible" || k == "showinvisiblematerials")
                ux3d::imgui::GltfExtensionSettings::instance().setEnabled("ASOBO_material_invisible", !val, false);
            else if (k == "asobo")
                ux3d::imgui::GltfExtensionSettings::instance().setEnabled("ASOBO", val, false);
            else
                return false;
            return true;
        };
        uiFrame.setAsoboParam = [&](const std::string& matName, const std::string& extName,
                                    const std::string& paramName, const std::string& value) {
            return ux3d::imgui::ExtensionInspector::instance().setMaterialAsoboParameter(
                gltfModel, matName, extName, paramName, value, sceneRevisions);
        };
        uiFrame.runCommand = [this](const std::string& cmd, const std::vector<std::string>& tokens, const std::string& line,
                                    std::function<void(bool, const std::string&, const nlohmann::json&)> done) {
            return runScriptCommand(cmd, tokens, line, std::move(done));
        };
        uiScenario.endFrame(window, uiFrame);
        if (uiScenario.failed()) {
            s_outputFailed = true;
        }
    }
}

} // namespace ux3d::imgui::application
