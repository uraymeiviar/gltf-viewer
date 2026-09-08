#pragma once

#include <nlohmann/json.hpp>
#include <vector>

namespace ux3d::operations {
class Library;
}
namespace ux3d::datastorage {
class DataStorage;
}
namespace ux3d::operations::meshQuantization {

nlohmann::json resolve(nlohmann::json::json_pointer accessorPath, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback);
template <typename T, typename F>
std::vector<uint8_t> convert(std::vector<uint8_t> data, F* conversion)
{
    std::vector<uint8_t> result;
    T value = 0;
    for (size_t i = 0; i < data.size(); ++i) {
        value = value | data[i] << 8 * (i % sizeof(T));
        if (i % sizeof(T) == sizeof(T) - 1) {
            float converted = (*conversion)(value);

            // save float in byte vector
            for (size_t k = 0; k < sizeof(float); ++k) {
                result.push_back(((uint8_t*)&converted)[k]);
            }

            value = 0;
        }
    }

    return result;
}

}
