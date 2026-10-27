// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "imgui/GladGL.h"
#include "imgui/Framebuffer.h"
#include "imgui/GridRenderer.h"
#include "imgui/StdFileSystem.h"
#include "imgui/Profiling.h"
#include "imgui/IdleWait.h"
#include "imgui/BoundingBoxRenderer.h"
#include "imgui/StatusBar.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/ViewportInstance.h"
#include "imgui/ViewportOverrides.h"
#include "imgui/SelfCheckResult.h"
#include "imgui/UiScenarioHarness.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "gltf/Gltf.h"
#include "gltf/Utils.h"
#include "renderer/Renderer.h"
#include "renderer/CommandGenerator.h"
#include "renderer/DecalPrepassLayer.h"
#include "renderer/DecalAccumulationLayer.h"
#include "renderer/PickingRenderLayer.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/TransformHierarchyNode.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ux3d::imgui::application {

using ux3d::imgui::LogEntry;
using ux3d::imgui::LogSeverityCounts;

class ConsoleLogStore {
public:
    void append(LogEntry entry)
    {
        switch (entry.level) {
        case ux3d::core::LogLevel::Error:
        case ux3d::core::LogLevel::Fatal:
            ++m_counts.errors;
            break;
        case ux3d::core::LogLevel::Warning:
            ++m_counts.warnings;
            break;
        default:
            ++m_counts.info;
            break;
        }
        m_entries.push_back(std::move(entry));
        ++m_revision;
    }

    void clear()
    {
        m_entries.clear();
        m_counts = {};
        ++m_revision;
        ++m_clearGeneration;
    }

    const std::vector<LogEntry>& entries() const { return m_entries; }
    const LogSeverityCounts& counts() const { return m_counts; }
    size_t size() const { return m_entries.size(); }
    uint64_t revision() const { return m_revision; }
    uint64_t clearGeneration() const { return m_clearGeneration; }

private:
    std::vector<LogEntry> m_entries;
    LogSeverityCounts m_counts;
    uint64_t m_revision = 0;
    uint64_t m_clearGeneration = 0;
};

struct ConsoleLogLayoutCache {
    bool valid = false;
    uint64_t revision = 0;
    uint64_t clearGeneration = 0;
    size_t entryCount = 0;
    ImFont* font = nullptr;
    float fontSize = 0.0f;
    float wrapWidth = 0.0f;
    float itemSpacingY = 0.0f;
    bool wrap = false;
    bool allSingleLine = true;
    float maxTextWidth = 0.0f;
    float contentHeight = 0.0f;
    size_t fullRebuilds = 0;
    size_t appendedRowsMeasured = 0;
    std::vector<float> rowOffsets;
    std::vector<float> rowHeights;
    std::vector<float> rowEnds;
};

struct NodeOBB {
    glm::vec3 localMin{std::numeric_limits<float>::max()};
    glm::vec3 localMax{std::numeric_limits<float>::lowest()};
    glm::mat4 modelMatrix{1.0f};
    bool valid = false;
};

struct BoundsCache {
    const ux3d::renderer::GltfRenderLayer* obbLayer = nullptr;
    const ux3d::gltf::Node* obbNode = nullptr;
    uint64_t obbPoseGeneration = 0;
    bool obbValid = false;
    NodeOBB obb;

    const ux3d::renderer::GltfRenderLayer* extentsLayer = nullptr;
    const ux3d::gltf::Scene* extentsScene = nullptr;
    uint64_t extentsSceneGeneration = 0;
    bool extentsValid = false;
    ux3d::gltf::Extends extents;
};

struct WindowState {
    int width = 1600;
    int height = 900;
    int posX = 100;
    int posY = 100;
    bool maximized = false;
};

struct FrameContext {
    ux3d::imgui::profiling::Clock::time_point start;
    uint64_t number = 0;
    float deltaTime = 1.0f / 60.0f;
    ux3d::imgui::profiling::FrameSample sample;
    bool profiling = false;
    void markStage(const char* name, ux3d::imgui::profiling::Clock::time_point stageStart)
    {
        if (profiling) sample.stages.push_back({name, ux3d::imgui::profiling::elapsedMs(stageStart)});
    }
};

// Owns the state shared by startup, UI, rendering and diagnostics.
// Module methods preserve the order of the original main loop.
class Application {
public:
    int run(int argc, char** argv);

private:
    class ConsoleLogCallback : public ux3d::core::LogCallback {
    public:
        explicit ConsoleLogCallback(Application& owner) : LogCallback(ux3d::core::LogLevel::All), owner(owner) {}
        void log(ux3d::core::LogLevel level, const char* message) override;

