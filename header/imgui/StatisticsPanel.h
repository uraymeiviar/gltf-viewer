// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include <string>

namespace ux3d::imgui {

class StatisticsPanel {
public:
    static void render(const ux3d::gltf::Gltf& gltfModel, const std::string& currentFilePath);
};

} // namespace ux3d::imgui
