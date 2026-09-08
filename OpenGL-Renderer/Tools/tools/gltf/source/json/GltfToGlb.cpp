#include "gltf/json/GltfToGlb.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/GlbBinaryResolver.h"
#include "gltf/Gltf.h"
#include "gltf/ResourceCollector.h"
#include "gltf/json/GlbFileHeader.h"
#include "gltf/json/GltfToJson.h"
#include "nlohmann/json.hpp"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult GltfToGlb::convert(core::Vector<uint8_t>& _outGlbBuffer)
{
    _outGlbBuffer.clear();

    ResourceCollector resourceCollector;
    GltfToJson gltfToJson(m_gltf, resourceCollector, m_exportOptions);
    ConversionResult result = gltfToJson.convert();

    nlohmann::json jsonRoot = gltfToJson.getJson();

    core::Vector<uint8_t> binaryData;
    GlbBinaryResolver binaryResolver(resourceCollector, m_exportOptions);
    binaryResolver.writeBinaryData(jsonRoot, binaryData);

    core::string gltfString = "";
    // nlohmann::json::error_handler_t::replace replaces invalid UTF-8 sequences with U+FFFD (without using exceptions)
    // this sequence can be detected with std::size_t found = gltfString.find("\uFFFD");
    gltfString = jsonRoot.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);

    while (gltfString.length() % 4 != 0) //ensure 4-byte alignment:
    {
        gltfString += " ";
    }

    size_t jsonChunkSize = sizeof(ChunkHeader) + sizeof(char) * gltfString.length();

    while (binaryData.size() % 4 != 0) //ensure 4-byte alignment:
    {
        binaryData.emplace_back(uint8_t(0));
    }

    size_t bufferChunkSize = sizeof(ChunkHeader) + sizeof(uint8_t) * binaryData.size();

    size_t glbSize = sizeof(GlbHeader) + jsonChunkSize;

    if (binaryData.empty() == false) {
        glbSize += bufferChunkSize;
    }

    if (_outGlbBuffer.resize(glbSize) == false) {
        return ConversionError::AllocationFailure;
    }

    core::BinaryStream stream(_outGlbBuffer.data(), _outGlbBuffer.size());
    writeHeader(stream, glbSize);
    writeJsonChunk(stream, gltfString);
    writeBufferChunk(stream, binaryData);

    return result;
}

void GltfToGlb::writeHeader(core::BinaryStream& _stream, const size_t _glbSize)
{
    GlbHeader glbHeader;
    glbHeader.length = uint32_t(_glbSize);
    glbHeader.version = 2;
    glbHeader.magic = 0x46546C67;
    _stream << glbHeader;
}

void GltfToGlb::writeJsonChunk(core::BinaryStream& _stream, const core::string& _gltfString)
{

    ChunkHeader chunkHeader;
    chunkHeader.length = uint32_t(sizeof(char) * _gltfString.length());
    chunkHeader.type = 0x4E4F534A;

    _stream << chunkHeader;
    memcpy(_stream.currentData<uint8_t>(), _gltfString.c_str(), chunkHeader.length);
    _stream.seekg(_stream.tellg() + chunkHeader.length);
}

void GltfToGlb::writeBufferChunk(core::BinaryStream& _stream, const core::Vector<uint8_t>& _binaryData)
{
    if (_binaryData.empty()) {
        return;
    }

    ChunkHeader chunkHeader;
    chunkHeader.length = uint32_t(sizeof(uint8_t) * _binaryData.size());
    chunkHeader.type = 0x004E4942;

    _stream << chunkHeader;

    memcpy(_stream.currentData<uint8_t>(), _binaryData.data(), chunkHeader.length);
    _stream.seekg(_stream.tellg() + chunkHeader.length);
}
