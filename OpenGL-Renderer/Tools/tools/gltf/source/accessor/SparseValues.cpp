#include "gltf/accessor/SparseValues.h"
#include "gltf/BufferView.h"

using namespace ux3d;
using namespace ux3d::gltf;

SparseValues::SparseValues(const SparseValues& _source)
    : GltfObject(_source)
    , m_byteOffset(_source.m_byteOffset)
{
}

ux3d::gltf::GltfResult ux3d::gltf::SparseValues::setBufferView(BufferView& _bufferView)
{
    if (_bufferView.hasTarget()) {
        if (_bufferView.getTarget().value() == BufferView::TargetArrayBuffer || _bufferView.getTarget().value() == BufferView::ElementArrayBuffer) {
            core::logError("gltf::SparseValues: Referenced bufferView cannot have ARRAY_BUFFER or ELEMENT_ARRAY_BUFFER target.");
            return GltfError::InvalidInput;
        }
    }

    m_bufferView = &_bufferView;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::SparseValues::setByteOffset(uint32_t _byteOffset)
{
    m_byteOffset = _byteOffset;
    return GltfResult::Success;
}

bool ux3d::gltf::SparseValues::isValid() const
{
    return true;
}

ux3d::gltf::GltfResult ux3d::gltf::SparseValues::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
