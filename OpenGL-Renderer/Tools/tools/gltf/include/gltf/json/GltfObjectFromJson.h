#pragma once
#include "core/String.h"
#include "gltf/ImportOptions.h"
#include "gltf/json/ConversionResult.h"
#include "nlohmann/json.hpp"
#include <optional>

namespace ux3d::gltf {
class GltfObject;

class GltfObjectFromJson {
public:
    static ConversionResult convertUnknownExtensions(const nlohmann::json& _json, GltfObject* _gltfObject, const core::string& _logMessage);
    static void convertExtras(const nlohmann::json& _json, GltfObject* _gltfObject);
    static inline std::optional<ImportOptions> g_importOptions; // this hack is needed because the other methods are statically called
};
}
