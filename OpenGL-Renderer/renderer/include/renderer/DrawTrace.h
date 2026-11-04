// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <cstdint>
#include <string>

namespace ux3d::renderer {
class Command;
}

// What the draw being issued belongs to, for the draw-call dump (Statistics -> Copy Draw Calls). The renderer fills it
// only while a dump records one frame; the viewer's draw counter reads it.
namespace ux3d::renderer::drawtrace {
inline bool active = false;
inline const Command* command = nullptr;
inline std::string stage;
inline std::string pass;
inline uint32_t viewport = 0;
}
