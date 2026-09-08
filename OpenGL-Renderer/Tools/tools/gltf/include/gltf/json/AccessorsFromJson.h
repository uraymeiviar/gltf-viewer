#pragma once

#include "ChildOfRootFromJson.h"
#include "core/Vector.h"
#include "gltf/AttributeType.h"

namespace ux3d::gltf {
class Accessor;
class Sparse;

class AccessorsFromJson : public ChildOfRootFromJson {
public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonAccessor) override;
    ConversionResult resolve(const nlohmann::json& _jsonAccessor, Accessor& _accessor);

private:
    ConversionResult convertAttributeType(const nlohmann::json::string_t& _jsonType, AttributeType& _type);
    ConversionResult convertExtends(const nlohmann::json::array_t& _jsonExtends, core::Vector<double>& _extends);
    ConversionResult convertStorageFormat(const nlohmann::json& _editorExtension, Accessor& _accessor);
    ConversionResult convertSparse(const nlohmann::json& _jsonSparse, Accessor& _accessor);
    ConversionResult convertSparseIndices(const nlohmann::json& _jsonIndices, Sparse& _sparse);
    ConversionResult convertSparseValues(const nlohmann::json& _jsonValues, Sparse& _sparse);

    ConversionResult resolveSparse(const nlohmann::json& _jsonSparse, Accessor& _accessor);
    ConversionResult resolveSparseIndices(const nlohmann::json& _jsonIndices, Sparse& _sparse);
    ConversionResult resolveSparseValues(const nlohmann::json& _jsonValues, Sparse& _sparse);
};
}
