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

namespace ux3d::imgui {

struct UiScenarioFrame {
    std::string selection;
    std::string camera;
    size_t logEntries = 0;
    int logErrors = 0;
    int logWarnings = 0;
    int logInfo = 0;
    uint64_t hierarchyBuildCount = 0;
    std::function<std::string()> expectedCopyText;
    std::function<void(const std::string&, int)> appendLog;
    std::function<bool(const std::string&)> saveFrame;
    std::function<bool(const std::string&)> loadModel;
    std::function<bool(int)> pickNode;
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

    bool configure(const std::string& scriptPath, const std::string& reportPath)
    {
        m_reportPath = reportPath;
        std::ifstream input(scriptPath);
        if (!input.is_open()) {
            std::fprintf(stderr, "UiScenario: cannot open script %s\n", scriptPath.c_str());
            m_failed = true;
            return false;
        }
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            const size_t first = line.find_first_not_of(" \t");
            if (first == std::string::npos || line[first] == '#') {
                continue;
            }
            m_commands.push_back(line.substr(first));
        }
        m_active = true;
        UiProbe::enabled() = true;
        return true;
    }

    // After the platform backend's NewFrame and before ImGui::NewFrame.
    void beginFrame(ImGuiIO& io)
    {
        UiProbe::entries().clear();
        // The script must work even when the OS did not give the window focus.
        io.AddFocusEvent(true);
        if (m_hasMouse) {
            io.AddMousePosEvent(m_mousePos.x, m_mousePos.y);
        }
        if (!m_queue.empty()) {
            auto op = std::move(m_queue.front());
            m_queue.pop_front();
            op(io);
        }
        // The platform backend re-reads the real cursor every frame, so the scripted
        // position has to be re-asserted after it.
        if (m_hasMouse) {
            io.AddMousePosEvent(m_mousePos.x, m_mousePos.y);
        }
    }

    // After ImGui::Render and before the buffer swap.
    void endFrame(GLFWwindow* window, UiScenarioFrame& frame)
    {
        ++m_frame;
        if (m_frame > kMaxFrames) {
            fail("script did not finish within the frame limit");
            finish(window);
            return;
        }
        if (m_scroll.active) {
            stepScroll();
            return;
        }
        if (m_waitFrames > 0) {
            --m_waitFrames;
            return;
        }
        if (!m_queue.empty()) {
            return;
        }
        if (m_waitUntil > std::chrono::steady_clock::now()) {
            return;
        }

        while (m_next < m_commands.size() && !m_failed) {
            const std::string command = m_commands[m_next++];
            execute(command, window, frame);
            if (m_scroll.active || m_waitFrames > 0 || !m_queue.empty()
                || m_waitUntil > std::chrono::steady_clock::now()) {
                return;
            }
        }
        finish(window);
    }

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
    std::string m_reportPath;
    std::vector<std::string> m_commands;
    std::vector<std::string> m_errors;
    std::vector<std::string> m_snaps;
    std::deque<std::function<void(ImGuiIO&)>> m_queue;
    ScrollTask m_scroll;
    bool m_hasMouse = false;
    ImVec2 m_mousePos;

