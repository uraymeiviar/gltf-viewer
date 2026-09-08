#pragma once

#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include <nlohmann/json.hpp>

namespace ux3d::operations::encoder {

// params["targetCompressionLevel"] : uint32_t
// params["sequentialEncoding"] : bool
// params["quantizationBits"]["position"] : uint32_t
// params["quantizationBits"]["normal"] : uint32_t
// params["quantizationBits"]["uv"] : uint32_t
// params["quantizationBits"]["color"] : uint32_t
// params["quantizationBits"]["generic"] : uint32_t

/// Encodes the given primitive with the current settings.
/// The primitive itself is modified during the process,
/// i.e. the compressed pirmitive is not a new glTF primitive.

void draco(const nlohmann::json& params, nlohmann::json& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback);
}
