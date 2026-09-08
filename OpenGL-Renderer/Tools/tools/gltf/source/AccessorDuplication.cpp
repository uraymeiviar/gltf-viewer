#include "gltf/AccessorDuplication.h"
#include "core/Vector.h"
#include "gltf/Accessor.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Gltf.h"

using namespace ux3d;
using namespace ux3d::gltf;

AccessorDuplication::AccessorDuplication(Gltf& _gltf)
    : m_gltf(_gltf)
{
}

bool ux3d::gltf::AccessorDuplication::canDuplicate(const Accessor& _accessor)
{
    // check if accessor is using data that is interleaved
    // interleaved data should not be duplicated without prior deinterleaving

    const BufferView* oldBufferView = _accessor.getBufferView();

    if (oldBufferView == nullptr) {
        return false;
    }

    //for now, we do not handle sparse data in this class
    if (_accessor.getSparse().has_value()) {
        return false;
    }

    if (!oldBufferView->getByteStride().has_value()) {
        return true;
    }

    size_t bufferViewStride = oldBufferView->getByteStride().value();
    size_t accessorStride = Accessor::getByteLengthFrom(_accessor.getComponentType(), _accessor.getType(), true);

    if (bufferViewStride == accessorStride) {
        return true;
    }

    return false;
}

Accessor* ux3d::gltf::AccessorDuplication::duplicate(const Accessor& _accessor, bool _isIndicesAccessor)
{
    if (!canDuplicate(_accessor)) {
        return nullptr;
    }

    const BufferView* oldBufferView = _accessor.getBufferView();
    const Buffer* oldBuffer = oldBufferView->getBuffer();

    // Spec: "For performance and compatibility reasons, each element of a vertex attribute
    // must be aligned to 4-byte boundaries inside bufferView"
    //
    // This does not apply to indices accessors:
    bool withPadding = true;
    if (_isIndicesAccessor) {
        withPadding = false;
    }

    size_t accessorStride = Accessor::getByteLengthFrom(_accessor.getComponentType(), _accessor.getType(), withPadding);

    core::MemoryRange<const uint8_t> accessorData;
    _accessor.getData(accessorData);

    Buffer* newBuffer = m_gltf.create<Buffer>();
    newBuffer->setName(oldBuffer->getName());
    newBuffer->setExtras(oldBuffer->getExtras());
    newBuffer->setStorageFormat(oldBuffer->getStorageFormat());
    newBuffer->setData(accessorData);

    BufferView* newBufferView = m_gltf.create<BufferView>();
    newBufferView->setBuffer(*newBuffer);
    newBufferView->setByteOffset(0u);
    newBufferView->setByteStride(static_cast<uint32_t>(accessorStride));
    newBufferView->setByteLength(newBuffer->getByteLength());
    newBufferView->setExtras(oldBufferView->getExtras());
    if (oldBufferView->getTarget().has_value()) {
        newBufferView->setTarget(oldBufferView->getTarget().value());
    }

    Accessor* newAccessor = m_gltf.create<Accessor>();
    newAccessor->setBufferView(*newBufferView);
    newAccessor->setByteOffset(0u);
    newAccessor->setComponentType(_accessor.getComponentType());
    newAccessor->setCount(_accessor.getCount());
    newAccessor->setExtras(_accessor.getExtras());
    if (_accessor.hasName()) {
        newAccessor->setName("Copy of " + _accessor.getName());
    }
    if (_accessor.hasNormalized()) {
        newAccessor->setNormalized(_accessor.isNormalized());
    }
    newAccessor->setStorageFormat(_accessor.getStorageFormat());
    newAccessor->setType(_accessor.getType());
    newAccessor->setUsageFlag(_accessor.getUsageFlag());
    newAccessor->recalculateMinMax(true);

    return newAccessor;
}
