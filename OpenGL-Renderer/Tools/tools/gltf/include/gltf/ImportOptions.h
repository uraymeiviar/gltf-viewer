#pragma once

#include "filesystem/Dir.h"
#include "gltf/ExportOptions.h"
#include <optional>
#include <string>
#include <unordered_set>

namespace ux3d::gltf {

struct ImportOptions {
    ConversionUsage conversionUsage = ConversionUsage::ValidGltf;
    std::optional<filesystem::Dir> imageDir = std::nullopt;
    std::optional<std::unordered_set<std::string>> skipExtensions = std::nullopt;
    // Keep material/texture metadata, but intentionally omit image payloads.
    bool loadImages = true;
};
}
