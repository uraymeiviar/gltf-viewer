// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ux3d::imgui {

// Agent control pipe (docs/AGENT_CONTROL_PLAN.md section 3). A listener thread owns
// \\.\pipe\glider-<pid>: line-delimited JSON, current-user DACL, remote clients rejected,
// one client at a time (a second client receives a "busy" error line and is disconnected).
// Requests are handed to the main thread through takeRequests(); replies go back through
// reply(). Nothing here touches OpenGL or ImGui.
class ControlPipe {
public:
    struct Request {
        uint64_t session = 0; // changes with every accepted client
        std::string line;
    };

    ControlPipe() = default;
    ~ControlPipe();
    ControlPipe(const ControlPipe&) = delete;
    ControlPipe& operator=(const ControlPipe&) = delete;

    // wake is called from the listener thread whenever the main loop has something to do
    // (glfwPostEmptyEvent). Returns false and sets error when the pipe cannot be created.
    bool start(std::function<void()> wake, std::string& error);
    void stop();
    bool running() const { return m_running; }
    const std::string& name() const { return m_name; }
    bool clientConnected() const { return m_connected.load(); }
    uint64_t currentSession() const { return m_session.load(); }
    uint64_t clientsAccepted() const { return m_accepted.load(); }
    uint64_t clientsRejected() const { return m_rejected.load(); }

    std::vector<Request> takeRequests();
    // Queues one reply line (without the newline) for the client of that session. Replies for
    // a client that has gone are dropped.
    void reply(uint64_t session, std::string line);

private:
    std::string m_name;
    std::function<void()> m_wake;
    std::thread m_thread;
    bool m_running = false;
    std::atomic<bool> m_connected{false};
    std::atomic<uint64_t> m_session{0};
    std::atomic<uint64_t> m_accepted{0};
    std::atomic<uint64_t> m_rejected{0};

    std::mutex m_mutex;
    std::vector<Request> m_incoming;
    std::deque<std::pair<uint64_t, std::string>> m_outgoing;

    void* m_stopEvent = nullptr;  // HANDLEs, kept opaque so windows.h stays out of headers
    void* m_replyEvent = nullptr;
    void* m_securityDescriptor = nullptr;
};

} // namespace ux3d::imgui