    static std::string escape(const std::string& text)
    {
        std::string out;
        for (unsigned char c : text) {
            switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out += static_cast<char>(c);
                }
            }
        }
        return out;
    }

    static std::string num(double value)
    {
        char buffer[48];
        std::snprintf(buffer, sizeof(buffer), "%.3f", value);
        return buffer;
    }

    static uint64_t fnv(const void* data, size_t size, uint64_t hash = 1469598103934665603ull)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 1099511628211ull;
        }
        return hash;
    }

    static std::string hex(uint64_t value)
    {
        char buffer[24];
        std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
        return buffer;
    }

    void fail(const std::string& message)
    {
        m_failed = true;
        m_errors.push_back(message);
        std::fprintf(stderr, "UiScenario: %s\n", message.c_str());
    }

    void queueFrame(std::function<void(ImGuiIO&)> op)
    {
        m_queue.push_back(std::move(op));
        m_waitFrames = kSettleFrames;
    }

    void queueMove(ImVec2 position)
    {
        queueFrame([this, position](ImGuiIO&) {
            m_mousePos = position;
            m_hasMouse = true;
        });
    }

    void queueClicks(int count)
    {
        for (int i = 0; i < count; ++i) {
            queueFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
            queueFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
        }
    }

    static std::vector<std::string> tokens(const std::string& line)
    {
        std::istringstream stream(line);
        std::vector<std::string> out;
        std::string token;
        while (stream >> token) {
            out.push_back(token);
        }
        return out;
    }

    static ImGuiWindow* findWindow(const std::string& substring)
    {
        ImGuiContext* context = ImGui::GetCurrentContext();
        ImGuiWindow* best = nullptr;
        for (ImGuiWindow* window : context->Windows) {
            if (!window->Active || std::strstr(window->Name, substring.c_str()) == nullptr) {
                continue;
            }
            if (best == nullptr || std::strlen(window->Name) > std::strlen(best->Name)) {
                best = window;
            }
        }
        return best;
    }

    static const UiProbeEntry* findProbe(const std::string& tag, long long id)
    {
        for (const UiProbeEntry& entry : UiProbe::entries()) {
            if (!entry.isMark && entry.id == id && entry.tag == tag) {
                return &entry;
            }
        }
        return nullptr;
    }

    static ImVec2 center(const UiProbeEntry& entry)
    {
        return ImVec2((entry.min.x + entry.max.x) * 0.5f, (entry.min.y + entry.max.y) * 0.5f);
    }

    // Clamp so the click lands inside the part of the row that is on screen.
    static ImVec2 clickPoint(const UiProbeEntry& entry, ImGuiWindow* clipWindow)
    {
        ImVec2 point = center(entry);
        if (clipWindow != nullptr) {
            const float top = std::max(entry.min.y, clipWindow->InnerRect.Min.y);
            const float bottom = std::min(entry.max.y, clipWindow->InnerRect.Max.y);
            point.y = (top + bottom) * 0.5f;
        }
        return point;
    }

    static ImGuiKey keyFromName(const std::string& name)
    {
        if (name == "down") return ImGuiKey_DownArrow;
        if (name == "up") return ImGuiKey_UpArrow;
        if (name == "left") return ImGuiKey_LeftArrow;
        if (name == "right") return ImGuiKey_RightArrow;
        if (name == "pgdn") return ImGuiKey_PageDown;
        if (name == "pgup") return ImGuiKey_PageUp;
        if (name == "home") return ImGuiKey_Home;
        if (name == "end") return ImGuiKey_End;
        if (name == "enter") return ImGuiKey_Enter;
        if (name == "esc") return ImGuiKey_Escape;
        if (name == "backspace") return ImGuiKey_Backspace;
        if (name == "tab") return ImGuiKey_Tab;
        if (name == "space") return ImGuiKey_Space;
        if (name == "a") return ImGuiKey_A;
        return ImGuiKey_None;
    }

    std::string appendText(const std::string& kind, int serial, const std::string& tag) const
    {
        char head[96];
        if (kind == "single") {
            std::snprintf(head, sizeof(head), "[%s] single-line entry %06d", tag.c_str(), serial);
            return head;
        }
        if (kind == "newline") {
            std::snprintf(head, sizeof(head), "[%s] multi-line entry %06d", tag.c_str(), serial);
            return std::string(head) + "\n    detail A " + std::to_string(serial) + "\n    detail B";
        }
        std::snprintf(head, sizeof(head), "[%s] wrapped entry %06d ", tag.c_str(), serial);
        std::string text = head;
        for (int i = 0; i < 14; ++i) {
            text += "lorem ipsum dolor sit amet ";
        }
        return text;
    }

    void execute(const std::string& line, GLFWwindow* window, UiScenarioFrame& frame)
    {
        std::vector<std::string> args = tokens(line);
        if (args.empty()) {
            return;
        }
        const std::string& cmd = args[0];
        if (args.size() > 1 && (cmd == "hoverp" || cmd == "clickp" || cmd == "dclickp" || cmd == "clickarrowp" || cmd == "clickrow" || cmd == "dclickrow" || cmd == "hoverrow" || cmd == "clickarrowrow")) {
            std::replace(args[1].begin(), args[1].end(), '_', ' '); // tags such as "Material Variants" are written with underscores
        }
        try {
            executeCommand(cmd, args, line, window, frame);
        } catch (const std::exception&) {
            fail("malformed command: " + line);
        }
    }

    void executeCommand(const std::string& cmd, const std::vector<std::string>& args, const std::string& line, GLFWwindow* window, UiScenarioFrame& frame)
    {
        auto need = [&](size_t count) {
            if (args.size() < count) {
                fail("missing arguments: " + line);
                return false;
            }
            return true;
        };

        if (cmd == "wait") {
            if (need(2)) m_waitFrames = std::stoi(args[1]);
        } else if (cmd == "waitms") {
            if (need(2)) m_waitUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::stoi(args[1]));
        } else if (cmd == "move") {
            if (need(3)) queueMove(ImVec2(std::stof(args[1]), std::stof(args[2])));
        } else if (cmd == "hover") {
            if (!need(2)) return;
            ImGuiWindow* target = findWindow(args[1]);
            if (target == nullptr) { fail("window not found: " + args[1]); return; }
            queueMove(target->InnerRect.GetCenter());
        } else if (cmd == "hoverp" || cmd == "clickp" || cmd == "dclickp") {
            if (!need(3)) return;
            const UiProbeEntry* entry = findProbe(args[1], std::stoll(args[2]));
            if (entry == nullptr) { fail("probe not visible: " + line); return; }
            queueMove(center(*entry));
            if (cmd == "clickp") queueClicks(1);
            if (cmd == "dclickp") queueClicks(2);
        } else if (cmd == "clickarrowp") {
            if (!need(3)) return;
            const UiProbeEntry* entry = findProbe(args[1], std::stoll(args[2]));
            if (entry == nullptr) { fail("probe not visible: " + line); return; }
            queueMove(ImVec2(entry->min.x + 8.0f, (entry->min.y + entry->max.y) * 0.5f));
            queueClicks(1);
        } else if (cmd == "clickrow" || cmd == "dclickrow" || cmd == "hoverrow" || cmd == "clickarrowrow") {
            // clickrow|dclickrow|hoverrow|clickarrowrow <tag> <k> <window>: k-th visible row (by y) of that tag, negative counts from the end.
            if (!need(4)) return;
            std::vector<const UiProbeEntry*> rows;
            for (const UiProbeEntry& entry : UiProbe::entries()) {
                if (!entry.isMark && entry.id >= 0 && entry.tag == args[1]) rows.push_back(&entry);
            }
            std::sort(rows.begin(), rows.end(), [](auto* a, auto* b) { return a->min.y < b->min.y; });
            int k = std::stoi(args[2]);
            if (k < 0) k += static_cast<int>(rows.size());
            if (k < 0 || k >= static_cast<int>(rows.size())) { fail("no such visible row: " + line); return; }
            if (cmd == "clickarrowrow") {
                queueMove(ImVec2(rows[static_cast<size_t>(k)]->min.x + 8.0f,
                                 (rows[static_cast<size_t>(k)]->min.y + rows[static_cast<size_t>(k)]->max.y) * 0.5f));
                queueClicks(1);
            } else {
                queueMove(clickPoint(*rows[static_cast<size_t>(k)], findWindow(args[3])));
                if (cmd != "hoverrow") queueClicks(cmd == "dclickrow" ? 2 : 1);
            }
        } else if (cmd == "click" || cmd == "dclick") {
            if (!need(3)) return;
            queueMove(ImVec2(std::stof(args[1]), std::stof(args[2])));
            queueClicks(cmd == "dclick" ? 2 : 1);
        } else if (cmd == "wheel") {
            if (need(2)) { const float dy = std::stof(args[1]); queueFrame([dy](ImGuiIO& io) { io.AddMouseWheelEvent(0.0f, dy); }); }
        } else if (cmd == "scroll") {
            if (!need(3)) return;
            m_scroll = ScrollTask{};
            m_scroll.active = true;
            m_scroll.window = args[1];
            m_scroll.target = args[2];
        } else if (cmd == "key") {
            if (!need(2)) return;
            const ImGuiKey key = keyFromName(args[1]);
            if (key == ImGuiKey_None) { fail("unknown key: " + line); return; }
            const bool ctrl = args.size() > 2 && args[2] == "ctrl";
            if (ctrl) queueFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, true); });
            queueFrame([key](ImGuiIO& io) { io.AddKeyEvent(key, true); });
            queueFrame([key](ImGuiIO& io) { io.AddKeyEvent(key, false); });
            if (ctrl) queueFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, false); });
        } else if (cmd == "type") {
            const size_t at = line.find(' ');
            const std::string text = at == std::string::npos ? "" : line.substr(at + 1);
            queueFrame([text](ImGuiIO& io) { io.AddInputCharactersUTF8(text.c_str()); });
        } else if (cmd == "append") {
            // append <single|newline|long> <count> <tag>
            if (!need(4) || !frame.appendLog) return;
            const int count = std::stoi(args[2]);
            for (int i = 0; i < count; ++i) {
                const int serial = m_appendSerial++;
                frame.appendLog(appendText(args[1], serial, args[3]), serial % 3);
            }
            m_waitFrames = kSettleFrames;
        } else if (cmd == "resize") {
            if (need(3)) { glfwSetWindowSize(window, std::stoi(args[1]), std::stoi(args[2])); m_waitFrames = 8; }
        } else if (cmd == "clipclear") {
            glfwSetClipboardString(window, "UI_SCENARIO_CLIPBOARD_SENTINEL");
            m_waitFrames = 2;
        } else if (cmd == "shot") {
            if (need(2) && (!frame.saveFrame || !frame.saveFrame(args[1]))) fail("frame capture failed: " + args[1]);
        } else if (cmd == "reload") {
            // reload <path>: goes through the application's loadGltfModel, as File > Open does.
            if (need(2)) {
                if (!frame.loadModel || !frame.loadModel(args[1])) fail("model load failed: " + args[1]);
                m_waitFrames = kSettleFrames;
            }
        } else if (cmd == "picknode") {
            // picknode <id>: viewport picking simulation invoking selection.selectNode and requestScrollToSelectedNode
            if (need(2)) {
                if (!frame.pickNode || !frame.pickNode(std::stoi(args[1]))) fail("picknode failed: " + line);
                m_waitFrames = kSettleFrames;
            }
        } else if (cmd == "snap") {
            if (need(2)) snap(args, window, frame);
        } else {
            fail("unknown command: " + line);
        }
    }

    void stepScroll()
    {
        ImGuiWindow* target = findWindow(m_scroll.window);
        if (target == nullptr) {
            fail("scroll window not found: " + m_scroll.window);
            m_scroll.active = false;
            return;
        }
        if (!m_queue.empty() || m_waitFrames > 0) {
            if (m_waitFrames > 0) --m_waitFrames;
            return;
        }
        const float current = target->Scroll.y;
        const float maxScroll = target->ScrollMax.y;
        float targetPixels = 0.0f;
        if (m_scroll.target == "bottom") targetPixels = maxScroll;
        else if (m_scroll.target != "top") targetPixels = std::floor(std::stof(m_scroll.target) * maxScroll);

        if (m_scroll.lastWheel != 0.0f && current != m_scroll.lastScroll && current > 0.5f && current < maxScroll - 0.5f) {
            m_scroll.stepEstimate = std::max(1.0f, std::fabs(current - m_scroll.lastScroll) / std::fabs(m_scroll.lastWheel));
        }
        const float remaining = targetPixels - current;
        if (std::getenv("UI_SCENARIO_TRACE") != nullptr) {
            std::fprintf(stderr, "scroll trace: cur=%.1f max=%.1f target=%.1f est=%.2f iter=%d\n", current, maxScroll, targetPixels, m_scroll.stepEstimate, m_scroll.iterations);
        }
        if (std::fabs(remaining) <= 0.6f) {
            if (++m_scroll.stable >= 3) {
                m_scroll.active = false;
                return;
            }
            m_waitFrames = 1;
            return;
        }
        m_scroll.stable = 0;
        if (++m_scroll.iterations > 80) {
            fail("scroll did not converge: " + m_scroll.window + " -> " + m_scroll.target);
            m_scroll.active = false;
            return;
        }
        float wheel = -remaining / m_scroll.stepEstimate;
        wheel = std::max(-2000.0f, std::min(2000.0f, wheel));
        m_scroll.lastScroll = current;
        m_scroll.lastWheel = wheel;
        const ImVec2 hover = target->InnerRect.GetCenter();
        m_queue.push_back([this, hover](ImGuiIO&) {
            m_mousePos = hover;
            m_hasMouse = true;
        });
        m_queue.push_back([wheel](ImGuiIO& io) { io.AddMouseWheelEvent(0.0f, wheel); });
        m_waitFrames = 4; // let the wheel event be consumed before the next measurement
    }

    std::string regionHash(GLFWwindow* window, const ImRect& rect) const
    {
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        const float scale = ImGui::GetIO().DisplayFramebufferScale.x;
        const int x0 = std::max(0, static_cast<int>(std::floor(rect.Min.x * scale)));
        const int x1 = std::min(framebufferWidth, static_cast<int>(std::ceil(rect.Max.x * scale)));
        const int yTop = std::max(0, static_cast<int>(std::floor(rect.Min.y * scale)));
        const int yBottom = std::min(framebufferHeight, static_cast<int>(std::ceil(rect.Max.y * scale)));
        if (x1 <= x0 || yBottom <= yTop) {
            return "empty";
        }
        std::vector<unsigned char> pixels(static_cast<size_t>(x1 - x0) * (yBottom - yTop) * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadBuffer(GL_BACK);
        glReadPixels(x0, framebufferHeight - yBottom, x1 - x0, yBottom - yTop, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return hex(fnv(pixels.data(), pixels.size())) + ":" + std::to_string(x1 - x0) + "x" + std::to_string(yBottom - yTop);
    }

    void snap(const std::vector<std::string>& args, GLFWwindow* window, UiScenarioFrame& frame)
    {
        const bool withPixels = std::find(args.begin(), args.end(), "pix") != args.end();
        const bool withClipboard = std::find(args.begin(), args.end(), "clip") != args.end();
        std::ostringstream out;
        out << "{\"label\":\"" << escape(args[1]) << "\",\"frame\":" << m_frame
            << ",\"mouse\":[" << num(ImGui::GetIO().MousePos.x) << "," << num(ImGui::GetIO().MousePos.y) << "]"
            << ",\"hoveredWindow\":\"" << escape(ImGui::GetCurrentContext()->HoveredWindow ? ImGui::GetCurrentContext()->HoveredWindow->Name : "") << "\""
            << ",\"selection\":\"" << escape(frame.selection) << "\""
            << ",\"camera\":\"" << escape(frame.camera) << "\""
            << ",\"hierarchyBuildCount\":" << frame.hierarchyBuildCount
            << ",\"logger\":{\"entries\":" << frame.logEntries << ",\"errors\":" << frame.logErrors
            << ",\"warnings\":" << frame.logWarnings << ",\"info\":" << frame.logInfo << "}";

        out << ",\"windows\":{";
        bool firstWindow = true;
        for (const char* name : { "LoggerScrollArea", "ResourcesTreeScrollRegion", "IssuesTable", "HierarchyTreeScrollRegion" }) {
            ImGuiWindow* tracked = findWindow(name);
            if (tracked == nullptr) {
                continue;
            }
            out << (firstWindow ? "" : ",") << "\"" << name << "\":{\"scrollY\":" << num(tracked->Scroll.y)
                << ",\"scrollMaxY\":" << num(tracked->ScrollMax.y)
                << ",\"scrollX\":" << num(tracked->Scroll.x)
                << ",\"scrollMaxX\":" << num(tracked->ScrollMax.x)
                << ",\"contentH\":" << num(tracked->ContentSize.y)
                << ",\"inner\":[" << num(tracked->InnerRect.Min.x) << "," << num(tracked->InnerRect.Min.y)
                << "," << num(tracked->InnerRect.Max.x) << "," << num(tracked->InnerRect.Max.y) << "]";
            if (withPixels) {
                out << ",\"pixels\":\"" << regionHash(window, tracked->InnerRect) << "\"";
            }
            out << "}";
            firstWindow = false;
        }
        out << "}";

        out << ",\"probes\":[";
        bool firstProbe = true;
        std::ostringstream marks;
        bool firstMark = true;
        for (const UiProbeEntry& entry : UiProbe::entries()) {
            if (entry.isMark) {
                marks << (firstMark ? "" : ",") << "{\"tag\":\"" << escape(entry.tag) << "\",\"id\":" << entry.id << "}";
                firstMark = false;
                continue;
            }
            out << (firstProbe ? "" : ",") << "{\"tag\":\"" << escape(entry.tag) << "\",\"id\":" << entry.id
                << ",\"x0\":" << num(entry.min.x) << ",\"y0\":" << num(entry.min.y)
                << ",\"x1\":" << num(entry.max.x) << ",\"y1\":" << num(entry.max.y)
                << ",\"hovered\":" << (entry.hovered ? "true" : "false")
                << ",\"focused\":" << (entry.focused ? "true" : "false")
                << ",\"open\":" << (entry.open ? "true" : "false")
                << ",\"selected\":" << (entry.selected ? "true" : "false") << "}";
            firstProbe = false;
        }
        out << "],\"marks\":[" << marks.str() << "]";

        if (withClipboard) {
            const char* clipboard = glfwGetClipboardString(window);
            const std::string text = clipboard != nullptr ? clipboard : "";
            const std::string expected = frame.expectedCopyText ? frame.expectedCopyText() : "";
            size_t lines = 0;
            for (char c : text) lines += (c == '\n');
            out << ",\"clipboard\":{\"bytes\":" << text.size() << ",\"newlines\":" << lines
                << ",\"hash\":\"" << hex(fnv(text.data(), text.size())) << "\""
                << ",\"expectedBytes\":" << expected.size()
                << ",\"expectedHash\":\"" << hex(fnv(expected.data(), expected.size())) << "\""
                << ",\"equalsExpected\":" << (text == expected ? "true" : "false")
                << ",\"isSentinel\":" << (text == "UI_SCENARIO_CLIPBOARD_SENTINEL" ? "true" : "false") << "}";
        }
        out << "}";
        m_snaps.push_back(out.str());
        m_waitFrames = 1;
    }

    void finish(GLFWwindow* window)
    {
        if (m_finished) {
            return;
        }
        m_finished = true;
        std::ostringstream out;
        out << "{\"frames\":" << m_frame << ",\"failed\":" << (m_failed ? "true" : "false") << ",\"errors\":[";
        for (size_t i = 0; i < m_errors.size(); ++i) {
            out << (i ? "," : "") << "\"" << escape(m_errors[i]) << "\"";
        }
        out << "],\"snaps\":[";
        for (size_t i = 0; i < m_snaps.size(); ++i) {
            out << (i ? ",\n" : "\n") << m_snaps[i];
        }
        out << "\n]}\n";
        std::ofstream report(m_reportPath, std::ios::binary | std::ios::trunc);
        report << out.str();
        report.flush();
        if (!report) {
            std::fprintf(stderr, "UiScenario: report write failed: %s\n", m_reportPath.c_str());
            m_failed = true;
        }
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
};

} // namespace ux3d::imgui
