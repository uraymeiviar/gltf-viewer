#pragma once

#include "core/Flag.h"
#include <stdint.h>

namespace ux3d::model {
enum class ReportBehaviorFlag : uint32_t {
    None = 0u,
    StateChange = 1u,
    Present = 2u,
    All = StateChange | Present
};

using ReportBehavior = core::Flag<ReportBehaviorFlag>;
}
