// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ControlPipe.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#endif

#include <algorithm>
#include <utility>

namespace ux3d::imgui {

namespace {

constexpr const char* kBusyLine =
    "{\"id\":null,\"ok\":false,\"error\":{\"code\":\"busy\",\"message\":\"Another agent client is already "
    "connected to this Glider instance; only one client at a time.\"}}\n";
constexpr size_t kMaxLineBytes = 64u * 1024u * 1024u;

#ifdef _WIN32
std::wstring widen(const std::string& text)
{
    return std::wstring(text.begin(), text.end()); // the pipe name is ASCII
}

HANDLE createInstance(const std::wstring& name, void* securityDescriptor, bool first)
{
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(attributes);
    attributes.lpSecurityDescriptor = securityDescriptor;
    attributes.bInheritHandle = FALSE;
    const DWORD openMode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | (first ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0);
    const DWORD pipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS;
    return CreateNamedPipeW(name.c_str(), openMode, pipeMode, PIPE_UNLIMITED_INSTANCES, 1u << 16, 1u << 16, 0,
                            &attributes);
}

// "D:P(A;;GA;;;<current user SID>)": only the current user may open the pipe.
void* currentUserSecurityDescriptor(std::string& error)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        error = "OpenProcessToken failed (" + std::to_string(GetLastError()) + ")";
        return nullptr;
    }
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<BYTE> buffer(size);
    LPWSTR sidText = nullptr;
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (size == 0 || !GetTokenInformation(token, TokenUser, buffer.data(), size, &size) ||
        !ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sidText)) {
        error = "cannot read the current user SID (" + std::to_string(GetLastError()) + ")";
    } else {
        const std::wstring sddl = std::wstring(L"D:P(A;;GA;;;") + sidText + L")";
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) {
            error = "cannot build the pipe security descriptor (" + std::to_string(GetLastError()) + ")";
            descriptor = nullptr;
        }
    }
    if (sidText != nullptr) LocalFree(sidText);
    CloseHandle(token);
    return descriptor;
}
#endif

} // namespace

ControlPipe::~ControlPipe()
{
    stop();
}

