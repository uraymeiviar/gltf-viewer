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
    // Extensions switched off in File > Settings > glTF Extensions (GltfExtensionSettings::loaderDisabled): the file
    // is read as if they were absent (removed from extensionsUsed and from every "extensions" object, so the core
    // fallbacks are used), and a file that requires one (or declares one whose ExtensionSupport::refuseWhenOff is
    // set) is not loaded. GltfFromJson::convert applies it.
    std::unordered_set<std::string> disabledExtensions;
    // Keep material/texture metadata, but intentionally omit image payloads.
    bool loadImages = true;
};
}
