// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include <cstdint>

namespace ux3d::renderer {
enum RenderPassType : uint32_t {
    Outline,
    Opaque2,
    Grid2,
    // A2.1: the decal prepass runs before the forward opaque pass so the forward shader
    // can read the receiver depth and normal (A2.2).
    DecalPrepass,
    // A2.2: decals accumulate into the shared buffer after the prepass and before the
    // forward opaque pass, which then reads the result.
    DecalAccumulation,
    Opaque, //2
    Grid, //2
    Transparent2,
    TransmissiveBlit,
    Transmissive,
    Transparent, //2
    Gizmos,
    PostGizmos,
    Foreground,
    Picking,
    Postprocessing,
    FullscreenCopy,

    MaxRenderPasses = 17
};
}
