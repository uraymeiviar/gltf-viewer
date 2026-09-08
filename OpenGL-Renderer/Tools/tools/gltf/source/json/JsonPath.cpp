#include "gltf/json/JsonPath.h"
#include "core/Logger.h"

using namespace ux3d;
using namespace ux3d::gltf;

nlohmann::json* JsonPathResolver::getJsonReference(const JsonPath& _jsonPath, nlohmann::json& _root)
{

    if (_jsonPath.empty()) {
        core::logWarning("Json path invalid");
        return nullptr;
    }

    nlohmann::json* currentJsonElement = &_root;

    for (JsonPathEntry segment : _jsonPath) {
        if (segment.hasType<uint32_t>()) {
            uint32_t arrayIndex = segment.unsafeGet<uint32_t>();

            if (!currentJsonElement->is_array()) {
                core::logWarning("Json path invalid");
                return nullptr;
            }
            if (arrayIndex >= currentJsonElement->size()) {
                core::logWarning("Json path invalid");
                return nullptr;
            }

            currentJsonElement = &currentJsonElement->at(arrayIndex);
        } else {
            core::string name = segment.unsafeGet<core::string>();

            if (currentJsonElement->contains(name)) {
                currentJsonElement = &currentJsonElement->at(name);
            } else {
                core::logWarning("Json path invalid");
                return nullptr;
            }
        }
    }

    return currentJsonElement;
}
