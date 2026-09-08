#pragma once

#include "GltfPropertyPointer.h"
#include "enums/GltfPropertyTypes.h"
#include <cstdint>

namespace ux3d::editor {
enum class SelectionSource : uint32_t {
    None,
    Hierarchy,
    Renderview,
    HierarchyInspector,
    ContentInspector,
    Content
};

struct SelectionInfo {
    SelectionSource source = SelectionSource::None;
    GltfPropertyType type = GltfPropertyType::None;
    GltfPropertyPointer pointer;
};
}