bool ControlPipe::start(std::function<void()> wake, std::string& error)
{
#ifdef _WIN32
    if (m_running) return true;
    m_wake = std::move(wake);
    m_name = "\\\\.\\pipe\\glider-" + std::to_string(GetCurrentProcessId());
    m_securityDescriptor = currentUserSecurityDescriptor(error);
    if (m_securityDescriptor == nullptr) return false;
    HANDLE first = createInstance(widen(m_name), m_securityDescriptor, true);
    if (first == INVALID_HANDLE_VALUE) {
        error = "CreateNamedPipe " + m_name + " failed (" + std::to_string(GetLastError()) + ")";
        LocalFree(m_securityDescriptor);
        m_securityDescriptor = nullptr;
        return false;
    }
    m_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    m_replyEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_running = true;
    m_thread = std::thread([this, first] {
        HANDLE listen = first;
        HANDLE stopEvent = static_cast<HANDLE>(m_stopEvent);
        HANDLE replyEvent = static_cast<HANDLE>(m_replyEvent);
        HANDLE connectEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HANDLE readEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HANDLE writeEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        const std::wstring wideName = widen(m_name);
        auto wakeMain = [this] { if (m_wake) m_wake(); };

        OVERLAPPED connectOv{};
        // A client can open the instance before ConnectNamedPipe runs (ERROR_PIPE_CONNECTED);
        // no overlapped operation is pending then, so its result is not asked for.
        bool connectedEarly = false;
        auto beginConnect = [&] {
            ResetEvent(connectEvent);
            connectOv = OVERLAPPED{};
            connectOv.hEvent = connectEvent;
            connectedEarly = false;
            if (ConnectNamedPipe(listen, &connectOv)) {
                SetEvent(connectEvent);
                return;
            }
            const DWORD code = GetLastError();
            if (code == ERROR_PIPE_CONNECTED) {
                connectedEarly = true;
                SetEvent(connectEvent);
            }
        };
        auto writeAll = [&](HANDLE handle, const std::string& data) {
            size_t offset = 0;
            while (offset < data.size()) {
                OVERLAPPED ov{};
                ov.hEvent = writeEvent;
                ResetEvent(writeEvent);
                const DWORD chunk = static_cast<DWORD>(std::min<size_t>(data.size() - offset, 1u << 20));
                if (!WriteFile(handle, data.data() + offset, chunk, nullptr, &ov) && GetLastError() != ERROR_IO_PENDING) {
                    return false;
                }
                HANDLE waits[2] = {writeEvent, stopEvent};
                if (WaitForMultipleObjects(2, waits, FALSE, INFINITE) != WAIT_OBJECT_0) {
                    DWORD ignored = 0;
                    CancelIoEx(handle, &ov);
                    GetOverlappedResult(handle, &ov, &ignored, TRUE);
                    return false;
                }
                DWORD written = 0;
                if (!GetOverlappedResult(handle, &ov, &written, FALSE) || written == 0) return false;
                offset += written;
            }
            return true;
        };

        if (listen != INVALID_HANDLE_VALUE) beginConnect();
        HANDLE active = INVALID_HANDLE_VALUE;
        uint64_t session = 0;
        OVERLAPPED readOv{};
        bool readPending = false;
        std::vector<char> buffer(16384);
        std::string partial;
        struct Rejected {
            HANDLE handle;
            ULONGLONG deadline;
        };
        std::vector<Rejected> rejected;

        auto closeActive = [&] {
            if (active == INVALID_HANDLE_VALUE) return;
            if (readPending) {
                DWORD ignored = 0;
                CancelIoEx(active, &readOv);
                GetOverlappedResult(active, &readOv, &ignored, TRUE);
                readPending = false;
            }
            DisconnectNamedPipe(active);
            CloseHandle(active);
            active = INVALID_HANDLE_VALUE;
            partial.clear();
            m_connected = false;
            wakeMain();
        };

        for (;;) {
            if (active != INVALID_HANDLE_VALUE && !readPending) {
                readOv = OVERLAPPED{};
                readOv.hEvent = readEvent;
                ResetEvent(readEvent);
                if (!ReadFile(active, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr, &readOv) &&
                    GetLastError() != ERROR_IO_PENDING) {
                    closeActive();
                    continue;
                }
                readPending = true;
            }
            ULONGLONG now = GetTickCount64();
            DWORD timeout = INFINITE;
            for (const Rejected& r : rejected) {
                timeout = std::min<DWORD>(timeout, r.deadline > now ? static_cast<DWORD>(r.deadline - now) : 0);
            }
            HANDLE waits[4] = {stopEvent, connectEvent, replyEvent, readEvent};
            const DWORD count = active != INVALID_HANDLE_VALUE ? 4 : 3;
            const DWORD signalled = WaitForMultipleObjects(count, waits, FALSE, timeout);

            now = GetTickCount64();
            for (auto it = rejected.begin(); it != rejected.end();) {
                if (it->deadline <= now) {
                    DisconnectNamedPipe(it->handle);
                    CloseHandle(it->handle);
                    it = rejected.erase(it);
                } else {
                    ++it;
                }
            }
            if (signalled == WAIT_OBJECT_0 || signalled == WAIT_OBJECT_0 + 2) {
                std::deque<std::pair<uint64_t, std::string>> outgoing;
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    outgoing.swap(m_outgoing);
                }
                for (auto& [replySession, line] : outgoing) {
                    if (replySession != session || active == INVALID_HANDLE_VALUE) continue;
                    line.push_back('\n');
                    if (!writeAll(active, line)) {
                        closeActive();
                    }
                }
                // On shutdown the last replies (quit) are written first. Closing without
                // DisconnectNamedPipe leaves them readable for the client.
                if (signalled == WAIT_OBJECT_0) {
                    if (active != INVALID_HANDLE_VALUE) {
                        if (readPending) {
                            DWORD ignored = 0;
                            CancelIoEx(active, &readOv);
                            GetOverlappedResult(active, &readOv, &ignored, TRUE);
                            readPending = false;
                        }
                        CloseHandle(active);
                        active = INVALID_HANDLE_VALUE;
                        m_connected = false;
                    }
                    break;
                }
            } else if (signalled == WAIT_OBJECT_0 + 1) {
                DWORD ignored = 0;
                const bool connected = connectedEarly || GetOverlappedResult(listen, &connectOv, &ignored, FALSE);
                ResetEvent(connectEvent);
                HANDLE client = listen;
                listen = createInstance(wideName, m_securityDescriptor, false);
                if (listen != INVALID_HANDLE_VALUE) beginConnect();
                if (!connected) {
                    CloseHandle(client);
                } else if (active != INVALID_HANDLE_VALUE) {
                    // One client at a time: tell the second one why, then drop it shortly after.
                    ++m_rejected;
                    writeAll(client, kBusyLine);
                    rejected.push_back({client, now + 2000});
                } else {
                    active = client;
                    session = ++m_session;
                    ++m_accepted;
                    m_connected = true;
                    wakeMain();
                }
            } else if (signalled == WAIT_OBJECT_0 + 3) {
                DWORD bytes = 0;
                const bool ok = GetOverlappedResult(active, &readOv, &bytes, FALSE);
                readPending = false;
                if (!ok) {
                    closeActive();
                    continue;
                }
                partial.append(buffer.data(), bytes);
                bool any = false;
                size_t newline = 0;
                while ((newline = partial.find('\n')) != std::string::npos) {
                    std::string line = partial.substr(0, newline);
                    partial.erase(0, newline + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.find_first_not_of(" \t") == std::string::npos) continue;
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_incoming.push_back({session, std::move(line)});
                    any = true;
                }
                if (partial.size() > kMaxLineBytes) closeActive();
                if (any) wakeMain();
            }
        }

        closeActive();
        for (const Rejected& r : rejected) {
            DisconnectNamedPipe(r.handle);
            CloseHandle(r.handle);
        }
        if (listen != INVALID_HANDLE_VALUE) {
            DWORD ignored = 0;
            CancelIoEx(listen, &connectOv);
            GetOverlappedResult(listen, &connectOv, &ignored, TRUE);
            CloseHandle(listen);
        }
        CloseHandle(connectEvent);
        CloseHandle(readEvent);
        CloseHandle(writeEvent);
    });
    return true;
#else
    (void)wake;
    error = "the control pipe needs Windows named pipes";
    return false;
#endif
}

void ControlPipe::stop()
{
#ifdef _WIN32
    if (!m_running) return;
    SetEvent(static_cast<HANDLE>(m_stopEvent));
    if (m_thread.joinable()) m_thread.join();
    CloseHandle(static_cast<HANDLE>(m_stopEvent));
    CloseHandle(static_cast<HANDLE>(m_replyEvent));
    m_stopEvent = m_replyEvent = nullptr;
    if (m_securityDescriptor != nullptr) LocalFree(m_securityDescriptor);
    m_securityDescriptor = nullptr;
    m_running = false;
    m_connected = false;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_incoming.clear();
    m_outgoing.clear();
#endif
}

std::vector<ControlPipe::Request> ControlPipe::takeRequests()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Request> requests;
    requests.swap(m_incoming);
    return requests;
}

void ControlPipe::reply(uint64_t session, std::string line)
{
#ifdef _WIN32
    if (!m_running) return;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_outgoing.emplace_back(session, std::move(line));
    }
    SetEvent(static_cast<HANDLE>(m_replyEvent));
#else
    (void)session;
    (void)line;
#endif
}

} // namespace ux3d::imgui
