// Copyright © 2022 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vector>

#include "datastorage/DataStorage.h"

namespace ux3d::reactor::data {
uint64_t getDecodedDracoSize(const nlohmann::json& state, const datastorage::DataStorage& datastorage);
uint64_t getDataSize(const nlohmann::json& state, const datastorage::DataStorage& datastorage);

}
