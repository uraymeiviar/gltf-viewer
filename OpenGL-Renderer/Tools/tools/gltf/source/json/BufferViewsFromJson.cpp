#include "gltf/json/BufferViewsFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

namespace {
bool meshoptSkipped(const std::string& _name)
{
    using ux3d::gltf::GltfObjectFromJson;
    if (ux3d::gltf::meshopt::decodingDisabled()) {
        return true;
    }
    return GltfObjectFromJson::g_importOptions.has_value() && GltfObjectFromJson::g_importOptions->skipExtensions.has_value()
        && GltfObjectFromJson::g_importOptions->skipExtensions->count(_name) != 0;
}

// bufferView.extensions.EXT_meshopt_compression / KHR_meshopt_compression; false on a malformed object (the view is
// then left without the extension and the loader reports the decode failure later only if it is required).
bool parseMeshopt(const std::string& _name, const nlohmann::json& _json, ux3d::gltf::MeshoptCompression& _out)
{
    using Mode = ux3d::gltf::MeshoptCompression::Mode;
    using Filter = ux3d::gltf::MeshoptCompression::Filter;
    if (!_json.is_object()) {
        return false;
    }
    _out.extension = _name;
    auto unsignedValue = [&](const char* _key, uint32_t& _value, bool _required) {
        auto it = _json.find(_key);
        if (it == _json.end()) {
            return !_required;
        }
        if (!it->is_number_unsigned() || it->get<uint64_t>() > 0xffffffffull) {
            return false;
        }
        _value = it->get<uint32_t>();
        return true;
    };
    uint32_t buffer = 0;
    if (!unsignedValue("buffer", buffer, true) || !unsignedValue("byteOffset", _out.byteOffset, false)
        || !unsignedValue("byteLength", _out.byteLength, true) || !unsignedValue("byteStride", _out.byteStride, true)
        || !unsignedValue("count", _out.count, true)) {
        return false;
    }
    _out.bufferIndex = buffer;
    auto mode = _json.find("mode");
    if (mode == _json.end() || !mode->is_string()) {
        return false;
    }
    const std::string modeText = mode->get<std::string>();
    if (modeText == "ATTRIBUTES") {
        _out.mode = Mode::Attributes;
    } else if (modeText == "TRIANGLES") {
        _out.mode = Mode::Triangles;
    } else if (modeText == "INDICES") {
        _out.mode = Mode::Indices;
    } else {
        return false;
    }
    _out.filter = Filter::None;
    if (auto filter = _json.find("filter"); filter != _json.end()) {
        if (!filter->is_string()) {
            return false;
        }
        const std::string filterText = filter->get<std::string>();
        if (filterText == "NONE") {
            _out.filter = Filter::None;
        } else if (filterText == "OCTAHEDRAL") {
            _out.filter = Filter::Octahedral;
        } else if (filterText == "QUATERNION") {
            _out.filter = Filter::Quaternion;
        } else if (filterText == "EXPONENTIAL") {
            _out.filter = Filter::Exponential;
        } else if (filterText == "COLOR") {
            _out.filter = Filter::Color;
        } else {
            return false;
        }
    }
    return true;
}
}

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
        } else if (key == "extensions" && value.is_object()) {
            for (const auto& [extensionName, extension] : value.items()) {
                if (!meshopt::isMeshoptExtension(extensionName) || meshoptSkipped(extensionName)) {
                    continue;
                }
                MeshoptCompression compression;
                if (parseMeshopt(extensionName, extension, compression)) {
                    pBufferView->setMeshoptCompression(compression);
                } else {
                    core::logError("bufferView.extensions.%s with value '%s' is invalid", extensionName.c_str(), extension.dump().c_str());
                    containsErrors = true;
                }
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

    if (auto& compression = _bufferView.getMeshoptCompression(); compression.has_value()) {
        if (compression->bufferIndex >= 0 && size_t(compression->bufferIndex) < m_pointerLookup.Buffers.size()) {
            compression->buffer = m_pointerLookup.Buffers[size_t(compression->bufferIndex)];
        } else {
            core::logWarning("bufferView.extensions.%s.buffer is out of range (%lld >= %u)", compression->extension.c_str(),
                static_cast<long long>(compression->bufferIndex), static_cast<uint32_t>(m_pointerLookup.Buffers.size()));
            containsErrors = true;
        }
    }

    if (containsErrors) {
        return ConversionError::IndexOutOfRange;
    }

    return ConversionResult::Success;
}