    private:
        Application& owner;
    };
    ConsoleLogCallback consoleLogger{*this};

    // Functional entry points and helpers.
    int writeSelfCheckReport(const ux3d::imgui::SelfCheckResult& result, const std::string& path, const char* label);
    bool saveImage(const char* filename, int width, int height, const uint8_t* rgba);
    uint64_t hashBytesFnv1a64(const uint8_t* data, size_t size);
    bool captureViewportTexture(GLuint texture, int width, int height, const std::string& path,
                                const std::string& region, uint64_t frame, bool firstFrame, double timeSeconds,
                                ux3d::imgui::profiling::CaptureRecord& record);
    ux3d::imgui::SelfCheckResult runConsoleLogLayoutSelfCheck(bool forceFailure);
    void logMessage(const std::string& msg, ImVec4 color = ImVec4(0.85f, 0.85f, 0.88f, 1.0f),
                    ux3d::core::LogLevel level = ux3d::core::LogLevel::Info);
    static void glfw_error_callback(int error, const char* description);
    ux3d::gltf::Extends getNodeExtends(ux3d::gltf::Node* node, const ux3d::gltf::Gltf* gltfModel,
                                       const ux3d::renderer::GltfRenderLayer* gltfLayer);
    void accumulateNodeOBB(const ux3d::gltf::Node* node, const glm::mat4& invRootWorld,
                           const ux3d::renderer::GltfRenderLayer* gltfLayer, NodeOBB& obb);
    NodeOBB getNodeOBB(const ux3d::gltf::Node* selectedNode, const ux3d::renderer::GltfRenderLayer* gltfLayer);
    const NodeOBB& getCachedNodeOBB(const ux3d::gltf::Node* node, const ux3d::renderer::GltfRenderLayer* gltfLayer);
    const ux3d::gltf::Extends& getCachedSceneExtents(const ux3d::gltf::Scene* scene, const ux3d::gltf::Gltf& gltfModel,
                                                     const ux3d::renderer::GltfRenderLayer* gltfLayer);
    void focusCameraOnNode(ux3d::gltf::Node* node, ux3d::gltf::Gltf& gltfModel,
                           ux3d::renderer::GltfRenderLayer* gltfLayer, ux3d::editor::CameraActor& cameraActor,
                           ux3d::imgui::AntiAliasing& antiAliasing, ux3d::imgui::SceneRevisions& revisions);
    std::vector<std::string> loadRecentFiles();
    void saveRecentFiles(const std::vector<std::string>& files);
    void addRecentFile(std::vector<std::string>& files, const std::string& path);
    WindowState loadWindowState();
    void saveWindowState(GLFWwindow* window);
    void loadImguiLayout();
    void saveImguiLayout();
    void captureLayoutBaseline();
    void resetImguiLayoutPersistence();
    bool parseWindowSize(const std::string& text, int& width, int& height);
    bool parseFloat3(const std::string& text, float& x, float& y, float& z);
    static std::string toLowerAscii(std::string text);
    void updateWindowTitle(GLFWwindow* window, const std::string& currentFilePath);
    ux3d::imgui::SceneEnvironmentState&
    getSceneEnvironment(std::vector<ux3d::imgui::SceneEnvironmentState>& sceneEnvironments,
                        const ux3d::gltf::Gltf& gltfModel, const ux3d::gltf::Scene* scene);
    bool loadGltfModel(const std::string& filePath, ux3d::gltf::Gltf& gltfModel, ux3d::gltf::Scene*& activeScene,
                       ux3d::imgui::SelectionState& selection, std::string& currentFilePath,
                       std::vector<std::unique_ptr<ux3d::imgui::ViewportInstance>>& viewports,
                       ux3d::renderer::GltfRenderLayer* gltfLayer, ux3d::imgui::AnimationPlayer& animPlayer,
                       ux3d::imgui::SceneRevisions& revisions,
                       std::vector<ux3d::imgui::SceneEnvironmentState>& sceneEnvironments, bool texturesOnly = false, bool withoutTextures = false);
    void newEmptyGltf(ux3d::gltf::Gltf& gltfModel, ux3d::gltf::Scene*& activeScene,
                      ux3d::imgui::SelectionState& selection, std::string& currentFilePath,
                      ux3d::editor::CameraActor& cameraActor, ux3d::renderer::GltfRenderLayer* gltfLayer,
                      ux3d::imgui::AnimationPlayer& animPlayer, ux3d::imgui::SceneRevisions& revisions);
    bool saveGltfModel(const std::string& filePath, const ux3d::gltf::Gltf& gltfModel, std::string& currentFilePath);
    void drawNodeCoordinateGizmo(ImDrawList* drawList, const glm::mat4& nodeWorld, const glm::mat4& viewProj,
                                 const ImVec2& vpPos, const ImVec2& vpSize, const glm::vec3& camEye,
                                 int axisConvention = 0);
    ux3d::filesystem::Path resolveRendererResourcesDir();
    ux3d::filesystem::Path resolveEnvironmentPath(const ux3d::filesystem::Path& rendererRes);
    ux3d::filesystem::Path resolveHdrPath(const std::string& hdrPath, const ux3d::filesystem::Path& rendererRes);
    std::string resolveSampleModelPath();
    std::string jsonEscape(const std::string& text);
    void writeJsonNumber(std::ostream& out, double value);
    void writeJsonVec3(std::ostream& out, const ux3d::math::vec3& value);
    void writeJsonMat4(std::ostream& out, const ux3d::math::mat4x4& matrix);
    bool hashFileFnv1a64(const std::string& path, uint64_t& hash, uint64_t& byteCount);
    const char* antiAliasingName(int index);
    const char* cameraTypeName(ux3d::renderer::Camera::Type type);
    const char* selectionTypeName(ux3d::imgui::SelectionType type);
    const char* antiAliasingModeName(int index);
    bool writeScenarioReport(const std::string& reportPath, const std::string& scenarioName, uint64_t frame,
                             GLFWwindow* window, const std::string& modelPath, bool modelLoaded,
                             ux3d::gltf::Gltf& gltfModel, ux3d::gltf::Scene* activeScene,
                             const ux3d::imgui::ViewportInstance& viewport,
                             const ux3d::imgui::SceneEnvironmentState& environment, const std::string& environmentPath,
                             bool environmentLoaded, const ux3d::imgui::AnimationPlayer& animationPlayer,
                             const ux3d::imgui::SelectionState& selection);
    ux3d::renderer::RenderMode getRenderModeFromIndex(int idx);
    std::optional<ux3d::renderer::RenderMode> getRenderModeFromName(const std::string& name);
    int getIndexFromRenderMode(ux3d::renderer::RenderMode mode);
    const char* getRenderModeLabel(ux3d::renderer::RenderMode mode);
    std::vector<const ux3d::gltf::Node*> collectSceneCameraNodes(const ux3d::gltf::Scene* scene);
    void setupDefaultDockLayout(ImGuiID dockspaceId, ImVec2 size, bool force = false);
    void fillGpuReport(ux3d::imgui::profiling::GpuReport& target, const ux3d::renderer::Renderer* renderer);
    std::string describeUiSelection(const ux3d::imgui::SelectionState& selection);
    std::optional<int> configure(int argc, char** argv);
    void runFrame();
    // Texture-only quality refreshes keep the presented frame on screen instead of
    // switching to the full loading screen. The snapshot is the last swapped frame,
    // captured in the frame that requested the refresh.
    void captureTextureRefreshSnapshot(int width, int height);
    void releaseTextureRefreshSnapshot();
    void processInput(float deltaTime);
    // A2.2: --material-edit-emissive <material> edits that material's emissive factor every
    // frame and marks SceneChange::Material, so the per-edit cost can be measured with
    // --profile-report (and compared against --force-full-sync).
    void driveMaterialEditCheck();
    void synchronizeScene(FrameContext& frame);
    void createViewport(const ux3d::gltf::Node* cameraNode);
    void drawViewportMenuBody();
    void drawShadingMenu(ux3d::imgui::ViewportInstance& viewport);
    void showViewportMenuFor(ux3d::imgui::ViewportInstance* vp);
    void drawMenus(FrameContext& frame);
    void drawViewports(FrameContext& frame);
    void drawPanels(FrameContext& frame);
    void drawLogger();
    void finishFrame(FrameContext& frame, int displayW, int displayH);
    // A2.1: true when this viewport's file has decal materials, ASOBO is on and the
    // viewport applies ASOBO_material_geometry_decal.
    bool viewportNeedsDecalBuffer(const ux3d::imgui::ViewportInstance& viewport) const;
    void releaseDecalBuffer();
    bool writeDecalPrepassCheck(const std::string& path, ux3d::imgui::ViewportInstance& viewport);
    int drainGLErrors(const char* where);

