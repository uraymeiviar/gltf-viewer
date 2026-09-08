#pragma once

#include "ConversionResult.h"
#include "nlohmann/json.hpp"
#include <optional>

namespace ux3d::gltf {
class Gltf;
struct PointerLookup;

class ChildOfRootFromJson {
protected:
    Gltf& m_gltf;
    PointerLookup& m_pointerLookup;

public:
    explicit ChildOfRootFromJson(Gltf& _gltf, PointerLookup& _pointerLookup)
        : m_gltf(_gltf)
        , m_pointerLookup(_pointerLookup)
    {
    }

    virtual ConversionResult convert(const nlohmann::json& _JsonObject) = 0;

protected:
    std::optional<uint32_t> getID(const nlohmann::json& _jsonObject) const;

    ConversionResult checkVector(const nlohmann::json& _jsonObject, size_t _requiredSize, const std::string& _propertyName) const;
    ConversionResult checkNumberArray(const nlohmann::json& _jsonObject, const std::string& _propertyName) const;
    ConversionResult checkIndex(const nlohmann::json& _jsonObject, size_t _arraySize, const std::string& _propertyName) const;
};
}
