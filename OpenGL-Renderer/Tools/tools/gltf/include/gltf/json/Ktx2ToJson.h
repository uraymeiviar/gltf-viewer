// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "nlohmann/json.hpp"

namespace ux3d::gltf {
class Ktx2;

class Ktx2ToJson {
public:
    void convert(const Ktx2& _ktx2, nlohmann::json& _jsonKtx2);

private:
    void convertBasicProperties(const Ktx2& _ktx2, nlohmann::json& _jsonKtx2) const;
    void convertLevels(const Ktx2& _ktx2, nlohmann::json& _jsonKtx2) const;
    void convertMetadata(const Ktx2& _ktx2, nlohmann::json& _jsonKtx2) const;
};
}
