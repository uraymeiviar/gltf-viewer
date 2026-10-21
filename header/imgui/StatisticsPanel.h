// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include <string>

namespace ux3d::imgui {

class StatisticsPanel {
public:
    static void render(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath);

    // Drops the cached snapshot after a load, reload, save, or explicit refresh.
    static void invalidate();
};

} // namespace ux3d::imgui
