// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Types.h"

namespace ux3d::gpu {
// specify one QueueCreateInfo for every queue that you need. Queue families will be automatically selected
struct QueueCreateInfo {
    bool computeSupport = false;
    bool graphicsSupport = true;
    float priority = 1.0;
};

struct Queue {
    Handle _handle = InvalidHandle;
    uint32_t _queueFamilyIndex = 0u;
    uint32_t _queueIndex = 0u;
};

// Queues are created together with the logical device
}
