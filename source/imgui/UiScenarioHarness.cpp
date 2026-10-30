// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// The UI scenario harness (declared in UiScenarioHarness.h): scripted UI frames, script parsing and commands, and
// the scenario report.

#include "imgui/UiScenarioHarness.h"

namespace ux3d::imgui {

bool UiScenario::configure(const std::string& scriptPath, const std::string& reportPath)
{
    m_reportPath = reportPath;
    std::ifstream input(scriptPath);
    if (!input.is_open()) {
        std::fprintf(stderr, "UiScenario: cannot open script %s\n", scriptPath.c_str());
        m_failed = true;
        return false;
    }
    std::string line;
    uint64_t lineNum = 0;
    while (std::getline(input, line)) {
        ++lineNum;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') {
            continue;
        }
        m_commands.push_back(line.substr(first));
        m_commandLines.push_back(lineNum);
    }
    m_active = true;
    UiProbe::enabled() = true;
    return true;
}

bool UiScenario::configureFollow(const std::string& scriptPath, const std::string& reportPath)
{
    m_followMode = true;
    m_followPath = scriptPath;
    m_outJsonlPath = scriptPath + ".out.jsonl";
    m_reportPath = reportPath;
    m_active = true;
    UiProbe::enabled() = true;
    m_followFileOffset = 0;
    m_fileLinesRead = 0;

    // Truncate out.jsonl for new session
    {
        std::ofstream out(m_outJsonlPath, std::ios::trunc);
    }
    // Ensure script file exists
    {
        std::ofstream touch(m_followPath, std::ios::app);
    }
    return true;
}

void UiScenario::beginFrame(ImGuiIO& io)
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

void UiScenario::endFrame(GLFWwindow* window, UiScenarioFrame& frame)
{
    ++m_frame;
    if (!m_followMode && m_frame > kMaxFrames) {
        fail("script did not finish within the frame limit");
        finish(window);
        return;
    }

    if (m_catchReallocVpId != 0) {
        for (const auto& v : frame.viewports) {
            if (v.id == m_catchReallocVpId && v.targetGeneration > m_catchReallocBaseGen) {
                if (!m_catchReallocShot.empty() && frame.saveFrame) {
                    if (!frame.saveFrame(m_catchReallocShot)) {
                        fail("frame capture failed: " + m_catchReallocShot);
                    }
                }
                if (!m_catchReallocSnap.empty()) {
                    snap({ "snap", m_catchReallocSnap }, window, frame);
                }
                m_catchReallocVpId = 0;
                m_catchReallocShot.clear();
                m_catchReallocSnap.clear();
                break;
            }
        }
    }

    if (m_scroll.active) {
        stepScroll();
        return;
    }

    // Read newly appended lines in follow mode
    if (m_followMode) {
        std::ifstream input(m_followPath, std::ios::binary);
        if (input.is_open()) {
            input.seekg(0, std::ios::end);
            std::streampos endPos = input.tellg();
            if (endPos > m_followFileOffset) {
                input.seekg(m_followFileOffset);
                std::string chunk;
                chunk.resize(static_cast<size_t>(endPos - m_followFileOffset));
                input.read(&chunk[0], chunk.size());
                size_t lastNewline = chunk.rfind('\n');
                if (lastNewline != std::string::npos) {
                    std::string completeLines = chunk.substr(0, lastNewline + 1);
                    m_followFileOffset += static_cast<std::streampos>(lastNewline + 1);
                    std::istringstream stream(completeLines);
                    std::string line;
                    while (std::getline(stream, line)) {
                        ++m_fileLinesRead;
                        if (!line.empty() && line.back() == '\r') {
                            line.pop_back();
                        }
                        const size_t first = line.find_first_not_of(" \t");
                        if (first == std::string::npos || line[first] == '#') {
                            continue;
                        }
                        m_commands.push_back(line.substr(first));
                        m_commandLines.push_back(m_fileLinesRead);
                    }
                }
            }
        }
    }

    if (m_waitFrames > 0) {
        --m_waitFrames;
        return;
    }
    if (!m_queue.empty() || m_commandPending) {
        return;
    }
    if (m_waitUntil > std::chrono::steady_clock::now()) {
        return;
    }

    while (m_next < m_commands.size() && !m_failed) {
        const std::string command = m_commands[m_next];
        const uint64_t cmdLine = m_commandLines[m_next];
        ++m_next;
        execute(command, cmdLine, window, frame);
        if (m_scroll.active || m_waitFrames > 0 || !m_queue.empty() || m_commandPending
            || m_waitUntil > std::chrono::steady_clock::now()) {
            return;
        }
    }
    if (!m_followMode) {
        finish(window);
    }
}