    static double wallClockMs()
    {
        return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count()) /
               1000.0;
    }

    // Scene, rendering, UI and loop lifetime.

    GLFWwindow* window = nullptr;
    std::unique_ptr<ux3d::renderer::GladGL> gladGL;
    ux3d::imgui::GridRenderer gridRenderer;
    ux3d::imgui::BoundingBoxRenderer bboxRenderer;
    ux3d::gltf::Gltf gltfModel;
    ux3d::gltf::Scene* activeScene = nullptr;
    std::vector<ux3d::imgui::SceneEnvironmentState> sceneEnvironments{1};
    ux3d::imgui::SelectionState selection;
    std::string currentFilePath;
    ux3d::imgui::SceneRevisions sceneRevisions;
    uint64_t lastRevisionMarkCount = 0;
    uint64_t sceneSyncCount = 0;
    uint64_t skippedSceneSyncCount = 0;
    uint64_t animationOnlySyncCount = 0;
    uint32_t lastFrameSceneChanges = 0;
    ux3d::imgui::AnimationPlayer animationPlayer;
    ux3d::filesystem::Path rendererResourcesDir{ux3d::imgui::StdFileSystem::instance()};
    std::unique_ptr<ux3d::renderer::GltfRenderLayer> gltfLayer;
    ux3d::imgui::TransmissionTarget transmissionTarget;
    ux3d::imgui::Framebuffer* transmissionSource = nullptr;
    ux3d::imgui::Framebuffer* transmissionDestination = nullptr;
    ux3d::imgui::Framebuffer glassCopyFbo;
    bool ssrCaptureDepth = false;
    bool ssrUseDecalDepth = false;
    float ssrInteriorSky = 0.5f;
    std::unique_ptr<ux3d::renderer::CommandGenerator> commandGenerator;
    std::unique_ptr<ux3d::renderer::Renderer> glRenderer;
    std::unique_ptr<ux3d::renderer::PickingRenderLayer> pickingLayer;
    ux3d::imgui::Framebuffer pickingTarget;
    // A2.1: one decal buffer and its prepass layer for all viewports.
    std::unique_ptr<ux3d::renderer::DecalPrepassLayer> decalPrepassLayer;
    // A2.2: the decal accumulation pass; it removes the decal draws from the forward scene.
    std::unique_ptr<ux3d::renderer::DecalAccumulationLayer> decalAccumulationLayer;
    ux3d::imgui::DecalBuffer decalBuffer;
    ux3d::renderer::ReverseMapping currentReverseMapping;
    std::vector<std::unique_ptr<ux3d::imgui::ViewportInstance>> viewports;
    ux3d::imgui::ViewportInstance* activeViewport = nullptr;
    std::string scenarioEnvironmentPath;
    bool scenarioEnvironmentLoaded = false;
    double lastFrameTime = 0.0;
    uint64_t frameCount = 0;
    ux3d::imgui::profiling::Clock::time_point previousFrameStart;
    uint32_t idleQuietFrames = 0;
    double idleWaitSeconds = 0.0;
    std::vector<double> idleFrameStartsMs;
    std::vector<double> idleWakesMs;
    ux3d::imgui::IdleWaitStats idleWaitStats;
    bool frameInProgress = false;
    bool loadingActive = false;
    bool pendingLoadFirstFrame = false;
    ux3d::imgui::profiling::Clock::time_point pendingLoadStarted;
    uint32_t loadedTextureCap = 0;
    GLuint textureRefreshSnapshot = 0;
    int textureRefreshSnapshotWidth = 0;
    int textureRefreshSnapshotHeight = 0;
    std::string queuedLoadPath;
    bool queuedLoadWithoutTextures = false;
    std::string loadingCapturePath;
    bool queuedLoadTestUsed = false;
    std::mutex consoleWriteMutex;
    bool pendingNewViewport = false;
    const ux3d::gltf::Node* pendingNewViewportCamera = nullptr;
    bool showDemoWindow = false;
    bool showInspector = true;
    bool showHierarchy = true;
    bool showResources = true;
    bool showLogger = false;
    bool showStatistics = false;
    bool showValidation = false;
    bool showSearchPaths = false;
    bool s_firstLayout = true;
    bool s_resetLayout = false;
    // The layout baseline is captured a few frames after startup or a reset, once
    // docking has settled. Only a real change from it is written to settings.ini.
    std::string layoutBaseline;
    int layoutBaselineCountdown = 3;
    bool layoutPersisted = false;
    std::string modelToLoad;
    std::string dumpFramePath = "";
    std::string dumpNextFramePath; // Optional consecutive-frame overlay diagnostic
    std::string s_decalPrepassCheckPath;
    bool s_decalPrepassCheckAttempted = false;
    // A2.1 check: the forward depth and Geometry Normal colour are read right after the
    // forward scene draw, before the grid, bounding-box and picking overlays.
    std::vector<float> s_decalCheckForwardDepth;
    std::vector<uint8_t> s_decalCheckForwardColor;
    int s_decalCheckWidth = 0;
    int s_decalCheckHeight = 0;
    bool s_decalCheckCaptured = false;
    uint64_t s_glErrorTotal = 0;
    std::string uiScriptPath;
    std::string uiReportPath;
    ux3d::imgui::UiScenario uiScenario;
    int dumpFrameCountdown = -1;
    bool exitAfterDump = false;
    bool loggerWrapArg = false;
    bool loggerAutoScrollArg = true;
    std::string scenarioReportPath;
    std::string scenarioName = "default";
    bool modelLoaded = false;

    // Automation and diagnostic controls.
    // P0.1 deterministic scenario state. When enabled, no user-persisted window, layout,
    // recent-file or application settings are read or written.
    bool s_isolatedState = false;
    bool s_windowSizeOverridden = false;
    static constexpr uint64_t SCENARIO_REPORT_FRAME = 8;

    // P0.4: requested swap interval. This must be known before the window is
    // created so it can be applied before the first frame is presented.
    bool s_vsyncEnabled = true;

    // P0.2 CPU/counter profiling. Detailed instrumentation is opt-in so ordinary
    // interactive runs do not pay for timers, query polling or counter updates.
    bool s_profilingEnabled = false;
    std::string s_profileReportPath;
    bool s_exitAfterProfile = false;
    bool s_profileReportAttempted = false;
    bool s_profileReportDeferredLogged = false;

    // P4.3 diagnostics. --fixed-frame-time advances playback by a constant step per
    // frame so runs are reproducible; --force-full-sync restores the P4.1 mapping
    // (any pending change runs the full renderer sync) as an A/B reference.
    float s_fixedFrameTime = -1.0f;
    bool s_forceFullSync = false;
    // A2.2 material-edit cost check: the material whose emissive factor is changed every
    // frame, and the resolved glTF material (null until the model is loaded).
    std::string s_materialEditEmissiveMaterial;
    ux3d::gltf::Material* s_materialEditTarget = nullptr;
    // P4.6: --disable-command-caches turns off the command-generation caches so the
    // same binary can serve as its own uncached A/B reference.
    bool s_disableCommandCaches = false;
    // P5.1: --always-pick renders the picking pass every frame (the pre-P5.1 schedule)
    // as a same-binary A/B reference. Totals feed the UI snapshot's revisions object.
    bool s_alwaysPick = false;
    // P5.2: --full-size-picking restores the per-viewport full-size picking buffer drawn
    // with the scene (the P5.1 path); --always-pick implies it.
    bool s_fullSizePicking = false;
    uint64_t s_pickingRenderTotal = 0;
    uint64_t s_pickingSkipTotal = 0;
    uint64_t s_pickReadTotal = 0;
    uint64_t s_pickUnservedTotal = 0;
    // P8.5: a click draws only the drawables whose bounds can reach the clicked pixel's
    // frustum. --disable-pick-culling draws all of them (the P5.2 path) as an A/B reference.
    bool s_disablePickCulling = false;
    // P8.3: --disable-instancing draws every drawable separately (the P8.1 path) as an A/B reference.
    bool s_disableInstancing = false;
    bool s_disableWireframeCache = false;
    // --msfs-emissive off: ASOBO assets use the clamped core emissive factor (A/B reference).
    bool s_disableMsfsEmissive = false;
    // --disable-shared-traversal: every viewport traverses the scene itself (A/B reference).
    bool s_disableSharedTraversal = false;
    uint64_t s_pickCulledTotal = 0;
    // P5.3: a viewport whose render key is unchanged shows its last image again.
    // --disable-redraw-skip redraws every frame (the pre-P5.3 schedule). TAA keeps
    // accumulating until this many consecutive renders with an unchanged key (8 full
    // cycles of its 8-phase jitter), then holds its image.
    bool s_disableRedrawSkip = false;
    static constexpr uint32_t kTaaSettleRenders = 64;
    uint64_t s_sceneRenderTotal = 0;
    uint64_t s_sceneSkipTotal = 0;
    // P5.4: when nothing needs a new frame, the loop blocks in waitForEvents (IdleWait.h)
    // instead of polling. Input and window events wake it at once; the timeout is a
    // catch-all. --disable-idle-wait polls every frame (the pre-P5.4 loop).
    // --idle-stats writes frame start times and wait counts at exit for idle tests.
    bool s_disableIdleWait = false;
    // Renders a frame from the framebuffer-size callback during a live window resize.
    std::function<void()> s_resizeFrame;
    static constexpr double kIdleWaitTimeoutSeconds = 0.25;
    static constexpr uint32_t kIdleQuietFrames = 3;
    std::string s_idleStatsPath;
    // P6.1: --validation-report writes each load's validation report (no timings) as JSON.
    std::string s_validationReportPath;
    // P6.2: image decode memory budget. Default: an eighth of physical memory, at least
    // 512 MiB. --decode-budget-mib N overrides it (0 = unlimited, the pre-P6.2 behaviour).
    int64_t s_decodeBudgetMiB = -1;
    // P5.5: viewport scene builds leave out primitives outside the camera frustum.
    // --disable-culling draws every primitive (the pre-P5.5 scene).
    bool s_disableCulling = false;
    uint64_t s_culledTotal = 0;
    uint64_t s_uncullableTotal = 0;
    std::deque<ux3d::imgui::profiling::FrameSample> s_frameSamples;
    uint64_t s_frameSamplesDropped = 0;
    ux3d::imgui::profiling::FrameSample s_firstFrameSample;
    bool s_haveFirstFrameSample = false;
    ux3d::imgui::profiling::CaptureRecord s_captureRecord;

    // P0.5 viewport-region capture. The steady capture is selected by frame or by
    // elapsed wall-clock time; the first rendered frame can be captured separately.
    std::string s_captureViewportPath;
    uint64_t s_captureFrame = 0;
    bool s_captureFrameProvided = false;
    double s_captureTimeSeconds = -1.0;
    bool s_captureTimeProvided = false;
    std::string s_captureFirstFramePath;
    bool s_captureAttempted = false;
    bool s_firstFrameCaptureAttempted = false;
    ux3d::imgui::profiling::CaptureRecord s_firstFrameCaptureRecord;
    double s_captureEpochSeconds = 0.0;

    // P0.4 bounded benchmark. The benchmark records lightweight frame timing even
    // when detailed P0.2 instrumentation is off. Warmup frames are rendered but not
    // retained; measured frames and the first rendered frame are reported separately.
    bool s_benchmarkEnabled = false;
    std::string s_benchmarkOutputPath;
    uint64_t s_benchmarkWarmupFrames = 8;
    uint64_t s_benchmarkMeasuredFrames = 120;
    bool s_exitAfterBenchmark = false;
    bool s_benchmarkReportAttempted = false;
    bool s_benchmarkReportWritten = false;
    std::vector<ux3d::imgui::profiling::FrameSample> s_benchmarkSamples;

    ux3d::imgui::SharedOverrideState s_sharedOverrides;
    uint64_t s_overrideApplyTotal = 0; // cumulative, for UI-script snapshots
    uint64_t s_overrideSkipTotal = 0;

    bool s_outputFailed = false;
    ConsoleLogStore s_consoleLogs;
    ConsoleLogLayoutCache s_consoleLogLayout;
    BoundsCache s_boundsCache;
    uint64_t s_boundsComputedTotal = 0;
    uint64_t s_boundsHitTotal = 0;
    std::vector<std::string> s_recentFiles;
    ImGuiID s_centralDockId = 0;

    // Stable ImGui window names used by docking and panel rendering.
    static constexpr const char* WND_HIERARCHY = ICON_MDI_FILE_TREE " Scene Hierarchy";
    static constexpr const char* WND_RESOURCES = ICON_MDI_FOLDER_MULTIPLE_OUTLINE " Resources";
    static constexpr const char* WND_INSPECTOR = ICON_MDI_TUNE " Inspector";
    static constexpr const char* WND_LOGGER = ICON_MDI_CONSOLE " Logger";
    static constexpr const char* WND_STATISTICS = ICON_MDI_CHART_BAR " Statistics";
    static constexpr const char* WND_VALIDATION = ICON_MDI_SHIELD_CHECK_OUTLINE " Validation###ValidationPanel";
};

} // namespace ux3d::imgui::application
