#pragma once

#include "ConversionResult.h"
#include "core/BinaryStreamRead.h"
#include "core/String.h"
#include "gltf/ImportOptions.h"
#include "gltf/json/GlbFileHeader.h"

namespace ux3d::gltf {
class Gltf;

class GltfFromBinary {
public:
    explicit GltfFromBinary(Gltf& _outGltf, const ImportOptions& _importOptions)
        : m_gltf(_outGltf)
        , m_importOptions(_importOptions)
    {
    }

    ConversionResult convert(const core::MemoryRange<uint8_t>& _data);

    uint32_t getVersion() const { return m_glbHeader.version; }
    uint32_t getLength() const { return m_glbHeader.length; }

private:
    ConversionResult parseHeader(core::BinaryStreamRead& _stream);
    ConversionResult extractJsonString(core::BinaryStreamRead& _stream, core::string& _outJson);
    ConversionResult createObjects(const core::string& _jsonChunk);
    ConversionResult setBufferData(core::BinaryStreamRead& _stream);

    Gltf& m_gltf;

    GlbHeader m_glbHeader;
    ChunkHeader m_jsonChunkHeader;
    ChunkHeader m_bufferChunkHeader;

    const ImportOptions m_importOptions;
};
}
