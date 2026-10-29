// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/Application.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/GlCaps.h"
#include "imgui/RenderQuality.h"
#include "imgui/GltfExtensionSettings.h"
#include "imgui/Theme.h"
#include "imgui/ElementInspector.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace ux3d::imgui::application {

void Application::glfw_error_callback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static bool noFocusRequested()
{
    const char* value = std::getenv("GLIDER_NO_FOCUS");
    return value != nullptr && std::strcmp(value, "1") == 0;
}

int Application::run(int argc, char** argv)
{
    // P0.1 pre-scan: window size and state isolation must be known before GLFW
    // creates the window and before persisted layouts are loaded.
    bool isolatedArg = false;
    bool profileArg = false;
    bool noProfileArg = false;
    std::string profileReportArg;
    int requestedWidth = 0;
    int requestedHeight = 0;
    bool agentGuideArg = false;
    std::string agentGuidePath;
    std::string agentGuideFormat;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--agent-guide") {
            agentGuideArg = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') agentGuidePath = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            agentGuideFormat = toLowerAscii(argv[++i]);
            if (agentGuideArg && agentGuidePath.empty() && i + 1 < argc && argv[i + 1][0] != '-') agentGuidePath = argv[++i];
        } else if (arg == "--isolated" || arg == "--no-persist") {
            isolatedArg = true;
        } else if (arg == "--ui-script-follow") {
            isolatedArg = true;
        } else if (arg == "--profile") {
            profileArg = true;
        } else if (arg == "--no-profile") {
            noProfileArg = true;
        } else if (arg == "--profile-report" && i + 1 < argc) {
            profileReportArg = argv[++i];
        } else if (arg == "--vsync" && i + 1 < argc) {
            const std::string vsyncValue = toLowerAscii(argv[++i]);
            if (vsyncValue == "on" || vsyncValue == "1" || vsyncValue == "true") {
                s_vsyncEnabled = true;
            } else if (vsyncValue == "off" || vsyncValue == "0" || vsyncValue == "false") {
                s_vsyncEnabled = false;
            } else {
                std::fprintf(stderr, "Invalid --vsync '%s' (expected on|off)\n", vsyncValue.c_str());
                return 1;
            }
        } else if (arg == "--window-size" && i + 1 < argc) {
            const std::string sizeText = argv[++i];
            if (!parseWindowSize(sizeText, requestedWidth, requestedHeight)) {
                std::fprintf(stderr, "Invalid --window-size '%s' (expected WIDTHxHEIGHT, minimum 320x240)\n",
                             sizeText.c_str());
                requestedWidth = 0;
                requestedHeight = 0;
            }
        }
    }
    s_isolatedState = isolatedArg;
    // Phase 7 quality settings: persisted choice first, then command-line overrides,
    // all before the default environment is filtered.
    ux3d::imgui::RenderQuality::setPersistenceEnabled(!s_isolatedState);
    ux3d::imgui::RenderQuality::load();
    ux3d::imgui::GltfExtensionSettings::instance().load(); // the glTF extension switches are an app setting
    for (int i = 1; i < argc; ++i) {
        std::string qualityError;
        if (ux3d::imgui::RenderQuality::parseArgument(argc, argv, i, qualityError) && !qualityError.empty()) {
            std::fprintf(stderr, "%s\n", qualityError.c_str());
            return 1;
        }
    }
    ux3d::imgui::RenderQuality::configureAuto(argc, argv);
    s_windowSizeOverridden = requestedWidth > 0 && requestedHeight > 0;
    s_profilingEnabled = !noProfileArg && (profileArg || !profileReportArg.empty());
    ux3d::imgui::profiling::setCountingEnabled(s_profilingEnabled);

    const auto startupStart = ux3d::imgui::profiling::Clock::now();

    // Initialize embedded Virtual File System (decompresses runtime resources in-memory)
