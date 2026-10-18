#pragma once

#include <cstdint>
#include <vector>

#include <QString>

#include "core/Flag.h"

namespace ux3d::editor {
enum class OpenFileFilter : uint32_t {
    None = 0,
    GltfAndGlbFiles = 1u << 0u,
    GltfFiles = 1u << 1u,
    GlbFiles = 1u << 2u,
    AllFiles = 1u << 3u,
    ProjectFiles = 1u << 4u,
    GltfAndGlbAndGestaltFiles = 1u << 5u,
    AllModelFiles = 1u << 6u
};

using OpenFileFilters = core::Flag<OpenFileFilter>;

QString openFileFiltersToString(const OpenFileFilters& _filters);
QString openFileFiltersToString(
    OpenFileFilter _a,
    OpenFileFilter _b = OpenFileFilter::None,
    OpenFileFilter _c = OpenFileFilter::None);

enum class SaveFileFilter : uint32_t {
    None = 0,
    Gltf = 1u << 0u,
    Glb = 1u << 1u
};

using SaveFileFilters = core::Flag<SaveFileFilter>;

SaveFileFilter saveFileFilterFromString(const QString& _filter);
QString saveFileFilterToString(const SaveFileFilter& _filter);
QString saveFileFiltersToString(const std::vector<SaveFileFilter>& _filters);
}
