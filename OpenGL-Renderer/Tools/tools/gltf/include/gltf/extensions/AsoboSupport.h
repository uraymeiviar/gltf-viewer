// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <string_view>
#include <vector>

namespace ux3d::gltf::asobo {

enum class Support { Rendered, Partial, ViewerSetting, Metadata, NotApplicable, NotYetSupported };

struct SupportInfo {
    std::string_view name;
    Support status;
    std::string_view phase;
    std::string_view note;
};

const std::vector<SupportInfo>& supportTable();
SupportInfo supportOf(std::string_view extensionName);
const char* supportLabel(Support status);

} // namespace ux3d::gltf::asobo