void UiScenario::writeResult(uint64_t lineNum, const std::string& cmd, bool ok, const std::string& error, const nlohmann::json& result)
{
    if (!m_followMode || m_outJsonlPath.empty()) {
        return;
    }
    nlohmann::json out;
    out["line"] = lineNum;
    out["cmd"] = cmd;
    out["ok"] = ok;
    if (!ok || !error.empty()) {
        out["error"] = error;
    } else {
        out["error"] = nullptr;
    }
    out["result"] = result;
    std::ofstream file(m_outJsonlPath, std::ios::app);
    if (file.is_open()) {
        file << out.dump() << "\n";
        file.flush();
    }
}

void UiScenario::reportError(uint64_t lineNum, const std::string& cmd, const std::string& msg)
{
    if (m_followMode) {
        writeResult(lineNum, cmd, false, msg, nullptr);
    } else {
        fail(msg);
    }
}

std::string UiScenario::escape(const std::string& text)
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

std::string UiScenario::num(double value)
{
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    return buffer;
}

uint64_t UiScenario::fnv(const void* data, size_t size, uint64_t hash)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string UiScenario::hex(uint64_t value)
{
    char buffer[24];
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
    return buffer;
}

void UiScenario::fail(const std::string& message)
{
    m_failed = true;
    m_errors.push_back(message);
    std::fprintf(stderr, "UiScenario: %s\n", message.c_str());
}

void UiScenario::queueFrame(std::function<void(ImGuiIO&)> op)
{
    m_queue.push_back(std::move(op));
    m_waitFrames = kSettleFrames;
}

void UiScenario::queueMove(ImVec2 position)
{
    queueFrame([this, position](ImGuiIO&) {
        m_mousePos = position;
        m_hasMouse = true;
    });
}

void UiScenario::queueClicks(int count, int button)
{
    for (int i = 0; i < count; ++i) {
        queueFrame([button](ImGuiIO& io) { io.AddMouseButtonEvent(button, true); });
        queueFrame([button](ImGuiIO& io) { io.AddMouseButtonEvent(button, false); });
    }
}

std::vector<std::string> UiScenario::tokens(const std::string& line)
{
    std::istringstream stream(line);
    std::vector<std::string> out;
    std::string token;
    while (stream >> token) {
        out.push_back(token);
    }
    return out;
}

std::string UiScenario::pathArgument(const std::string& line)
{
    const size_t separator = line.find_first_of(" \t");
    if (separator == std::string::npos) return {};
    const size_t start = line.find_first_not_of(" \t", separator);
    if (start == std::string::npos) return {};
    std::string path = line.substr(start);
    if (path.size() >= 2 && path.front() == '"' && path.back() == '"') path = path.substr(1,path.size()-2);
    return path;
}

ImGuiWindow* UiScenario::findWindow(const std::string& substring)
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

const UiProbeEntry* UiScenario::findProbe(const std::string& tag, long long id)
{
    for (const UiProbeEntry& entry : UiProbe::entries()) {
        if (!entry.isMark && entry.id == id && entry.tag == tag) {
            return &entry;
        }
    }
    return nullptr;
}

ImVec2 UiScenario::clickPoint(const UiProbeEntry& entry, ImGuiWindow* clipWindow)
{
    ImVec2 point = center(entry);
    if (clipWindow != nullptr) {
        const float top = std::max(entry.min.y, clipWindow->InnerRect.Min.y);
        const float bottom = std::min(entry.max.y, clipWindow->InnerRect.Max.y);
        point.y = (top + bottom) * 0.5f;
    }
    return point;
}

ImGuiKey UiScenario::keyFromName(const std::string& name)
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
    if (name == "f") return ImGuiKey_F;
    if (name == "h") return ImGuiKey_H;
    if (name == "slash") return ImGuiKey_Slash;
    if (name == "lbracket") return ImGuiKey_LeftBracket;
    if (name == "rbracket") return ImGuiKey_RightBracket;
    return ImGuiKey_None;
}

std::string UiScenario::appendText(const std::string& kind, int serial, const std::string& tag) const
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

