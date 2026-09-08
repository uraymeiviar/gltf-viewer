// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once
#include <nlohmann/json.hpp>
#include <vector>

namespace ux3d::reactor::image {
struct ImageData;

ImageData decodeWebP(const std::vector<uint8_t>& rawData);
nlohmann::json decodeWebPInfo(const std::vector<uint8_t>& rawData);

std::vector<uint8_t> encodeWebP(const ImageData& image, const nlohmann::json& compressionConfig);
}
