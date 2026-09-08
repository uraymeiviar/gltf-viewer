#include "gltf/DracoEncoder.h"
#include "core/Platform.h"
#include "gltf/Gltf.h"
#include "gltf/StorageFormats.h"
#include "gltf/mesh/DracoMeshCompression.h"

UX3D_BEGIN_IGNORE_WARNINGS
#include <draco/compression/encode.h>
UX3D_END_IGNORE_WARNINGS

using namespace ux3d;
using namespace ux3d::gltf;

DracoEncoder::DracoEncoder(Gltf& gltf)
    : m_gltf(gltf)
{
}

void ux3d::gltf::DracoEncoder::setPreserveTriangleOrder(bool enabled)
{
    if (enabled) {
        m_targetMeshEncoderMethod = SEQUENTIAL_ENCODING;
    } else {
        m_targetMeshEncoderMethod = EDGEBREAKER_ENCODING;
    }
}

void ux3d::gltf::DracoEncoder::setCompressionLevel(int compressionLevel)
{
    m_targetCompressionLevel = compressionLevel;

    if (m_targetCompressionLevel > 10) {
        m_targetCompressionLevel = 10;
    } else if (m_targetCompressionLevel < 0) {
        m_targetCompressionLevel = 0;
    }
}

void gltf::DracoEncoder::setQuantizationBits(const gltf::DracoEncoder::QuantizationBits& quantizationBits)
{
    m_quantizationBits = quantizationBits;
}