void UiScenario::execute(const std::string& line, uint64_t cmdLine, GLFWwindow* window, UiScenarioFrame& frame)
{
    std::vector<std::string> args = tokens(line);
    if (args.empty()) {
        return;
    }
    const std::string& cmd = args[0];
    if (args.size() > 1 && (cmd == "hoverp" || cmd == "clickp" || cmd == "dclickp" || cmd == "rclickp" || cmd == "clickarrowp" || cmd == "clickrow" || cmd == "dclickrow" || cmd == "hoverrow" || cmd == "clickarrowrow")) {
        // Prefer a literal registered tag. Legacy scripts encode spaces as
        // underscores, but newer tags can contain real underscores themselves.
        const auto& entries = UiProbe::entries();
        if (std::none_of(entries.begin(), entries.end(), [&](const UiProbeEntry& entry) { return entry.tag == args[1]; }))
            std::replace(args[1].begin(), args[1].end(), '_', ' ');
    }
    try {
        executeCommand(cmd, args, line, cmdLine, window, frame);
    } catch (const std::exception& e) {
        reportError(cmdLine, cmd, std::string("malformed command: ") + e.what());
    }
}

void UiScenario::executeCommand(const std::string& cmd, const std::vector<std::string>& args, const std::string& line, uint64_t cmdLine, GLFWwindow* window, UiScenarioFrame& frame)
{
    auto need = [&](size_t count) {
        if (args.size() < count) {
            reportError(cmdLine, cmd, "missing arguments: " + line);
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
    } else if (cmd == "hoverp" || cmd == "clickp" || cmd == "dclickp" || cmd == "rclickp") {
        if (!need(3)) return;
        const UiProbeEntry* entry = findProbe(args[1], std::stoll(args[2]));
        if (entry == nullptr) { fail("probe not visible: " + line); return; }
        queueMove(center(*entry));
        if (cmd == "clickp") queueClicks(1);
        if (cmd == "rclickp") queueClicks(1, ImGuiMouseButton_Right);
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
    } else if (cmd == "click" || cmd == "dclick" || cmd == "rclick") {
        if (!need(3)) return;
        queueMove(ImVec2(std::stof(args[1]), std::stof(args[2])));
        queueClicks(cmd == "dclick" ? 2 : 1, cmd == "rclick" ? ImGuiMouseButton_Right : 0);
    } else if (cmd == "drag") {
        // drag <x0> <y0> <x1> <y1>: left-button drag, one interpolated move per frame.
        if (!need(5)) return;
        const ImVec2 from(std::stof(args[1]), std::stof(args[2]));
        const ImVec2 to(std::stof(args[3]), std::stof(args[4]));
        constexpr int steps = 8;
        queueMove(from);
        queueFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, true); });
        for (int s = 1; s <= steps; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(steps);
            queueMove(ImVec2(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t));
        }
        queueFrame([](ImGuiIO& io) { io.AddMouseButtonEvent(0, false); });
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
        const bool shift = args.size() > 2 && args[2] == "shift";
        const bool alt = args.size() > 2 && args[2] == "alt";
        if (ctrl) queueFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, true); });
        if (shift) queueFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Shift, true); });
        if (alt) queueFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Alt, true); });
        queueFrame([key](ImGuiIO& io) { io.AddKeyEvent(key, true); });
        queueFrame([key](ImGuiIO& io) { io.AddKeyEvent(key, false); });
        if (ctrl) queueFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Ctrl, false); });
        if (shift) queueFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Shift, false); });
        if (alt) queueFrame([](ImGuiIO& io) { io.AddKeyEvent(ImGuiMod_Alt, false); });
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
        if (need(2)) {
            if (frame.saveFrame && frame.saveFrame(args[1])) {
                if (m_followMode) writeResult(cmdLine, "shot", true, "", nlohmann::json{{"path", args[1]}});
            } else {
                reportError(cmdLine, "shot", "frame capture failed: " + args[1]);
            }
        }
    } else if (cmd == "reloadwithouttextures") {
        if (need(2)) {
            if (!frame.loadModelWithoutTextures || !frame.loadModelWithoutTextures(pathArgument(line))) fail("model load failed: " + line);
            m_waitFrames = kSettleFrames;
        }
    } else if (cmd == "reload") {
        // reload <path>: goes through the application's loadGltfModel, as File > Open does.
        if (need(2)) {
            if (!frame.loadModel || !frame.loadModel(pathArgument(line))) fail("model load failed: " + pathArgument(line));
            m_waitFrames = kSettleFrames;
        }
    } else if (cmd == "picknode") {
        // picknode <id>: viewport picking simulation invoking selection.selectNode and requestScrollToSelectedNode
        if (need(2)) {
            if (!frame.pickNode || !frame.pickNode(std::stoi(args[1]))) fail("picknode failed: " + line);
            m_waitFrames = kSettleFrames;
        }
    } else if (cmd == "aamode") {
        if (need(2)) {
            uint32_t vpId = 0;
            if (args.size() > 2) {
                vpId = static_cast<uint32_t>(std::stoul(args[2]));
            }
            if (!frame.setAaMode || !frame.setAaMode(args[1], vpId)) fail("aamode failed: " + line);
            m_waitFrames = 8;
        }
    } else if (cmd == "vpcreate") {
        if (!frame.createViewport || !frame.createViewport()) fail("vpcreate failed");
        m_waitFrames = 8;
    } else if (cmd == "catch_realloc") {
        if (need(4)) {
            m_catchReallocVpId = static_cast<uint32_t>(std::stoul(args[1]));
            m_catchReallocShot = args[2];
            m_catchReallocSnap = args[3];
            m_catchReallocBaseGen = 0;
            for (const auto& v : frame.viewports) {
                if (v.id == m_catchReallocVpId) {
                    m_catchReallocBaseGen = v.targetGeneration;
                    break;
                }
            }
        }
    } else if (cmd == "vpfocus") {
        if (need(2) && (!frame.focusViewport || !frame.focusViewport(static_cast<uint32_t>(std::stoul(args[1]))))) fail("vpfocus failed: " + line);
        m_waitFrames = 8;
    } else if (cmd == "vptoggle") {
        if (need(2)) {
            uint32_t vpId = static_cast<uint32_t>(std::stoul(args[1]));
            if (!frame.toggleViewport || !frame.toggleViewport(vpId)) fail("vptoggle failed: " + line);
            m_waitFrames = 4;
        }
    } else if (cmd == "vpsetting") {
        if (need(4)) {
            const uint32_t vpId = static_cast<uint32_t>(std::stoul(args[1]));
            const std::string key = args[2];
            std::string valStr = args[3];
            for (char& c : valStr) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const bool val = (valStr == "on" || valStr == "1" || valStr == "true");
            if (!frame.setViewportSetting || !frame.setViewportSetting(vpId, key, val)) fail("vpsetting failed: " + line);
            m_waitFrames = 4;
        }
    } else if (cmd == "animplay") {
        // animplay on|off: starts or pauses playback the way the toolbar Play button does.
        if (need(2)) {
            const bool play = (args[1] == "on" || args[1] == "1" || args[1] == "true");
            if (!frame.setAnimationPlaying || !frame.setAnimationPlaying(play)) fail("animplay failed: " + line);
            m_waitFrames = 4;
        }
    } else if (cmd == "asoboparam") {
        if (need(5)) {
            if (!frame.setAsoboParam || !frame.setAsoboParam(args[1], args[2], args[3], args[4])) fail("asoboparam failed: " + line);
            m_waitFrames = 4;
        }
    } else if (cmd == "vpsplit") {
        // vpsplit <id>: docks viewport <id> to the right of viewport 1 so both render every frame.
        if (need(2)) {
            const uint32_t vpId = static_cast<uint32_t>(std::stoul(args[1]));
            if (!frame.splitViewport || !frame.splitViewport(vpId)) fail("vpsplit failed: " + line);
            m_waitFrames = 8;
        }
    } else if (cmd == "snap") {
        if (need(2)) {
            snap(args, window, frame);
            if (m_followMode) writeResult(cmdLine, "snap", true, "", args.size() > 1 ? args[1] : "snap");
        }
    } else if (frame.runCommand) {
        m_commandPending = true;
        m_dispatching = true;
        const bool known = frame.runCommand(cmd, args, line,
            [this, cmdLine, cmd](bool ok, const std::string& error, const nlohmann::json& result) {
                m_commandPending = false;
                // A multi-frame command finished during this frame's tick: like the Phase 0
                // capture/open waits, the next line runs in the following frame.
                if (!m_dispatching) m_waitFrames = 1;
                if (ok) {
                    writeResult(cmdLine, cmd, true, "", result);
                } else {
                    reportError(cmdLine, cmd, error);
                }
            });
        m_dispatching = false;
        if (!known) {
            m_commandPending = false;
            reportError(cmdLine, cmd, "unknown command: " + line);
        }
    } else {
        reportError(cmdLine, cmd, "unknown command: " + line);
    }
}

