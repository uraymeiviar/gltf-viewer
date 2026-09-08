// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/AccessorsToJson.h"
#include "gltf/Accessor.h"
#include "gltf/BufferView.h"
#include "gltf/IndexLookup.h"
#include "gltf/ResourceCollector.h"
#include "gltf/StorageFormats.h"
#include <gltf/ExportOptions.h>

using namespace ux3d;
using namespace ux3d::gltf;

void AccessorsToJson::convert(const Accessor& _accessor, nlohmann::json& _jsonAccessor, const JsonPath& _jsonPath) const
{
    if (m_exportOptions.conversionUsage == ConversionUsage::ValidGltf) {
        if (_accessor.getBufferView() != nullptr) {
            if (!_accessor.getBufferView()->isValid()) {
                core::logWarning("Accessor references invalid bufferView. Skipping Accessor.");
                return;
            }
        }
    }

    convertBase(_accessor, _jsonAccessor);
    convertBufferView(_accessor, _jsonAccessor);
    convertByteOffset(_accessor, _jsonAccessor);
    convertComponentType(_accessor, _jsonAccessor);
    convertNormalized(_accessor, _jsonAccessor);
    convertCount(_accessor, _jsonAccessor);
    convertType(_accessor, _jsonAccessor);
    convertMax(_accessor, _jsonAccessor);
    convertMin(_accessor, _jsonAccessor);
    convertSparse(_accessor, _jsonAccessor);

    if (m_exportOptions.conversionUsage == ConversionUsage::StateHistory || m_exportOptions.conversionUsage == ConversionUsage::ProjectFile) {
        convertStorageType(_accessor, _jsonAccessor);
    }

    m_resourceCollector.add(_accessor);
}

void AccessorsToJson::convertBufferView(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    if (_accessor.getBufferView() == nullptr) {
        return;
    }

    if (_accessor.getStorageFormat() == StorageFormat::NotStored && m_exportOptions.conversionUsage != ConversionUsage::StateHistory) {
        return;
    }

    size_t index = m_indices[_accessor.getBufferView()];
    _jsonAccessor["bufferView"] = index;
}

void AccessorsToJson::convertByteOffset(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    if (!_accessor.hasByteOffset()) {
        return;
    }

    if (_accessor.getStorageFormat() == StorageFormat::NotStored && m_exportOptions.conversionUsage != ConversionUsage::StateHistory) {
        return;
    }

    _jsonAccessor["byteOffset"] = _accessor.getByteOffset();
}

void AccessorsToJson::convertComponentType(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    if (_accessor.getComponentType() == ComponentType::None) {
        return;
    }

    _jsonAccessor["componentType"] = static_cast<uint32_t>(_accessor.getComponentType());
}

void AccessorsToJson::convertNormalized(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    if (!_accessor.hasNormalized()) {
        return;
    }

    _jsonAccessor["normalized"] = _accessor.isNormalized();
}

void AccessorsToJson::convertCount(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    if (_accessor.getCount() == 0u) {
        return;
    }

    _jsonAccessor["count"] = _accessor.getCount();
}

void AccessorsToJson::convertType(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    core::string typeString = attributeTypeToString(_accessor.getType());

    if (typeString.empty()) {
        core::logWarning("AccessorsToJson: Unknown AttributeType");
    } else {
        _jsonAccessor["type"] = typeString;
    }
}

void AccessorsToJson::convertMax(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    if (_accessor.getMax().empty()) {
        return;
    }

    _jsonAccessor["max"] = _accessor.getMax();
}

void AccessorsToJson::convertMin(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    if (_accessor.getMin().empty()) {
        return;
    }

    _jsonAccessor["min"] = _accessor.getMin();
}

void AccessorsToJson::convertSparse(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    const auto& sparse = _accessor.getSparse();
    if (sparse.has_value() == false) {
        return;
    }

    if (_accessor.getStorageFormat() == StorageFormat::NotStored && m_exportOptions.conversionUsage != ConversionUsage::StateHistory) {
        return;
    }

    if (sparse->isValid() == false) {
        core::logWarning("AccessorsToJson: sparse accessor invalid");
        if (m_exportOptions.conversionUsage == ConversionUsage::ValidGltf) {
            return;
        }
    }

    nlohmann::json jsonSparse = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(sparse.value(), jsonSparse);
    gltfObjectToJson.convertUnknownExtensions(sparse.value(), jsonSparse);

    jsonSparse["count"] = sparse->getCount();

    //Sparse Indices:
    const SparseIndices& indices = sparse->getIndices();

    if (indices.isValid() == false) {
        core::logWarning("AccessorsToJson: sparse indices invalid");
    }

    if (indices.isValid() || m_exportOptions.conversionUsage != ConversionUsage::ValidGltf) {
        nlohmann::json jsonIndices = nlohmann::json::object();

        gltfObjectToJson.convertExtras(indices, jsonIndices);
        gltfObjectToJson.convertUnknownExtensions(indices, jsonIndices);

        jsonIndices["bufferView"] = m_indices[indices.getBufferView()];
        jsonIndices["componentType"] = indices.getComponentType();
        if (indices.getByteOffset().has_value()) {
            jsonIndices["byteOffset"] = indices.getByteOffset().value();
        }
        jsonSparse["indices"] = jsonIndices;
    }

    //Sparse Values:
    const SparseValues& values = sparse->getValues();

    if (values.isValid() == false) {
        core::logWarning("AccessorsToJson: sparse values invalid");
    }

    if (values.isValid() || m_exportOptions.conversionUsage != ConversionUsage::ValidGltf) {
        nlohmann::json jsonValues = nlohmann::json::object();

        gltfObjectToJson.convertExtras(values, jsonValues);
        gltfObjectToJson.convertUnknownExtensions(values, jsonValues);

        jsonValues["bufferView"] = m_indices[values.getBufferView()];
        if (values.getByteOffset().has_value()) {
            jsonValues["byteOffset"] = values.getByteOffset().value();
        }
        jsonSparse["values"] = jsonValues;
    }

    _jsonAccessor["sparse"] = jsonSparse;
}

void AccessorsToJson::convertStorageType(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const
{
    if (_accessor.getStorageFormat() != StorageFormat::Undefined) {
        _jsonAccessor["extensions"]["ux3d_gestaltor"]["storageFormat"] = storageFormatToString(_accessor.getStorageFormat());
    }
}
