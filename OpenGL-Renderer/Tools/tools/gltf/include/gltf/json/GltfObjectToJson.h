// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "nlohmann/json.hpp"

namespace ux3d::gltf {
struct ExportOptions;
class GltfObject;

class GltfObjectToJson {
public:
    GltfObjectToJson(const ExportOptions& _exportOptions)
        : m_exportOptions(_exportOptions)
    {
    }

    void convertUnknownExtensions(const GltfObject& _gltfObject, nlohmann::json& _jsonObject) const;
    void convertExtras(const GltfObject& _gltfObject, nlohmann::json& _jsonObject) const;
    void convertExtras(const core::string& _extras, nlohmann::json& _jsonObject) const;

protected:
    const ExportOptions& m_exportOptions;
};
}
