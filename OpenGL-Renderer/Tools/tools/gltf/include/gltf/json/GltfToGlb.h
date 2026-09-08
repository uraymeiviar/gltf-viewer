// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/BinaryStream.h"
#include "core/String.h"
#include "core/Vector.h"
#include "gltf/json/ConversionResult.h"

namespace ux3d::gltf {
struct ExportOptions;
class Gltf;
class ChildOfRoot;

class GltfToGlb {
public:
    explicit GltfToGlb(const Gltf& _gltf, const ExportOptions& _exportOptions)
        : m_gltf(_gltf)
        , m_exportOptions(_exportOptions)
    {
    }

    ConversionResult convert(core::Vector<uint8_t>& _outGlbBuffer);

private:
    void writeHeader(core::BinaryStream& _stream, size_t _glbSize);
    void writeJsonChunk(core::BinaryStream& _stream, const core::string& _gltfString);
    void writeBufferChunk(core::BinaryStream& _stream, const core::Vector<uint8_t>& _binaryData);

    const Gltf& m_gltf;
    const ExportOptions& m_exportOptions;
};
}
