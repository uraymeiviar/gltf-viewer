// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <cstdint>
#include <functional>

struct GLFWwindow;

namespace ux3d::imgui {

struct IdleWaitStats {
    uint64_t waits = 0;           // calls that blocked
    uint64_t timeouts = 0;        // calls that ended at the timeout
    uint64_t inputWakes = 0;      // calls that ended for input or a window change
    uint64_t sentOnlyWakes = 0;   // wakes for messages sent by other threads/processes only, dispatched without a frame
};

// Blocks until an event that needs a frame arrives or the timeout elapses, and
// processes the pending events (P5.4). On Windows this also wakes for messages sent
// by other threads or processes (window moves and resizes from the shell), which
// glfwWaitEventsTimeout does not wait for; when such a wake brings no input
// (hasNewInput false) and no framebuffer-size change, the messages are dispatched
// and the wait continues until the same deadline.
void waitForEvents(GLFWwindow* window, double timeoutSeconds, const std::function<bool()>& hasNewInput, IdleWaitStats& stats);

} // namespace ux3d::imgui