void UiScenario::stepScroll()
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

std::string UiScenario::regionHash(GLFWwindow* window, const ImRect& rect) const
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

void UiScenario::snap(const std::vector<std::string>& args, GLFWwindow* window, UiScenarioFrame& frame)
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
        << ",\"revisions\":" << frame.revisionsJson
        << ",\"logger\":{\"entries\":" << frame.logEntries << ",\"errors\":" << frame.logErrors
        << ",\"warnings\":" << frame.logWarnings << ",\"info\":" << frame.logInfo << "}"
        << ",\"decalBuffer\":{\"active\":" << (frame.decalBufferActive ? "true" : "false")
        << ",\"width\":" << frame.decalBufferWidth
        << ",\"height\":" << frame.decalBufferHeight
        << ",\"bytes\":" << frame.decalBufferBytes
        << ",\"allocations\":" << frame.decalBufferAllocations
        << ",\"resizes\":" << frame.decalBufferResizes
        << ",\"releases\":" << frame.decalBufferReleases << "}"
        << ",\"glErrors\":" << frame.glErrors
        << ",\"idConflict\":" << ImGui::GetCurrentContext()->DebugDrawIdConflictsId
        << ",\"hoveredIdCount\":" << ImGui::GetCurrentContext()->HoveredIdPreviousFrameItemCount;

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
    out << ",\"viewports\":[";
    bool firstVp = true;
    for (const auto& v : frame.viewports) {
        out << (firstVp ? "" : ",") << "{\"id\":" << v.id
            << ",\"name\":\"" << escape(v.name) << "\""
            << ",\"open\":" << (v.open ? "true" : "false")
            << ",\"renderW\":" << v.renderW
            << ",\"renderH\":" << v.renderH
            << ",\"aaMode\":" << v.aaMode
            << ",\"aaW\":" << v.aaW
            << ",\"aaH\":" << v.aaH
            << ",\"aaTargets\":" << v.aaTargets
            << ",\"aaFbos\":" << v.aaFbos
            << ",\"aaBytes\":" << v.aaBytes
            << ",\"aaHistoryValid\":" << (v.aaHistoryValid ? "true" : "false")
            << ",\"aaAllocs\":" << v.aaAllocs
            << ",\"aaDeletes\":" << v.aaDeletes
            << ",\"targetGeneration\":" << v.targetGeneration
            << ",\"lastTaaHistoryValid\":" << (v.lastTaaHistoryValid ? "true" : "false")
            << ",\"lastTaaBlendFactor\":" << num(v.lastTaaBlendFactor)
            << ",\"lastTaaTargetGeneration\":" << v.lastTaaTargetGeneration
            << ",\"wireframe\":" << (v.wireframe ? "true" : "false")
            << ",\"showGrid\":" << (v.showGrid ? "true" : "false")
            << ",\"showSceneBoundingBox\":" << (v.showSceneBoundingBox ? "true" : "false")
            << ",\"forceDoubleSided\":" << (v.forceDoubleSided ? "true" : "false")
            << ",\"invertWinding\":" << (v.invertWinding ? "true" : "false")
            << ",\"renderCount\":" << v.renderCount
            << ",\"activePasses\":[";
        for (size_t p = 0; p < v.activePasses.size(); ++p) {
            if (p > 0) out << ",";
            out << "\"" << escape(v.activePasses[p]) << "\"";
        }
        out << "]}";
        firstVp = false;
    }
    out << "]}";
    m_snaps.push_back(out.str());
    m_waitFrames = 1;
}

void UiScenario::finish(GLFWwindow* window)
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
    if (!m_reportPath.empty()) {
        std::ofstream report(m_reportPath, std::ios::binary | std::ios::trunc);
        report << out.str();
        report.flush();
        if (!report) {
            std::fprintf(stderr, "UiScenario: report write failed: %s\n", m_reportPath.c_str());
            m_failed = true;
        }
    }
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

} // namespace ux3d::imgui
