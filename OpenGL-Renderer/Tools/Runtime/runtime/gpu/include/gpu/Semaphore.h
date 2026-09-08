// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Semaphore.h
//  middleware
//
//  Created by Moritz Becher on 13.06.19.
//

#pragma once

#include "LogicalDevice.h"

namespace ux3d::gpu {
struct Semaphore {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

GpuResult createSemaphore(const LogicalDevice& device, Semaphore& semaphore);

void destroySemaphore(Semaphore& semaphore);
}