#ifdef _WIN32
    // --agent-guide without a file writes the guide to stdout: keep the VFS status line out of it.
    const bool guideToStdout = agentGuideArg && agentGuidePath.empty() && agentGuideFormat != "skill";
    int savedStdout = -1;
    if (guideToStdout) {
        std::fflush(stdout);
        savedStdout = _dup(1);
        const int nul = _open("NUL", _O_WRONLY);
        if (savedStdout >= 0 && nul >= 0) _dup2(nul, 1);
        if (nul >= 0) _close(nul);
    }
    ux3d::imgui::EmbeddedVFS::instance().initialize();
    if (savedStdout >= 0) {
        std::fflush(stdout);
        _dup2(savedStdout, 1);
        _close(savedStdout);
    }
#else
    ux3d::imgui::EmbeddedVFS::instance().initialize();
#endif
    ux3d::imgui::profiling::startupProfile().vfsDecompressMs = ux3d::imgui::profiling::elapsedMs(startupStart);

    // --agent-guide [file] [--format skill <dir>]: the embedded guide plus the command table
    // generated from the registry. Never opens a window.
    if (agentGuideArg) {
        return writeAgentGuide(agentGuidePath, agentGuideFormat);
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    WindowState winState = s_isolatedState ? WindowState() : loadWindowState();
    if (s_windowSizeOverridden) {
        winState.width = requestedWidth;
        winState.height = requestedHeight;
        winState.maximized = false;
    }

    // Target OpenGL 4.6 Core Profile (compute shaders, SSBOs, image load/store); fall back to 4.5, 4.3 and
    // finally 4.1. Every existing shader stays at #version 410, so rendering does not depend on the version.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, winState.maximized ? GLFW_TRUE : GLFW_FALSE);
    if (noFocusRequested()) {
        // Harness runs show and render normally but must never activate the window (AGENTS.md 4.2).
        glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
        glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    }
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create GLFW window (starts at restored or default size)
    static constexpr int kContextVersions[][2] = { { 4, 6 }, { 4, 5 }, { 4, 3 }, { 4, 1 } };
    for (const auto& version : kContextVersions) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, version[0]);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, version[1]);
        window = glfwCreateWindow(winState.width, winState.height, "Glider - glTF 2.0 Viewer", nullptr, nullptr);
        if (window) break;
    }
    if (!window) {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    if (!winState.maximized) {
        glfwSetWindowPos(window, winState.posX, winState.posY);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(s_vsyncEnabled ? 1 : 0);

    // Initialize core logger callback
    ux3d::core::addLoggerCallback(&consoleLogger);

    // Initialize GLAD
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::fprintf(stderr, "Failed to initialize GLAD\n");
        ux3d::core::removeLoggerCallback(&consoleLogger);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    logMessage(std::string("OpenGL Version: ") + reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    logMessage(std::string("Renderer: ") + reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    ux3d::imgui::initGlCaps();
    {
        const auto& caps = ux3d::imgui::glCaps();
        logMessage("OpenGL context " + std::to_string(caps.major) + "." + std::to_string(caps.minor) + " core obtained" +
                   (caps.compute ? "; compute shaders available (max work group invocations " +
                                       std::to_string(caps.maxComputeWorkGroupInvocations) + ")"
                                 : "; compute features are unavailable below OpenGL 4.3"));
    }

    // Initialize GladGL abstraction
    gladGL = std::make_unique<ux3d::renderer::GladGL>();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;
    if (!s_isolatedState) {
        loadImguiLayout();
    }

    // Load FSS Fonts (fss-ui.ttf and fss-ui-mono.ttf with embedded MDI icons)
    ux3d::imgui::loadFssFonts(io);

    // Apply FSS ImGui Theme (aviation dark palette with emerald green accents)
    ux3d::imgui::applyFssTheme();

    // Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // Initialize 3D Viewport components
    gridRenderer.init();
    bboxRenderer.init();

    rendererResourcesDir = resolveRendererResourcesDir();
    auto shaderSource = rendererResourcesDir / "shaders";

    gltfLayer = std::make_unique<ux3d::renderer::GltfRenderLayer>(gladGL.get(), shaderSource / "ibl_sampler");
    ux3d::imgui::RenderQuality::applyIblParameters(*gltfLayer);

    // Transmissive materials sample a mipmapped copy of the opaque scene. The texture
    // must exist before any model loads: materials attach it only when they are built.
    // The TransmissiveBlit pass copies transmissionSource. For ASOBO glass this is
    // the floating point behind target; the callback restores the forward target.
    transmissionTarget.create();
    gltfLayer->getResourceManager().createFramebufferTexture(transmissionTarget.getTexture());
    gltfLayer->getResourceManager().setBlitFramebufferFunction(
        [this] {
            if (transmissionSource != nullptr) {
                if (ssrCaptureDepth) {
                    const bool depthValid = ssrUseDecalDepth || transmissionTarget.captureDepth(*transmissionSource);
                    commandGenerator->setSsrDepth(depthValid ? (ssrUseDecalDepth ? decalBuffer.getDepthTexture()
                                                                                           : transmissionTarget.getDepthTexture()) : 0, ssrUseDecalDepth);
                    if (!depthValid) commandGenerator->setSsrOptions(false, 1, ssrInteriorSky);
                }
                const bool copied = transmissionTarget.capture(*transmissionSource, transmissionDestination != nullptr);
                commandGenerator->setAsoboGlassCopyValid(copied && transmissionDestination != nullptr);
                if (transmissionDestination != nullptr) transmissionDestination->bind();
                else transmissionSource->bind();
            }
        },
        ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));
    gltfLayer->getResourceManager().setTransmissiveTextureFunction([] {});
    commandGenerator = std::make_unique<ux3d::renderer::CommandGenerator>(gladGL.get(), shaderSource);
    glRenderer = std::make_unique<ux3d::renderer::Renderer>(gladGL.get());
    glRenderer->init(ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));
    glRenderer->setGpuProfilingEnabled(s_profilingEnabled);

    pickingLayer = std::make_unique<ux3d::renderer::PickingRenderLayer>(gladGL.get());
    // A2.1: the decal prepass adds its own program and pass to the scene built below.
    decalPrepassLayer = std::make_unique<ux3d::renderer::DecalPrepassLayer>(gladGL.get());
    // A2.2: the accumulation pass draws the decals the prepass excluded.
    decalAccumulationLayer = std::make_unique<ux3d::renderer::DecalAccumulationLayer>(gladGL.get());
    // A3.2: the sun shadow caster pass.
    shadowCasterLayer = std::make_unique<ux3d::renderer::ShadowCasterLayer>(gladGL.get());
    sunShadowCamera = std::make_unique<ux3d::renderer::Camera>(gladGL.get());
    // A3.9: the sky occlusion bake's caster pass and camera.
    skyOcclusionCasterLayer = std::make_unique<ux3d::renderer::ShadowCasterLayer>(gladGL.get());
    skyOcclusionCamera = std::make_unique<ux3d::renderer::Camera>(gladGL.get());
    // P5.2: the one picking target shared by all viewports, a single pixel.
    pickingTarget.resize(1, 1, 1);

    viewports.push_back(std::make_unique<ux3d::imgui::ViewportInstance>(1, "3D Viewport", gladGL.get()));
    activeViewport = viewports[0].get();

    auto envPath = resolveEnvironmentPath(rendererResourcesDir);
    scenarioEnvironmentPath = envPath.toString().c_str();
    auto& startupProfile = ux3d::imgui::profiling::startupProfile();
    if ((ux3d::imgui::RenderQuality::allowIbl() || ux3d::imgui::RenderQuality::allowSkybox(true)) &&
        ux3d::imgui::StdFileSystem::instance().isFile(envPath)) {
        const auto environmentInitStart = ux3d::imgui::profiling::Clock::now();
        gltfLayer->setEnvironmentMap(envPath);
        startupProfile.environmentInitMs += ux3d::imgui::profiling::elapsedMs(environmentInitStart);
        const auto environmentFilterStart = ux3d::imgui::profiling::Clock::now();
        if (gltfLayer->filterEnvironmentMap()) {
            startupProfile.environmentFilterMs += ux3d::imgui::profiling::elapsedMs(environmentFilterStart);
            startupProfile.environmentLoads++;
            scenarioEnvironmentLoaded = true;
            commandGenerator->setIBLEnabled(true);
            logMessage(std::string("Loaded default environment map: ") + ux3d::imgui::kDefaultEnvironmentFile);
        }
    }

    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_HILL);
    commandGenerator->setExposure(1.0f);
    commandGenerator->setPunctualLightsEnabled(true);
    commandGenerator->setEnvironmentMapRotation(0.0f);
    commandGenerator->setEnvironmentMapBlurNormalized(0.0f);
    commandGenerator->setEnvironmentMapIntensity(1.0f);

    gltfLayer->renderSettings.enableSpecularGlossiness = true;
    gltfLayer->renderSettings.enableUnlit = true;
    gltfLayer->renderSettings.enableClearCoat = true;
    gltfLayer->renderSettings.enableSheen = true;
    gltfLayer->renderSettings.enableTransmission = true;
    gltfLayer->renderSettings.enableVolume = true;
    gltfLayer->renderSettings.enableIOR = true;
    gltfLayer->renderSettings.enableSpecular = true;

    if (const auto exitCode = configure(argc, argv)) return *exitCode;
    // The agent `reset` command returns to the state the startup flags produced.
    if (activeViewport != nullptr) agentStartupViewportSettings = activeViewport->settings;
    agentStartupEnvironment = getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
    agentStartupQuality = ux3d::imgui::RenderQuality::settings();
    agentStartupGltfExtensions = ux3d::imgui::GltfExtensionSettings::instance().snapshot();
    resolveAllViewportSettings();
    if (controlPipeArg) startControlPipe();
    // P0.2 startup snapshot: load/decode, renderer upload, validation and IBL
    // are measured separately. Counter state is then reset for frame sampling.
    {
        auto& startup = ux3d::imgui::profiling::startupProfile();
        startup.totalStartupMs = ux3d::imgui::profiling::elapsedMs(startupStart);
        startup.stages = {{"startup.vfsDecompress", startup.vfsDecompressMs},
                          {"startup.environmentInit", startup.environmentInitMs},
                          {"startup.environmentFilter", startup.environmentFilterMs},
                          {"startup.loadDecode", startup.loadDecodeMs},
                          {"startup.rendererUpload", startup.rendererUploadMs},
                          {"startup.validation", startup.validationMs},
                          {"validation.read", startup.validationReadMs},
                          {"validation.parse", startup.validationParseMs},
                          {"validation.schema", startup.validationSchemaMs},
                          {"validation.semantic", startup.validationSemanticMs},
                          {"startup.total", startup.totalStartupMs}};
        startup.counters = ux3d::imgui::profiling::frameCounters();
        ux3d::imgui::profiling::resetFrameCounters();
    }
    if (gltfLayer != nullptr) {
        gltfLayer->setCpuProfilingEnabled(s_profilingEnabled);
    }
    if (glRenderer != nullptr) {
        glRenderer->setGpuProfilingEnabled(s_profilingEnabled);
    }

    if (!uiScriptFollowPath.empty()) {
        if (!uiScenario.configureFollow(uiScriptFollowPath, uiReportPath)) {
            std::fprintf(stderr, "--ui-script-follow requires a valid script path\n");
            s_outputFailed = true;
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    } else if (!uiScriptPath.empty()) {
        if (uiReportPath.empty() || !uiScenario.configure(uiScriptPath, uiReportPath)) {
            std::fprintf(stderr, "--ui-script requires a readable script and --ui-report\n");
            s_outputFailed = true;
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    lastFrameTime = glfwGetTime();
    previousFrameStart = ux3d::imgui::profiling::Clock::now();
    s_resizeFrame = [&]() {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (!frameInProgress && width > 0 && height > 0) {
            runFrame();
        }
    };
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* resizedWindow, int, int) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(resizedWindow));
        if (app != nullptr && app->s_resizeFrame) app->s_resizeFrame();
    });

    // Main Loop
    while (!glfwWindowShouldClose(window)) {
        // Automation (UI scripts, benchmarks, captures, dumps, reports) runs on frame
        // counts, so it always polls.
        const bool automationPending = (uiScenario.active() && !uiScenario.isFollowMode()) || s_benchmarkEnabled || !dumpFramePath.empty() ||
                                       !s_decalPrepassCheckPath.empty() ||
                                       (!s_captureViewportPath.empty() && !s_captureAttempted) ||
                                       (!s_profileReportPath.empty() && !s_profileReportAttempted) ||
                                       (!scenarioReportPath.empty() && frameCount <= SCENARIO_REPORT_FRAME) ||
                                       agentWorkPending();
        if (!s_disableIdleWait && !automationPending && idleQuietFrames >= kIdleQuietFrames) {
            const double waitStart = glfwGetTime();
            ux3d::imgui::waitForEvents(
                window, kIdleWaitTimeoutSeconds, [] { return ImGui::GetCurrentContext()->InputEventsQueue.Size > 0; },
                idleWaitStats);
            idleWaitSeconds += glfwGetTime() - waitStart;
            if (!s_idleStatsPath.empty() && idleWakesMs.size() < 200000) {
                idleWakesMs.push_back(wallClockMs());
            }
        } else {
            glfwPollEvents();
        }
        runFrame();
    }
    glfwSetFramebufferSizeCallback(window, nullptr);
    s_resizeFrame = nullptr;
    glfwSetWindowUserPointer(window, nullptr);

    if (!s_idleStatsPath.empty()) {
        std::ofstream idleOut(s_idleStatsPath, std::ios::binary | std::ios::trunc);
        idleOut << std::fixed << std::setprecision(3);
        idleOut << "{\"schema\":\"glider.idle.v1\",\"frames\":" << frameCount << ",\"waits\":" << idleWaitStats.waits
                << ",\"timeouts\":" << idleWaitStats.timeouts << ",\"inputWakes\":" << idleWaitStats.inputWakes
                << ",\"sentOnlyWakes\":" << idleWaitStats.sentOnlyWakes << ",\"waitSeconds\":" << idleWaitSeconds
                << ",\"idleWaitEnabled\":" << (s_disableIdleWait ? "false" : "true") << ",\"frameStartsMs\":[";
        for (size_t f = 0; f < idleFrameStartsMs.size(); ++f) {
            idleOut << (f ? "," : "") << idleFrameStartsMs[f];
        }
        idleOut << "],\"wakesMs\":[";
        for (size_t f = 0; f < idleWakesMs.size(); ++f) {
            idleOut << (f ? "," : "") << idleWakesMs[f];
        }
        idleOut << "]}\n";
    }

    // Cleanup
    stopControlPipe();
    agentRegistry.cancelAll("shutting_down", "Glider is closing");
    releaseTextureRefreshSnapshot();
    ux3d::imgui::RenderQuality::releaseGpuSamples();
    ux3d::imgui::ElementInspector::clearPreviewCache();
    gridRenderer.cleanup();
    bboxRenderer.cleanup();
    viewports.clear();
    pickingTarget = ux3d::imgui::Framebuffer();
    if (glRenderer != nullptr) {
        glRenderer->uninit();
        glRenderer.reset();
    }
    if (commandGenerator != nullptr) {
        commandGenerator->clearVertexLayoutCache();
        commandGenerator.reset();
    }
    // Drop the decal buffer and its layer before the GL context goes away; the pooled
    // material copies hold no GL objects, but the buffer's textures and FBOs do.
    releaseDecalBuffer();
    decalPrepassLayer.reset();
    shadowCasterLayer.reset();
    sunShadowCamera.reset();
    skyOcclusionVolume.release();
    skyOcclusionCasterLayer.reset();
    skyOcclusionCamera.reset();
    // A6.2: the procedural sky's textures, while the GL context still exists.
    releaseSkyEnvironment();
    if (gltfLayer != nullptr) {
        gltfLayer.reset();
    }
    transmissionTarget.destroy();
    glassCopyFbo = ux3d::imgui::Framebuffer();

    if (!s_isolatedState) {
        saveImguiLayout();
        ux3d::imgui::RenderQuality::save();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    ux3d::core::removeLoggerCallback(&consoleLogger);

    if (!s_isolatedState && !s_windowSizeOverridden) {
        saveWindowState(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    if (s_outputFailed) {
        std::fprintf(stderr, "Exiting with failure status because an output or capture operation failed\n");
        return 1;
    }
    return 0;
}

} // namespace ux3d::imgui::application

namespace ux3d::imgui {

int runApplication(int argc, char** argv)
{
    application::Application app;
    return app.run(argc, argv);
}

} // namespace ux3d::imgui
