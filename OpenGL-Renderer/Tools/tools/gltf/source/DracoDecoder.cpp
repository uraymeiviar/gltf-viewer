#include "gltf/DracoDecoder.h"
#include "core/Platform.h"
#include "gltf/Gltf.h"
#include "gltf/StorageFormats.h"

UX3D_BEGIN_IGNORE_WARNINGS
#include <draco/compression/decode.h>
UX3D_END_IGNORE_WARNINGS

using namespace ux3d;
using namespace ux3d::gltf;

DracoDecoder::DracoDecoder(Gltf& gltf)
    : m_gltf(gltf)
{
}

GltfResult DracoDecoder::decodePrimitive(Primitive& primitive)
{
    if (!primitive.getDracoMeshCompression()) {
        core::logError("Primitive has no DracoMeshCompression object required for Draco");
        return GltfError::ObjectNotFound;
    }

    Accessor* indexAccessor = primitive.getIndicesAccessor();

    const DracoMeshCompression& info = *primitive.getDracoMeshCompression();
    if (info.getBufferView() == nullptr) {
        core::logError("DracoMeshCompression object has no valid BufferView required for Draco");
        return GltfError::ObjectNotFound;
    }

    auto decodedPrimitive = m_gltf.create<Buffer>();

    if (decodedPrimitive == nullptr) {
        return GltfError::AllocationFailure;
    }

    core::MemoryRange<const uint8_t> compressedData;
    if (GltfResult result = info.getBufferView()->getData(compressedData); result != GltfResult::Success) {
        return result;
    }

    draco::DecoderBuffer decBuffer;
    decBuffer.Init((const char*)compressedData.data(), compressedData.size());

    if (auto statType = draco::Decoder::GetEncodedGeometryType(&decBuffer); statType.ok() == false || statType.value() != draco::TRIANGULAR_MESH) {
        core::logError("Draco buffer geometry type not supported (glTF requires TRIANGULAR_MESH)");
        return GltfError::DracoDecodingError;
    }

    draco::Decoder dracoDec;
    core::Vector<uint8_t> stagingBuffer;
    auto decMesh = dracoDec.DecodeMeshFromBuffer(&decBuffer);

    if (decMesh.ok() == false) {
        core::logError("Draco decoding from buffer failed");
        return GltfError::DracoDecodingError;
    }

    const auto& dracoMesh = decMesh.value();

    uint32_t faces = dracoMesh->num_faces();
    uint32_t indices = faces * 3u;
    uint32_t indexElementSize = (indexAccessor != nullptr) ? static_cast<uint32_t>(Accessor::getByteLengthFrom(indexAccessor->getComponentType(), AttributeType::Scalar)) : 0;
    uint32_t indexBufferSize = indices * indexElementSize;

    if (indexAccessor != nullptr && indices != indexAccessor->getCount()) {
        core::logError("Draco index count %u doesn't match index accessor count %u", indices, indexAccessor->getCount());
        return GltfError::DracoDecodingError;
    }

    uint32_t vertexCount = dracoMesh->num_points();

    // index data is prepended to the front of the buffer
    uint32_t primitiveSize = indexBufferSize;

    // accumulate attribute sizes
    for (const auto& attrib : info.getAttributes()) {
        auto it = std::find_if(primitive.getAttributes().begin(), primitive.getAttributes().end(), [&attrib](const gltf::Attribute& m_gltf) { return m_gltf.getName() == attrib.name; });
        if (it == primitive.getAttributes().end()) {
            core::logError("glTF attribute %s not found for draco id %u", attrib.name.c_str(), attrib.id);
            return GltfError::ObjectNotFound;
        }

        Accessor* pAccessor = (*it).getAccessor();
        if (pAccessor == nullptr) {
            core::logError("Accessor for '%s' attribute not set", attrib.name.c_str());
            return GltfError::ObjectNotFound;
        }

        if (pAccessor->getCount() != vertexCount) {
            core::logError("Draco vertex count %u doesn't match vertex accessor count %u", vertexCount, pAccessor->getCount());
            return GltfError::DracoDecodingError;
        }

        uint32_t stride = static_cast<uint32_t>(Accessor::getByteLengthFrom(pAccessor->getComponentType(), pAccessor->getType(), true));

        primitiveSize += vertexCount * stride;
    }

    if (stagingBuffer.resize(primitiveSize) == false) {
        return GltfError::AllocationFailure;
    }

    // copy indices
    if (indexAccessor != nullptr) {
        if (indexElementSize == 4u) {
            uint32_t* pIndices = reinterpret_cast<uint32_t*>(stagingBuffer.data());
            for (draco::FaceIndex f(0u); f < faces; ++f) {
                const auto& face = dracoMesh->face(f);
                *(pIndices++) = face[0].value();
                *(pIndices++) = face[1].value();
                *(pIndices++) = face[2].value();
            }
        } else if (indexElementSize == 2u) {
            uint16_t* pIndices = reinterpret_cast<uint16_t*>(stagingBuffer.data());
            for (draco::FaceIndex f(0u); f < faces; ++f) {
                const auto& face = dracoMesh->face(f);
                *(pIndices++) = static_cast<uint16_t>(face[0].value());
                *(pIndices++) = static_cast<uint16_t>(face[1].value());
                *(pIndices++) = static_cast<uint16_t>(face[2].value());
            }
        } else if (indexElementSize == 1u) {
            uint8_t* pIndices = reinterpret_cast<uint8_t*>(stagingBuffer.data());
            for (draco::FaceIndex f(0u); f < faces; ++f) {
                const auto& face = dracoMesh->face(f);
                *(pIndices++) = static_cast<uint8_t>(face[0].value());
                *(pIndices++) = static_cast<uint8_t>(face[1].value());
                *(pIndices++) = static_cast<uint8_t>(face[2].value());
            }
        } else {
            core::logError("Index format (of size %u) not supported", indexElementSize);
            return GltfError::DracoDecodingError;
        }

        auto indexView = m_gltf.create<BufferView>();

        if (indexView == nullptr) {
            return GltfError::AllocationFailure;
        }

        indexView->setBuffer(*decodedPrimitive);
        indexView->setByteOffset(0u);
        indexView->setByteLength(indexBufferSize);

        indexAccessor->setBufferView(*indexView);
        indexAccessor->setStorageFormat(StorageFormat::NotStored);
    }

    // copy vertex attributes
    uint32_t attributeOffset = indexBufferSize;
    for (const auto& attrib : info.getAttributes()) {
        auto it = std::find_if(primitive.getAttributes().begin(), primitive.getAttributes().end(), [&attrib](const gltf::Attribute& m_gltf) { return m_gltf.getName() == attrib.name; });
        if (it == primitive.getAttributes().end()) {
            return GltfError::ObjectNotFound;
        }

        Accessor* pAccessor = (*it).getAccessor();
        if (pAccessor == nullptr) {
            return GltfError::ObjectNotFound;
        }

        BufferView* attributeView = m_gltf.create<BufferView>();

        if (attributeView == nullptr) {
            return GltfError::AllocationFailure;
        }

        uint32_t stride = static_cast<uint32_t>(Accessor::getByteLengthFrom(pAccessor->getComponentType(), pAccessor->getType(), true));

        const draco::PointAttribute* pointAttribute = dracoMesh->GetAttributeByUniqueId(attrib.id);

        uint8_t* data = &stagingBuffer.data()[attributeOffset];
        for (uint32_t i = 0; i < vertexCount; ++i) {
            auto idx = pointAttribute->mapped_index(draco::PointIndex(i));

            uint8_t* value = data + static_cast<size_t>(i) * stride;

            // draco attribute size and type can differ from gltf accessor type, cant mem copy
            // convert value from draco attribute type to gltf attribute type
            bool converted = false;
            switch (pAccessor->getComponentType()) {
            case ComponentType::Byte:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<int8_t*>(value));
                break;
            case ComponentType::UnsignedByte:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<uint8_t*>(value));
                break;
            case ComponentType::Short:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<short*>(value));
                break;
            case ComponentType::UnsignedShort:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<unsigned short*>(value));
                break;
            case ComponentType::UnsignedInt:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<unsigned int*>(value));
                break;
            case ComponentType::Float:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<float*>(value));
                break;
            default:
                break;
            }

            if (converted == false) {
                core::logError("Failed to convert draco attribute type to glTF accessor type");
                return GltfError::DracoDecodingError;
            }
        }

        attributeView->setBuffer(*decodedPrimitive);
        attributeView->setByteOffset(attributeOffset);
        attributeView->setByteLength(vertexCount * stride);
        attributeView->setByteStride(static_cast<uint32_t>(stride));

        attributeOffset += attributeView->getByteLength();

        pAccessor->setBufferView(*attributeView);
        pAccessor->setStorageFormat(StorageFormat::NotStored);
        if (pAccessor->hasByteOffset()) {
            pAccessor->setByteOffset(0);
        }
        if (pAccessor->hasNormalized() || pointAttribute->normalized()) {
            pAccessor->setNormalized(pointAttribute->normalized());
        }
    }

    decodedPrimitive->setData(stagingBuffer.getMemoryRange());
    // for now, we do not want to export uncompressed data
    decodedPrimitive->setStorageFormat(StorageFormat::NotStored);
    // set an uri so that the glTF is valid
    decodedPrimitive->setUri("draco_decoder_" + decodedPrimitive->createUniqueURI(m_gltf));

    for (const auto& attrib : info.getAttributes()) {
        auto it = std::find_if(primitive.getAttributes().begin(), primitive.getAttributes().end(), [&attrib](const gltf::Attribute& m_gltf) { return m_gltf.getName() == attrib.name; });

        Accessor* pAccessor = (*it).getAccessor();
        if (pAccessor != nullptr) {
            pAccessor->recalculateMinMax();
        }
    }

    return GltfResult::Success;
}
