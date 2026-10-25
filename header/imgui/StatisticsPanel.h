// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include <cstdint>
#include <string>

namespace ux3d::imgui {

// A2.1: memory and state of the renderer buffers that are not part of the glTF asset.
// The decal buffer is shared by every viewport, so its bytes are reported once.
struct RenderBufferStatistics {
    bool decalBufferActive = false;
    int decalBufferWidth = 0;
    int decalBufferHeight = 0;
    uint64_t decalBufferBytes = 0;
    uint64_t decalBufferAllocations = 0;
    uint64_t decalBufferResizes = 0;
    uint64_t decalBufferReleases = 0;

    bool operator==(const RenderBufferStatistics&) const = default;
};

class StatisticsPanel {
public:
    static void render(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath,
                       const RenderBufferStatistics& buffers = {});

    // Drops the cached snapshot after a load, reload, save, or explicit refresh.
    static void invalidate();
};

} // namespace ux3d::imgui
