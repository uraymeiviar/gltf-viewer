// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

namespace ux3d::imgui {

// A3.5: screen-space ambient occlusion of one viewport (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.5). It attenuates
// only the sky light (IBL diffuse, specular, sheen, clearcoat), never the sun, punctual lights, emissive or the
// background. User-selectable per viewport; Auto quality never changes it. Off by default in this commit, so
// every existing image stays byte-identical (the maintainer may make it default on later).
enum class SsaoQuality : int {
    Low = 0,    // 2 slices x 4 steps per side
    Medium = 1, // 3 slices x 6 steps
    High = 2,   // 4 slices x 8 steps
};

struct SsaoSettings {
    bool enabled = false;
    float radius = 0.5f;     // world (scene) units: occluders farther away than this do not count
    float intensity = 1.0f;  // exponent on the visibility (1 = the GTAO value)
    SsaoQuality quality = SsaoQuality::Medium;
    bool halfResolution = false; // compute at half resolution, depth-aware upsample

    static int sliceCount(SsaoQuality q) { return q == SsaoQuality::Low ? 2 : q == SsaoQuality::High ? 4 : 3; }
    static int stepCount(SsaoQuality q) { return q == SsaoQuality::Low ? 4 : q == SsaoQuality::High ? 8 : 6; }

    bool operator==(const SsaoSettings&) const = default;
};

} // namespace ux3d::imgui
