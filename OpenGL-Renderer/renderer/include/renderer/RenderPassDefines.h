// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include <cstdint>

namespace ux3d::renderer {
enum RenderPassType : uint32_t {
    Outline,
    Opaque2,
    Grid2,
    Transparent2,
    TransmissiveBlit,
    Opaque, //2
    Grid, //2
    Transmissive,
    Transparent, //2
    Gizmos,
    PostGizmos,
    Foreground,
    Picking,
    Postprocessing,
    FullscreenCopy,

    MaxRenderPasses = 16
};
}
