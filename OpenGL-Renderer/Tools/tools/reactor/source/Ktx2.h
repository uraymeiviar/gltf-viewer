// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once
#include <nlohmann/json.hpp>
#include <vector>

struct ktxTexture2;

namespace ux3d::reactor::image {
struct ImageData;

ImageData decodeKTX(const std::vector<uint8_t>& rawData);
nlohmann::json decodeKTXInfo(const std::vector<uint8_t>& rawData);

std::vector<uint8_t> encodeKTX(const ImageData& image, const nlohmann::json& _basisUCompressionConfig);

}
