#include "gltf/json/BufferViewsFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

ux3d::gltf::ConversionResult ux3d::gltf::BufferViewsFromJson::convert(const nlohmann::json& _jsonBufferView)
{
    BufferView* pBufferView = m_gltf.create<BufferView>(getID(_jsonBufferView));
    if (pBufferView == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pBufferView)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonBufferView.items()) {
        if (key == "byteOffset") {
            if (value.is_number_unsigned()) {
                pBufferView->setByteOffset(value);
            } else {
                core::logWarning("bufferView.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "byteLength") {
            if (value.is_number_unsigned()) {
                pBufferView->setByteLength(value);
            } else {
                core::logWarning("bufferView.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "byteStride") {
            if (value.is_number_unsigned()) {
                pBufferView->setByteStride(value);
            } else {
                core::logWarning("bufferView.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "target") {
            if (value.is_number_unsigned()) {
                pBufferView->setTarget(value);
            } else {
                core::logWarning("bufferView.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "name") {
            if (value.is_string()) {
                pBufferView->setName(value.get<std::string>());
            } else {
                core::logWarning("bufferView.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonBufferView, pBufferView, "bufferView")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonBufferView, pBufferView);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ux3d::gltf::ConversionResult ux3d::gltf::BufferViewsFromJson::resolve(const nlohmann::json& _jsonBufferView, BufferView& _bufferView)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonBufferView.items()) {
        if (key == "buffer") {
            if (core::Succeeded(checkIndex(value, m_pointerLookup.Buffers.size(), "bufferView.buffer"))) {
                Buffer* pBuffer = m_pointerLookup.Buffers[value.get<uint32_t>()];
                _bufferView.setBuffer(*pBuffer);
            } else {
                containsErrors = true;
            }
        }
    }

    if (containsErrors) {
        return ConversionError::IndexOutOfRange;
    }

    return ConversionResult::Success;
}
