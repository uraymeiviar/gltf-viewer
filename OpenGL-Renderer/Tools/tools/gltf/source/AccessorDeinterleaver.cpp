#include "gltf/AccessorDeinterleaver.h"
#include "core/Logger.h"
#include "core/Vector.h"
#include "gltf/Accessor.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Gltf.h"

using namespace ux3d;
using namespace ux3d::gltf;

AccessorDeinterleaver::AccessorDeinterleaver(Gltf& _gltf, Accessor& _accessor)
    : m_gltf(_gltf)
    , m_accessor(_accessor)
{
}

bool AccessorDeinterleaver::isInterleaved(Gltf& gltf, Accessor& accessor)
{
    BufferView* bufferView = accessor.getBufferView();

    if (bufferView == nullptr) {
        return false;
    }

    if (!bufferView->getByteStride().has_value()) {
        return false;
    }

    uint32_t bufferViewStride = bufferView->getByteStride().value();
    size_t accessorStride = Accessor::getByteLengthFrom(accessor.getComponentType(), accessor.getType(), true);

    if (bufferViewStride == static_cast<uint32_t>(accessorStride)) {
        return false;
    }
    return true;
}

GltfResult AccessorDeinterleaver::deinterleave()
{
    BufferView* oldBufferView = m_accessor.getBufferView();

    if (oldBufferView == nullptr) {
        return GltfError::ObjectNotFound;
    }

    Buffer* oldBuffer = oldBufferView->getBuffer();

    if (!oldBufferView->getByteStride().has_value()) {
        return GltfResult::Success;
    }

    uint32_t bufferViewStride = oldBufferView->getByteStride().value();

    size_t accessorStride = Accessor::getByteLengthFrom(m_accessor.getComponentType(), m_accessor.getType(), true);
    uint32_t accessorOffset = m_accessor.getByteOffset();

    if (bufferViewStride == static_cast<uint32_t>(accessorStride)) {
        return GltfResult::Success;
    }

    if (bufferViewStride < static_cast<uint32_t>(accessorStride)) {
        return GltfError::InvalidInput;
    }

    core::Vector<uint8_t> newDataVector;

    if (newDataVector.resize(m_accessor.getCount() * accessorStride) == false) {
        return GltfError::AllocationFailure;
    }

    core::MemoryRange<const uint8_t> bufferViewData;
    oldBufferView->getData(bufferViewData);

    size_t requiredSize = accessorOffset + (m_accessor.getCount() - 1) * bufferViewStride + Accessor::getByteLengthFrom(m_accessor.getComponentType(), m_accessor.getType(), false);
    if (bufferViewData.size() < requiredSize) {
        core::logError("Accessor (offset: %d, length: %d) does not fit referenced bufferView (length: %d)", accessorOffset, (m_accessor.getCount() - 1) * bufferViewStride + Accessor::getByteLengthFrom(m_accessor.getComponentType(), m_accessor.getType(), false), bufferViewData.size());
        return GltfError::InvalidInput;
    }

    for (size_t i = 0; i < m_accessor.getCount(); ++i) {
        for (size_t j = 0; j < accessorStride; ++j) {
            size_t bufferViewIndex = accessorOffset + i * bufferViewStride + j;
            size_t accessorIndex = i * accessorStride + j;
            newDataVector[accessorIndex] = bufferViewData[bufferViewIndex];
        }
    }

    Buffer* newBuffer = m_gltf.create<Buffer>();
    newBuffer->setName(oldBuffer->getName());
    newBuffer->setExtras(oldBuffer->getExtras());
    newBuffer->setStorageFormat(oldBuffer->getStorageFormat());
    core::MemoryRange<uint8_t> newData = newDataVector.getMemoryRange();
    newBuffer->setData(newData);

    BufferView* newBufferView = m_gltf.create<BufferView>();
    newBufferView->setBuffer(*newBuffer);
    newBufferView->setByteOffset(0u);
    newBufferView->setByteStride(static_cast<uint32_t>(accessorStride));
    newBufferView->setByteLength(newBuffer->getByteLength());
    newBufferView->setExtras(oldBufferView->getExtras());
    if (oldBufferView->getTarget().has_value()) {
        newBufferView->setTarget(oldBufferView->getTarget().value());
    }

    m_accessor.setBufferView(*newBufferView);
    m_accessor.setByteOffset(0u);
    return GltfResult::Success;
}
