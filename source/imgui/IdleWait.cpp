// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/IdleWait.h"

#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ux3d::imgui {

void waitForEvents(GLFWwindow* window, double timeoutSeconds, const std::function<bool()>& hasNewInput, IdleWaitStats& stats)
{
    ++stats.waits;
#ifdef _WIN32
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const double deadline = glfwGetTime() + timeoutSeconds;
    for (;;) {
        const double remaining = deadline - glfwGetTime();
        if (remaining <= 0.0) {
            ++stats.timeouts;
            glfwPollEvents();
            return;
        }
        // GLFW's Win32 wait uses QS_ALLEVENTS, which leaves out QS_SENDMESSAGE: a resize
        // sent from another process would block its sender until the timeout. Wait on
        // all input, including messages already in the queue (MWMO_INPUTAVAILABLE).
        const DWORD result = MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(remaining * 1000.0) + 1,
                                                         QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_TIMEOUT) {
            ++stats.timeouts;
            glfwPollEvents();
            return;
        }
        const DWORD status = GetQueueStatus(QS_ALLINPUT);
        const DWORD kinds = HIWORD(status) | LOWORD(status);
        glfwPollEvents();
        int newWidth = 0;
        int newHeight = 0;
        glfwGetFramebufferSize(window, &newWidth, &newHeight);
        const bool sentOnly = (kinds & ~static_cast<DWORD>(QS_SENDMESSAGE)) == 0;
        if (!sentOnly || hasNewInput() || newWidth != width || newHeight != height || glfwWindowShouldClose(window)) {
            ++stats.inputWakes;
            return;
        }
        ++stats.sentOnlyWakes;
    }
#else
    (void)hasNewInput;
    glfwWaitEventsTimeout(timeoutSeconds);
    ++stats.inputWakes;
    (void)window;
#endif
}

} // namespace ux3d::imgui
