// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/ResourcesPanel.h"

namespace ux3d::imgui {

class ElementInspector {
public:
    static void render(SelectionState& selection, ux3d::gltf::Gltf& gltfModel, bool& isSceneDirty);
};

} // namespace ux3d::imgui
