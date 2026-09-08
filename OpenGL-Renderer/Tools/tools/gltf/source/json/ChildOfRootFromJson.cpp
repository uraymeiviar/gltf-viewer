// Copyright © 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/ChildOfRootFromJson.h"
#include "core/Logger.h"

using namespace ux3d;
using namespace ux3d::gltf;

std::optional<uint32_t> ChildOfRootFromJson::getID(const nlohmann::json& _jsonObject) const
{
    if (_jsonObject.contains("extensions") == false) {
        return {};
    }

    auto extensions = _jsonObject["extensions"];
    if (extensions.is_object() == false) {
        core::logWarning("'extensions' must be an object! "
                         "JSON object: '%s'",
            _jsonObject.dump().c_str());
        return {};
    }

    if (extensions.contains("ux3d_gestaltor") == false) {
        return {};
    }

    auto gestaltorExt = extensions["ux3d_gestaltor"];
    if (gestaltorExt.is_object() == false) {
        core::logWarning("'ux3d_gestaltor' extension must be an object! "
                         "JSON object: '%s'",
            _jsonObject.dump().c_str());
        return {};
    }

    if (gestaltorExt.contains("id") == false) {
        return {};
    }

    auto id = gestaltorExt["id"];
    if (id.is_number_integer() == false || id.is_number_unsigned() == false) {
        core::logWarning("'id' of 'ux3d_gestaltor' extension must be an unsigned integer! "
                         "JSON object: '%s'",
            _jsonObject.dump().c_str());
        return {};
    }

    return id.get<uint32_t>();
}

ConversionResult ChildOfRootFromJson::checkVector(const nlohmann::json& _jsonObject, size_t _requiredSize, const std::string& _propertyName) const
{
    if (ConversionResult arrayCheck = checkNumberArray(_jsonObject, _propertyName);
        core::Failed(arrayCheck)) {
        return arrayCheck;
    }

    if (_jsonObject.size() != _requiredSize) {
        core::logWarning("%s has incorrect element count (%u != %u)", _propertyName.c_str(), _jsonObject.size(), _requiredSize);
        return ConversionError::InvalidVectorLength;
    }

    return ConversionResult::Success;
}

ConversionResult ChildOfRootFromJson::checkNumberArray(const nlohmann::json& _jsonObject, const std::string& _propertyName) const
{
    if (_jsonObject.is_array() == false) {
        core::logWarning("%s with value '%s' has invalid type", _propertyName.c_str(), _jsonObject.dump().c_str());
        return ConversionError::InvalidType;
    }

    bool error = false;
    for (const auto& element : _jsonObject) {
        if (element.is_number() == false) {
            core::logWarning("%s has element with value '%s' of invalid type", _propertyName.c_str(), element.dump().c_str());
            error = true;
        }
    }

    return error ? ConversionError::InvalidType : ConversionResult::Success;
}

ConversionResult ChildOfRootFromJson::checkIndex(const nlohmann::json& _jsonObject, size_t _arraySize, const std::string& _propertyName) const
{
    if (_jsonObject.is_number_unsigned() == false) {
        core::logWarning("%s with value '%s' has invalid type", _propertyName.c_str(), _jsonObject.dump().c_str());
        return ConversionError::InvalidType;
    }

    uint32_t index = _jsonObject.get<uint32_t>();

    if (index >= _arraySize) {
        core::logWarning("%s is out of range (%u >= %u)", _propertyName.c_str(), index, _arraySize);
        return ConversionError::IndexOutOfRange;
    }

    return ConversionResult::Success;
}
