#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "imgui/UiProbe.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace ux3d::imgui {

struct UiScenarioFrame {
    struct ViewportInfo {
        uint32_t id = 0;
        std::string name;
        bool open = false;
        int renderW = 0;
        int renderH = 0;
        int aaMode = 0;
        int aaW = 0;
        int aaH = 0;
        size_t aaTargets = 0;
        size_t aaFbos = 0;
        uint64_t aaBytes = 0;
        bool aaHistoryValid = false;
        uint64_t aaAllocs = 0;
        uint64_t aaDeletes = 0;
        uint64_t targetGeneration = 0;
        bool lastTaaHistoryValid = false;
        float lastTaaBlendFactor = 0.0f;
        uint64_t lastTaaTargetGeneration = 0;
        std::vector<std::string> activePasses;
        uint64_t renderCount = 0;
        bool wireframe = false;
        bool showGrid = false;
        bool showSceneBoundingBox = false;
        bool forceDoubleSided = false;
        bool invertWinding = false;
    };
    std::string selection;
    std::string camera;
    size_t logEntries = 0;
    int logErrors = 0;
    int logWarnings = 0;
    int logInfo = 0;
    uint64_t hierarchyBuildCount = 0;
    // P4.1 revision contract: pre-rendered JSON object with per-kind revisions,
    // full-sync count and the kinds consumed at the most recent sync point.
    std::string revisionsJson = "{}";
    // A2.1: the shared decal buffer's live state, counted once for all viewports.
    bool decalBufferActive = false;
    int decalBufferWidth = 0;
    int decalBufferHeight = 0;
    uint64_t decalBufferBytes = 0;
    uint64_t decalBufferAllocations = 0;
    uint64_t decalBufferResizes = 0;
    uint64_t decalBufferReleases = 0;
    uint64_t glErrors = 0;
    std::vector<ViewportInfo> viewports;
    std::function<std::string()> expectedCopyText;
    std::function<void(const std::string&, int)> appendLog;
    std::function<bool(const std::string&)> saveFrame;
    std::function<bool(const std::string&)> loadModel;
    std::function<bool(const std::string&)> loadModelWithoutTextures;
    std::function<bool(int)> pickNode;
    std::function<bool()> createViewport;
    std::function<bool(const std::string&, uint32_t)> setAaMode;
    std::function<bool(uint32_t)> toggleViewport;
    std::function<bool(uint32_t)> focusViewport;
    std::function<bool(uint32_t, const std::string&, bool)> setViewportSetting;
    std::function<bool(bool)> setAnimationPlaying;
    std::function<bool(uint32_t)> splitViewport;
    std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&)> setAsoboParam;
    // Live commands (frame, select, capture, ... and every newer registry command) run through
    // the shared command registry (docs/AGENT_CONTROL_PLAN.md). Returns false when cmd is not a
    // registry command; done is called now, or in a later frame for multi-frame commands.
    std::function<bool(const std::string& cmd, const std::vector<std::string>& tokens, const std::string& line,
                       std::function<void(bool ok, const std::string& error, const nlohmann::json& result)> done)>
        runCommand;
};

// Drives real ImGui frames from a text script (--ui-script) by queueing mouse,
// wheel, key and text events into ImGuiIO and recording what the frame did
// (scroll state, probed item rectangles, selection, clipboard, pixel hashes).
// The same script and the same probe lines run against the accepted baseline
// build, so the two reports can be compared.
class UiScenario {
public:
    bool active() const { return m_active; }
    bool failed() const { return m_failed; }
    bool isFollowMode() const { return m_followMode; }

    bool configure(const std::string& scriptPath, const std::string& reportPath);

    bool configureFollow(const std::string& scriptPath, const std::string& reportPath);

    // After the platform backend's NewFrame and before ImGui::NewFrame.
    void beginFrame(ImGuiIO& io);

    // After ImGui::Render and before the buffer swap.
    void endFrame(GLFWwindow* window, UiScenarioFrame& frame);

private:
    static constexpr uint64_t kMaxFrames = 60000;
    static constexpr int kSettleFrames = 4;

    struct ScrollTask {
        bool active = false;
        std::string window;
        std::string target;
        int iterations = 0;
        int stable = 0;
        float lastScroll = 0.0f;
        float lastWheel = 0.0f;
        float stepEstimate = 80.0f;
    };

    bool m_active = false;
    bool m_failed = false;
    bool m_finished = false;
    uint64_t m_frame = 0;
    int m_waitFrames = 0;
    std::chrono::steady_clock::time_point m_waitUntil{};
    size_t m_next = 0;
    int m_appendSerial = 0;
    bool m_followMode = false;
    std::string m_followPath;
    std::string m_outJsonlPath;
    std::streampos m_followFileOffset = 0;
    uint64_t m_fileLinesRead = 0;
    std::vector<uint64_t> m_commandLines;

    // A registry command (capture, open, ...) is still running; later lines wait for it.
    bool m_commandPending = false;
    bool m_dispatching = false;

    void writeResult(uint64_t lineNum, const std::string& cmd, bool ok, const std::string& error, const nlohmann::json& result);

    void reportError(uint64_t lineNum, const std::string& cmd, const std::string& msg);

    std::string m_reportPath;
    std::vector<std::string> m_commands;
    std::vector<std::string> m_errors;
    std::vector<std::string> m_snaps;
    std::deque<std::function<void(ImGuiIO&)>> m_queue;
    ScrollTask m_scroll;
    bool m_hasMouse = false;
    ImVec2 m_mousePos;
    uint32_t m_catchReallocVpId = 0;
    uint64_t m_catchReallocBaseGen = 0;
    std::string m_catchReallocShot;
    std::string m_catchReallocSnap;

    static std::string escape(const std::string& text);

    static std::string num(double value);

    static uint64_t fnv(const void* data, size_t size, uint64_t hash = 1469598103934665603ull);

    static std::string hex(uint64_t value);

    void fail(const std::string& message);

    void queueFrame(std::function<void(ImGuiIO&)> op);

    void queueMove(ImVec2 position);

    void queueClicks(int count, int button = 0);

    static std::vector<std::string> tokens(const std::string& line);

    // Reload takes one path, which may contain spaces (e.g. PMDG aircraft folders).
    static std::string pathArgument(const std::string& line);

    static ImGuiWindow* findWindow(const std::string& substring);

    static const UiProbeEntry* findProbe(const std::string& tag, long long id);

    static ImVec2 center(const UiProbeEntry& entry)
    {
        return ImVec2((entry.min.x + entry.max.x) * 0.5f, (entry.min.y + entry.max.y) * 0.5f);
    }

    // Clamp so the click lands inside the part of the row that is on screen.
    static ImVec2 clickPoint(const UiProbeEntry& entry, ImGuiWindow* clipWindow);

    static ImGuiKey keyFromName(const std::string& name);

    std::string appendText(const std::string& kind, int serial, const std::string& tag) const;

    void execute(const std::string& line, uint64_t cmdLine, GLFWwindow* window, UiScenarioFrame& frame);

    void executeCommand(const std::string& cmd, const std::vector<std::string>& args, const std::string& line, uint64_t cmdLine, GLFWwindow* window, UiScenarioFrame& frame);

    void stepScroll();

    std::string regionHash(GLFWwindow* window, const ImRect& rect) const;

    void snap(const std::vector<std::string>& args, GLFWwindow* window, UiScenarioFrame& frame);

    void finish(GLFWwindow* window);
};

} // namespace ux3d::imgui
