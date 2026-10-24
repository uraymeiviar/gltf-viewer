// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <cstdint>

namespace ux3d::renderer {

// A1.4: the material classes ASOBO_material_draw_order applies to (docs/ASOBO_EXTENSIONS_PHASE_1.md).
// Decals specify their order by offset first; glass and windshield add the offset to the distance key.
enum class DrawOrderClass : uint8_t {
    None,
    Decal,
    Glass,
};

} // namespace ux3d::renderer
