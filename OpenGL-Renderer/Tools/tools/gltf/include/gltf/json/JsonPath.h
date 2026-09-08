// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/VariantWrapper.h"
#include "nlohmann/json.hpp"
#include <vector>

namespace ux3d::gltf {
using JsonPathEntry = core::VariantWrapper<core::string, uint32_t>;

//usage of core::Vector not recommended:
using JsonPath = std::vector<JsonPathEntry>;

class JsonPathResolver {
public:
    static nlohmann::json* getJsonReference(const JsonPath& _jsonPath, nlohmann::json& _root);
};

}
