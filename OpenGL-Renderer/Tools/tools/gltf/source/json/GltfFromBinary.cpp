#include "gltf/json/GltfFromBinary.h"
#include "core/BinaryStream.h"
#include "core/Logger.h"
#include "core/Slice.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfFromJson.h"

ux3d::gltf::ConversionResult ux3d::gltf::GltfFromBinary::convert(const core::MemoryRange<uint8_t>& _data)
{
    ConversionResult result;

    if (_data.size() == 0) {
        return ConversionError::FileNotFound;
    }

    core::BinaryStreamRead binaryStream(_data.data(), _data.size());

    result = parseHeader(binaryStream);

    if (result != ConversionResult::Success) {
        //if we have a more critical error or we expected valid gltf, we have to abort the process
        if (result != ConversionError::SuccessWithErrors || (result == ConversionError::SuccessWithErrors && m_importOptions.conversionUsage == ConversionUsage::ValidGltf)) {
            core::logError("Could not parse binary header");
            return result;
        }
    }

    core::string jsonChunk;

    result = extractJsonString(binaryStream, jsonChunk);
    if (result != ConversionResult::Success) {
        if (result != ConversionError::SuccessWithErrors || (result == ConversionError::SuccessWithErrors && m_importOptions.conversionUsage == ConversionUsage::ValidGltf)) {
            core::logError("Could not extract json string from binary file");
            return result;
        }
    }

    result = createObjects(jsonChunk);
    if (result != ConversionResult::Success) {
        if (result != ConversionError::SuccessWithErrors) {
            core::logError("Could not create valid glTF from json string");
            return result;
        }

        if (m_importOptions.conversionUsage == ConversionUsage::ValidGltf) {
            core::logWarning("Could not create valid glTF from json string");
        }
    }

    result = setBufferData(binaryStream);

    if (result != ConversionResult::Success) {
        core::logError("Could not extract buffers from file");
    }

    return result;
}

ux3d::gltf::ConversionResult ux3d::gltf::GltfFromBinary::parseHeader(core::BinaryStreamRead& _stream)
{
    if (_stream.size() < sizeof(GlbHeader)) // Header == 12 Byte
    {
        return ConversionError::UnsupportedFileFormat;
    }

    _stream.seekg(0);
    _stream >> m_glbHeader;

    core::string typeString(reinterpret_cast<char*>(&m_glbHeader.magic), 4);
    if (typeString != "glTF") {
        return ConversionError::UnsupportedFileFormat;
    }

    if (getLength() != _stream.size()) {
        return ConversionError::InvalidGlbFileLength;
    }

    return ConversionResult::Success;
}

ux3d::gltf::ConversionResult ux3d::gltf::GltfFromBinary::extractJsonString(core::BinaryStreamRead& _stream, core::string& _outJson)
{
    _stream.seekg(sizeof(GlbHeader)); //start reading after 12 Byte header

    if (_stream.size() < _stream.tellg() + sizeof(ChunkHeader)) // ChunkHeader == 8 Byte
    {
        return ConversionError::InvalidGlbFileLength;
    }

    _stream >> m_jsonChunkHeader;

    if (_stream.tellg() + m_jsonChunkHeader.length > _stream.size()) {
        return ConversionError::InvalidGlbFileLength;
    }

    core::string typeString(reinterpret_cast<char*>(&m_jsonChunkHeader.type), 4);
    if (typeString != "JSON") {
        return ConversionError::UnsupportedFileFormat;
    }

    const char* jsonString = _stream.currentData<const char>();
    _outJson = core::string(jsonString, m_jsonChunkHeader.length);

    return ConversionResult::Success;
}

ux3d::gltf::ConversionResult ux3d::gltf::GltfFromBinary::createObjects(const core::string& _jsonChunk)
{
    GltfFromJson converter(m_gltf);
    return converter.convert(_jsonChunk);
}

ux3d::gltf::ConversionResult ux3d::gltf::GltfFromBinary::setBufferData(core::BinaryStreamRead& _stream)
{
    auto& bufferList = m_gltf.getBuffers();

    if (bufferList.empty()) {
        return ConversionResult::Success; // buffers are not required
    }

    size_t binaryBufferOffset = sizeof(GlbHeader) + sizeof(ChunkHeader) + m_jsonChunkHeader.length;

    if (_stream.size() < binaryBufferOffset + sizeof(ChunkHeader)) // ChunkHeader == 8 Byte
    {
        return ConversionError::InvalidGlbFileLength;
    }

    _stream.seekg(binaryBufferOffset);

    _stream >> m_bufferChunkHeader;

    core::string typeString(reinterpret_cast<char*>(&m_bufferChunkHeader.type), 3);
    if (typeString != "BIN") {
        return ConversionError::UnsupportedFileFormat;
    }

    if (_stream.tellg() + m_bufferChunkHeader.length > _stream.size()) {
        return ConversionError::InvalidGlbFileLength;
    }

    size_t glbBufferSize = _stream.size() - (binaryBufferOffset + sizeof(ChunkHeader));
    size_t bufferSize = bufferList.front().getByteLength(); //specified in the associated text chunk

    if (bufferSize > glbBufferSize) {
        return ConversionError::InvalidGlbFileLength;
    }

    _stream.seekg(0);
    const uint8_t* p_bufferData = _stream.currentData<uint8_t>() + binaryBufferOffset + sizeof(ChunkHeader);
    const core::MemoryRange<const uint8_t> memoryRange(p_bufferData, bufferSize);

    GltfResult gltfResult = bufferList.front().setData(memoryRange);
    bufferList.front().setStorageFormat(StorageFormat::File);

    if (gltfResult != GltfResult::Success) {
        return ConversionError::AllocationFailure;
    }

    GltfResult loadDataToMemory = m_gltf.loadBinaryDataToMemory({ /* no base directory needed for glb */ }, {}, false, m_importOptions.loadImages);

    if (loadDataToMemory == GltfError::ObjectAlreadyExists) {
        // ToDo: We should avoid trying to fill the buffer again.
        // But images have to fetch their data from the buffer via this function.
        return ConversionResult::Success;
    }

    if (loadDataToMemory != GltfResult::Success) {
        return ConversionError::LoadDataToMemoryFailure;
    }

    return ConversionResult::Success;
}
