#include "gltf/json/AccessorsFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/ConversionResult.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

#include <iostream>

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult AccessorsFromJson::convert(const nlohmann::json& _jsonAccessor)
{
    Accessor* pAccessor = m_gltf.create<Accessor>(getID(_jsonAccessor));
    if (pAccessor == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pAccessor)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;

    if (_jsonAccessor.contains("sparse")) {
        if (core::Failed(convertSparse(_jsonAccessor["sparse"], *pAccessor))) {
            containsErrors = true;
        }
    }

    for (const auto& [key, value] : _jsonAccessor.items()) {
        if (key == "byteOffset") {
            if (value.is_number_unsigned()) {
                pAccessor->setByteOffset(value);
            } else {
                core::logWarning("accessor.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "componentType") {
            if (value.is_number_unsigned()) {
                pAccessor->setComponentType(value);
            } else {
                core::logWarning("accessor.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "normalized") {
            if (value.is_boolean()) {
                pAccessor->setNormalized(value);
            } else {
                core::logWarning("accessor.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "count") {
            if (value.is_number_unsigned()) {
                pAccessor->setCount(value);
            } else {
                core::logWarning("accessor.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "type") {
            if (value.is_string()) {
                AttributeType type;
                if (!convertAttributeType(value, type)) {
                    containsErrors = true;
                } else {
                    pAccessor->setType(type);
                }
            } else {
                core::logWarning("accessor.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "name") {
            if (value.is_string()) {
                pAccessor->setName(value.get<std::string>());
            } else {
                core::logWarning("accessor.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "max") {
            if (value.is_array()) {
                if (!convertExtends(value, pAccessor->getMax())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("accessor.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "min") {
            if (value.is_array()) {
                if (!convertExtends(value, pAccessor->getMin())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("accessor.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "ux3d_gestaltor") {
                        if (extension.is_object()) {
                            if (core::Failed(convertStorageFormat(extension, *pAccessor))) {
                                containsErrors = true;
                            }
                        } else {
                            core::logWarning("accessor.extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
            }
        }
    }

    // fix min max
    // We need to get the number of components to check that min and max have the correct
    // component count to fit the accessor.
    if (_jsonAccessor.contains("min") || _jsonAccessor.contains("max")) {
        if (!_jsonAccessor.contains("type")) {
            containsErrors = true;
        }
        auto typeStr = _jsonAccessor["type"].get<std::string>();
        AttributeType type;
        if (!convertAttributeType(typeStr, type)) {
            containsErrors = true;
        }
        if ((_jsonAccessor.contains("min") && _jsonAccessor["min"].size() != Accessor::getComponentCountFrom(type)) || (_jsonAccessor.contains("max") && _jsonAccessor["max"].size() != Accessor::getComponentCountFrom(type))) {
            core::logError("Accessor min/max are defined but their length does not match with the accessor type");
            return ConversionError::InvalidGltf;
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonAccessor, pAccessor, "accessor")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonAccessor, pAccessor);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }
    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::convertAttributeType(const nlohmann::json::string_t& _jsonType, AttributeType& _type)
{
    if (_jsonType == "SCALAR") {
        _type = AttributeType::Scalar;
    } else if (_jsonType == "VEC2") {
        _type = AttributeType::Vec2;
    } else if (_jsonType == "VEC3") {
        _type = AttributeType::Vec3;
    } else if (_jsonType == "VEC4") {
        _type = AttributeType::Vec4;
    } else if (_jsonType == "MAT2") {
        _type = AttributeType::Mat2;
    } else if (_jsonType == "MAT3") {
        _type = AttributeType::Mat3;
    } else if (_jsonType == "MAT4") {
        _type = AttributeType::Mat4;
    } else {
        core::logWarning("accessor.attribute.type with value '%s' is invalid", _jsonType.c_str());
        return ConversionError::EnumMismatch;
    }

    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::convertExtends(const nlohmann::json::array_t& _jsonExtends, core::Vector<double>& _extends)
{
    for (const auto& jsonNumber : _jsonExtends) {
        if (!jsonNumber.is_number()) {
            core::logWarning("accessor.min/max has element with value '%s' of invalid type", jsonNumber.dump().c_str());
            return ConversionError::InvalidType;
        }

        if (_extends.emplace_back(jsonNumber.get<double>()) == nullptr) {
            return ConversionError::AllocationFailure;
        }
    }
    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::resolve(const nlohmann::json& _jsonAccessor, Accessor& _accessor)
{
    bool containsErrors = false;

    if (_jsonAccessor.contains("sparse")) {
        if (core::Failed(resolveSparse(_jsonAccessor["sparse"], _accessor))) {
            containsErrors = true;
        }
    }

    if (_jsonAccessor.contains("bufferView")) {
        if (core::Succeeded(checkIndex(_jsonAccessor.at("bufferView"), m_pointerLookup.BufferViews.size(), "accessor.bufferView"))) {
            BufferView* pBufferView = m_pointerLookup.BufferViews[_jsonAccessor.at("bufferView").get<uint32_t>()];
            _accessor.setBufferView(*pBufferView);
        } else {
            containsErrors = true;
        }
    } else {
        _accessor.m_internalData.resizeWithValue(_accessor.getSizeInBytes(), 0);
    }
    auto byteOffset = _accessor.getByteOffset();
    if (auto bufferView = _accessor.getBufferView(); bufferView != nullptr) {
        byteOffset += bufferView->getByteOffset();
    }
    auto componentByteLength = Accessor::getByteLengthFrom(_accessor.getComponentType());
    if (byteOffset % componentByteLength != 0) {
        core::logWarning("Resolved accessor byteOffset %u is not a multiple of componentType length %u", byteOffset, componentByteLength);
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::convertStorageFormat(const nlohmann::json& _editorExtension, Accessor& _accessor)
{
    if (_editorExtension.contains("storageFormat") == false) {
        return ConversionResult::Success;
    }

    auto jsonFormat = _editorExtension["storageFormat"];
    if (jsonFormat.is_string() == false) {
        core::logWarning("ux3d_gestaltor.storageFormat must be a string");
        return ConversionError::InvalidType;
    }

    _accessor.setStorageFormat(storageFormatFromString(jsonFormat.get<core::string>()));
    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::convertSparse(const nlohmann::json& _jsonSparse, Accessor& _accessor)
{
    if (_jsonSparse.is_object() == false) {
        return ConversionError::InvalidType;
    }
    if (core::Failed(_accessor.createSparse())) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;

    auto& sparse = _accessor.getSparse();

    if (_jsonSparse.contains("count")) {
        const auto& jsonCount = _jsonSparse["count"];
        if (jsonCount.is_number_unsigned()) {
            sparse->setCount(jsonCount.get<uint32_t>());
        } else {
            core::logWarning("accessor.sparse.count with value '%s' has invalid type", jsonCount.dump().c_str());
            containsErrors = true;
        }
    }
    if (_jsonSparse.contains("indices")) {
        convertSparseIndices(_jsonSparse["indices"], sparse.value());
    }
    if (_jsonSparse.contains("values")) {
        convertSparseValues(_jsonSparse["values"], sparse.value());
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonSparse, &sparse.value(), "accessor.sparse")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonSparse, &sparse.value());
    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }
    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::convertSparseIndices(const nlohmann::json& _jsonIndices, Sparse& _sparse)
{
    if (_jsonIndices.is_object() == false) {
        return ConversionError::InvalidType;
    }

    bool containsErrors = false;
    auto& indices = _sparse.getIndices();

    if (_jsonIndices.contains("byteOffset")) {
        const auto& jsonByteOffset = _jsonIndices["byteOffset"];
        if (jsonByteOffset.is_number_unsigned()) {
            indices.setByteOffset(jsonByteOffset.get<uint32_t>());
        } else {
            core::logWarning("accessor.sparse.indices.byteOffset with value '%s' has invalid type", jsonByteOffset.dump().c_str());
            containsErrors = true;
        }
    }
    if (_jsonIndices.contains("componentType")) {
        const auto& jsonComponentType = _jsonIndices["componentType"];
        if (jsonComponentType.is_number_unsigned()) {
            indices.setComponentType(jsonComponentType.get<ComponentType>());
        } else {
            core::logWarning("accessor.sparse.indices.componentType with value '%s' has invalid type", jsonComponentType.dump().c_str());
            containsErrors = true;
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonIndices, &indices, "accessor.sparse.indices")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonIndices, &indices);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }
    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::convertSparseValues(const nlohmann::json& _jsonValues, Sparse& _sparse)
{
    if (_jsonValues.is_object() == false) {
        return ConversionError::InvalidType;
    }

    bool containsErrors = false;
    auto& values = _sparse.getValues();

    if (_jsonValues.contains("byteOffset")) {
        const auto& jsonByteOffset = _jsonValues["byteOffset"];
        if (jsonByteOffset.is_number_unsigned()) {
            values.setByteOffset(jsonByteOffset.get<uint32_t>());
        } else {
            core::logWarning("accessor.sparse.values.byteOffset with value '%s' has invalid type", jsonByteOffset.dump().c_str());
            containsErrors = true;
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonValues, &values, "accessor.sparse.values")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonValues, &values);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }
    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::resolveSparse(const nlohmann::json& _jsonSparse, Accessor& _accessor)
{
    if (_jsonSparse.is_object() == false) {
        return ConversionError::InvalidType;
    }

    auto& sparse = _accessor.getSparse();

    if (_jsonSparse.contains("indices")) {
        resolveSparseIndices(_jsonSparse["indices"], sparse.value());
    }
    if (_jsonSparse.contains("values")) {
        resolveSparseValues(_jsonSparse["values"], sparse.value());
    }

    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::resolveSparseIndices(const nlohmann::json& _jsonIndices, Sparse& _sparse)
{
    if (_jsonIndices.is_object() == false) {
        return ConversionError::InvalidType;
    }

    auto& indices = _sparse.getIndices();

    if (_jsonIndices.contains("bufferView")) {
        const auto& jsonBufferView = _jsonIndices["bufferView"];
        if (core::Succeeded(checkIndex(jsonBufferView, m_pointerLookup.BufferViews.size(),
                "accessor.sparse.indices.bufferView"))) {
            BufferView* bufferView = m_pointerLookup.BufferViews[jsonBufferView.get<size_t>()];
            indices.setBufferView(*bufferView);
        }
    }

    return ConversionResult::Success;
}

ConversionResult AccessorsFromJson::resolveSparseValues(const nlohmann::json& _jsonValues, Sparse& _sparse)
{
    if (_jsonValues.is_object() == false) {
        return ConversionError::InvalidType;
    }

    auto& values = _sparse.getValues();

    if (_jsonValues.contains("bufferView")) {
        const auto& jsonBufferView = _jsonValues["bufferView"];
        if (core::Succeeded(checkIndex(jsonBufferView, m_pointerLookup.BufferViews.size(),
                "accessor.sparse.values.bufferView"))) {
            BufferView* bufferView = m_pointerLookup.BufferViews[jsonBufferView.get<size_t>()];
            values.setBufferView(*bufferView);
        }
    }

    return ConversionResult::Success;
}
