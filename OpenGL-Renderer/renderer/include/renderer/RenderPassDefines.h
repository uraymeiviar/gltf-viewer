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
    // A2.1: the decal prepass runs before the forward opaque pass so the forward shader
    // can read the receiver depth and normal (A2.2). Nothing is drawn into it yet.
    DecalPrepass,
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
