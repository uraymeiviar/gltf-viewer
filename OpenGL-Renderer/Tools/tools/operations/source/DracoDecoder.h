#pragma once

#include <nlohmann/json.hpp>

namespace ux3d::operations {
class Library;
}
namespace ux3d::datastorage {
class DataStorage;
}
namespace ux3d::operations::decoder {

nlohmann::json draco(const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback);
}
