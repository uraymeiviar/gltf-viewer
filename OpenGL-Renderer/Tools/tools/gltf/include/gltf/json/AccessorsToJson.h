// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Accessor;
class ResourceCollector;

class AccessorsToJson : ChildOfRootToJson {
public:
    explicit AccessorsToJson(const IndexLookup& _indexLookup, ResourceCollector& _resourceCollector, const ExportOptions& _exportOptions)
        : ChildOfRootToJson(_indexLookup, _exportOptions)
        , m_resourceCollector(_resourceCollector)
    {
    }

    void convert(const Accessor& _accessor, nlohmann::json& _jsonAccessor, const JsonPath& _jsonPath) const;

private:
    void convertBufferView(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    void convertByteOffset(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    void convertComponentType(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    void convertNormalized(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    void convertCount(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    void convertType(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    void convertMax(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    void convertMin(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    void convertSparse(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;

    void convertStorageType(const Accessor& _accessor, nlohmann::json& _jsonAccessor) const;
    ResourceCollector& m_resourceCollector;
};
}
