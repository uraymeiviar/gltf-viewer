#pragma once

#include "core/String.h"
#include "gltf/GltfType.h"
#include "gltf/json/GltfObjectToJson.h"
#include "nlohmann/json.hpp"

namespace ux3d::gltf {
struct ExportOptions;
class ChildOfRoot;
class IndexLookup;

class ChildOfRootToJson : public GltfObjectToJson {
public:
    explicit ChildOfRootToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions)
        : GltfObjectToJson(_exportOptions)
        , m_indices(_indices)
    {
    }

protected:
    void convertBase(const ChildOfRoot& _gltfObject, nlohmann::json& _jsonObject) const;
    const IndexLookup& m_indices;

private:
    void checkValidity(const ChildOfRoot& _gltfObject) const;
    void convertName(const ChildOfRoot& _gltfObject, nlohmann::json& _jsonObject) const;
    void convertId(const ChildOfRoot& _gltfObject, nlohmann::json& _jsonObject) const;
};
}
