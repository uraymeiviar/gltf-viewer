// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <cstdint>

#include "core/String.h"

namespace ux3d::gpu {
struct ApplicationInfo {
    core::string appName;
    uint32_t appVersion;
    core::string engineName;
    uint32_t engineVersion;
};
}
