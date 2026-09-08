// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "AL/al.h"

namespace ux3d::renderer {
struct AudioBuffer {
    ALuint buffer;
    bool errorOnLoading = false;
    float durInSec = 0.0f; // duration in seconds
};

}
