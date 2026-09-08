// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once
#include "core/Vector.h"
#include "nlohmann/json.hpp"
#include <cstdint>

namespace ux3d::gltf {
struct ExportOptions;
class ResourceCollector;

class GlbBinaryResolver {
public:
    GlbBinaryResolver(ResourceCollector& _resourceCollector, const ExportOptions& _exportOptions)
        : m_resourceCollector(_resourceCollector)
        , m_exportOptions(_exportOptions)
    {
    }
    void writeBinaryData(nlohmann::json& _jsonRoot, core::Vector<uint8_t>& _mainBufferData);

private:
    void clearJsonBuffers(nlohmann::json& _jsonRoot);
    ResourceCollector& m_resourceCollector;
    const ExportOptions& m_exportOptions;
};
}
