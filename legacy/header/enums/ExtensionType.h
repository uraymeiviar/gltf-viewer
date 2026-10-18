#pragma once

#include <cstdint>

namespace ux3d::editor {
enum class ExtensionType : uint32_t {
    None,
    KHR,
    EXT,
    Known,
    Unknown
};
}
