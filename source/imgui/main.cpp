// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include <imgui_internal.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <ImGuizmo.h>
#include <portable-file-dialogs.h>

#include "imgui/GladGL.h"
#include "imgui/Framebuffer.h"
#include "imgui/GridRenderer.h"
#include "imgui/StdFileSystem.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/Profiling.h"
#include "renderViewInput/CameraActor.h"

#include "gltf/Gltf.h"
#include "gltf/GltfParsing.h"
#include "gltf/Utils.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"
#include "gltf/Mesh.h"
#include "gltf/Accessor.h"
#include "gltf/GlTypes.h"
#include "gltf/Material.h"
#include "gltf/ExportOptions.h"
#include "gltf/ImportOptions.h"

#include "renderer/Renderer.h"
#include "renderer/gl/Sampler.h"
#include "renderer/CommandExecutor.h"
#include "renderer/Camera.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/CommandGenerator.h"
#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/PickingRenderLayer.h"

#include "core/Logger.h"

#include "imgui/MaterialInspector.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/StatisticsPanel.h"
#include "imgui/ValidationPanel.h"
#include "imgui/GltfValidator.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/ElementInspector.h"
#include "imgui/AnimationPlayer.h"
#include "imgui/AntiAliasing.h"
#include "imgui/SamplerTest.h"
#include "imgui/VaoTest.h"
#include "imgui/PolygonModeTest.h"
#include "imgui/CullWindingTest.h"
#include "imgui/DepthTest.h"
#include "imgui/BlendTest.h"
#include "imgui/BoundingBoxRenderer.h"
#include "imgui/Theme.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/AppSettings.h"
#include "imgui/SearchPathsPanel.h"
#include "imgui/SceneHierarchyPanel.h"
#include "imgui/SelfCheckResult.h"
#include "imgui/StatusBar.h"
#include "imgui/ViewportInstance.h"
#include "imgui/UiScenarioHarness.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#include "imgui/stb_image_write.h"
#include <cctype>
#include <deque>

static bool s_outputFailed = false;

// Self-check exit contract:
//   0 = all checks passed and the report was written
//   1 = one or more checks failed (a FAIL report was still written)
//   2 = the report could not be opened/written
static int writeSelfCheckReport(const ux3d::imgui::SelfCheckResult& result,
                                const std::string& path,
                                const char* label)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        std::fprintf(stderr, "Could not open %s self-check output '%s'\n", label, path.c_str());
        return 2;
    }

    output << result.report;
    output.flush();
    if (!output.good()) {
        std::fprintf(stderr, "Could not write %s self-check output '%s'\n", label, path.c_str());
        return 2;
    }
    output.close();
    if (!output.good()) {
        std::fprintf(stderr, "Could not close %s self-check output '%s'\n", label, path.c_str());
        return 2;
    }

    if (!result.success) {
        std::fprintf(stderr, "%s self-check failed; report: %s\n", label, path.c_str());
        return 1;
    }
    return 0;
}

static bool saveImage(const char* filename, int width, int height, const uint8_t* rgba) {
    if (!filename || width <= 0 || height <= 0 || !rgba) return false;

    // OpenGL glReadPixels returns bottom-to-top image; flip vertically to save right-side up
    stbi_flip_vertically_on_write(1);

    std::string fn = filename;
    std::string ext = "";
    auto dotPos = fn.find_last_of('.');
    if (dotPos != std::string::npos) {
        ext = fn.substr(dotPos);
        for (char& c : ext) c = static_cast<char>(std::tolower(c));
    }

    if (ext == ".jpg" || ext == ".jpeg") {
        return stbi_write_jpg(filename, width, height, 4, rgba, 90) != 0;
    }
    if (ext == ".png") {
        return stbi_write_png(filename, width, height, 4, rgba, width * 4) != 0;
    }
    return stbi_write_bmp(filename, width, height, 4, rgba) != 0;
}

static uint64_t hashBytesFnv1a64(const uint8_t* data, size_t size) {
    uint64_t value = 14695981039346656037ULL;
    for (size_t i = 0; i < size; ++i) {
        value ^= static_cast<uint64_t>(data[i]);
        value *= 1099511628211ULL;
    }
    return value;
}

static bool captureViewportTexture(GLuint texture,
                                   int width,
                                   int height,
                                   const std::string& path,
                                   const std::string& region,
                                   uint64_t frame,
                                   bool firstFrame,
                                   double timeSeconds,
                                   ux3d::imgui::profiling::CaptureRecord& record)
{
    record = ux3d::imgui::profiling::CaptureRecord();
    record.firstFrame = firstFrame;
    record.source = "viewport-texture";
    record.region = region;
    record.path = path;
    record.frame = frame;
    record.width = width;
    record.height = height;
    record.timeSeconds = timeSeconds;

    if (texture == 0 || width <= 0 || height <= 0 || path.empty()) {
        std::fprintf(stderr, "Capture skipped: invalid texture, size or path\n");
        return false;
    }

    const std::filesystem::path outputPath = std::filesystem::u8path(path);
    if (outputPath.has_parent_path()) {
        std::error_code directoryError;
        std::filesystem::create_directories(outputPath.parent_path(), directoryError);
    }

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
    GLint previousTexture = 0;
    GLint previousPackAlignment = 4;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPackAlignment);
    while (glGetError() != GL_NO_ERROR) {
    }

    const auto readbackStart = ux3d::imgui::profiling::Clock::now();
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const GLenum readError = glGetError();
    glPixelStorei(GL_PACK_ALIGNMENT, previousPackAlignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    record.readbackMs = ux3d::imgui::profiling::elapsedMs(readbackStart);

    if (readError != GL_NO_ERROR) {
        std::fprintf(stderr, "Capture failed: glGetTexImage error 0x%X\n", readError);
        return false;
    }

    record.rgbaHash = hashBytesFnv1a64(pixels.data(), pixels.size());
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    const auto writeStart = ux3d::imgui::profiling::Clock::now();
    const bool wroteImage = saveImage(path.c_str(), width, height, pixels.data());
    record.writeMs = ux3d::imgui::profiling::elapsedMs(writeStart);
    if (!wroteImage) {
        std::fprintf(stderr, "Capture failed: could not write %s\n", path.c_str());
        return false;
    }

    std::error_code sizeError;
    const uint64_t fileBytes = static_cast<uint64_t>(std::filesystem::file_size(outputPath, sizeError));
    if (sizeError) {
        std::fprintf(stderr, "Capture failed: could not read output size for %s\n", path.c_str());
        return false;
    }

    record.fileBytes = fileBytes;
    record.valid = true;
    std::printf("Captured viewport frame %llu to %s (%dx%d, %llu bytes, readback %.3f ms, write %.3f ms)\n",
                static_cast<unsigned long long>(frame),
                path.c_str(),
                width,
                height,
                static_cast<unsigned long long>(fileBytes),
                record.readbackMs,
                record.writeMs);
    return true;
}

// Log console message history
using ux3d::imgui::LogEntry;
using ux3d::imgui::LogSeverityCounts;

// P1.5: keep severity totals in step with the append/clear path so the status
// bar never rescans the retained log on every frame. The entries vector remains
// the single source of truth for display and Copy.
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

static ConsoleLogStore s_consoleLogs;

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

static ConsoleLogLayoutCache s_consoleLogLayout;

static ImVec2 measureConsoleLogText(const LogEntry& entry,
                                    ImFont* font,
                                    float fontSize,
                                    bool wrap,
                                    float wrapWidth)
{
    const char* textBegin = entry.text.c_str();
    const char* textEnd = textBegin + entry.text.size();
    return font->CalcTextSizeA(fontSize,
                               std::numeric_limits<float>::max(),
                               wrap ? wrapWidth : 0.0f,
                               textBegin,
                               textEnd);
}

static bool consoleLogLayoutSettingsMatch(const ConsoleLogLayoutCache& cache,
                                          ImFont* font,
                                          float fontSize,
                                          bool wrap,
                                          float wrapWidth,
                                          float itemSpacingY)
{
    return cache.valid
        && cache.font == font
        && cache.fontSize == fontSize
        && cache.wrap == wrap
        && cache.wrapWidth == wrapWidth
        && cache.itemSpacingY == itemSpacingY;
}

static void appendConsoleLogLayoutRows(ConsoleLogLayoutCache& cache,
                                       const std::vector<LogEntry>& entries,
                                       size_t startIndex)
{
    for (size_t i = startIndex; i < entries.size(); ++i) {
        const LogEntry& entry = entries[i];
        const ImVec2 textSize = measureConsoleLogText(entry, cache.font, cache.fontSize, cache.wrap, cache.wrapWidth);
        const float rowHeight = std::max(cache.fontSize, textSize.y);
        if (!cache.rowHeights.empty()) {
            cache.contentHeight += cache.itemSpacingY;
        }
        cache.rowOffsets.push_back(cache.contentHeight);
        cache.rowHeights.push_back(rowHeight);
        cache.rowEnds.push_back(cache.contentHeight + rowHeight);
        cache.contentHeight += rowHeight;
        cache.maxTextWidth = std::max(cache.maxTextWidth, textSize.x);
        cache.allSingleLine = cache.allSingleLine
            && entry.text.find_first_of("\r\n") == std::string::npos;
        ++cache.appendedRowsMeasured;
    }
}

static void ensureConsoleLogLayout(ConsoleLogLayoutCache& cache,
                                   const ConsoleLogStore& store,
                                   ImFont* font,
                                   float fontSize,
                                   bool wrap,
                                   float wrapWidth,
                                   float itemSpacingY)
{
    const auto& entries = store.entries();
    const bool sameSettings = consoleLogLayoutSettingsMatch(cache, font, fontSize, wrap, wrapWidth, itemSpacingY);
    const bool canExtend = sameSettings
        && cache.clearGeneration == store.clearGeneration()
        && cache.entryCount <= entries.size();
    const bool contentChanged = !cache.valid
        || cache.revision != store.revision()
        || cache.entryCount != entries.size()
        || !sameSettings
        || cache.clearGeneration != store.clearGeneration();

    if (!contentChanged) {
        return;
    }

    if (canExtend) {
        appendConsoleLogLayoutRows(cache, entries, cache.entryCount);
    } else {
        cache.valid = true;
        cache.font = font;
        cache.fontSize = fontSize;
        cache.wrap = wrap;
        cache.wrapWidth = wrapWidth;
        cache.itemSpacingY = itemSpacingY;
        cache.allSingleLine = true;
        cache.maxTextWidth = 0.0f;
        cache.contentHeight = 0.0f;
        cache.rowOffsets.clear();
        cache.rowHeights.clear();
        cache.rowEnds.clear();
        ++cache.fullRebuilds;
        appendConsoleLogLayoutRows(cache, entries, 0);
    }

    cache.revision = store.revision();
    cache.clearGeneration = store.clearGeneration();
    cache.entryCount = entries.size();
}

static std::pair<size_t, size_t> visibleConsoleLogRows(const ConsoleLogLayoutCache& cache,
                                                      float scrollY,
                                                      float viewportHeight)
{
    if (cache.rowEnds.empty() || viewportHeight <= 0.0f) {
        return { 0, 0 };
    }

    const auto first = std::upper_bound(cache.rowEnds.begin(), cache.rowEnds.end(), scrollY);
    const auto last = std::lower_bound(cache.rowOffsets.begin(), cache.rowOffsets.end(), scrollY + viewportHeight);
    return {
        static_cast<size_t>(std::distance(cache.rowEnds.begin(), first)),
        static_cast<size_t>(std::distance(cache.rowOffsets.begin(), last))
    };
}

static std::string buildConsoleLogCopyText(const std::vector<LogEntry>& entries)
{
    std::string allLogs;
    for (const auto& entry : entries) {
        allLogs += entry.text;
        allLogs += "\n";
    }
    return allLogs;
}

static ux3d::imgui::SelfCheckResult runConsoleLogLayoutSelfCheck(bool forceFailure)
{
    ux3d::imgui::SelfCheckResult result;
    int checks = 0;
    std::vector<std::string> failures;
    const auto check = [&](bool condition, const std::string& message) {
        ++checks;
        if (!condition && failures.size() < 20) {
            failures.push_back(message);
        }
    };

    ConsoleLogStore store;
    store.append({ "single row", ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Info });
    store.append({ "first line\nsecond line", ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Warning });
    store.append({ std::string(160, 'W'), ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Error });
    check(store.counts().info == 1 && store.counts().warnings == 1 && store.counts().errors == 1,
          "append did not preserve severity totals");
    check(buildConsoleLogCopyText(store.entries())
              == "single row\nfirst line\nsecond line\n" + std::string(160, 'W') + "\n",
          "Copy text omitted or changed a stored entry");

    ConsoleLogLayoutCache layout;
    ImFont* monoFont = ux3d::imgui::getMonoFont();
    ImGui::PushFont(monoFont);
    const float fontSize = ImGui::GetFontSize();
    const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize, false, 0.0f, itemSpacingY);
    check(layout.rowHeights.size() == 3 && !layout.allSingleLine,
          "unwrapped multiline entries were not classified as variable-height");
    check(layout.rowHeights.size() == 3 && layout.rowHeights[1] > layout.rowHeights[0],
          "embedded newline did not increase the cached row height");
    check(layout.maxTextWidth > fontSize * 80.0f,
          "content-width cache omitted a long off-screen log entry");

    const size_t rebuildsBeforeWarmLayout = layout.fullRebuilds;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize, false, 0.0f, itemSpacingY);
    check(layout.fullRebuilds == rebuildsBeforeWarmLayout,
          "unchanged logger content rebuilt its warm layout");

    const size_t rebuildsBeforeUnwrappedResize = layout.fullRebuilds;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize, false, 500.0f, itemSpacingY);
    check(layout.wrapWidth == 500.0f && layout.fullRebuilds == rebuildsBeforeUnwrappedResize + 1,
          "available-width change did not invalidate the unwrapped layout");

    if (layout.rowOffsets.size() == 3 && layout.rowHeights.size() == 3) {
        const auto top = visibleConsoleLogRows(layout, 0.0f, layout.rowHeights[0] + 1.0f);
        const auto middle = visibleConsoleLogRows(layout, layout.rowOffsets[1], layout.rowHeights[1] + 1.0f);
        const auto bottom = visibleConsoleLogRows(layout, layout.rowOffsets[2], layout.rowHeights[2]);
        check(top.first == 0 && top.second == 1, "top visible-row range included a non-visible row");
        check(middle.first == 1 && middle.second == 2, "middle visible-row range lost its source row");
        check(bottom.first == 2 && bottom.second == 3, "bottom visible-row range lost its last row");
    } else {
        check(false, "visible-row range checks require three cached rows");
    }

    const size_t fullRebuildsBeforeWrap = layout.fullRebuilds;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize, true, 80.0f, itemSpacingY);
    const float narrowWrappedHeight = layout.rowHeights.size() == 3 ? layout.rowHeights[2] : 0.0f;
    check(layout.wrap && layout.wrapWidth == 80.0f && layout.fullRebuilds == fullRebuildsBeforeWrap + 1,
          "wrap toggle did not rebuild variable-height layout");
    check(narrowWrappedHeight > fontSize, "long wrapped entry did not occupy multiple lines");

    ensureConsoleLogLayout(layout, store, monoFont, fontSize, true, 160.0f, itemSpacingY);
    const float wideWrappedHeight = layout.rowHeights.size() == 3 ? layout.rowHeights[2] : 0.0f;
    check(layout.wrapWidth == 160.0f && wideWrappedHeight < narrowWrappedHeight,
          "available-width change did not update wrapped row height");

    const size_t rebuildsBeforeFontChange = layout.fullRebuilds;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize + 2.0f, true, 160.0f, itemSpacingY);
    check(layout.fullRebuilds == rebuildsBeforeFontChange + 1
              && layout.rowHeights.size() == 3
              && layout.rowHeights[0] > fontSize,
          "font-size change did not invalidate and rebuild row heights");

    const size_t rebuildsBeforeAppend = layout.fullRebuilds;
    const size_t appendedRowsBeforeAppend = layout.appendedRowsMeasured;
    store.append({ "appended\nentry", ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Fatal });
    ensureConsoleLogLayout(layout, store, monoFont, fontSize + 2.0f, true, 160.0f, itemSpacingY);
    check(layout.fullRebuilds == rebuildsBeforeAppend && layout.entryCount == 4
              && layout.appendedRowsMeasured == appendedRowsBeforeAppend + 1,
          "append failed to extend the cached layout without rebuilding existing rows");
    check(store.counts().errors == 2
              && buildConsoleLogCopyText(store.entries()).find("appended\nentry\n") != std::string::npos,
          "append changed severity totals or Copy contents");

    store.clear();
    ensureConsoleLogLayout(layout, store, monoFont, fontSize + 2.0f, true, 160.0f, itemSpacingY);
    const auto emptyRange = visibleConsoleLogRows(layout, 0.0f, 100.0f);
    check(store.size() == 0 && store.counts().errors == 0 && store.counts().warnings == 0
              && store.counts().info == 0 && buildConsoleLogCopyText(store.entries()).empty(),
          "Clear did not reset entries, severity totals and Copy contents");
    check(layout.rowOffsets.empty() && emptyRange.first == 0 && emptyRange.second == 0,
          "empty log layout produced visible rows");

    ConsoleLogStore fixedStore;
    fixedStore.append({ "single fixed-height row", ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Info });
    ConsoleLogLayoutCache fixedLayout;
    ensureConsoleLogLayout(fixedLayout, fixedStore, monoFont, fontSize, false, 500.0f, itemSpacingY);
    check(fixedLayout.allSingleLine && fixedLayout.rowHeights.size() == 1,
          "single-line logger entries were not classified for fixed-height clipping");

    ImGui::PopFont();
    if (forceFailure) {
        check(false, "forced failure requested");
    }

    result.success = failures.empty();
    std::ostringstream report;
    report << "Logger layout self-check\n"
           << "Checks: " << checks << "\n"
           << "Result: " << (result.success ? "PASS" : "FAIL") << "\n";
    for (const auto& failure : failures) {
        report << "Failure: " << failure << "\n";
    }
    result.report = report.str();
    return result;
}

// P0.1 deterministic scenario state. When enabled, no user-persisted window, layout,
// recent-file or application settings are read or written.
static bool s_isolatedState = false;
static bool s_windowSizeOverridden = false;
constexpr uint64_t SCENARIO_REPORT_FRAME = 8;

// P0.4: requested swap interval. This must be known before the window is
// created so it can be applied before the first frame is presented.
static bool s_vsyncEnabled = true;

// P0.2 CPU/counter profiling. Detailed instrumentation is opt-in so ordinary
// interactive runs do not pay for timers, query polling or counter updates.
static bool s_profilingEnabled = false;
static std::string s_profileReportPath;
static bool s_exitAfterProfile = false;
static bool s_profileReportAttempted = false;
static bool s_profileReportDeferredLogged = false;
static std::deque<ux3d::imgui::profiling::FrameSample> s_frameSamples;
static uint64_t s_frameSamplesDropped = 0;
static ux3d::imgui::profiling::FrameSample s_firstFrameSample;
static bool s_haveFirstFrameSample = false;
static ux3d::imgui::profiling::CaptureRecord s_captureRecord;

// P0.5 viewport-region capture. The steady capture is selected by frame or by
// elapsed wall-clock time; the first rendered frame can be captured separately.
static std::string s_captureViewportPath;
static uint64_t s_captureFrame = 0;
static bool s_captureFrameProvided = false;
static double s_captureTimeSeconds = -1.0;
static bool s_captureTimeProvided = false;
static std::string s_captureFirstFramePath;
static bool s_captureAttempted = false;
static bool s_firstFrameCaptureAttempted = false;
static ux3d::imgui::profiling::CaptureRecord s_firstFrameCaptureRecord;
static double s_captureEpochSeconds = 0.0;

// P0.4 bounded benchmark. The benchmark records lightweight frame timing even
// when detailed P0.2 instrumentation is off. Warmup frames are rendered but not
// retained; measured frames and the first rendered frame are reported separately.
static bool s_benchmarkEnabled = false;
static std::string s_benchmarkOutputPath;
static uint64_t s_benchmarkWarmupFrames = 8;
static uint64_t s_benchmarkMeasuredFrames = 120;
static bool s_exitAfterBenchmark = false;
static bool s_benchmarkReportAttempted = false;
static bool s_benchmarkReportWritten = false;
static std::vector<ux3d::imgui::profiling::FrameSample> s_benchmarkSamples;