GltfResult DracoEncoder::encodePrimitive(Primitive& primitive)
{
    if (!primitive.createDracoMeshCompression()) {
        core::logWarning("Primitive is already Draco compressed");
        return GltfResult::Success;
    }

    if (primitive.getPrimitiveMode() != PrimitiveMode::Triangles) {
        return GltfError::DracoEncodingError;
    }

    draco::Mesh dracoMesh;

    DracoMeshCompression& gltfDracoMeshCompression = *primitive.getDracoMeshCompression();

    std::vector<draco::DataBuffer> dracoAttributeDataBuffers;

    for (gltf::Attribute& attribute : primitive.getAttributes()) {
        draco::DataType dataType = draco::DT_INVALID;
        size_t attributeElementCount = attribute.getAccessor()->getCount();

        switch (attribute.getAccessor()->getComponentType()) {
        case ComponentType::None:
            return GltfError::DracoEncodingError;
        case ComponentType::Byte:
            dataType = draco::DT_INT8;
            break;
        case ComponentType::UnsignedByte:
            dataType = draco::DT_UINT8;
            break;
        case ComponentType::Short:
            dataType = draco::DT_INT16;
            break;
        case ComponentType::UnsignedShort:
            dataType = draco::DT_UINT16;
            break;
        case ComponentType::UnsignedInt:
            dataType = draco::DT_UINT32;
            break;
        case ComponentType::Float:
            dataType = draco::DT_FLOAT32;
            break;
        }

        int8_t componentCount = static_cast<int8_t>(Accessor::getComponentCountFrom(attribute.getAccessor()->getType()));

        core::MemoryRange<const uint8_t> attributeBytes;
        attribute.getAccessor()->getData(attributeBytes);
        size_t stride = Accessor::getByteLengthFrom(attribute.getAccessor()->getComponentType(), attribute.getAccessor()->getType(), true);

        draco::DataBuffer& dracoBuffer = dracoAttributeDataBuffers.emplace_back();
        dracoBuffer.Resize(static_cast<int64_t>(attributeBytes.size()));

        draco::GeometryAttribute dracoAttribute;

        draco::GeometryAttribute::Type dracoAttributeType;
        if (attribute.getName() == "POSITION") {
            dracoAttributeType = draco::GeometryAttribute::POSITION;
            dracoMesh.set_num_points(attribute.getAccessor()->getCount());
        } else if (attribute.getName() == "NORMAL") {
            dracoAttributeType = draco::GeometryAttribute::NORMAL;
        } else if (attribute.getName() == "COLOR") {
            dracoAttributeType = draco::GeometryAttribute::COLOR;
        } else if (strncmp(attribute.getName().c_str(), "TEXCOORD_", sizeof("TEXCOORD_"))) {
            dracoAttributeType = draco::GeometryAttribute::TEX_COORD;
        } else {
            dracoAttributeType = draco::GeometryAttribute::GENERIC;
        }
        dracoAttribute.Init(dracoAttributeType, &dracoBuffer, componentCount, dataType, attribute.getAccessor()->isNormalized(), static_cast<int64_t>(stride), 0);

        uint32_t dracoAttributeId;
        if (int id = dracoMesh.AddAttribute(dracoAttribute, true, static_cast<draco::AttributeValueIndex::ValueType>(attributeElementCount));
            id >= 0) {
            dracoAttributeId = static_cast<uint32_t>(id);
        } else {
            return GltfError::DracoEncodingError;
        }

        for (draco::AttributeValueIndex::ValueType i = 0; i < attributeElementCount; ++i) {
            const uint8_t* bytes = &attributeBytes[i * stride];
            dracoMesh.attribute(static_cast<int32_t>(dracoAttributeId))->SetAttributeValue(draco::AttributeValueIndex(i), bytes);
        }

        DracoMeshCompression::DracoAttribute gltfDracoAttribute;
        gltfDracoAttribute.id = dracoAttributeId;
        gltfDracoAttribute.name = attribute.getName();

        gltfDracoMeshCompression.getAttributes().push_back(gltfDracoAttribute);
    }

    // Indices
    if (primitive.getIndicesAccessor() != nullptr) {
        core::MemoryRange<const uint8_t> byteData;
        primitive.getIndicesAccessor()->getData(byteData);
        size_t const componentSize = Accessor::getByteLengthFrom(primitive.getIndicesAccessor()->getComponentType());
        size_t stride = Accessor::getByteLengthFrom(primitive.getIndicesAccessor()->getComponentType(), primitive.getIndicesAccessor()->getType());

        const size_t indicesCount = primitive.getIndicesAccessor()->getCount();
        for (size_t i = 0; i < indicesCount; i += 3) {
            draco::Mesh::Face face;

            if (componentSize == 4) {
                const uint32_t* ints = reinterpret_cast<const uint32_t*>(&byteData[i * stride]);
                face[0] = ints[0];
                face[1] = ints[1];
                face[2] = ints[2];
            } else if (componentSize == 2) {
                const uint16_t* shorts = reinterpret_cast<const uint16_t*>(&byteData[i * stride]);
                face[0] = shorts[0];
                face[1] = shorts[1];
                face[2] = shorts[2];
            } else if (componentSize == 1) {
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&byteData[i * stride]);
                face[0] = bytes[0];
                face[1] = bytes[1];
                face[2] = bytes[2];
            } else {
                return GltfError::DracoEncodingError;
            }

            dracoMesh.AddFace(face);
        }

        //Cleanup Accessor (original data is no longer valid):
        primitive.getIndicesAccessor()->unsetBufferView();
        if (primitive.getIndicesAccessor()->hasByteOffset()) {
            primitive.getIndicesAccessor()->setByteOffset(0);
        }
    } else {
        const uint32_t numFaces = dracoMesh.num_points() / 3;
        for (uint32_t i = 0; i < numFaces; i++) {
            draco::Mesh::Face face;
            face[0] = 3 * i;
            face[1] = 3 * i + 1;
            face[2] = 3 * i + 2;
            dracoMesh.AddFace(face);
        }
    }

    //Cleanup Accessor (original data is no longer valid):
    for (gltf::Attribute& attribute : primitive.getAttributes()) {

        attribute.getAccessor()->unsetBufferView();
        if (attribute.getAccessor()->hasByteOffset()) {
            attribute.getAccessor()->setByteOffset(0);
        }
    }

    draco::EncoderBuffer dracoEncoderBuffer;
    draco::Encoder dracoEncoder;

    dracoEncoder.SetSpeedOptions(m_targetCompressionLevel, m_targetCompressionLevel);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, m_quantizationBits.position);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, m_quantizationBits.normal);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, m_quantizationBits.uv);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, m_quantizationBits.color);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, m_quantizationBits.generic);

    bool useSequentialEncoding = m_targetMeshEncoderMethod == SEQUENTIAL_ENCODING || !primitive.getTargets().empty();
    dracoEncoder.SetEncodingMethod(useSequentialEncoding ? draco::MESH_SEQUENTIAL_ENCODING : draco::MESH_EDGEBREAKER_ENCODING);

    dracoEncoder.SetTrackEncodedProperties(true);

    bool successfulEncoding = dracoEncoder.EncodeMeshToBuffer(dracoMesh, &dracoEncoderBuffer).ok();
    if (!successfulEncoding) {
        return GltfError::DracoEncodingError;
    }

    // Draco can change the number of vertices:
    if (!useSequentialEncoding) {
        for (auto& attribute : primitive.getAttributes()) {
            attribute.getAccessor()->setCount(dracoEncoder.num_encoded_points());
        }
        if (primitive.getIndicesAccessor() != nullptr) {
            primitive.getIndicesAccessor()->setCount(3 * dracoEncoder.num_encoded_faces());
        }
    }

    // Add encoded data to glTF via buffers and bufferViews:

    Buffer* buffer = m_gltf.create<Buffer>();
    buffer->setData(core::MemoryRange(reinterpret_cast<uint8_t const*>(dracoEncoderBuffer.buffer()->data()), dracoEncoderBuffer.buffer()->size()));
    buffer->setByteLength(static_cast<uint32_t>(dracoEncoderBuffer.buffer()->size()));

    BufferView* bufferView = m_gltf.create<BufferView>();
    bufferView->setBuffer(*buffer);
    bufferView->setByteLength(static_cast<uint32_t>(dracoEncoderBuffer.buffer()->size()));
    gltfDracoMeshCompression.setBufferView(*bufferView);

    if (m_logOutputEnabled) {
        core::debugLog(core::LogLevel::Info, "Mesh Compression: encoded %zu points", dracoEncoder.num_encoded_points());
        core::debugLog(core::LogLevel::Info, "Mesh Compression: encoded %zu triangles", dracoEncoder.num_encoded_faces());
        core::logInfo("Compressed mesh size: %zu bytes", dracoEncoderBuffer.buffer()->size());
    }

    return GltfResult::Success;
}
