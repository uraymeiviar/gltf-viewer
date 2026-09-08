// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/EnumOps.h"
#include <cstdint>

namespace ux3d {
namespace filesystem {
    enum class DirReturnTypeE : uint32_t {
        Directories = 1,
        Files = 2
    };
}
}