static void logMessage(const std::string& msg, ImVec4 color = ImVec4(0.85f, 0.85f, 0.88f, 1.0f), ux3d::core::LogLevel level = ux3d::core::LogLevel::Info)
{
    s_consoleLogs.append({ msg, color, level });
    std::fprintf(stderr, "%s\n", msg.c_str());
    std::fflush(stderr);
}

class ConsoleLogCallback : public ux3d::core::LogCallback {
public:
    ConsoleLogCallback() : LogCallback(ux3d::core::LogLevel::All) {}
    void log(ux3d::core::LogLevel level, const char* message) override {
        if (!message) return;
        ImVec4 color(0.85f, 0.85f, 0.88f, 1.0f);
        if (level == ux3d::core::LogLevel::Warning) {
            color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
        } else if (level == ux3d::core::LogLevel::Error || level == ux3d::core::LogLevel::Fatal) {
            color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
        } else if (level == ux3d::core::LogLevel::Info) {
            color = ImVec4(0.5f, 0.85f, 1.0f, 1.0f);
        }
        logMessage(message, color, level);
    }
};

static void glfw_error_callback(int error, const char* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static ux3d::gltf::Extends getNodeExtends(ux3d::gltf::Node* node,
                                          const ux3d::gltf::Gltf* gltfModel,
                                          const ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (node == nullptr || gltfModel == nullptr) {
        return ux3d::gltf::Extends();
    }
    ux3d::math::dmat4 parentTransform = (gltfLayer != nullptr)
        ? ux3d::math::dmat4(gltfLayer->getParentWorldTransform(node))
        : ux3d::math::identity<ux3d::math::dmat4>();
    return ux3d::gltf::utils::getExtends({ node }, gltfModel, parentTransform);
}

// Oriented Bounding Box calculation for selected node matching Blender's local bounding box
struct NodeOBB {
    glm::vec3 localMin{ std::numeric_limits<float>::max() };
    glm::vec3 localMax{ std::numeric_limits<float>::lowest() };
    glm::mat4 modelMatrix{ 1.0f };
    bool valid = false;
};

static void accumulateNodeOBB(const ux3d::gltf::Node* node,
                              const glm::mat4& invRootWorld,
                              const ux3d::renderer::GltfRenderLayer* gltfLayer,
                              NodeOBB& obb)
{
    if (node == nullptr) return;

    glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
    glm::mat4 toRootLocal = invRootWorld * nodeWorld;

    const auto* mesh = node->getMesh();
    if (mesh != nullptr) {
        for (const auto& primitive : mesh->getPrimitives()) {
            for (const auto& attribute : primitive.getAttributes()) {
                if (attribute.getName() != "POSITION") continue;
                const auto* accessor = attribute.getAccessor();
                if (accessor == nullptr) continue;
                if (accessor->getMin().size() < 3 || accessor->getMax().size() < 3) continue;

                glm::vec3 pMin(static_cast<float>(accessor->getMin()[0]),
                               static_cast<float>(accessor->getMin()[1]),
                               static_cast<float>(accessor->getMin()[2]));
                glm::vec3 pMax(static_cast<float>(accessor->getMax()[0]),
                               static_cast<float>(accessor->getMax()[1]),
                               static_cast<float>(accessor->getMax()[2]));

                if (accessor->isNormalized()) {
                    float minVal = std::abs(static_cast<float>(ux3d::gltf::GlTypes::minimumValueFromComponentType(accessor->getComponentType())));
                    float maxVal = std::abs(static_cast<float>(ux3d::gltf::GlTypes::maximumValueFromComponentType(accessor->getComponentType())));
                    if (minVal != 0.0f) pMin /= minVal;
                    if (maxVal != 0.0f) pMax /= maxVal;
                }

                glm::vec3 corners[8] = {
                    { pMin.x, pMin.y, pMin.z },
                    { pMax.x, pMin.y, pMin.z },
                    { pMax.x, pMin.y, pMax.z },
                    { pMin.x, pMin.y, pMax.z },
                    { pMin.x, pMax.y, pMin.z },
                    { pMax.x, pMax.y, pMin.z },
                    { pMax.x, pMax.y, pMax.z },
                    { pMin.x, pMax.y, pMax.z }
                };

                for (const auto& c : corners) {
                    glm::vec4 pt = toRootLocal * glm::vec4(c, 1.0f);
                    obb.localMin = glm::min(obb.localMin, glm::vec3(pt));
                    obb.localMax = glm::max(obb.localMax, glm::vec3(pt));
                    obb.valid = true;
                }
            }
        }
    }

    for (const auto* child : node->getChildren()) {
        accumulateNodeOBB(child, invRootWorld, gltfLayer, obb);
    }
}

static NodeOBB getNodeOBB(const ux3d::gltf::Node* selectedNode,
                          const ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    NodeOBB obb;
    if (selectedNode == nullptr) return obb;

    obb.modelMatrix = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(selectedNode)) : glm::mat4(selectedNode->getTransformMatrix());
    float det = glm::determinant(obb.modelMatrix);
    glm::mat4 invRootWorld = (std::abs(det) > 1e-7f) ? glm::inverse(obb.modelMatrix) : glm::mat4(1.0f);

    accumulateNodeOBB(selectedNode, invRootWorld, gltfLayer, obb);

    if (obb.valid) {
        for (int i = 0; i < 3; ++i) {
            if (obb.localMax[i] - obb.localMin[i] < 1e-5f) {
                obb.localMin[i] -= 0.001f;
                obb.localMax[i] += 0.001f;
            }
        }
    }

    return obb;
}

// Focus and look at node using Gestaltor's native CameraActor
static void focusCameraOnNode(ux3d::gltf::Node* node,
                              ux3d::gltf::Gltf& gltfModel,
                              ux3d::renderer::GltfRenderLayer* gltfLayer,
                              ux3d::editor::CameraActor& cameraActor,
                              ux3d::imgui::AntiAliasing& antiAliasing,
                              bool& sceneDirty)
{
    if (node == nullptr) return;
    cameraActor.presentFocusNode(gltfLayer, node, &gltfModel);
    antiAliasing.resetHistory();
    sceneDirty = true;
}


// Recent Files Persistence
static std::vector<std::string> s_recentFiles;

static std::string getRecentFilesConfigPath() {
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && std::strlen(appData) > 0) {
        std::filesystem::path dir = std::filesystem::path(appData) / "Glider";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::path newPath = dir / "recent_files.txt";
        if (!std::filesystem::exists(newPath)) {
            std::filesystem::path oldPath = std::filesystem::path(appData) / "Gestaltor" / "recent_files.txt";
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::copy_file(oldPath, newPath, ec);
            }
        }
        return newPath.string();
    }
    return "recent_files.txt";
}

static std::vector<std::string> loadRecentFiles() {
    std::vector<std::string> files;
    std::ifstream in(getRecentFilesConfigPath());
    if (!in.is_open()) return files;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        while (!line.empty() && (line.front() == ' ')) line.erase(line.begin());
        if (!line.empty()) {
            files.push_back(line);
        }
    }
    return files;
}

static void saveRecentFiles(const std::vector<std::string>& files) {
    std::ofstream out(getRecentFilesConfigPath());
    if (!out.is_open()) return;
    for (const auto& path : files) {
        out << path << "\n";
    }
}

static void addRecentFile(std::vector<std::string>& files, const std::string& path) {
    if (path.empty()) return;
    if (s_isolatedState) return; // Do not touch user-persisted state in scenario runs.
    std::string normPath = path;
    for (char& c : normPath) {
        if (c == '\\') c = '/';
    }
    files.erase(std::remove_if(files.begin(), files.end(), [&](const std::string& f) {
        return f == normPath;
    }), files.end());

    files.insert(files.begin(), normPath);
    if (files.size() > 10) {
        files.resize(10);
    }
    saveRecentFiles(files);
}

// Window State Persistence
struct WindowState {
    int width = 1600;
    int height = 900;
    int posX = 100;
    int posY = 100;
    bool maximized = false;
};

static std::string getWindowStateConfigPath() {
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && std::strlen(appData) > 0) {
        std::filesystem::path dir = std::filesystem::path(appData) / "Glider";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::path newPath = dir / "window_state.txt";
        if (!std::filesystem::exists(newPath)) {
            std::filesystem::path oldPath = std::filesystem::path(appData) / "Gestaltor" / "window_state.txt";
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::copy_file(oldPath, newPath, ec);
            }
        }
        return newPath.string();
    }
    return "window_state.txt";
}

static WindowState loadWindowState() {
    WindowState state;
    std::ifstream in(getWindowStateConfigPath());
    if (in.is_open()) {
        std::string key;
        int val = 0;
        while (in >> key >> val) {
            if (key == "width" && val >= 640) state.width = val;
            else if (key == "height" && val >= 480) state.height = val;
            else if (key == "posX") state.posX = val;
            else if (key == "posY") state.posY = val;
            else if (key == "maximized") state.maximized = (val != 0);
        }
    }
    return state;
}

static void saveWindowState(GLFWwindow* window) {
    if (window == nullptr) return;
    WindowState state;
    state.maximized = (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0);
    if (!state.maximized) {
        glfwGetWindowSize(window, &state.width, &state.height);
        glfwGetWindowPos(window, &state.posX, &state.posY);
    } else {
        WindowState prev = loadWindowState();
        state.width = prev.width;
        state.height = prev.height;
        state.posX = prev.posX;
        state.posY = prev.posY;
    }

    std::ofstream out(getWindowStateConfigPath());
    if (out.is_open()) {
        out << "width " << state.width << "\n";
        out << "height " << state.height << "\n";
        out << "posX " << state.posX << "\n";
        out << "posY " << state.posY << "\n";
        out << "maximized " << (state.maximized ? 1 : 0) << "\n";
    }
}

// P0.1 scenario parser helpers. Window size and camera values use explicit,
// locale-independent separators so command lines are reproducible.
static bool parseWindowSize(const std::string& text, int& width, int& height)
{
    std::string normalized = text;
    for (char& c : normalized) {
        if (c == 'x' || c == 'X' || c == ',' || c == ';') c = ' ';
    }
    std::istringstream stream(normalized);
    int w = 0;
    int h = 0;
    if (!(stream >> w >> h)) return false;
    if (w < 320 || h < 240) return false;
    width = w;
    height = h;
    return true;
}

static bool parseFloat3(const std::string& text, float& x, float& y, float& z)
{
    std::string normalized = text;
    for (char& c : normalized) {
        if (c == ',' || c == ';') c = ' ';
    }
    std::istringstream stream(normalized);
    float px = 0.0f;
    float py = 0.0f;
    float pz = 0.0f;
    if (!(stream >> px >> py >> pz)) return false;
    x = px;
    y = py;
    z = pz;
    return true;
}

