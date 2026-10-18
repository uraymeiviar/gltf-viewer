// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <imgui.h>
#include "imgui/IconsMaterialDesignIcons.h"

namespace ux3d::imgui {

void applyFssTheme();
void loadFssFonts(ImGuiIO& io);

ImFont* getUiFont();
ImFont* getMonoFont();

} // namespace ux3d::imgui
