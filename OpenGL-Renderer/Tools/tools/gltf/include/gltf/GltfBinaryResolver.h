// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "filesystem/File.h"
#include "nlohmann/json.hpp"
#include "json/ConversionResult.h"

namespace ux3d::gltf {
struct ExportOptions;
class Gltf;
class ResourceCollector;

class GltfBinaryResolver {
public:
    GltfBinaryResolver(ResourceCollector& _resourceCollector, const filesystem::File& _gltfFile, const ExportOptions& _exportOptions)
        : m_gltfFile(_gltfFile)
        , m_resourceCollector(_resourceCollector)
        , m_exportOptions(_exportOptions)
    {
    }

    ConversionResult writeBinaryData(nlohmann::json& _jsonRoot, const Gltf& _gltf);

private:
    filesystem::File m_gltfFile;
    ResourceCollector& m_resourceCollector;
    const ExportOptions& m_exportOptions;
};
}