static std::string toLowerAscii(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

static void updateWindowTitle(GLFWwindow* window, const std::string& currentFilePath) {
    if (window == nullptr) return;
    std::string title = "Glider - ";
    if (currentFilePath.empty()) {
        title += "untitled.glb";
    } else {
        std::filesystem::path p(currentFilePath);
        title += p.filename().string();
    }
    glfwSetWindowTitle(window, title.c_str());
}

// glTF loader helper
static bool loadGltfModel(const std::string& filePath,
                          ux3d::gltf::Gltf& gltfModel,
                          ux3d::gltf::Scene*& activeScene,
                          ux3d::imgui::SelectionState& selection,
                          std::string& currentFilePath,
                          std::vector<std::unique_ptr<ux3d::imgui::ViewportInstance>>& viewports,
                          ux3d::renderer::GltfRenderLayer* gltfLayer,
                          ux3d::imgui::AnimationPlayer& animPlayer,
                          bool& sceneDirty)
{
    logMessage("Opening file: " + filePath);
    ux3d::imgui::StatisticsPanel::invalidate();
    ux3d::imgui::AppSettings::instance().setCurrentModelPath(filePath);
    ux3d::gltf::ImportOptions importOptions;
    auto& startupProfile = ux3d::imgui::profiling::startupProfile();
    const auto loadStart = ux3d::imgui::profiling::Clock::now();
    ux3d::filesystem::File file = ux3d::imgui::StdFileSystem::instance().toFile(filePath.c_str());

    ux3d::imgui::ElementInspector::clearPreviewCache();
    gltfModel.reset();
    auto result = ux3d::gltf::GltfParsing::fromFile(gltfModel, file, importOptions);
    startupProfile.loadDecodeMs = ux3d::imgui::profiling::elapsedMs(loadStart);
    if (result.derived == ux3d::gltf::GltfFromFileResult::Success) {
        currentFilePath = filePath;
        activeScene = gltfModel.getDefaultScene();
        if (!activeScene && !gltfModel.getElements<ux3d::gltf::Scene>().empty()) {
            activeScene = &gltfModel.getElements<ux3d::gltf::Scene>().front();
        }
        selection = ux3d::imgui::SelectionState();

        const auto uploadStart = ux3d::imgui::profiling::Clock::now();
        if (gltfLayer != nullptr) {
            gltfLayer->updateGltf(gltfModel);
            if (activeScene != nullptr) {
                gltfLayer->updateScene(gltfModel, *activeScene);
                gltfLayer->applyAnimationTime(0.0f);
            }
        }
        startupProfile.rendererUploadMs = ux3d::imgui::profiling::elapsedMs(uploadStart);

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
        std::snprintf(infoBuf, sizeof(infoBuf), "Model Stats: %zu nodes, %zu meshes, %zu materials", nodeCount, meshCount, matCount);
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
                          extends.min.x, extends.min.y, extends.min.z,
                          extends.max.x, extends.max.y, extends.max.z);
            logMessage(boundsBuf, ImVec4(0.7f, 0.7f, 0.9f, 1.0f));
        }

        sceneDirty = true;
        logMessage("Loaded successfully: " + filePath, ImVec4(0.2f, 1.0f, 0.4f, 1.0f));

        // Auto-run glTF 2.0 validation
        const auto validationStart = ux3d::imgui::profiling::Clock::now();
        auto valReport = ux3d::imgui::GltfValidator::instance().validate(filePath, gltfModel);
        startupProfile.validationMs = ux3d::imgui::profiling::elapsedMs(validationStart);
        startupProfile.validationReportedMs = valReport.durationMs;
        if (valReport.errorCount > 0) {
            char valBuf[256];
            std::snprintf(valBuf, sizeof(valBuf), "Validation: %d %s, %d %s (%.1f ms)",
                          valReport.errorCount, valReport.errorCount == 1 ? "error" : "errors",
                          valReport.warningCount, valReport.warningCount == 1 ? "warning" : "warnings",
                          valReport.durationMs);
            logMessage(valBuf, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
        } else if (valReport.warningCount > 0) {
            char valBuf[256];
            std::snprintf(valBuf, sizeof(valBuf), "Validation: %d %s (%.1f ms)",
                          valReport.warningCount, valReport.warningCount == 1 ? "warning" : "warnings",
                          valReport.durationMs);
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

// glTF creator helper
[[maybe_unused]] static void newEmptyGltf(ux3d::gltf::Gltf& gltfModel,
                         ux3d::gltf::Scene*& activeScene,
                         ux3d::imgui::SelectionState& selection,
                         std::string& currentFilePath,
                         ux3d::editor::CameraActor& cameraActor,
                         ux3d::renderer::GltfRenderLayer* gltfLayer,
                         ux3d::imgui::AnimationPlayer& animPlayer,
                         bool& sceneDirty)
{
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
    sceneDirty = true;
    logMessage("Created new empty glTF", ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
}

// glTF saver helper
[[maybe_unused]] static bool saveGltfModel(const std::string& filePath,
                          const ux3d::gltf::Gltf& gltfModel,
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



static void drawNodeCoordinateGizmo(ImDrawList* drawList,
                                    const glm::mat4& nodeWorld,
                                    const glm::mat4& viewProj,
                                    const ImVec2& vpPos,
                                    const ImVec2& vpSize,
                                    const glm::vec3& camEye,
                                    int axisConvention = 0)
{
    glm::vec3 origin = glm::vec3(nodeWorld[3]);
    float distToCam = glm::length(origin - camEye);
    float axisWorldLen = std::max(0.002f, distToCam * 0.085f);

    glm::vec3 xDir, yDir, zDir;
    if (axisConvention == 0) {
        // Blender Source Convention (Z-Up):
        // Red   (X): Blender local X = glTF column 0 (width)
        // Green (Y): Blender local Y = -glTF column 2 (length towards top of panel)
        // Blue  (Z): Blender local Z = +glTF column 1 (normal pointing out of panel face)
        xDir = glm::length(glm::vec3(nodeWorld[0])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[0])) : glm::vec3(1, 0, 0);
        yDir = glm::length(glm::vec3(nodeWorld[2])) > 1e-5f ? -glm::normalize(glm::vec3(nodeWorld[2])) : glm::vec3(0, 0, -1);
        zDir = glm::length(glm::vec3(nodeWorld[1])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[1])) : glm::vec3(0, 1, 0);
    } else {
        // glTF Native Convention (Y-Up):
        // Red   (X): glTF column 0
        // Green (Y): glTF column 1
        // Blue  (Z): glTF column 2
        xDir = glm::length(glm::vec3(nodeWorld[0])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[0])) : glm::vec3(1, 0, 0);
        yDir = glm::length(glm::vec3(nodeWorld[1])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[1])) : glm::vec3(0, 1, 0);
        zDir = glm::length(glm::vec3(nodeWorld[2])) > 1e-5f ? glm::normalize(glm::vec3(nodeWorld[2])) : glm::vec3(0, 0, 1);
    }

    glm::vec3 xEnd = origin + xDir * axisWorldLen;
    glm::vec3 yEnd = origin + yDir * axisWorldLen;
    glm::vec3 zEnd = origin + zDir * axisWorldLen;

    auto project = [&](const glm::vec3& p, ImVec2& outPt, float& outZ) -> bool {
        glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
        if (clip.w <= 0.0001f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
        outPt.x = vpPos.x + (ndc.x * 0.5f + 0.5f) * vpSize.x;
        outPt.y = vpPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * vpSize.y;
        outZ = ndc.z;
        return true;
    };

    ImVec2 ptOrigin;
    float zOrigin;
    if (!project(origin, ptOrigin, zOrigin)) return;

    struct AxisItem {
        ImVec2 endPt;
        ImU32 color;
        const char* label;
        float z;
    };
    std::vector<AxisItem> axes;

    ImVec2 ptX, ptY, ptZ;
    float zX, zY, zZ;
    if (project(xEnd, ptX, zX)) axes.push_back({ ptX, IM_COL32(235, 45, 45, 255), "X", zX });
    if (project(yEnd, ptY, zY)) axes.push_back({ ptY, IM_COL32(45, 215, 45, 255), "Y", zY });
    if (project(zEnd, ptZ, zZ)) axes.push_back({ ptZ, IM_COL32(45, 120, 245, 255), "Z", zZ });

    std::sort(axes.begin(), axes.end(), [](const AxisItem& a, const AxisItem& b) {
        return a.z > b.z;
    });

    for (const auto& axis : axes) {
        drawList->AddLine(ptOrigin, axis.endPt, axis.color, 3.0f);
        glm::vec2 dir(axis.endPt.x - ptOrigin.x, axis.endPt.y - ptOrigin.y);
        float len = glm::length(dir);
        if (len > 5.0f) {
            dir /= len;
            glm::vec2 perp(-dir.y, dir.x);
            float arrowSize = 11.0f;
            ImVec2 tip = axis.endPt;
            ImVec2 p1(axis.endPt.x - dir.x * arrowSize + perp.x * (arrowSize * 0.45f),
                      axis.endPt.y - dir.y * arrowSize + perp.y * (arrowSize * 0.45f));
            ImVec2 p2(axis.endPt.x - dir.x * arrowSize - perp.x * (arrowSize * 0.45f),
                      axis.endPt.y - dir.y * arrowSize - perp.y * (arrowSize * 0.45f));
            drawList->AddTriangleFilled(tip, p1, p2, axis.color);

            ImVec2 labelSize = ImGui::CalcTextSize(axis.label);
            ImVec2 labelPos(tip.x + dir.x * 10.0f - labelSize.x * 0.5f,
                            tip.y + dir.y * 10.0f - labelSize.y * 0.5f);
            drawList->AddText(ImVec2(labelPos.x + 1.0f, labelPos.y + 1.0f), IM_COL32(0, 0, 0, 220), axis.label);
            drawList->AddText(labelPos, axis.color, axis.label);
        }
    }

    drawList->AddCircleFilled(ptOrigin, 4.5f, IM_COL32(255, 255, 255, 255));
    drawList->AddCircle(ptOrigin, 4.5f, IM_COL32(30, 30, 30, 255), 0, 1.5f);
}

static ux3d::filesystem::Path resolveRendererResourcesDir()
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    if (fs.isFile(fs.toPath("resources/shaders/primitive.vert")) ||
        fs.isFile(fs.toPath("shaders/primitive.vert"))) {
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

static ux3d::filesystem::Path resolveEnvironmentPath(const ux3d::filesystem::Path& rendererRes)
{
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    auto env1 = rendererRes / "environments" / "footprint_court.hdr";
    if (fs.isFile(env1)) return env1;

    auto env2 = fs.toPath("resources/environments/footprint_court.hdr");
    if (fs.isFile(env2)) return env2;

    auto env3 = fs.toPath("environments/footprint_court.hdr");
    if (fs.isFile(env3)) return env3;

    auto exeDir = fs.getExecutableDirPath();
    auto env4 = exeDir / "resources" / "environments" / "footprint_court.hdr";
    if (fs.isFile(env4)) return env4;

    return env1;
}

static std::string resolveSampleModelPath()
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

// -----------------------------------------------------------------------------
// P0.1 — deterministic scenario manifest
// -----------------------------------------------------------------------------

static std::string jsonEscape(const std::string& text)
{
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    for (unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned int>(c));
                out += buffer;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    out.push_back('"');
    return out;
}

static void writeJsonNumber(std::ostream& out, double value)
{
    if (!std::isfinite(value)) {
        out << "null";
        return;
    }
    out << value;
}

static void writeJsonVec3(std::ostream& out, const ux3d::math::vec3& value)
{
    out << "[";
    writeJsonNumber(out, value.x);
    out << ", ";
    writeJsonNumber(out, value.y);
    out << ", ";
    writeJsonNumber(out, value.z);
    out << "]";
}

static void writeJsonMat4(std::ostream& out, const ux3d::math::mat4x4& matrix)
{
    out << "[";
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (column != 0 || row != 0) out << ", ";
            writeJsonNumber(out, matrix[column][row]);
        }
    }
    out << "]";
}

static bool hashFileFnv1a64(const std::string& path, uint64_t& hash, uint64_t& byteCount)
{
    if (path.empty()) return false;
    auto& fs = ux3d::imgui::StdFileSystem::instance();
    ux3d::filesystem::File file = fs.toFile(path.c_str());
    ux3d::core::Vector<uint8_t> data;
    if (file.load(data) != ux3d::filesystem::FileLoadResult::Success) {
        return false;
    }

    uint64_t value = 14695981039346656037ULL; // FNV-1a 64-bit offset basis
    for (size_t i = 0; i < data.size(); ++i) {
        value ^= static_cast<uint64_t>(data[i]);
        value *= 1099511628211ULL; // FNV prime
    }
    hash = value;
    byteCount = static_cast<uint64_t>(data.size());
    return true;
}

static const char* antiAliasingName(int index)
{
    switch (index) {
    case 0: return "none";
    case 1: return "msaa2";
    case 2: return "msaa4";
    case 3: return "msaa8";
    case 4: return "fxaa";
    case 5: return "taa";
    default: return "unknown";
    }
}

static const char* cameraTypeName(ux3d::renderer::Camera::Type type)
{
    switch (type) {
    case ux3d::renderer::Camera::Type::Perspective: return "perspective";
    case ux3d::renderer::Camera::Type::Orthographic: return "orthographic";
    case ux3d::renderer::Camera::Type::Custom: return "custom";
    default: return "unknown";
    }
}

static const char* selectionTypeName(ux3d::imgui::SelectionType type)
{
    using ux3d::imgui::SelectionType;
    switch (type) {
    case SelectionType::None: return "none";
    case SelectionType::Node: return "node";
    case SelectionType::Material: return "material";
    case SelectionType::Mesh: return "mesh";
    case SelectionType::Camera: return "camera";
    case SelectionType::Light: return "light";
    case SelectionType::Texture: return "texture";
    case SelectionType::Image: return "image";
    case SelectionType::Animation: return "animation";
    case SelectionType::Skin: return "skin";
    case SelectionType::MaterialVariant: return "materialVariant";
    case SelectionType::Scene: return "scene";
    case SelectionType::Sampler: return "sampler";
    case SelectionType::Accessor: return "accessor";
    case SelectionType::BufferView: return "bufferView";
    case SelectionType::Buffer: return "buffer";
    case SelectionType::Viewport: return "viewport";
    default: return "unknown";
    }
}

static const char* antiAliasingModeName(int index)
{
    switch (index) {
    case 0: return "none";
    case 1: return "msaa2";
    case 2: return "msaa4";
    case 3: return "msaa8";
    case 4: return "fxaa";
    case 5: return "taa";
    default: return "unknown";
    }
}

static bool writeScenarioReport(const std::string& reportPath,
                                const std::string& scenarioName,
                                uint64_t frame,
                                GLFWwindow* window,
                                const std::string& modelPath,
                                bool modelLoaded,
                                ux3d::gltf::Gltf& gltfModel,
                                ux3d::gltf::Scene* activeScene,
                                const ux3d::imgui::ViewportInstance& viewport,
                                const std::string& environmentPath,
                                bool environmentLoaded,
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
        out << "\"0x" << std::hex << std::setw(16) << std::setfill('0') << modelHash
            << std::dec << std::setfill(' ') << "\"";
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
    if (hasSceneExtends) writeJsonVec3(out, ux3d::math::vec3(sceneExtends.min));
    else out << "null";
    out << ",\n";
    out << "    \"boundsMax\": ";
    if (hasSceneExtends) writeJsonVec3(out, ux3d::math::vec3(sceneExtends.max));
    else out << "null";
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
        out << "\"0x" << std::hex << std::setw(16) << std::setfill('0') << environmentHash
            << std::dec << std::setfill(' ') << "\"";
    } else {
        out << "null";
    }
    out << ",\n";
    out << "    \"uiPath\": " << jsonEscape(settings.currentHdrPath) << ",\n";
    out << "    \"iblEnabled\": " << (settings.iblEnabled ? "true" : "false") << ",\n";
    out << "    \"rotation\": ";
    writeJsonNumber(out, settings.hdrRotation);
    out << ",\n";
    out << "    \"intensity\": ";
    writeJsonNumber(out, settings.hdrIntensity);
    out << ",\n";
    out << "    \"blur\": ";
    writeJsonNumber(out, settings.hdrBlur);
    out << "\n";
    out << "  },\n";

    out << "  \"settings\": {\n";
    out << "    \"antiAliasing\": " << jsonEscape(antiAliasingName(settings.antiAliasingIdx)) << ",\n";
    out << "    \"antiAliasingIndex\": " << settings.antiAliasingIdx << ",\n";
    out << "    \"renderModeIndex\": " << settings.renderModeIdx << ",\n";
    out << "    \"toneMappingIndex\": " << settings.toneMappingIdx << ",\n";
    out << "    \"exposureEV\": ";
    writeJsonNumber(out, settings.exposureEV);
    out << ",\n";
    out << "    \"showBackground\": " << (settings.showBackground ? "true" : "false") << ",\n";
    out << "    \"showGrid\": " << (settings.showGrid ? "true" : "false") << ",\n";
    out << "    \"forceDoubleSided\": " << (settings.forceDoubleSided ? "true" : "false") << ",\n";
    out << "    \"invertWinding\": " << (settings.invertWinding ? "true" : "false") << ",\n";
    out << "    \"wireframeOverlay\": " << (settings.wireframeOverlay ? "true" : "false") << ",\n";
    out << "    \"showSceneBoundingBox\": " << (settings.showSceneBoundingBox ? "true" : "false") << ",\n";
    out << "    \"showSelectedBoundingBox\": " << (settings.showSelectedBoundingBox ? "true" : "false") << ",\n";
    out << "    \"showNodeCoordinateGizmo\": " << (settings.showNodeCoordinateGizmo ? "true" : "false") << ",\n";
    out << "    \"showOrientationGizmo\": " << (settings.showOrientationGizmo ? "true" : "false") << ",\n";
    out << "    \"gizmoAxisConvention\": " << settings.gizmoAxisConvention << ",\n";
    out << "    \"turntable\": " << (settings.turntable ? "true" : "false") << ",\n";
    out << "    \"turntableSpeed\": ";
    writeJsonNumber(out, settings.turntableSpeed);
    out << ",\n";
    out << "    \"punctualLightsEnabled\": " << (settings.punctualLightsEnabled ? "true" : "false") << ",\n";
    out << "    \"clearColor\": [";
    writeJsonNumber(out, settings.clearColor.r);
    out << ", ";
    writeJsonNumber(out, settings.clearColor.g);
    out << ", ";
    writeJsonNumber(out, settings.clearColor.b);
    out << ", ";
    writeJsonNumber(out, settings.clearColor.a);
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
    if (selection.selectedNode != nullptr) out << selection.selectedNode->m_ID;
    else out << "null";
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

static ux3d::renderer::RenderMode getRenderModeFromIndex(int idx)
{
    switch (idx) {
        case 1: return ux3d::renderer::RenderMode::Wireframe;
        case 2: return ux3d::renderer::RenderMode::NormalTexture;
        case 3: return ux3d::renderer::RenderMode::ShadingNormal;
        case 4: return ux3d::renderer::RenderMode::NormalGeometry;
        case 5: return ux3d::renderer::RenderMode::Tangent;
        case 6: return ux3d::renderer::RenderMode::Bitangent;
        case 7: return ux3d::renderer::RenderMode::BaseColor;
        case 8: return ux3d::renderer::RenderMode::Metallic;
        case 9: return ux3d::renderer::RenderMode::Roughness;
        case 10: return ux3d::renderer::RenderMode::MetallicRoughness;
        case 11: return ux3d::renderer::RenderMode::Occlusion;
        case 12: return ux3d::renderer::RenderMode::Emissive;
        case 13: return ux3d::renderer::RenderMode::Alpha;
        case 14: return ux3d::renderer::RenderMode::TexCoord_0;
        case 15: return ux3d::renderer::RenderMode::TexCoord_1;
        case 16: return ux3d::renderer::RenderMode::Clearcoat;
        case 17: return ux3d::renderer::RenderMode::Sheen;
        case 18: return ux3d::renderer::RenderMode::Specular;
        case 19: return ux3d::renderer::RenderMode::TransmissionVolume;
        case 20: return ux3d::renderer::RenderMode::Iridescence;
        case 21: return ux3d::renderer::RenderMode::ClearcoatFactor;
        case 22: return ux3d::renderer::RenderMode::ClearcoatRoughness;
        case 23: return ux3d::renderer::RenderMode::ClearcoatNormal;
        case 24: return ux3d::renderer::RenderMode::SheenColor;
        case 25: return ux3d::renderer::RenderMode::SheenRoughness;
        case 26: return ux3d::renderer::RenderMode::SpecularFactor;
        case 27: return ux3d::renderer::RenderMode::SpecularColor;
        case 28: return ux3d::renderer::RenderMode::TransmissionFactor;
        case 29: return ux3d::renderer::RenderMode::VolumeThickness;
        case 30: return ux3d::renderer::RenderMode::IridescenceFactor;
        case 31: return ux3d::renderer::RenderMode::IridescenceThickness;
        default: return ux3d::renderer::RenderMode::None;
    }
}

static int getIndexFromRenderMode(ux3d::renderer::RenderMode mode)
{
    using RM = ux3d::renderer::RenderMode;
    switch (mode) {
        case RM::Wireframe: return 1;
        case RM::NormalTexture: return 2;
        case RM::ShadingNormal: return 3;
        case RM::NormalGeometry: return 4;
        case RM::Tangent: return 5;
        case RM::Bitangent: return 6;
        case RM::BaseColor: return 7;
        case RM::Metallic: return 8;
        case RM::Roughness: return 9;
        case RM::MetallicRoughness: return 10;
        case RM::Occlusion: return 11;
        case RM::Emissive: return 12;
        case RM::Alpha: return 13;
        case RM::TexCoord_0: return 14;
        case RM::TexCoord_1: return 15;
        case RM::Clearcoat: return 16;
        case RM::Sheen: return 17;
        case RM::Specular: return 18;
        case RM::TransmissionVolume: return 19;
        case RM::Iridescence: return 20;
        case RM::ClearcoatFactor: return 21;
        case RM::ClearcoatRoughness: return 22;
        case RM::ClearcoatNormal: return 23;
        case RM::SheenColor: return 24;
        case RM::SheenRoughness: return 25;
        case RM::SpecularFactor: return 26;
        case RM::SpecularColor: return 27;
        case RM::TransmissionFactor: return 28;
        case RM::VolumeThickness: return 29;
        case RM::IridescenceFactor: return 30;
        case RM::IridescenceThickness: return 31;
        default: return 0;
    }
}

struct ShadingModeInfo {
    const char* label;
    ux3d::renderer::RenderMode mode;
    const char* shortcut;
    bool isSeparator;
};

static const ShadingModeInfo s_shadingModes[] = {
    { "Default Shaded", ux3d::renderer::RenderMode::None, "Ctrl+Alt+1", false },
    { "Wireframe", ux3d::renderer::RenderMode::Wireframe, "Ctrl+Alt+2", false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Texture Coordinates 0", ux3d::renderer::RenderMode::TexCoord_0, nullptr, false },
    { "Texture Coordinates 1", ux3d::renderer::RenderMode::TexCoord_1, nullptr, false },
    { "Normal Texture", ux3d::renderer::RenderMode::NormalTexture, nullptr, false },
    { "Geometry Normal", ux3d::renderer::RenderMode::NormalGeometry, nullptr, false },
    { "Geometry Tangent", ux3d::renderer::RenderMode::Tangent, nullptr, false },
    { "Geometry Bitangent", ux3d::renderer::RenderMode::Bitangent, nullptr, false },
    { "Shading Normal", ux3d::renderer::RenderMode::ShadingNormal, nullptr, false },
    { "Alpha", ux3d::renderer::RenderMode::Alpha, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Occlusion", ux3d::renderer::RenderMode::Occlusion, nullptr, false },
    { "Emissive", ux3d::renderer::RenderMode::Emissive, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Metallic Roughness", ux3d::renderer::RenderMode::MetallicRoughness, nullptr, false },
    { "    BaseColor", ux3d::renderer::RenderMode::BaseColor, nullptr, false },
    { "    Roughness", ux3d::renderer::RenderMode::Roughness, nullptr, false },
    { "    Metallic", ux3d::renderer::RenderMode::Metallic, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Clearcoat", ux3d::renderer::RenderMode::Clearcoat, nullptr, false },
    { "    Clearcoat Factor", ux3d::renderer::RenderMode::ClearcoatFactor, nullptr, false },
    { "    Clearcoat Roughness", ux3d::renderer::RenderMode::ClearcoatRoughness, nullptr, false },
    { "    Clearcoat Normal", ux3d::renderer::RenderMode::ClearcoatNormal, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Sheen", ux3d::renderer::RenderMode::Sheen, nullptr, false },
    { "    Sheen Color", ux3d::renderer::RenderMode::SheenColor, nullptr, false },
    { "    Sheen Roughness", ux3d::renderer::RenderMode::SheenRoughness, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Specular", ux3d::renderer::RenderMode::Specular, nullptr, false },
    { "    Specular Factor", ux3d::renderer::RenderMode::SpecularFactor, nullptr, false },
    { "    Specular Color", ux3d::renderer::RenderMode::SpecularColor, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Transmission/Volume", ux3d::renderer::RenderMode::TransmissionVolume, nullptr, false },
    { "    Transmission Factor", ux3d::renderer::RenderMode::TransmissionFactor, nullptr, false },
    { "    Volume Thickness", ux3d::renderer::RenderMode::VolumeThickness, nullptr, false },
    { "", ux3d::renderer::RenderMode::None, nullptr, true },
    { "Iridescence", ux3d::renderer::RenderMode::Iridescence, nullptr, false },
    { "    Iridescence Factor", ux3d::renderer::RenderMode::IridescenceFactor, nullptr, false },
    { "    Iridescence Thickness", ux3d::renderer::RenderMode::IridescenceThickness, nullptr, false },
};

static const char* getRenderModeLabel(ux3d::renderer::RenderMode mode)
{
    using RM = ux3d::renderer::RenderMode;
    switch (mode) {
        case RM::Wireframe: return "Wireframe";
        case RM::TexCoord_0: return "TexCoord 0";
        case RM::TexCoord_1: return "TexCoord 1";
        case RM::NormalTexture: return "Normal Texture";
        case RM::ShadingNormal: return "Shading Normal";
        case RM::NormalGeometry: return "Geometry Normal";
        case RM::Tangent: return "Geometry Tangent";
        case RM::Bitangent: return "Geometry Bitangent";
        case RM::Alpha: return "Alpha";
        case RM::Occlusion: return "Occlusion";
        case RM::Emissive: return "Emissive";
        case RM::MetallicRoughness: return "Metallic Roughness";
        case RM::Roughness: return "Roughness";
        case RM::Metallic: return "Metallic";
        case RM::BaseColor: return "BaseColor";
        case RM::Clearcoat: return "Clearcoat";
        case RM::ClearcoatFactor: return "Clearcoat Factor";
        case RM::ClearcoatRoughness: return "Clearcoat Roughness";
        case RM::ClearcoatNormal: return "Clearcoat Normal";
        case RM::Sheen: return "Sheen";
        case RM::SheenColor: return "Sheen Color";
        case RM::SheenRoughness: return "Sheen Roughness";
        case RM::Specular: return "Specular";
        case RM::SpecularFactor: return "Specular Factor";
        case RM::SpecularColor: return "Specular Color";
        case RM::TransmissionVolume: return "Transmission/Volume";
        case RM::TransmissionFactor: return "Transmission Factor";
        case RM::VolumeThickness: return "Volume Thickness";
        case RM::Iridescence: return "Iridescence";
        case RM::IridescenceFactor: return "Iridescence Factor";
        case RM::IridescenceThickness: return "Iridescence Thickness";
        default: return "Default Shaded";
    }
}

static std::vector<const ux3d::gltf::Node*> collectSceneCameraNodes(const ux3d::gltf::Scene* scene)
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

static const char* const WND_HIERARCHY          = ICON_MDI_FILE_TREE " Scene Hierarchy";
static const char* const WND_RESOURCES          = ICON_MDI_FOLDER_MULTIPLE_OUTLINE " Resources";
static const char* const WND_INSPECTOR          = ICON_MDI_TUNE " Inspector";
static const char* const WND_LOGGER             = ICON_MDI_CONSOLE " Logger";
static const char* const WND_STATISTICS         = ICON_MDI_CHART_BAR " Statistics";
static const char* const WND_VALIDATION         = ICON_MDI_SHIELD_CHECK_OUTLINE " Validation###ValidationPanel";

static ImGuiID s_centralDockId = 0;

static void setupDefaultDockLayout(ImGuiID dockspaceId, ImVec2 size, bool force = false)
{
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspaceId);
    if (node == nullptr || force) {
        if (node != nullptr) {
            ImGui::DockBuilderRemoveNode(dockspaceId);
        }
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, size);

        ImGuiID dock_main_id = dockspaceId;

        // 1. Split Left column (Hierarchy on top, Resources on bottom)
        ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
        ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.45f, nullptr, &dock_left);

        // 2. Split Right column (Inspector full height)
        ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.26f, nullptr, &dock_main_id);

        // 3. Center column: Viewport on top, Logger on bottom splitting the viewport (not whole window)
        ImGuiID dock_center_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);

        s_centralDockId = dock_main_id;

        ImGui::DockBuilderDockWindow(ICON_MDI_CUBE_OUTLINE " 3D Viewport###Viewport_1", dock_main_id);
        ImGui::DockBuilderDockWindow(WND_LOGGER, dock_center_bottom);
        ImGui::DockBuilderDockWindow(WND_HIERARCHY, dock_left);
        ImGui::DockBuilderDockWindow(WND_RESOURCES, dock_left_bottom);
        ImGui::DockBuilderDockWindow(WND_INSPECTOR, dock_right);

        // Dock optional panels in center-bottom if opened later
        ImGui::DockBuilderDockWindow(WND_STATISTICS, dock_center_bottom);
        ImGui::DockBuilderDockWindow(WND_VALIDATION, dock_center_bottom);

        ImGui::DockBuilderFinish(dockspaceId);
    } else {
        s_centralDockId = dockspaceId;
    }
}

static void fillGpuReport(ux3d::imgui::profiling::GpuReport& target,
                          const ux3d::renderer::Renderer* renderer)
{
    target = ux3d::imgui::profiling::GpuReport();
    if (renderer == nullptr) {
        return;
    }

    const auto& profiler = renderer->getGpuProfiler();
    target.enabled = profiler.enabled();
    target.method = "GL_TIME_ELAPSED; polled with GL_QUERY_RESULT_AVAILABLE after >=2 later submissions; no glFinish";
    target.cpuSubmissionStage = "draw.submit (CPU, includes command submission only)";
    target.pendingFrames = profiler.pendingFrameCount();
    target.unavailablePolls = profiler.unavailablePollCount();
    target.droppedFrames = profiler.droppedFrameCount();
    target.completedFrameLimit = profiler.completedFrameLimit();
    target.completedFramesDropped = profiler.completedFrameDroppedCount();

    const auto convertGpuFrame = [](const ux3d::renderer::GpuFrameSample& source) {
        ux3d::imgui::profiling::GpuFrameReport converted;
        converted.sequence = source.sequence;
        converted.frame = source.frame;
        converted.viewport = source.viewport;
        converted.label = source.label;
        for (const auto& pass : source.passes) {
            converted.passes.push_back(ux3d::imgui::profiling::GpuPassReport{
                pass.name, pass.available, pass.milliseconds
            });
        }
        return converted;
    };

    for (const auto& frame : profiler.completedFrames()) {
        target.completed.push_back(convertGpuFrame(frame));
    }
    const auto pendingGpuFrames = profiler.pendingFrames();
    for (const auto& frame : pendingGpuFrames) {
        target.pending.push_back(convertGpuFrame(frame));
    }
}

static std::string describeUiSelection(const ux3d::imgui::SelectionState& selection)
{
    auto nameOf = [](auto* element) -> std::string {
        if (element == nullptr) return "";
        return element->hasName() ? std::string(element->getName().c_str()) : std::string("<unnamed>");
    };
    std::string text = "type=" + std::to_string(static_cast<int>(selection.type));
    text += "|node=" + nameOf(selection.selectedNode);
    text += "|mesh=" + nameOf(selection.selectedMesh);
    text += "|material=" + nameOf(selection.selectedMaterial);
    text += "|variant=" + nameOf(selection.selectedVariant);
    text += "|camera=" + nameOf(selection.selectedCamera);
    text += "|light=" + nameOf(selection.selectedLight);
    text += "|viewport=" + std::to_string(selection.selectedViewportId);
    return text;
}

int main(int argc, char** argv)
{
    // P0.1 pre-scan: window size and state isolation must be known before GLFW
    // creates the window and before persisted layouts are loaded.
    bool isolatedArg = false;
    bool profileArg = false;
    bool noProfileArg = false;
    std::string profileReportArg;
    int requestedWidth = 0;
    int requestedHeight = 0;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--isolated" || arg == "--no-persist") {
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
                std::fprintf(stderr, "Invalid --window-size '%s' (expected WIDTHxHEIGHT, minimum 320x240)\n", sizeText.c_str());
                requestedWidth = 0;
                requestedHeight = 0;
            }
        }
    }
    s_isolatedState = isolatedArg;
    s_windowSizeOverridden = requestedWidth > 0 && requestedHeight > 0;
    s_profilingEnabled = !noProfileArg && (profileArg || !profileReportArg.empty());
    ux3d::imgui::profiling::setCountingEnabled(s_profilingEnabled);

    const auto startupStart = ux3d::imgui::profiling::Clock::now();

    // Initialize embedded Virtual File System (decompresses runtime resources in-memory)
    ux3d::imgui::EmbeddedVFS::instance().initialize();
    ux3d::imgui::profiling::startupProfile().vfsDecompressMs =
        ux3d::imgui::profiling::elapsedMs(startupStart);

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

    // Target OpenGL 4.1 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, winState.maximized ? GLFW_TRUE : GLFW_FALSE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create GLFW window (starts at restored or default size)
    GLFWwindow* window = glfwCreateWindow(winState.width, winState.height, "Glider - glTF 2.0 Viewer", nullptr, nullptr);
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
    ConsoleLogCallback consoleLogger;
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

    // Initialize GladGL abstraction
    auto gladGL = std::make_unique<ux3d::renderer::GladGL>();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (s_isolatedState) {
        // Scenario runs must not inherit an interactive docking layout from imgui.ini.
        io.IniFilename = nullptr;
    }

    // Load FSS Fonts (fss-ui.ttf and fss-ui-mono.ttf with embedded MDI icons)
    ux3d::imgui::loadFssFonts(io);

    // Apply FSS ImGui Theme (aviation dark palette with emerald green accents)
    ux3d::imgui::applyFssTheme();

    // Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // Initialize 3D Viewport components
    ux3d::imgui::GridRenderer gridRenderer;
    gridRenderer.init();
    ux3d::imgui::BoundingBoxRenderer bboxRenderer;
    bboxRenderer.init();

    ux3d::gltf::Gltf gltfModel;
    ux3d::gltf::Scene* activeScene = nullptr;
    ux3d::imgui::SelectionState selection;
    std::string currentFilePath = "";
    bool isSceneDirty = false;

    ux3d::imgui::AnimationPlayer animationPlayer;

    auto rendererResourcesDir = resolveRendererResourcesDir();
    auto shaderSource = rendererResourcesDir / "shaders";

    auto gltfLayer = std::make_unique<ux3d::renderer::GltfRenderLayer>(gladGL.get(), shaderSource / "ibl_sampler");
    auto commandGenerator = std::make_unique<ux3d::renderer::CommandGenerator>(gladGL.get(), shaderSource);
    auto glRenderer = std::make_unique<ux3d::renderer::Renderer>(gladGL.get());
    glRenderer->init(ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));
    glRenderer->setGpuProfilingEnabled(s_profilingEnabled);

    ux3d::renderer::PickingRenderLayer pickingLayer(gladGL.get());
    ux3d::renderer::ReverseMapping currentReverseMapping;

    std::vector<std::unique_ptr<ux3d::imgui::ViewportInstance>> viewports;
    viewports.push_back(std::make_unique<ux3d::imgui::ViewportInstance>(1, "3D Viewport", gladGL.get()));
    ux3d::imgui::ViewportInstance* activeViewport = viewports[0].get();

    auto envPath = resolveEnvironmentPath(rendererResourcesDir);
    std::string scenarioEnvironmentPath = envPath.toString().c_str();
    bool scenarioEnvironmentLoaded = false;
    auto& startupProfile = ux3d::imgui::profiling::startupProfile();
    if (ux3d::imgui::StdFileSystem::instance().isFile(envPath)) {
        const auto environmentInitStart = ux3d::imgui::profiling::Clock::now();
        gltfLayer->setEnvironmentMap(envPath);
        startupProfile.environmentInitMs += ux3d::imgui::profiling::elapsedMs(environmentInitStart);
        const auto environmentFilterStart = ux3d::imgui::profiling::Clock::now();
        if (gltfLayer->filterEnvironmentMap()) {
            startupProfile.environmentFilterMs += ux3d::imgui::profiling::elapsedMs(environmentFilterStart);
            startupProfile.environmentLoads++;
            scenarioEnvironmentLoaded = true;
            commandGenerator->setIBLEnabled(true);
            logMessage("Loaded default environment map: footprint_court.hdr");
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

    std::string modelToLoad = resolveSampleModelPath();
    std::string reloadModelArg;
    std::string dumpFramePath = "";
    std::string uiScriptPath;
    std::string uiReportPath;
    ux3d::imgui::UiScenario uiScenario;
    int dumpFrameCountdown = -1;
    bool exitAfterDump = false;
    std::string selectNodeArg;
    std::string hideNodeArg;
    std::string renameNodeArg;
    std::string renameMeshArg;
    std::string renameMaterialArg;
    bool hierarchyDisplayProvided = false;
    bool resourceDisplayProvided = false;
    ux3d::imgui::HierarchyDisplayOptions hierarchyDisplayValue;
    ux3d::imgui::ResourceDisplayOptions resourceDisplayValue;
    std::string selectMaterialArg;
    std::string selectImageArg;
    std::string selectTextureArg;
    uint32_t selectViewportArg = 0;
    std::string newViewportCamArg;
    bool showValidationArg = false;
    bool showSearchPathsArg = false;
    bool showStatisticsArg = false;
    bool showLoggerArg = false;
    bool loggerWrapArg = false;
    bool loggerAutoScrollArg = true;
    bool loggerVariableEntriesArg = false;
    size_t logTestCountArg = 0;
    float animTimeArg = -1.0f;
    std::string conflictSelfCheckOutputPath;
    std::string validationCacheSelfCheckOutputPath;
    std::string cacheInvalidationSelfCheckOutputPath;
    std::string loggerLayoutSelfCheckOutputPath;
    std::string aaTargetSelfCheckOutputPath;
    std::string samplerSelfCheckOutputPath;
    std::string samplerSelfCheckCase = "all";
    std::string samplerDumpPatternPath;
    std::string vaoSelfCheckOutputPath;
    std::string vaoSelfCheckCase = "all";
    std::string polygonModeSelfCheckOutputPath;
    std::string polygonModeSelfCheckCase = "all";
    std::string cullWindingSelfCheckOutputPath;
    std::string cullWindingSelfCheckCase = "all";
    std::string depthSelfCheckOutputPath;
    std::string depthSelfCheckCase = "all";
    std::string blendSelfCheckOutputPath;
    std::string blendSelfCheckCase = "all";
    bool selfCheckForceFailure = false;
    std::string scenarioReportPath;
    std::string scenarioName = "default";
    bool cameraYawProvided = false;
    bool cameraPitchProvided = false;
    bool cameraDistanceProvided = false;
    bool cameraTargetProvided = false;
    bool cameraFovProvided = false;
    float cameraYawArg = 0.0f;
    float cameraPitchArg = 0.0f;
    float cameraDistanceArg = 0.0f;
    float cameraTargetX = 0.0f;
    float cameraTargetY = 0.0f;
    float cameraTargetZ = 0.0f;
    float cameraFovArg = 0.0f;
    std::string cameraProjectionArg;
    bool modelLoaded = false;

    auto parseDisplayOptions = [](const std::string& spec, ux3d::imgui::HierarchyDisplayOptions& options) {
        std::stringstream stream(spec);
        std::string token;
        int part = 0;
        while (std::getline(stream, token, ',') && part < 4) {
            const bool enabled = token == "1" || toLowerAscii(token) == "true" || toLowerAscii(token) == "on";
            switch (part) {
                case 0: options.showIcon = enabled; break;
                case 1: options.showTitle = enabled; break;
                case 2: options.showIndex = enabled; break;
                case 3: options.showName = enabled; break;
                default: break;
            }
            ++part;
        }
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dump-frame" && i + 1 < argc) {
            dumpFramePath = argv[++i];
            dumpFrameCountdown = 8;
        } else if (arg == "--capture-viewport" && i + 1 < argc) {
            s_captureViewportPath = argv[++i];
        } else if (arg == "--capture-frame" && i + 1 < argc) {
            s_captureFrame = static_cast<uint64_t>(std::stoull(argv[++i]));
            s_captureFrameProvided = true;
        } else if (arg == "--capture-time" && i + 1 < argc) {
            s_captureTimeSeconds = std::stod(argv[++i]);
            s_captureTimeProvided = true;
        } else if (arg == "--capture-first-frame" && i + 1 < argc) {
            s_captureFirstFramePath = argv[++i];
        } else if (arg == "--exit-after-dump") {
            exitAfterDump = true;
        } else if (arg == "--profile") {
            s_profilingEnabled = true;
        } else if (arg == "--no-profile") {
            s_profilingEnabled = false;
        } else if (arg == "--profile-report" && i + 1 < argc) {
            s_profileReportPath = argv[++i];
            s_profilingEnabled = true;
        } else if (arg == "--exit-after-profile") {
            s_exitAfterProfile = true;
        } else if (arg == "--benchmark") {
            s_benchmarkEnabled = true;
        } else if (arg == "--benchmark-output" && i + 1 < argc) {
            s_benchmarkOutputPath = argv[++i];
            s_benchmarkEnabled = true;
        } else if (arg == "--benchmark-warmup" && i + 1 < argc) {
            s_benchmarkWarmupFrames = static_cast<uint64_t>(std::stoull(argv[++i]));
            s_benchmarkEnabled = true;
        } else if (arg == "--benchmark-frames" && i + 1 < argc) {
            const uint64_t requested = static_cast<uint64_t>(std::stoull(argv[++i]));
            if (requested == 0) {
                std::fprintf(stderr, "--benchmark-frames must be at least 1\n");
                return 1;
            }
            s_benchmarkMeasuredFrames = requested;
            s_benchmarkEnabled = true;
        } else if (arg == "--exit-after-benchmark") {
            s_exitAfterBenchmark = true;
            s_benchmarkEnabled = true;
        } else if (arg == "--vsync" && i + 1 < argc) {
            ++i; // Applied by the pre-scan before window creation.
        } else if (arg == "--anim-time" && i + 1 < argc) {
            animTimeArg = std::stof(argv[++i]);
        } else if (arg == "--scenario-report" && i + 1 < argc) {
            scenarioReportPath = argv[++i];
        } else if (arg == "--scenario-name" && i + 1 < argc) {
            scenarioName = argv[++i];
        } else if (arg == "--camera-yaw" && i + 1 < argc) {
            cameraYawArg = std::stof(argv[++i]);
            cameraYawProvided = true;
        } else if (arg == "--camera-pitch" && i + 1 < argc) {
            cameraPitchArg = std::stof(argv[++i]);
            cameraPitchProvided = true;
        } else if (arg == "--camera-distance" && i + 1 < argc) {
            cameraDistanceArg = std::stof(argv[++i]);
            cameraDistanceProvided = true;
        } else if (arg == "--camera-target" && i + 1 < argc) {
            const std::string targetText = argv[++i];
            if (parseFloat3(targetText, cameraTargetX, cameraTargetY, cameraTargetZ)) {
                cameraTargetProvided = true;
            } else {
                std::fprintf(stderr, "Invalid --camera-target '%s' (expected X,Y,Z)\n", targetText.c_str());
            }
        } else if (arg == "--camera-fov" && i + 1 < argc) {
            cameraFovArg = std::stof(argv[++i]);
            cameraFovProvided = true;
        } else if (arg == "--camera-projection" && i + 1 < argc) {
            cameraProjectionArg = toLowerAscii(argv[++i]);
        } else if (arg == "--window-size" && i + 1 < argc) {
            ++i; // Value was applied by the pre-scan before window creation.
        } else if (arg == "--ui-script" && i + 1 < argc) {
            uiScriptPath = argv[++i];
        } else if (arg == "--ui-report" && i + 1 < argc) {
            uiReportPath = argv[++i];
        } else if (arg == "--isolated" || arg == "--no-persist") {
            s_isolatedState = true; // Also applied by the pre-scan.
        } else if (arg == "--select-node" && i + 1 < argc) {
            selectNodeArg = argv[++i];
        } else if (arg == "--hide-node" && i + 1 < argc) {
            hideNodeArg = argv[++i];
        } else if (arg == "--reload-model" && i + 1 < argc) {
            reloadModelArg = argv[++i];
        } else if (arg == "--rename-node" && i + 1 < argc) {
            renameNodeArg = argv[++i];
        } else if (arg == "--rename-mesh" && i + 1 < argc) {
            renameMeshArg = argv[++i];
        } else if (arg == "--rename-material" && i + 1 < argc) {
            renameMaterialArg = argv[++i];
        } else if (arg == "--hierarchy-display" && i + 1 < argc) {
            hierarchyDisplayProvided = true;
            parseDisplayOptions(argv[++i], hierarchyDisplayValue);
        } else if (arg == "--resource-display" && i + 1 < argc) {
            resourceDisplayProvided = true;
            parseDisplayOptions(argv[++i], resourceDisplayValue);
        } else if (arg == "--select-material" && i + 1 < argc) {
            selectMaterialArg = argv[++i];
        } else if (arg == "--select-image" && i + 1 < argc) {
            selectImageArg = argv[++i];
        } else if (arg == "--select-texture" && i + 1 < argc) {
            selectTextureArg = argv[++i];
        } else if (arg == "--select-viewport" && i + 1 < argc) {
            selectViewportArg = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--create-viewport-cam" && i + 1 < argc) {
            newViewportCamArg = argv[++i];
        } else if (arg == "--show-validation" || arg == "--validate") {
            showValidationArg = true;
        } else if (arg == "--show-search-paths" || arg == "--search-paths") {
            showSearchPathsArg = true;
        } else if (arg == "--show-statistics") {
            showStatisticsArg = true;
        } else if (arg == "--show-logger") {
            showLoggerArg = true;
        } else if (arg == "--logger-wrap") {
            loggerWrapArg = true;
        } else if (arg == "--logger-no-auto-scroll") {
            loggerAutoScrollArg = false;
        } else if (arg == "--log-test-variable") {
            loggerVariableEntriesArg = true;
        } else if (arg == "--log-test" && i + 1 < argc) {
            logTestCountArg = static_cast<size_t>(std::stoull(argv[++i]));
        } else if (arg == "--filter-node" && i + 1 < argc) {
            ux3d::imgui::SceneHierarchyPanel::setSearchFilter(argv[++i]);
        } else if (arg == "--filter-resource" && i + 1 < argc) {
            ux3d::imgui::ResourcesPanel::setSearchFilter(argv[++i]);
        } else if (arg == "--filter-validation" && i + 1 < argc) {
            ux3d::imgui::ValidationPanel::setSearchFilter(argv[++i]);
        } else if (arg == "--validation-severity" && i + 1 < argc) {
            const std::string severity = toLowerAscii(argv[++i]);
            if (severity == "errors" || severity == "error") {
                ux3d::imgui::ValidationPanel::setSeverityFilter(true, false, false);
            } else if (severity == "warnings" || severity == "warning") {
                ux3d::imgui::ValidationPanel::setSeverityFilter(false, true, false);
            } else if (severity == "infos" || severity == "info") {
                ux3d::imgui::ValidationPanel::setSeverityFilter(false, false, true);
            } else {
                ux3d::imgui::ValidationPanel::setSeverityFilter(true, true, true);
            }
        } else if (arg == "--conflict-selfcheck" && i + 1 < argc) {
            conflictSelfCheckOutputPath = argv[++i];
        } else if (arg == "--validation-cache-selfcheck" && i + 1 < argc) {
            validationCacheSelfCheckOutputPath = argv[++i];
        } else if (arg == "--cache-invalidation-selfcheck" && i + 1 < argc) {
            cacheInvalidationSelfCheckOutputPath = argv[++i];
        } else if (arg == "--logger-layout-selfcheck" && i + 1 < argc) {
            loggerLayoutSelfCheckOutputPath = argv[++i];
        } else if (arg == "--aa-target-selfcheck" && i + 1 < argc) {
            aaTargetSelfCheckOutputPath = argv[++i];
        } else if (arg == "--sampler-selfcheck" && i + 1 < argc) {
            samplerSelfCheckOutputPath = argv[++i];
        } else if (arg == "--sampler-selfcheck-case" && i + 1 < argc) {
            samplerSelfCheckCase = argv[++i];
        } else if (arg == "--sampler-dump-pattern" && i + 1 < argc) {
            samplerDumpPatternPath = argv[++i];
        } else if (arg == "--vao-selfcheck" && i + 1 < argc) {
            vaoSelfCheckOutputPath = argv[++i];
        } else if (arg == "--vao-selfcheck-case" && i + 1 < argc) {
            vaoSelfCheckCase = argv[++i];
        } else if (arg == "--polygon-mode-selfcheck" && i + 1 < argc) {
            polygonModeSelfCheckOutputPath = argv[++i];
        } else if (arg == "--polygon-mode-selfcheck-case" && i + 1 < argc) {
            polygonModeSelfCheckCase = argv[++i];
        } else if (arg == "--cull-winding-selfcheck" && i + 1 < argc) {
            cullWindingSelfCheckOutputPath = argv[++i];
        } else if (arg == "--cull-winding-selfcheck-case" && i + 1 < argc) {
            cullWindingSelfCheckCase = argv[++i];
        } else if (arg == "--depth-selfcheck" && i + 1 < argc) {
            depthSelfCheckOutputPath = argv[++i];
        } else if (arg == "--depth-selfcheck-case" && i + 1 < argc) {
            depthSelfCheckCase = argv[++i];
        } else if (arg == "--blend-selfcheck" && i + 1 < argc) {
            blendSelfCheckOutputPath = argv[++i];
        } else if (arg == "--blend-selfcheck-case" && i + 1 < argc) {
            blendSelfCheckCase = argv[++i];
        } else if (arg == "--force-double-sided") {
            activeViewport->settings.forceDoubleSided = true;
        } else if (arg == "--invert-winding") {
            activeViewport->settings.invertWinding = true;
        } else if (arg == "--selfcheck-force-failure") {
            selfCheckForceFailure = true;
        } else if ((arg == "--env" || arg == "--hdr") && i + 1 < argc) {
            std::string customHdr = argv[++i];
            auto path = ux3d::imgui::StdFileSystem::instance().toPath(customHdr.c_str());
            scenarioEnvironmentPath = path.toString().c_str();
            scenarioEnvironmentLoaded = false;
            const auto environmentInitStart = ux3d::imgui::profiling::Clock::now();
            gltfLayer->setEnvironmentMap(path);
            startupProfile.environmentInitMs += ux3d::imgui::profiling::elapsedMs(environmentInitStart);
            const auto environmentFilterStart = ux3d::imgui::profiling::Clock::now();
            if (gltfLayer->filterEnvironmentMap()) {
                startupProfile.environmentFilterMs += ux3d::imgui::profiling::elapsedMs(environmentFilterStart);
                startupProfile.environmentLoads++;
                scenarioEnvironmentLoaded = true;
                activeViewport->settings.currentHdrPath = customHdr;
                commandGenerator->setIBLEnabled(true);
                logMessage("Loaded environment map: " + customHdr);
            }
        } else if ((arg == "--aa" || arg == "--anti-aliasing") && i + 1 < argc) {
            std::string aaVal = argv[++i];
            if (aaVal == "none" || aaVal == "0") activeViewport->settings.antiAliasingIdx = 0;
            else if (aaVal == "msaa2" || aaVal == "1") activeViewport->settings.antiAliasingIdx = 1;
            else if (aaVal == "msaa4" || aaVal == "2") activeViewport->settings.antiAliasingIdx = 2;
            else if (aaVal == "msaa8" || aaVal == "3") activeViewport->settings.antiAliasingIdx = 3;
            else if (aaVal == "fxaa" || aaVal == "4") activeViewport->settings.antiAliasingIdx = 4;
            else if (aaVal == "taa" || aaVal == "5") activeViewport->settings.antiAliasingIdx = 5;
        } else if (arg == "--render-mode" && i + 1 < argc) {
            std::string rmVal = toLowerAscii(argv[++i]);
            if (rmVal == "wireframe" || rmVal == "1") {
                activeViewport->settings.renderMode = ux3d::renderer::RenderMode::Wireframe;
                activeViewport->settings.renderModeIdx = 1;
            } else if (rmVal == "default" || rmVal == "shaded" || rmVal == "0" || rmVal == "none") {
                activeViewport->settings.renderMode = ux3d::renderer::RenderMode::None;
                activeViewport->settings.renderModeIdx = 0;
            }
        } else if (!arg.empty() && arg[0] != '-') {
            modelToLoad = arg;
        }
    }

    ux3d::imgui::profiling::setCountingEnabled(s_profilingEnabled);

    // P1.5 diagnostic control: append deterministic entries covering every
    // severity without echoing them to stderr. Used to exercise counters, Copy
    // and Clear with the Logger both closed and open.
    for (size_t i = 0; i < logTestCountArg; ++i) {
        ux3d::core::LogLevel level = ux3d::core::LogLevel::Info;
        ImVec4 color(0.85f, 0.85f, 0.88f, 1.0f);
        const char* severity = "Info";
        switch (i % 4) {
        case 1:
            level = ux3d::core::LogLevel::Warning;
            color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
            severity = "Warning";
            break;
        case 2:
            level = ux3d::core::LogLevel::Error;
            color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            severity = "Error";
            break;
        case 3:
            level = ux3d::core::LogLevel::Fatal;
            color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            severity = "Fatal";
            break;
        default:
            break;
        }
        std::string text = "Log test #" + std::to_string(i) + " [" + severity + "]";
        if (loggerVariableEntriesArg && (i % 3) == 1) {
            text += "\ncontinued test line " + std::to_string(i);
        } else if (loggerVariableEntriesArg && (i % 3) == 2) {
            text += " " + std::string(160, 'W');
        }
        s_consoleLogs.append({ std::move(text), color, level });
    }

    if (s_benchmarkEnabled && s_benchmarkOutputPath.empty()) {
        s_benchmarkOutputPath = "glider-benchmark.json";
    }
    if (!s_captureViewportPath.empty() && !s_captureFrameProvided && !s_captureTimeProvided) {
        s_captureFrame = 8;
        s_captureFrameProvided = true;
    }
    if (s_captureFrameProvided && s_captureFrame == 0) {
        std::fprintf(stderr, "--capture-frame must be at least 1\n");
        return 1;
    }
    if (s_captureTimeProvided && s_captureTimeSeconds < 0.0) {
        std::fprintf(stderr, "--capture-time must be non-negative\n");
        return 1;
    }

    if (!conflictSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(
            animationPlayer.runConflictSelfCheck(selfCheckForceFailure),
            conflictSelfCheckOutputPath,
            "conflict");
    }

    if (!validationCacheSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(
            ux3d::imgui::ValidationPanel::runCacheSelfCheck(selfCheckForceFailure),
            validationCacheSelfCheckOutputPath,
            "validation cache");
    }

    if (!cacheInvalidationSelfCheckOutputPath.empty()) {
        const auto hierarchyResult =
            ux3d::imgui::SceneHierarchyPanel::runCacheInvalidationSelfCheck(selfCheckForceFailure);
        const auto resourcesResult =
            ux3d::imgui::ResourcesPanel::runCacheInvalidationSelfCheck(selfCheckForceFailure);
        const auto validationResult =
            ux3d::imgui::ValidationPanel::runReportRevisionSelfCheck(selfCheckForceFailure);

        ux3d::imgui::SelfCheckResult combined;
        combined.success = hierarchyResult.success && resourcesResult.success && validationResult.success;
        std::ostringstream report;
        report << hierarchyResult.report;
        report << "\n";
        report << resourcesResult.report;
        report << "\n";
        report << validationResult.report;
        combined.report = report.str();
        return writeSelfCheckReport(combined, cacheInvalidationSelfCheckOutputPath, "cache invalidation");
    }

    if (!loggerLayoutSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(
            runConsoleLogLayoutSelfCheck(selfCheckForceFailure),
            loggerLayoutSelfCheckOutputPath,
            "logger layout");
    }

    if (!aaTargetSelfCheckOutputPath.empty()) {
        return writeSelfCheckReport(
            ux3d::imgui::AntiAliasing::runTargetAllocationSelfCheck(selfCheckForceFailure),
            aaTargetSelfCheckOutputPath,
            "AA target allocation");
    }

    if (!samplerSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({ commandGenerator.get() });
        };
        return writeSelfCheckReport(
            ux3d::imgui::runSamplerSelfCheck(gladGL.get(), selfCheckForceFailure, samplerSelfCheckCase, samplerDumpPatternPath,
                                             glRenderer.get(), reloadAndDraw),
            samplerSelfCheckOutputPath,
            "sampler objects and cache");
    }

    if (!vaoSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({ commandGenerator.get() });
        };
        return writeSelfCheckReport(
            ux3d::imgui::runVaoSelfCheck(gladGL.get(), selfCheckForceFailure, vaoSelfCheckCase,
                                         glRenderer.get(), commandGenerator.get(), reloadAndDraw),
            vaoSelfCheckOutputPath,
            "VAO reuse and cache");
    }

    if (!polygonModeSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({ commandGenerator.get() });
        };
        return writeSelfCheckReport(
            ux3d::imgui::runPolygonModeSelfCheck(gladGL.get(), selfCheckForceFailure, polygonModeSelfCheckCase,
                                                 glRenderer.get(), commandGenerator.get(), reloadAndDraw),
            polygonModeSelfCheckOutputPath,
            "polygon mode cache");
    }

    if (!cullWindingSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({ commandGenerator.get() });
        };
        return writeSelfCheckReport(
            ux3d::imgui::runCullWindingSelfCheck(gladGL.get(), selfCheckForceFailure, cullWindingSelfCheckCase,
                                                 glRenderer.get(), commandGenerator.get(), reloadAndDraw),
            cullWindingSelfCheckOutputPath,
            "culling and winding cache");
    }

    if (!depthSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({ commandGenerator.get() });
        };
        return writeSelfCheckReport(
            ux3d::imgui::runDepthSelfCheck(gladGL.get(), selfCheckForceFailure, depthSelfCheckCase,
                                           glRenderer.get(), commandGenerator.get(), reloadAndDraw),
            depthSelfCheckOutputPath,
            "depth test and depth func cache");
    }

    if (!blendSelfCheckOutputPath.empty()) {
        auto reloadAndDraw = [&](const std::string& path) -> bool {
            bool ok = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
            if (!ok || activeScene == nullptr) return false;
            auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
            commandGenerator->update(scene);
            return glRenderer->draw({ commandGenerator.get() });
        };
        return writeSelfCheckReport(
            ux3d::imgui::runBlendSelfCheck(gladGL.get(), selfCheckForceFailure, blendSelfCheckCase,
                                           glRenderer.get(), commandGenerator.get(), reloadAndDraw),
            blendSelfCheckOutputPath,
            "blending state cache");
    }

    if (!s_isolatedState) {
        ux3d::imgui::AppSettings::instance().loadSettings();
        s_recentFiles = loadRecentFiles();
    } else {
        // Keep any panel-triggered save away from the user's settings.ini.
        std::error_code tempError;
        std::filesystem::path tempDir = std::filesystem::temp_directory_path(tempError);
        std::filesystem::path tempSettings = tempError
            ? std::filesystem::path("glider-scenario-settings.ini")
            : tempDir / "glider-scenario-settings.ini";
        ux3d::imgui::AppSettings::instance().setIniFilePath(tempSettings.string());
    }

    if (hierarchyDisplayProvided) {
        ux3d::imgui::AppSettings::instance().setHierarchyOptions(hierarchyDisplayValue);
    }
    if (resourceDisplayProvided) {
        ux3d::imgui::AppSettings::instance().setResourceOptions(resourceDisplayValue);
    }

    if (ux3d::imgui::StdFileSystem::instance().isFile(ux3d::imgui::StdFileSystem::instance().toFile(modelToLoad.c_str()))) {
        modelLoaded = loadGltfModel(modelToLoad, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
        updateWindowTitle(window, currentFilePath);

        auto renameNamedElement = [](auto& elements, const std::string& assignment, const char* label) {
            const size_t separator = assignment.find('=');
            if (separator == std::string::npos) {
                return false;
            }
            const std::string oldName = assignment.substr(0, separator);
            const std::string newName = assignment.substr(separator + 1);
            for (auto& element : elements) {
                if (element.hasName() && element.getName() == oldName.c_str()) {
                    element.setName(newName.c_str());
                    std::printf("Renamed %s '%s' to '%s'\n", label, oldName.c_str(), newName.c_str());
                    return true;
                }
            }
            return false;
        };

        if (!renameNodeArg.empty()) {
            renameNamedElement(gltfModel.getNodes(), renameNodeArg, "node");
        }
        if (!renameMeshArg.empty()) {
            renameNamedElement(gltfModel.getMeshes(), renameMeshArg, "mesh");
        }
        if (!renameMaterialArg.empty()) {
            renameNamedElement(gltfModel.getElements<ux3d::gltf::Material>(), renameMaterialArg, "material");
        }

        if (!selectNodeArg.empty()) {
            for (auto& node : gltfModel.getNodes()) {
                if (node.hasName() && node.getName() == selectNodeArg.c_str()) {
                    selection.selectNode(&node);
                    if (activeViewport) {
                        focusCameraOnNode(&node, gltfModel, gltfLayer.get(), activeViewport->cameraActor, activeViewport->antiAliasing, isSceneDirty);
                    }
                    break;
                }
            }
        }
        if (!hideNodeArg.empty()) {
            std::string target = hideNodeArg;
            if (target.size() >= 2 && target.front() == '"' && target.back() == '"') {
                target = target.substr(1, target.size() - 2);
            }
            for (auto& node : gltfModel.getNodes()) {
                if ((node.hasName() && (node.getName() == target || node.getName() == hideNodeArg)) ||
                    std::to_string(node.m_ID) == hideNodeArg) {
                    node.setVisible(false);
                    isSceneDirty = true;
                    std::printf("Hiding node %u: %s\n", node.m_ID, node.hasName() ? node.getName().c_str() : "");
                    break;
                }
            }
        }
        if (!selectMaterialArg.empty()) {
            for (auto& mat : gltfModel.getElements<ux3d::gltf::Material>()) {
                if ((mat.hasName() && mat.getName() == selectMaterialArg.c_str()) ||
                    std::to_string(mat.m_ID) == selectMaterialArg) {
                    selection.selectMaterial(&mat);
                    break;
                }
            }
        }
        if (!selectImageArg.empty()) {
            for (auto& img : gltfModel.getElements<ux3d::gltf::Image>()) {
                if ((img.hasName() && img.getName() == selectImageArg.c_str()) ||
                    std::to_string(img.m_ID) == selectImageArg ||
                    (!img.getUri().empty() && img.getUri().find(selectImageArg) != std::string::npos)) {
                    selection.selectImage(&img);
                    break;
                }
            }
        }
        if (!selectTextureArg.empty()) {
            for (auto& tex : gltfModel.getElements<ux3d::gltf::Texture>()) {
                if ((tex.hasName() && tex.getName() == selectTextureArg.c_str()) ||
                    std::to_string(tex.m_ID) == selectTextureArg) {
                    selection.selectTexture(&tex);
                    break;
                }
            }
        }
        if (animTimeArg >= 0.0f) {
            animationPlayer.setCurrentTime(animTimeArg, gltfLayer.get());
        }

        if (!newViewportCamArg.empty() && activeScene != nullptr) {
            std::string camTarget = newViewportCamArg;
            if (camTarget.size() >= 2 && camTarget.front() == '"' && camTarget.back() == '"') {
                camTarget = camTarget.substr(1, camTarget.size() - 2);
            }
            std::function<void(const ux3d::gltf::Node*)> checkNode = [&](const ux3d::gltf::Node* n) {
                if (!n) return;
                if (n->getCamera() != nullptr) {
                    std::string cName = n->hasName() ? n->getName() : ("Camera Node " + std::to_string(n->m_ID));
                    if (cName == camTarget || std::to_string(n->m_ID) == camTarget) {
                        uint32_t nextId = 1;
                        for (const auto& v : viewports) {
                            if (v->id >= nextId) nextId = v->id + 1;
                        }
                        std::string vpName = "Camera: " + cName;
                        auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
                        newVp->isAttachedToCamera = true;
                        newVp->attachedCameraNode = n;
                        newVp->attachedCameraNodeId = n->m_ID;
                        newVp->attachedCameraNodeName = cName;
                        if (activeViewport != nullptr) {
                            newVp->settings = activeViewport->settings;
                        }
                        newVp->requestFocus = true;
                        activeViewport = newVp.get();
                        viewports.push_back(std::move(newVp));
                        isSceneDirty = true;
                        return;
                    }
                }
                for (auto* child : n->getChildren()) {
                    checkNode(child);
                }
            };
            for (auto* rootNode : activeScene->getNodes()) {
                checkNode(rootNode);
            }
        }

        if (selectViewportArg != 0) {
            for (auto& vp : viewports) {
                if (vp && vp->id == selectViewportArg) {
                    selection.selectViewport(vp->id);
                    break;
                }
            }
        }
    } else {
        updateWindowTitle(window, "untitled.glb");
    }

    if (!reloadModelArg.empty()) {
        modelLoaded = loadGltfModel(reloadModelArg, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
        updateWindowTitle(window, currentFilePath);
        std::printf("Reloaded model: %s\n", reloadModelArg.c_str());
    }

    // P0.1 explicit camera/projection controls are applied after model load and
    // after --select-node framing so the requested pose always wins.
    const bool cameraOverrideRequested = cameraYawProvided || cameraPitchProvided ||
        cameraDistanceProvided || cameraTargetProvided || cameraFovProvided ||
        !cameraProjectionArg.empty();
    if (cameraOverrideRequested && activeViewport != nullptr) {
        auto& viewport = *activeViewport;

        if (cameraFovProvided) {
            if (cameraFovArg > 0.1f && cameraFovArg < 179.0f) {
                auto& camera = viewport.rendererCamera;
                camera.setPerspective(camera.getAspectRatio(), glm::radians(cameraFovArg),
                                      camera.getZNearPlane(), camera.getZFarPlane());
            } else {
                logMessage("Ignoring --camera-fov outside (0.1, 179) degrees", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
        }

        if (cameraTargetProvided) {
            viewport.cameraActor.setTarget(ux3d::math::vec3(cameraTargetX, cameraTargetY, cameraTargetZ));
        }

        if (cameraDistanceProvided) {
            if (cameraDistanceArg > 0.0f) {
                viewport.cameraActor.setZoom(cameraDistanceArg);
            } else {
                logMessage("Ignoring non-positive --camera-distance", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
        }

        if (cameraYawProvided || cameraPitchProvided) {
            const float yaw = cameraYawProvided ? glm::radians(cameraYawArg) : viewport.cameraActor.getRotationX();
            const float pitch = cameraPitchProvided ? glm::radians(cameraPitchArg) : viewport.cameraActor.getRotationY();
            viewport.cameraActor.setCameraRotation(yaw, pitch);
        } else {
            viewport.cameraActor.updateUserCameraTransform();
        }

        if (!cameraProjectionArg.empty()) {
            if (cameraProjectionArg == "ortho" || cameraProjectionArg == "orthographic") {
                viewport.cameraActor.changeUserCameraType(ux3d::renderer::Camera::Type::Orthographic);
            } else if (cameraProjectionArg == "perspective" || cameraProjectionArg == "persp") {
                viewport.cameraActor.changeUserCameraType(ux3d::renderer::Camera::Type::Perspective);
            } else {
                logMessage("Unknown --camera-projection (use perspective or orthographic)", ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
            }
        }

        viewport.antiAliasing.resetHistory();
        isSceneDirty = true;
    }

    // UI state
    bool showDemoWindow = false;
    static bool showInspector = true;
    static bool showHierarchy = true;
    static bool showResources = true;
    static bool showLogger = showLoggerArg;
    static bool showStatistics = showStatisticsArg;
    static bool showValidation = false;
    static bool showSearchPaths = false;
    if (showValidationArg) {
        showValidation = true;
        ux3d::imgui::ValidationPanel::requestFocus();
    }
    if (showSearchPathsArg) {
        showSearchPaths = true;
    }

    // P0.2 startup snapshot: load/decode, renderer upload, validation and IBL
    // are measured separately. Counter state is then reset for frame sampling.
    {
        auto& startup = ux3d::imgui::profiling::startupProfile();
        startup.totalStartupMs = ux3d::imgui::profiling::elapsedMs(startupStart);
        startup.stages = {
            { "startup.vfsDecompress", startup.vfsDecompressMs },
            { "startup.environmentInit", startup.environmentInitMs },
            { "startup.environmentFilter", startup.environmentFilterMs },
            { "startup.loadDecode", startup.loadDecodeMs },
            { "startup.rendererUpload", startup.rendererUploadMs },
            { "startup.validation", startup.validationMs },
            { "startup.total", startup.totalStartupMs }
        };
        startup.counters = ux3d::imgui::profiling::frameCounters();
        ux3d::imgui::profiling::resetFrameCounters();
    }
    if (gltfLayer != nullptr) {
        gltfLayer->setCpuProfilingEnabled(s_profilingEnabled);
    }
    if (glRenderer != nullptr) {
        glRenderer->setGpuProfilingEnabled(s_profilingEnabled);
    }

    if (!uiScriptPath.empty()) {
        if (uiReportPath.empty() || !uiScenario.configure(uiScriptPath, uiReportPath)) {
            std::fprintf(stderr, "--ui-script requires a readable script and --ui-report\n");
            s_outputFailed = true;
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    double lastFrameTime = glfwGetTime();
    uint64_t frameCount = 0;
    ux3d::imgui::profiling::Clock::time_point previousFrameStart = ux3d::imgui::profiling::Clock::now();

    // Main Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const auto frameStart = ux3d::imgui::profiling::Clock::now();
        const uint64_t frameNumber = frameCount + 1;
        ux3d::imgui::profiling::FrameSample frameSample;
        frameSample.frame = frameNumber;
        if (frameNumber > 1) {
            frameSample.frameIntervalMs = ux3d::imgui::profiling::elapsedMs(previousFrameStart, frameStart);
        }
        if (frameNumber == 1) {
            s_captureEpochSeconds = glfwGetTime();
        }
        previousFrameStart = frameStart;
        ux3d::imgui::profiling::resetFrameCounters();
        const auto markStage = [&](const char* name, ux3d::imgui::profiling::Clock::time_point start) {
            if (s_profilingEnabled) {
                frameSample.stages.push_back({ name, ux3d::imgui::profiling::elapsedMs(start) });
            }
        };

        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastFrameTime);
        lastFrameTime = currentTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // Update animation player
        const auto animationUpdateStart = ux3d::imgui::profiling::Clock::now();
        animationPlayer.update(deltaTime, gltfLayer.get());
        markStage("animation.update", animationUpdateStart);

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
                    activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Back : ux3d::editor::CameraOrientation::Front);
                    activeViewport->antiAliasing.resetHistory();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad3)) {
                    activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Left : ux3d::editor::CameraOrientation::Right);
                    activeViewport->antiAliasing.resetHistory();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad7)) {
                    activeViewport->cameraActor.orientUserCamera(io.KeyCtrl ? ux3d::editor::CameraOrientation::Bottom : ux3d::editor::CameraOrientation::Top);
                    activeViewport->antiAliasing.resetHistory();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Keypad5)) {
                    activeViewport->cameraActor.changeUserCameraType((activeViewport->rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic) ? ux3d::renderer::Camera::Type::Perspective : ux3d::renderer::Camera::Type::Orthographic);
                    activeViewport->antiAliasing.resetHistory();
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal)) {
                if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                    activeViewport->cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
                } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                    activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                }
                activeViewport->antiAliasing.resetHistory();
            }

            // Ctrl+Alt+1: Default Shaded, Ctrl+Alt+2: Wireframe
            if (io.KeyCtrl && io.KeyAlt) {
                if (ImGui::IsKeyPressed(ImGuiKey_1)) {
                    activeViewport->settings.renderMode = ux3d::renderer::RenderMode::None;
                    activeViewport->settings.renderModeIdx = 0;
                    isSceneDirty = true;
                } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
                    activeViewport->settings.renderMode = ux3d::renderer::RenderMode::Wireframe;
                    activeViewport->settings.renderModeIdx = 1;
                    isSceneDirty = true;
                }
            }
        }

        // Start ImGui frame
        const auto uiNewFrameStart = ux3d::imgui::profiling::Clock::now();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        if (uiScenario.active()) {
            uiScenario.beginFrame(ImGui::GetIO());
        }
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        markStage("ui.newFrame", uiNewFrameStart);
        const auto uiChromeStart = ux3d::imgui::profiling::Clock::now();

        // Main DockSpace covering the whole window (minus status bar at bottom)
        float statusBarHeight = ImGui::GetFrameHeight() + 8.0f;
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImVec2 dockspaceSize = ImVec2(mainViewport->WorkSize.x, mainViewport->WorkSize.y - statusBarHeight);
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(dockspaceSize);
        ImGui::SetNextWindowViewport(mainViewport->ID);
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("GliderDockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("GliderMainDockSpace_v1");

        static bool s_firstLayout = true;
        static bool s_resetLayout = false;
        if (s_firstLayout || s_resetLayout) {
            bool force = s_resetLayout || s_isolatedState;
            s_firstLayout = false;
            s_resetLayout = false;
            setupDefaultDockLayout(dockspaceId, dockspaceSize, force);
        }

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

        // Global Keyboard Shortcuts (File & View)
        if (!io.WantTextInput && io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_O)) {
                auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                if (!openResult.empty()) {
                    loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                    updateWindowTitle(window, currentFilePath);
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_1)) showInspector = !showInspector;
            if (ImGui::IsKeyPressed(ImGuiKey_2)) showHierarchy = !showHierarchy;
            if (ImGui::IsKeyPressed(ImGuiKey_3)) {
                if (activeViewport) {
                    activeViewport->isOpen = !activeViewport->isOpen;
                    if (activeViewport->isOpen) activeViewport->requestFocus = true;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_4)) showResources = !showResources;
            if (ImGui::IsKeyPressed(ImGuiKey_5)) showLogger = !showLogger;
            if (ImGui::IsKeyPressed(ImGuiKey_6)) {
                if (activeViewport) selection.selectViewport(activeViewport->id);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_7)) showStatistics = !showStatistics;
            if (ImGui::IsKeyPressed(ImGuiKey_8)) showValidation = !showValidation;
            if (ImGui::IsKeyPressed(ImGuiKey_L)) {
                showInspector = showHierarchy = showResources = true;
                showLogger = showStatistics = showValidation = false;
                for (auto& vp : viewports) vp->isOpen = true;
                s_resetLayout = true;
            }
        }

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem(ICON_MDI_FOLDER_OPEN " Open ...", "Ctrl+O")) {
                    auto openResult = pfd::open_file("Open glTF / GLB", "", { "glTF files (*.gltf *.glb)", "*.gltf *.glb" }).result();
                    if (!openResult.empty()) {
                        loadGltfModel(openResult[0], gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                        updateWindowTitle(window, currentFilePath);
                    }
                }
                if (ImGui::BeginMenu(ICON_MDI_HISTORY " Open Recent")) {
                    if (s_recentFiles.empty()) {
                        ImGui::MenuItem("(No Recent Files)", nullptr, false, false);
                    } else {
                        std::string fileToOpen = "";
                        for (const auto& recentPath : s_recentFiles) {
                            if (ImGui::MenuItem(recentPath.c_str())) {
                                fileToOpen = recentPath;
                            }
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem(ICON_MDI_DELETE_OUTLINE " Clear Menu")) {
                            s_recentFiles.clear();
                            saveRecentFiles(s_recentFiles);
                        }
                        if (!fileToOpen.empty()) {
                            loadGltfModel(fileToOpen, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                            updateWindowTitle(window, currentFilePath);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MDI_FOLDER_SEARCH_OUTLINE " Search Paths...", nullptr, &showSearchPaths)) {
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MDI_EXIT_TO_APP " Exit")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            // Viewport Menu (Replaces previous View menu)
            if (ImGui::BeginMenu("Viewport")) {
                if (ImGui::MenuItem(ICON_MDI_PLUS " New Viewport")) {
                    uint32_t nextId = 1;
                    for (const auto& v : viewports) {
                        if (v->id >= nextId) nextId = v->id + 1;
                    }
                    std::string vpName = "Viewport " + std::to_string(nextId);
                    auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
                    if (activeViewport != nullptr) {
                        newVp->settings = activeViewport->settings;
                        if (activeScene != nullptr) {
                            newVp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                        }
                    }
                    newVp->requestFocus = true;
                    activeViewport = newVp.get();
                    selection.selectViewport(newVp->id);
                    viewports.push_back(std::move(newVp));
                }

                if (ImGui::BeginMenu(ICON_MDI_CAMERA_PLUS_OUTLINE " New Viewport From")) {
                    auto cameraNodes = collectSceneCameraNodes(activeScene);
                    if (cameraNodes.empty()) {
                        ImGui::MenuItem("(No Cameras in Scene)", nullptr, false, false);
                    } else {
                        for (const auto* camNode : cameraNodes) {
                            std::string camName = camNode->hasName() ? camNode->getName() : ("Camera Node " + std::to_string(camNode->m_ID));
                            std::string itemTitle = ICON_MDI_CAMERA " " + camName;
                            if (ImGui::MenuItem(itemTitle.c_str())) {
                                uint32_t nextId = 1;
                                for (const auto& v : viewports) {
                                    if (v->id >= nextId) nextId = v->id + 1;
                                }
                                std::string vpName = "Camera: " + camName;
                                auto newVp = std::make_unique<ux3d::imgui::ViewportInstance>(nextId, vpName, gladGL.get());
                                newVp->isAttachedToCamera = true;
                                newVp->attachedCameraNode = camNode;
                                newVp->attachedCameraNodeId = camNode->m_ID;
                                newVp->attachedCameraNodeName = camName;
                                if (activeViewport != nullptr) {
                                    newVp->settings = activeViewport->settings;
                                }
                                newVp->requestFocus = true;
                                activeViewport = newVp.get();
                                selection.selectViewport(newVp->id);
                                viewports.push_back(std::move(newVp));
                            }
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();

                bool hasActiveVp = (activeViewport != nullptr);

                bool showGrid = hasActiveVp && activeViewport->settings.showGrid;
                if (ImGui::MenuItem(ICON_MDI_GRID " Toggle Show Grid", nullptr, showGrid, hasActiveVp)) {
                    activeViewport->settings.showGrid = !activeViewport->settings.showGrid;
                }

                bool showBBox = hasActiveVp && (activeViewport->settings.showSceneBoundingBox || activeViewport->settings.showSelectedBoundingBox);
                if (ImGui::MenuItem(ICON_MDI_CUBE_OUTLINE " Toggle Show Bounding Box", nullptr, showBBox, hasActiveVp)) {
                    bool next = !showBBox;
                    activeViewport->settings.showSceneBoundingBox = next;
                    activeViewport->settings.showSelectedBoundingBox = next;
                }

                bool isPersp = hasActiveVp && (activeViewport->rendererCamera.getType() == ux3d::renderer::Camera::Type::Perspective);
                if (ImGui::MenuItem(ICON_MDI_CAMERA " Toggle Persp / Orthographic", "Num 5", isPersp, hasActiveVp && !activeViewport->isAttachedToCamera)) {
                    activeViewport->cameraActor.changeUserCameraType(isPersp ? ux3d::renderer::Camera::Type::Orthographic : ux3d::renderer::Camera::Type::Perspective);
                    activeViewport->antiAliasing.resetHistory();
                }

                bool isTurntable = hasActiveVp && activeViewport->settings.turntable;
                if (ImGui::MenuItem(ICON_MDI_ROTATE_ORBIT " Toggle Orbit (Turntable)", nullptr, isTurntable, hasActiveVp)) {
                    activeViewport->settings.turntable = !activeViewport->settings.turntable;
                }

                ImGui::Separator();

                bool isAttached = hasActiveVp && activeViewport->isAttachedToCamera;
                if (ImGui::MenuItem(ICON_MDI_CAMERA_OFF " Detach from Camera", nullptr, false, isAttached)) {
                    activeViewport->isAttachedToCamera = false;
                    activeViewport->attachedCameraNode = nullptr;
                    activeViewport->attachedCameraNodeId = 0;
                    activeViewport->attachedCameraNodeName = "";
                    isSceneDirty = true;
                }

                if (ImGui::BeginMenu(ICON_MDI_COMPASS_OUTLINE " Orientation", hasActiveVp && !isAttached)) {
                    if (ImGui::MenuItem("Front", "Num 1")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Front); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Back", "Ctrl+Num 1")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Back); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Right", "Num 3")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Right); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Left", "Ctrl+Num 3")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Left); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Top", "Num 7")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Top); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Bottom", "Ctrl+Num 7")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Bottom); activeViewport->antiAliasing.resetHistory(); }
                    if (ImGui::MenuItem("Isometric")) { activeViewport->cameraActor.orientUserCamera(ux3d::editor::CameraOrientation::Isometric); activeViewport->antiAliasing.resetHistory(); }
                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem(ICON_MDI_HOME_OUTLINE " Reset Camera", "Home", false, hasActiveVp)) {
                    if (activeScene != nullptr) activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                    activeViewport->antiAliasing.resetHistory();
                }

                if (ImGui::MenuItem(ICON_MDI_CROSSHAIRS_GPS " Fit to Scene", "F", false, hasActiveVp && activeScene != nullptr)) {
                    if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                        activeViewport->cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
                    } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                        activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                    }
                    activeViewport->antiAliasing.resetHistory();
                }

                if (ImGui::BeginMenu(ICON_MDI_SHIELD_OUTLINE " Shading", hasActiveVp)) {
                    for (const auto& item : s_shadingModes) {
                        if (item.isSeparator) {
                            ImGui::Separator();
                        } else {
                            bool isSelected = (activeViewport->settings.renderMode == item.mode);
                            if (ImGui::MenuItem(item.label, item.shortcut, isSelected)) {
                                activeViewport->settings.renderMode = item.mode;
                                activeViewport->settings.renderModeIdx = getIndexFromRenderMode(item.mode);
                                isSceneDirty = true;
                            }
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Panel")) {
                ImGui::MenuItem(WND_INSPECTOR, "Ctrl+1", &showInspector);
                ImGui::MenuItem(WND_HIERARCHY, "Ctrl+2", &showHierarchy);
                ImGui::MenuItem(WND_RESOURCES, "Ctrl+4", &showResources);
                ImGui::MenuItem(WND_LOGGER, "Ctrl+5", &showLogger);
                ImGui::MenuItem(WND_STATISTICS, "Ctrl+7", &showStatistics);
                ImGui::MenuItem(WND_VALIDATION, "Ctrl+8", &showValidation);
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_MDI_REFRESH " Reset Layout", "Ctrl+L")) {
                    showInspector = showHierarchy = showResources = true;
                    showLogger = showStatistics = showValidation = false;
                    for (auto& vp : viewports) vp->isOpen = true;
                    s_resetLayout = true;
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::End(); // End DockSpace

        // -------------------------------------------------------------
        // Synchronize glTF Scene & Animation before Viewports Render
        // -------------------------------------------------------------
        markStage("ui.chrome", uiChromeStart);
        const auto dirtySyncStart = ux3d::imgui::profiling::Clock::now();
        if (animationPlayer.isPlaying()) {
            isSceneDirty = true;
        }

        if (isSceneDirty && gltfLayer != nullptr) {
            ux3d::imgui::profiling::countDirtySync();
            auto stageStart = ux3d::imgui::profiling::Clock::now();
            gltfLayer->updateGltf(gltfModel);
            if (s_profilingEnabled) {
                ++ux3d::imgui::profiling::frameCounters().updateGltfCalls;
                ++ux3d::imgui::profiling::frameCounters().cacheRebuilds;
            }
            markStage("scene.updateGltf", stageStart);

            if (activeScene != nullptr) {
                stageStart = ux3d::imgui::profiling::Clock::now();
                gltfLayer->updateScene(gltfModel, *activeScene);
                if (s_profilingEnabled) {
                    ++ux3d::imgui::profiling::frameCounters().updateSceneCalls;
                    ++ux3d::imgui::profiling::frameCounters().cacheRebuilds;
                }
                markStage("scene.updateScene", stageStart);
            }

            stageStart = ux3d::imgui::profiling::Clock::now();
            gltfLayer->applyAnimationTime(animationPlayer.getCurrentTime());
            if (s_profilingEnabled) {
                ++ux3d::imgui::profiling::frameCounters().applyAnimationCalls;
                const auto& animationProfile = gltfLayer->getLastAnimationProfile();
                frameSample.stages.push_back({ "animation.channels", animationProfile.channelsMs });
                frameSample.stages.push_back({ "animation.transforms", animationProfile.transformsMs });
                frameSample.stages.push_back({ "animation.skins", animationProfile.skinsMs });
            }
            markStage("scene.applyAnimation", stageStart);
        }
        markStage("scene.dirtySync", dirtySyncStart);

        bool frameSceneDirty = isSceneDirty;
        isSceneDirty = false;

        // -------------------------------------------------------------
        // Viewport Windows (Multi-Viewport)
        // -------------------------------------------------------------
        for (auto& vp : viewports) {
            if (!vp->isOpen) {
                vp->antiAliasing.releaseTargets();
                continue;
            }

            std::string wndTitle = std::string(ICON_MDI_CUBE_OUTLINE " ") + vp->name + "###Viewport_" + std::to_string(vp->id);

            ImGui::SetNextWindowDockID(s_centralDockId, ImGuiCond_Appearing);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            bool isVpVisible = ImGui::Begin(wndTitle.c_str(), &vp->isOpen);
            ImGui::PopStyleVar();

            if (!vp->isOpen) {
                vp->antiAliasing.releaseTargets();
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

            // Quick Navigation & Inspection Toolbar Strip
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 2.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
            ImGui::BeginGroup();

            // 1. Perspective / Orthographic Toggle (E)
            bool isOrtho = (vp->rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic);
            const char* projIcon = isOrtho ? ICON_MDI_PERSPECTIVE_LESS : ICON_MDI_PERSPECTIVE_MORE;
            if (ImGui::SmallButton(projIcon)) {
                if (!vp->isAttachedToCamera) {
                    vp->cameraActor.changeUserCameraType(isOrtho ? ux3d::renderer::Camera::Type::Perspective : ux3d::renderer::Camera::Type::Orthographic);
                    vp->antiAliasing.resetHistory();
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(isOrtho ? "Orthographic (Click to switch to Perspective)" : "Perspective (Click to switch to Orthographic)");
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
                ImGui::SetTooltip(vp->settings.showGrid ? "Floor Grid (Active - click to hide)" : "Floor Grid (Hidden - click to show)");
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
                ImGui::SetTooltip(bboxActive ? "Bounding Boxes (Active - click to hide)" : "Bounding Boxes (Hidden - click to show)");
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
                ImGui::SetTooltip(vp->settings.turntable ? "Turntable Orbit (Active - click to stop)" : "Turntable Orbit (Stopped - click to start)");
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
                            isSceneDirty = true;
                        }
                    }
                    for (const auto& v : variants) {
                        bool isSel = (gltfLayer && gltfLayer->getMaterialVariant().has_value() && gltfLayer->getMaterialVariant().value() == v.m_ID);
                        std::string vLabel = v.hasName() ? v.getName().c_str() : ("Var " + std::to_string(v.m_ID));
                        if (ImGui::Selectable(vLabel.c_str(), isSel)) {
                            if (gltfLayer) {
                                gltfLayer->setMaterialVariant(v.m_ID);
                                isSceneDirty = true;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            // Focus (F) button
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_MDI_CROSSHAIRS_GPS " Focus (F)")) {
                if (selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                    vp->cameraActor.presentFocusNode(gltfLayer.get(), selection.selectedNode, &gltfModel);
                } else if (activeScene != nullptr && !activeScene->getNodes().empty()) {
                    vp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                }
                vp->antiAliasing.resetHistory();
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
                ImGui::SetTooltip(vp->isAttachedToCamera ? "Camera Attached (Click to switch camera or Free Look)" : "Free Look (Click to attach to a glTF camera)");
            }

            if (ImGui::BeginPopup(camPopupId.c_str())) {
                bool isFreeLook = !vp->isAttachedToCamera;
                if (ImGui::MenuItem(ICON_MDI_EYE_OUTLINE " Free Look", nullptr, isFreeLook)) {
                    vp->isAttachedToCamera = false;
                    vp->attachedCameraNode = nullptr;
                    vp->attachedCameraNodeId = 0;
                    vp->attachedCameraNodeName = "";
                    isSceneDirty = true;
                }
                ImGui::Separator();

                auto cameraNodes = collectSceneCameraNodes(activeScene);
                if (cameraNodes.empty()) {
                    ImGui::TextDisabled("  (No Cameras in Scene)");
                } else {
                    for (const auto* camNode : cameraNodes) {
                        std::string cName = camNode->hasName() ? camNode->getName() : ("Camera Node " + std::to_string(camNode->m_ID));
                        std::string camItemLabel = ICON_MDI_CAMERA " " + cName;
                        bool isSelected = vp->isAttachedToCamera && (vp->attachedCameraNodeId == camNode->m_ID || vp->attachedCameraNode == camNode);
                        if (ImGui::MenuItem(camItemLabel.c_str(), nullptr, isSelected)) {
                            vp->isAttachedToCamera = true;
                            vp->attachedCameraNode = camNode;
                            vp->attachedCameraNodeId = camNode->m_ID;
                            vp->attachedCameraNodeName = cName;
                            isSceneDirty = true;
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
                for (const auto& item : s_shadingModes) {
                    if (item.isSeparator) {
                        ImGui::Separator();
                    } else {
                        bool isSelected = (vp->settings.renderMode == item.mode);
                        if (ImGui::MenuItem(item.label, item.shortcut, isSelected)) {
                            vp->settings.renderMode = item.mode;
                            vp->settings.renderModeIdx = getIndexFromRenderMode(item.mode);
                            isSceneDirty = true;
                        }
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::EndGroup();
            ImGui::PopStyleVar(2);

            ImVec2 viewportPos = ImGui::GetCursorScreenPos();
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();

            int vpW = static_cast<int>(viewportSize.x);
            int vpH = static_cast<int>(viewportSize.y);
            if (vpW < 1) vpW = 1;
            if (vpH < 1) vpH = 1;

            auto aaMode = static_cast<ux3d::imgui::AntiAliasingMode>(vp->settings.antiAliasingIdx);
            int desiredSamples = ux3d::imgui::AntiAliasing::getSampleCount(aaMode);

            if (vp->sceneFbo.getWidth() != vpW || vp->sceneFbo.getHeight() != vpH || vp->sceneFbo.getSamples() != desiredSamples) {
                vp->sceneFbo.resize(vpW, vpH, desiredSamples);
                if (gltfLayer != nullptr) {
                    gltfLayer->getResourceManager().createFramebufferTexture(vp->sceneFbo.getColorTexture());
                    gltfLayer->getResourceManager().setBlitFramebufferFunction([] {}, ux3d::math::vec4(0.12f, 0.12f, 0.14f, 1.0f));
                    gltfLayer->getResourceManager().setTransmissiveTextureFunction([] {});
                }
                vp->antiAliasing.resetHistory();
            }

            if (vp->pickingFbo.getWidth() != vpW || vp->pickingFbo.getHeight() != vpH) {
                vp->pickingFbo.resize(vpW, vpH, 1);
            }

            pickingLayer.prePass = [&]() {
                vp->pickingFbo.bind();
                glViewport(0, 0, vpW, vpH);
            };
            pickingLayer.postPass = [&]() {
                vp->sceneFbo.bind();
                glViewport(0, 0, vpW, vpH);
            };

            // Clear picking buffer
            if (vp->pickingFbo.isValid()) {
                vp->pickingFbo.bind();
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            }

            // Render to offscreen Framebuffer
            vp->sceneFbo.bind();
            glViewport(0, 0, vpW, vpH);
            if (vp->settings.showBackground) {
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            } else {
                glClearColor(vp->settings.clearColor.r, vp->settings.clearColor.g, vp->settings.clearColor.b, vp->settings.clearColor.a);
            }
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
                    cameraEye = glm::vec3(vp->cameraActor.getEyePosition().x, vp->cameraActor.getEyePosition().y, vp->cameraActor.getEyePosition().z);
                    cameraZoom = vp->cameraActor.getZoom();
                }
            } else {
                vp->rendererCamera.setViewportResolution(vpW, vpH);
                vp->rendererCamera.setAspectRatio(aspect);
                vp->cameraActor.updateUserCameraTransform();
                gltfLayer->setActiveCamera(&vp->rendererCamera);
                view = vp->rendererCamera.getViewMatrix();
                proj = vp->rendererCamera.getProjectionMatrix();
                cameraEye = glm::vec3(vp->cameraActor.getEyePosition().x, vp->cameraActor.getEyePosition().y, vp->cameraActor.getEyePosition().z);
                cameraZoom = vp->cameraActor.getZoom();
            }

            // Subpixel jitter for TAA
            glm::vec2 jitter = vp->antiAliasing.getJitter(aaMode, vpW, vpH);
            glm::mat4 jitterMat = (aaMode == ux3d::imgui::AntiAliasingMode::TAA)
                ? glm::translate(glm::mat4(1.0f), glm::vec3(2.0f * jitter.x, 2.0f * jitter.y, 0.0f))
                : glm::mat4(1.0f);
            glm::mat4 jitteredProj = jitterMat * proj;
            glm::mat4 viewProj = jitteredProj * view;

            if (aaMode == ux3d::imgui::AntiAliasingMode::TAA) {
                if (gltfLayer->getActiveCamera()) {
                    gltfLayer->getActiveCamera()->setProjection(ux3d::math::mat4x4(jitteredProj));
                }
            }

            // 1. Render glTF scene & background skybox
            if (gltfLayer != nullptr && glRenderer != nullptr && commandGenerator != nullptr) {
                const auto sceneBuildStart = ux3d::imgui::profiling::Clock::now();
                gltfLayer->enableCubeMap(vp->settings.showBackground);

                auto mode = vp->settings.renderMode;

                // Apply Face Culling override and Debug Mode cleanly via materialMapping
                for (auto& [gltfMat, renMat] : gltfLayer->m_materialMapping.map) {
                    if (renMat != nullptr) {
                        if (vp->settings.forceDoubleSided) {
                            renMat->isDoubleSided = true;
                        } else {
                            renMat->isDoubleSided = gltfMat ? gltfMat->isDoubleSidedOrDefault() : false;
                        }
                        renMat->defines.define("DEBUG", static_cast<int>(mode));
                    }
                }

                for (auto& [gltfTuple, renMesh] : gltfLayer->m_meshMapping.map) {
                    if (renMesh != nullptr) {
                        auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;
                        auto nodeIt = gltfLayer->m_transformHierarchyMapping.map.find(gltfNode);
                        float det = 1.0f;
                        if (nodeIt != gltfLayer->m_transformHierarchyMapping.map.end() && nodeIt->second != nullptr) {
                            det = ux3d::math::determinant(nodeIt->second->getWorldTransform());
                        }
                        bool baseClockwise = (det < 0.0f);
                        bool isClockwise = baseClockwise ^ vp->settings.invertWinding;
                        renMesh->frontFace = isClockwise ? ux3d::renderer::gl::Winding::Clockwise : ux3d::renderer::gl::Winding::CounterClockwise;

                        renMesh->polygonMode = (mode == ux3d::renderer::RenderMode::Wireframe)
                            ? ux3d::renderer::gl::PolygonMode::Line
                            : ux3d::renderer::gl::PolygonMode::Fill;
                        renMesh->defines.define("DEBUG", static_cast<int>(mode));
                    }
                }

                const auto createSceneStart = ux3d::imgui::profiling::Clock::now();
                auto [scene, reverseMapping] = gltfLayer->createScene(activeScene);
                markStage("scene.create", createSceneStart);
                if (s_profilingEnabled) {
                    const auto& sceneStats = gltfLayer->getLastSceneStats();
                    ux3d::imgui::profiling::countSceneBuild(sceneStats.drawables);
                    ux3d::imgui::profiling::countSceneNodesVisited(sceneStats.nodesVisited);
                }
                currentReverseMapping = reverseMapping;

                auto cubeTuple = gltfLayer->getResourceManager().getCubeMapTuple();
                auto* cubeMesh = std::get<0>(cubeTuple);
                auto* cubeMaterial = std::get<1>(cubeTuple);
                for (auto& [mesh, material] : scene.drawables) {
                    if (mesh != nullptr && mesh != cubeMesh) {
                        mesh->polygonMode = (mode == ux3d::renderer::RenderMode::Wireframe)
                            ? ux3d::renderer::gl::PolygonMode::Line
                            : ux3d::renderer::gl::PolygonMode::Fill;
                        mesh->defines.define("DEBUG", static_cast<int>(mode));
                    }
                    if (material != nullptr && material != cubeMaterial) {
                        material->defines.define("DEBUG", static_cast<int>(mode));
                        if (vp->settings.forceDoubleSided) {
                            material->isDoubleSided = true;
                        }
                    }
                }

                // Apply GPU picking layer
                const auto pickingBuildStart = ux3d::imgui::profiling::Clock::now();
                scene = pickingLayer.applyLayer(std::move(scene));
                markStage("picking.build", pickingBuildStart);
                ux3d::imgui::profiling::countPickingBuild();
                markStage("scene.build", sceneBuildStart);

                // Continuously synchronize viewport lighting, exposure, and environment parameters
                commandGenerator->setExposure(std::pow(2.0f, vp->settings.exposureEV));
                commandGenerator->setEnvironmentMapIntensity(vp->settings.hdrIntensity);
                commandGenerator->setEnvironmentMapRotation(vp->settings.hdrRotation);
                commandGenerator->setEnvironmentMapBlurNormalized(vp->settings.hdrBlur);
                commandGenerator->setIBLEnabled(vp->settings.iblEnabled);
                commandGenerator->setPunctualLightsEnabled(vp->settings.punctualLightsEnabled);

                const auto commandGenerationStart = ux3d::imgui::profiling::Clock::now();
                commandGenerator->update(scene);
                markStage("commands.generate", commandGenerationStart);
                ux3d::imgui::profiling::countCommandGeneration();
                const auto drawSubmissionStart = ux3d::imgui::profiling::Clock::now();
                glRenderer->setGpuFrameContext(frameNumber, vp->id, "main");
                glRenderer->draw({ commandGenerator.get() });
                vp->lastExecutedPasses = glRenderer->getLastExecutedPasses();
                ++vp->renderCount;
                markStage("draw.submit", drawSubmissionStart);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                // Restore unjittered camera projection so subsequent passes and next frame are clean
                if (aaMode == ux3d::imgui::AntiAliasingMode::TAA && gltfLayer->getActiveCamera()) {
                    gltfLayer->getActiveCamera()->setProjection(ux3d::math::mat4x4(proj));
                }

                if (vp->settings.wireframeOverlay && mode != ux3d::renderer::RenderMode::Wireframe) {
                    const auto wireframeStart = ux3d::imgui::profiling::Clock::now();
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glEnable(GL_POLYGON_OFFSET_LINE);
                    glPolygonOffset(-1.0f, -1.0f);
                    glLineWidth(1.0f);
                    auto [wireScene, _] = gltfLayer->createScene(activeScene);
                    for (auto& [mesh, material] : wireScene.drawables) {
                        if (mesh != nullptr && mesh != cubeMesh) {
                            mesh->polygonMode = ux3d::renderer::gl::PolygonMode::Line;
                        }
                    }
                    commandGenerator->update(wireScene);
                    glRenderer->setGpuFrameContext(frameNumber, vp->id, "wireframe");
                    glRenderer->draw({ commandGenerator.get() });
                vp->lastExecutedPasses = glRenderer->getLastExecutedPasses();
                ++vp->renderCount;
                    glDisable(GL_POLYGON_OFFSET_LINE);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    markStage("scene.wireframeOverlay", wireframeStart);
                }
            } else {
                currentReverseMapping.clear();
            }

            // 2. Render ground grid
            const auto overlayStart = ux3d::imgui::profiling::Clock::now();
            if (vp->settings.showGrid) {
                gridRenderer.draw(view, (aaMode == ux3d::imgui::AntiAliasingMode::TAA ? jitteredProj : proj), ux3d::math::vec3(cameraEye.x, cameraEye.y, cameraEye.z), cameraZoom);
                ux3d::imgui::profiling::countOverlayDraw();
            }

            // 3. Render bounding boxes
            if (vp->settings.showSceneBoundingBox && activeScene != nullptr && !activeScene->getNodes().empty()) {
                ux3d::gltf::Extends ext = ux3d::gltf::utils::getExtends(activeScene->getNodes(), &gltfModel);
                bboxRenderer.draw(glm::vec3(ext.min.x, ext.min.y, ext.min.z),
                                  glm::vec3(ext.max.x, ext.max.y, ext.max.z),
                                  glm::vec4(1.0f, 0.85f, 0.2f, 0.85f),
                                  viewProj);
                ux3d::imgui::profiling::countOverlayDraw();
            }
            if (vp->settings.showSelectedBoundingBox && selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr) {
                NodeOBB obb = getNodeOBB(selection.selectedNode, gltfLayer.get());
                if (obb.valid) {
                    bboxRenderer.drawOBB(obb.localMin,
                                         obb.localMax,
                                         obb.modelMatrix,
                                         glm::vec4(1.0f, 0.62f, 0.12f, 1.0f),
                                         viewProj);
                    ux3d::imgui::profiling::countOverlayDraw();
                }
            }
            markStage("overlays.gridBbox", overlayStart);

            vp->sceneFbo.unbind();

            // 4. Post-processing Anti-Aliasing pass
            const auto antiAliasingStart = ux3d::imgui::profiling::Clock::now();
            GLuint displayTexture = vp->antiAliasing.render(aaMode,
                                                            vp->sceneFbo.getColorTexture(),
                                                            vpW, vpH,
                                                            proj * view,
                                                            frameSceneDirty);
            markStage("aa.render", antiAliasingStart);

            if (vp.get() == activeViewport && displayTexture != 0) {
                const double captureElapsed = glfwGetTime() - s_captureEpochSeconds;
                if (!s_captureFirstFramePath.empty() &&
                    frameNumber == 1 &&
                    !s_firstFrameCaptureAttempted) {
                    s_firstFrameCaptureAttempted = true;
                    const bool firstCaptureSucceeded = captureViewportTexture(
                        displayTexture, vpW, vpH,
                        s_captureFirstFramePath,
                        "active-viewport-texture",
                        frameNumber, true, captureElapsed,
                        s_firstFrameCaptureRecord);
                    s_firstFrameCaptureRecord.viewportId = vp->id;
                    if (!firstCaptureSucceeded) {
                        s_outputFailed = true;
                    }
                }

                if (!s_captureViewportPath.empty() && !s_captureAttempted) {
                    const bool byFrame = s_captureFrameProvided && frameNumber == s_captureFrame;
                    const bool byTime = s_captureTimeProvided && captureElapsed >= s_captureTimeSeconds;
                    if (byFrame || byTime) {
                        s_captureAttempted = true;
                        const bool captureSucceeded = captureViewportTexture(
                            displayTexture, vpW, vpH,
                            s_captureViewportPath,
                            "active-viewport-texture",
                            frameNumber, false, captureElapsed,
                            s_captureRecord);
                        s_captureRecord.viewportId = vp->id;
                        if (!captureSucceeded) {
                            s_outputFailed = true;
                        }
                    }
                }
            }

            // Draw offscreen FBO texture inside ImGui viewport window
            if (displayTexture != 0) {
                ImGui::Image(static_cast<ImTextureID>(displayTexture),
                             viewportSize,
                             ImVec2(0.0f, 1.0f),
                             ImVec2(1.0f, 0.0f));
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

                AxisDef axisDefs[] = {
                    { glm::vec3( 1.0f,  0.0f,  0.0f), "X",  IM_COL32(235, 75, 75, 255),   IM_COL32(140, 40, 40, 160), ux3d::editor::CameraOrientation::Right },
                    { glm::vec3(-1.0f,  0.0f,  0.0f), "-X", IM_COL32(180, 70, 70, 200),  IM_COL32(100, 35, 35, 120), ux3d::editor::CameraOrientation::Left },
                    { glm::vec3( 0.0f,  1.0f,  0.0f), "Y",  IM_COL32(75, 220, 95, 255),   IM_COL32(40, 130, 50, 160), ux3d::editor::CameraOrientation::Top },
                    { glm::vec3( 0.0f, -1.0f,  0.0f), "-Y", IM_COL32(70, 170, 85, 200),  IM_COL32(35, 95, 45, 120),  ux3d::editor::CameraOrientation::Bottom },
                    { glm::vec3( 0.0f,  0.0f,  1.0f), "Z",  IM_COL32(70, 140, 245, 255),  IM_COL32(40, 80, 150, 160), ux3d::editor::CameraOrientation::Front },
                    { glm::vec3( 0.0f,  0.0f, -1.0f), "-Z", IM_COL32(65, 115, 200, 200), IM_COL32(35, 65, 120, 120), ux3d::editor::CameraOrientation::Back }
                };

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
                    projAxes.push_back({ v, &a, pt });
                }
                std::sort(projAxes.begin(), projAxes.end(), [](const ProjectedAxis& a, const ProjectedAxis& b) {
                    return a.v.z < b.v.z;
                });

                ImVec2 mousePos = io.MousePos;
                bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
                float distCenterSq = (mousePos.x - center.x) * (mousePos.x - center.x) + (mousePos.y - center.y) * (mousePos.y - center.y);
                if (distCenterSq <= (radius + 6.0f) * (radius + 6.0f)) {
                    clickedCompass = true;
                }

                // Middle part / center hub -> click turns into isometric mode!
                float hubRadius = 7.0f;
                bool hoveredHub = (distCenterSq <= (hubRadius + 2.0f) * (hubRadius + 2.0f));

                drawList->AddCircleFilled(center, hubRadius, hoveredHub ? IM_COL32(255, 255, 255, 255) : IM_COL32(160, 165, 185, 220));
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

                    float distSq = (mousePos.x - pa.pt.x) * (mousePos.x - pa.pt.x) + (mousePos.y - pa.pt.y) * (mousePos.y - pa.pt.y);
                    bool hovered = (distSq <= (dotRadius + 3.0f) * (dotRadius + 3.0f));

                    ImU32 col = hovered ? IM_COL32(255, 255, 255, 255) : (pa.v.z >= 0.0f ? pa.def->color : pa.def->dimColor);
                    drawList->AddCircleFilled(pa.pt, dotRadius, col);

                    if (isPositive) {
                        ImVec2 textSize = ImGui::CalcTextSize(pa.def->name);
                        drawList->AddText(ImVec2(pa.pt.x - textSize.x * 0.5f, pa.pt.y - textSize.y * 0.5f), IM_COL32(20, 20, 25, 255), pa.def->name);
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
                            vp->cameraActor.panCamera(-static_cast<int32_t>(io.MouseDelta.x), -static_cast<int32_t>(io.MouseDelta.y));
                        } else if (io.KeyCtrl) {
                            vp->cameraActor.zoomCamera(-io.MouseDelta.y * 10.0f);
                        } else {
                            vp->cameraActor.rotateCamera(static_cast<int32_t>(io.MouseDelta.x), static_cast<int32_t>(io.MouseDelta.y));
                        }
                    }
                }

                // Left-click 3D mesh picking
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 4.0f)) {
                    float localX = io.MousePos.x - imagePos.x;
                    float localY = io.MousePos.y - imagePos.y;
                    int px = static_cast<int>(localX);
                    int py = vpH - static_cast<int>(localY) - 1;

                    if (px < 0) px = 0;
                    if (px >= vpW) px = vpW - 1;
                    if (py < 0) py = 0;
                    if (py >= vpH) py = vpH - 1;

                    if (vp->pickingFbo.isValid()) {
                        uint8_t pixel[4] = { 0, 0, 0, 0 };
                        glPixelStorei(GL_PACK_ALIGNMENT, 1);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, vp->pickingFbo.getFbo());
                        glReadBuffer(GL_COLOR_ATTACHMENT0);
                        glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

                        uint32_t pickingId = (static_cast<uint32_t>(pixel[0]) << 16) |
                                             (static_cast<uint32_t>(pixel[1]) << 8)  |
                                              static_cast<uint32_t>(pixel[2]);

                        if (pickingId != 0) {
                            auto* pickedMesh = pickingLayer.getMesh(pickingId);
                            if (pickedMesh != nullptr) {
                                auto it = currentReverseMapping.find(pickedMesh);
                                if (it != currentReverseMapping.end() && it->second != nullptr) {
                                    auto* pickedNode = const_cast<ux3d::gltf::Node*>(it->second);
                                    selection.selectNode(pickedNode);
                                    ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
                                    isSceneDirty = true;
                                    logMessage("Picked: " + (pickedNode->hasName() ? pickedNode->getName() : ("Node " + std::to_string(pickedNode->m_ID))));
                                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                                        focusCameraOnNode(pickedNode, gltfModel, gltfLayer.get(), vp->cameraActor, vp->antiAliasing, isSceneDirty);
                                    }
                                } else {
                                    selection = ux3d::imgui::SelectionState();
                                    isSceneDirty = true;
                                }
                            } else {
                                selection = ux3d::imgui::SelectionState();
                                isSceneDirty = true;
                            }
                        } else {
                            selection = ux3d::imgui::SelectionState();
                            isSceneDirty = true;
                        }
                    }
                }
            }

            // 3D Coordinate Gizmo
            if (vp->settings.showNodeCoordinateGizmo && selection.type == ux3d::imgui::SelectionType::Node && selection.selectedNode != nullptr && vp->sceneFbo.isValid()) {
                auto* node = selection.selectedNode;
                glm::mat4 nodeWorld = gltfLayer ? glm::mat4(gltfLayer->getWorldTransform(node)) : glm::mat4(node->getTransformMatrix());
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawNodeCoordinateGizmo(drawList, nodeWorld, viewProj, imagePos, imageSize, ux3d::math::vec3(cameraEye.x, cameraEye.y, cameraEye.z), vp->settings.gizmoAxisConvention);
            }

            ImGui::End(); // End Viewport Window
        }

        // -------------------------------------------------------------
        // Hierarchy Panel
        // -------------------------------------------------------------
        const auto panelsStart = ux3d::imgui::profiling::Clock::now();
        if (showHierarchy) {
            ux3d::editor::CameraActor dummyActor;
            ux3d::imgui::AntiAliasing dummyAA;
            ux3d::imgui::SceneHierarchyPanel::render(
                gltfModel, activeScene, selection, gltfLayer.get(),
                activeViewport ? activeViewport->cameraActor : dummyActor,
                activeViewport ? activeViewport->antiAliasing : dummyAA,
                isSceneDirty, &showHierarchy, &viewports, &activeViewport
            );
        }

        // -------------------------------------------------------------
        // Resources Panel
        // -------------------------------------------------------------
        if (showResources) {
            ux3d::imgui::ResourcesPanel::render(gltfModel, selection, gltfLayer.get(), &isSceneDirty, &showResources);
        }

        // -------------------------------------------------------------
        // Inspector Panel
        // -------------------------------------------------------------
        if (showInspector) {
            if (ImGui::Begin(WND_INSPECTOR, &showInspector)) {
                ux3d::imgui::ElementInspector::render(selection, gltfModel, isSceneDirty, &animationPlayer, gltfLayer.get(), &viewports, commandGenerator.get());
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Statistics Panel
        // -------------------------------------------------------------
        if (showStatistics) {
            if (ImGui::Begin(WND_STATISTICS, &showStatistics)) {
                ux3d::imgui::StatisticsPanel::render(gltfModel, currentFilePath);
            }
            ImGui::End();
        }

        // -------------------------------------------------------------
        // Validation Panel
        // -------------------------------------------------------------
        if (showValidation) {
            ux3d::editor::CameraActor dummyActor;
            ux3d::imgui::ValidationPanel::render(&showValidation, currentFilePath, gltfModel, selection, activeViewport ? activeViewport->cameraActor : dummyActor, gltfLayer.get());
        }

        // -------------------------------------------------------------
        // Search Paths Window
        // -------------------------------------------------------------
        if (showSearchPaths) {
            ux3d::imgui::renderSearchPathsWindow(&showSearchPaths, [&]() {
                if (!currentFilePath.empty()) {
                    loadGltfModel(currentFilePath, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                }
            });
        }

        // -------------------------------------------------------------
        // Logger Panel
        // -------------------------------------------------------------
        if (showLogger) {
            if (ImGui::Begin(WND_LOGGER, &showLogger)) {
                static bool s_loggerWrap = loggerWrapArg;
                static bool s_loggerAutoScroll = loggerAutoScrollArg;

                const bool loggerClearClicked = ImGui::Button(ICON_MDI_DELETE_OUTLINE " Clear");
                ux3d::imgui::UiProbe::item("log.clear", 0);
                if (loggerClearClicked) {
                    s_consoleLogs.clear();
                }
                ImGui::SameLine();
                const bool loggerCopyClicked = ImGui::Button(ICON_MDI_CONTENT_COPY " Copy");
                ux3d::imgui::UiProbe::item("log.copy", 0);
                if (loggerCopyClicked) {
                    const std::string allLogs = buildConsoleLogCopyText(s_consoleLogs.entries());
                    ImGui::SetClipboardText(allLogs.c_str());
                }
                ImGui::SameLine();
                ImGui::Checkbox(ICON_MDI_WRAP " Wrap", &s_loggerWrap);
                ux3d::imgui::UiProbe::item("log.wrap", 0);
                ImGui::SameLine();
                ImGui::Checkbox("Auto-Scroll", &s_loggerAutoScroll);
                ux3d::imgui::UiProbe::item("log.autoscroll", 0);
                ImGui::SameLine();
                ImGui::PushFont(ux3d::imgui::getMonoFont());
                ImGui::TextDisabled("| Messages: %zu", s_consoleLogs.size());
                ImGui::PopFont();
                ImGui::Separator();

                ImGuiWindowFlags childFlags = 0;
                if (!s_loggerWrap) {
                    childFlags |= ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar;
                }

                const auto& entries = s_consoleLogs.entries();
                ImFont* monoFont = ux3d::imgui::getMonoFont();
                ImGui::PushFont(monoFont);
                const float fontSize = ImGui::GetFontSize();
                const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
                if (!s_loggerWrap) {
                    const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
                    ensureConsoleLogLayout(s_consoleLogLayout,
                                           s_consoleLogs,
                                           monoFont,
                                           fontSize,
                                           false,
                                           availableWidth,
                                           itemSpacingY);
                    ImGui::SetNextWindowContentSize(ImVec2(s_consoleLogLayout.maxTextWidth, 0.0f));
                }

                ImGui::BeginChild("LoggerScrollArea", ImVec2(0, 0), false, childFlags);
                if (s_loggerWrap) {
                    const float wrapWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
                    ensureConsoleLogLayout(s_consoleLogLayout,
                                           s_consoleLogs,
                                           monoFont,
                                           fontSize,
                                           true,
                                           wrapWidth,
                                           itemSpacingY);
                }

                if (!s_loggerWrap && s_consoleLogLayout.allSingleLine) {
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(entries.size()));
                    while (clipper.Step()) {
                        for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                            const auto& entry = entries[static_cast<size_t>(rowIndex)];
                            ImGui::TextColored(entry.color, "%s", entry.text.c_str());
                            ux3d::imgui::UiProbe::item("log.row", static_cast<long long>(&entry - entries.data()));
                        }
                    }
                } else {
                    // The clipper assumes uniform heights; draw only the intersecting cached rows here.
                    const ImVec2 contentPosition = ImGui::GetCursorScreenPos();
                    const ImVec2 available = ImGui::GetContentRegionAvail();
                    const auto visibleRows = visibleConsoleLogRows(
                        s_consoleLogLayout,
                        ImGui::GetScrollY(),
                        available.y);
                    const float contentWidth = s_loggerWrap ? 0.0f : s_consoleLogLayout.maxTextWidth;
                    ImGui::Dummy(ImVec2(contentWidth, s_consoleLogLayout.contentHeight));

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const float wrapWidth = s_loggerWrap
                        ? std::max(1.0f, available.x)
                        : 0.0f;
                    for (size_t rowIndex = visibleRows.first; rowIndex < visibleRows.second; ++rowIndex) {
                        const auto& entry = entries[rowIndex];
                        drawList->AddText(
                            monoFont,
                            fontSize,
                            ImVec2(contentPosition.x, contentPosition.y + s_consoleLogLayout.rowOffsets[rowIndex]),
                            ImGui::ColorConvertFloat4ToU32(entry.color),
                            entry.text.c_str(),
                            nullptr,
                            wrapWidth);
                        ux3d::imgui::UiProbe::rect(
                            "log.row",
                            static_cast<long long>(rowIndex),
                            ImVec2(contentPosition.x, contentPosition.y + s_consoleLogLayout.rowOffsets[rowIndex]),
                            ImVec2(contentPosition.x + std::max(1.0f, available.x),
                                   contentPosition.y + s_consoleLogLayout.rowEnds[rowIndex]));
                    }
                }
                if (s_loggerAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();
                ImGui::PopFont();
            }
            ImGui::End();
        }

        // Demo Window (for debug/testing)
        if (showDemoWindow) {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        // -------------------------------------------------------------
        // Application Status Bar (Bottom)
        // -------------------------------------------------------------
        markStage("ui.panels", panelsStart);
        const auto statusBarStart = ux3d::imgui::profiling::Clock::now();
        ux3d::imgui::StatusBar::render(gltfModel, gltfLayer.get(), animationPlayer, isSceneDirty, showLogger, showValidation, s_consoleLogs.counts(), activeViewport ? &activeViewport->antiAliasing : nullptr);
        markStage("ui.statusBar", statusBarStart);

        // Rendering main window
        const auto uiRenderStart = ux3d::imgui::profiling::Clock::now();
        ImGui::Render();
        markStage("ui.render", uiRenderStart);
        int displayW = 0, displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.11f, 0.11f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const auto uiSubmitStart = ux3d::imgui::profiling::Clock::now();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        markStage("ui.submit", uiSubmitStart);
        if (const ImDrawData* drawData = ImGui::GetDrawData(); drawData != nullptr) {
            ux3d::imgui::profiling::countUiDrawData(
                static_cast<uint64_t>(drawData->CmdListsCount),
                static_cast<uint64_t>(drawData->TotalVtxCount),
                static_cast<uint64_t>(drawData->TotalIdxCount));
        }

        frameSample.cpuFrameMs = ux3d::imgui::profiling::elapsedMs(frameStart);
        frameSample.firstFrame = (frameNumber == 1);
        if (s_profilingEnabled) {
            frameSample.counters = ux3d::imgui::profiling::frameCounters();
        }
        if (frameSample.firstFrame && (s_profilingEnabled || s_benchmarkEnabled)) {
            s_firstFrameSample = frameSample;
            s_haveFirstFrameSample = true;
        }
        if (s_profilingEnabled) {
            s_frameSamples.push_back(frameSample);
            if (s_frameSamples.size() > ux3d::imgui::profiling::kMaxRetainedFrameSamples) {
                s_frameSamples.pop_front();
                ++s_frameSamplesDropped;
            }
        }
        if (s_benchmarkEnabled &&
            frameNumber > s_benchmarkWarmupFrames &&
            s_benchmarkSamples.size() < s_benchmarkMeasuredFrames) {
            s_benchmarkSamples.push_back(frameSample);
        }

        ++frameCount;
        const bool profileCaptureReady = s_captureViewportPath.empty() || s_captureAttempted;
        const bool profileReportReady = !s_profileReportPath.empty() &&
            !s_profileReportAttempted &&
            frameCount >= SCENARIO_REPORT_FRAME &&
            activeViewport != nullptr &&
            profileCaptureReady;

        if (!profileReportReady &&
            !s_profileReportPath.empty() &&
            !s_profileReportAttempted &&
            !s_profileReportDeferredLogged &&
            frameCount >= SCENARIO_REPORT_FRAME &&
            !s_captureViewportPath.empty() &&
            !s_captureAttempted) {
            std::printf("Deferring profile report until the requested viewport capture completes\n");
            s_profileReportDeferredLogged = true;
        }

        if (profileReportReady) {
            s_profileReportAttempted = true;
            ux3d::imgui::profiling::ProfileReport report;
            report.scenarioName = scenarioName;
            report.modelPath = currentFilePath.empty() ? modelToLoad : currentFilePath;
            report.build = std::string(__DATE__) + " " + __TIME__;
            const auto* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
            const auto* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
            const auto* glRendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
            report.glVersion = glVersion != nullptr ? glVersion : "";
            report.gpuVendor = glVendor != nullptr ? glVendor : "";
            report.gpuRenderer = glRendererName != nullptr ? glRendererName : "";
            report.isolated = s_isolatedState;
            report.instrumentationEnabled = s_profilingEnabled;
            report.vsyncRequested = s_vsyncEnabled;
            glfwGetWindowSize(window, &report.windowWidth, &report.windowHeight);
            report.viewportWidth = activeViewport->sceneFbo.getWidth();
            report.viewportHeight = activeViewport->sceneFbo.getHeight();
            report.viewportSamples = activeViewport->sceneFbo.getSamples();
            report.antiAliasing = antiAliasingModeName(activeViewport->settings.antiAliasingIdx);
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
            if (ux3d::imgui::profiling::writeProfileReport(s_profileReportPath, report)) {
                std::printf("Wrote profile report to %s (finalized at frame %llu)\n",
                            s_profileReportPath.c_str(),
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
            if (writeScenarioReport(scenarioReportPath, scenarioName, frameCount, window,
                                    reportModelPath, modelLoaded, gltfModel, activeScene,
                                    *activeViewport, scenarioEnvironmentPath, scenarioEnvironmentLoaded,
                                    animationPlayer, selection)) {
                std::printf("Wrote scenario report to %s\n", scenarioReportPath.c_str());
            } else {
                s_outputFailed = true;
                std::fprintf(stderr, "Failed to write scenario report to %s\n", scenarioReportPath.c_str());
            }
        }

        if (s_benchmarkEnabled &&
            !s_benchmarkReportAttempted &&
            s_benchmarkSamples.size() >= s_benchmarkMeasuredFrames) {
            ux3d::imgui::profiling::BenchmarkReport report;
            report.scenarioName = scenarioName;
            report.modelPath = currentFilePath.empty() ? modelToLoad : currentFilePath;
            report.build = std::string(__DATE__) + " " + __TIME__;
            const auto* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
            const auto* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
            const auto* glRendererName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
            report.glVersion = glVersion != nullptr ? glVersion : "";
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
                report.antiAliasing = antiAliasingModeName(activeViewport->settings.antiAliasingIdx);
            }
            report.warmupFrames = s_benchmarkWarmupFrames;
            report.measuredFramesRequested = s_benchmarkMeasuredFrames;
            report.hasFirstFrame = s_haveFirstFrameSample;
            report.firstFrame = s_firstFrameSample;
            report.measuredFrames = s_benchmarkSamples;
            fillGpuReport(report.gpu, glRenderer.get());

            s_benchmarkReportAttempted = true;
            s_benchmarkReportWritten =
                ux3d::imgui::profiling::writeBenchmarkReport(s_benchmarkOutputPath, report);
            if (s_benchmarkReportWritten) {
                std::printf("Wrote benchmark report to %s (%zu measured frames, warmup %llu, vsync %s)\n",
                            s_benchmarkOutputPath.c_str(),
                            s_benchmarkSamples.size(),
                            static_cast<unsigned long long>(s_benchmarkWarmupFrames),
                            s_vsyncEnabled ? "on" : "off");
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
                if (exitAfterDump) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
            }
        }

        if (uiScenario.active()) {
            ux3d::imgui::UiScenarioFrame uiFrame;
            uiFrame.selection = describeUiSelection(selection);
            if (activeViewport != nullptr) {
                const auto target = activeViewport->cameraActor.getLookTarget();
                char cameraText[160];
                std::snprintf(cameraText, sizeof(cameraText), "target=%.3f,%.3f,%.3f zoom=%.3f",
                              target.x, target.y, target.z, activeViewport->cameraActor.getZoom());
                uiFrame.camera = cameraText;
            }
            uiFrame.logEntries = s_consoleLogs.size();
            uiFrame.logErrors = s_consoleLogs.counts().errors;
            uiFrame.logWarnings = s_consoleLogs.counts().warnings;
            uiFrame.logInfo = s_consoleLogs.counts().info;
            uiFrame.hierarchyBuildCount = ux3d::imgui::SceneHierarchyPanel::rowModelBuildCount();
            uiFrame.expectedCopyText = [&]() {
                std::string expected;
                for (const auto& entry : s_consoleLogs.entries()) {
                    expected += entry.text;
                    expected += "\n";
                }
                return expected;
            };
            uiFrame.appendLog = [&](const std::string& text, int severity) {
                const auto level = severity == 2 ? ux3d::core::LogLevel::Error
                    : severity == 1 ? ux3d::core::LogLevel::Warning
                    : ux3d::core::LogLevel::Info;
                const ImVec4 color = severity == 2 ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                    : severity == 1 ? ImVec4(1.0f, 0.85f, 0.3f, 1.0f)
                    : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
                s_consoleLogs.append({ text, color, level });
            };
            uiFrame.saveFrame = [&](const std::string& path) {
                std::vector<uint8_t> pixels(static_cast<size_t>(displayW) * displayH * 4);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadBuffer(GL_BACK);
                glReadPixels(0, 0, displayW, displayH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                return saveImage(path.c_str(), displayW, displayH, pixels.data());
            };
            uiFrame.loadModel = [&](const std::string& path) {
                const bool loaded = loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(), animationPlayer, isSceneDirty);
                updateWindowTitle(window, currentFilePath);
                return loaded;
            };
            uiFrame.pickNode = [&](int id) {
                for (const auto& node : gltfModel.getNodes()) {
                    if (static_cast<int>(node.m_ID) == id) {
                        auto* pickedNode = const_cast<ux3d::gltf::Node*>(&node);
                        selection.selectNode(pickedNode);
                        ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
                        isSceneDirty = true;
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
                std::string m = toLowerAscii(modeStr);
                if (m == "none" || m == "0") targetVp->settings.antiAliasingIdx = 0;
                else if (m == "msaa2" || m == "1") targetVp->settings.antiAliasingIdx = 1;
                else if (m == "msaa4" || m == "2") targetVp->settings.antiAliasingIdx = 2;
                else if (m == "msaa8" || m == "3") targetVp->settings.antiAliasingIdx = 3;
                else if (m == "fxaa" || m == "4") targetVp->settings.antiAliasingIdx = 4;
                else if (m == "taa" || m == "5") targetVp->settings.antiAliasingIdx = 5;
                else return false;
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
                    newVp->settings = activeViewport->settings;
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
            uiFrame.focusViewport = [&](uint32_t id) {
                for (const auto& vp : viewports) {
                    if (vp && vp->id == id && vp->isOpen) { vp->requestFocus = true; return true; }
                }
                return false;
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
                if (k == "wireframe" || k == "wireframeoverlay") targetVp->settings.wireframeOverlay = val;
                else if (k == "grid" || k == "showgrid") targetVp->settings.showGrid = val;
                else if (k == "bbox" || k == "boundingbox" || k == "showsceneboundingbox") targetVp->settings.showSceneBoundingBox = val;
                else if (k == "doublesided" || k == "forcedoublesided") targetVp->settings.forceDoubleSided = val;
                else if (k == "invertwinding") targetVp->settings.invertWinding = val;
                else return false;
                return true;
            };
            uiScenario.endFrame(window, uiFrame);
            if (uiScenario.failed()) {
                s_outputFailed = true;
            }
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ux3d::imgui::ElementInspector::clearPreviewCache();
    gridRenderer.cleanup();
    bboxRenderer.cleanup();
    viewports.clear();
    if (glRenderer != nullptr) {
        glRenderer->uninit();
        glRenderer.reset();
    }
    if (commandGenerator != nullptr) {
        commandGenerator->clearVertexLayoutCache();
        commandGenerator.reset();
    }
    if (gltfLayer != nullptr) {
        gltfLayer.reset();
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

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    return main(__argc, __argv);
}
#endif
