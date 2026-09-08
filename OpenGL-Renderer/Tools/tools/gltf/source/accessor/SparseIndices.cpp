#include "gltf/accessor/SparseIndices.h"
#include "gltf/BufferView.h"

using namespace ux3d;
using namespace ux3d::gltf;

SparseIndices::SparseIndices(const SparseIndices& _source)
    : GltfObject(_source)
    , m_byteOffset(_source.m_byteOffset)
    , m_componentType(_source.m_componentType)
{
}

ux3d::gltf::GltfResult ux3d::gltf::SparseIndices::setBufferView(BufferView& _bufferView)
{
    if (_bufferView.hasTarget()) {
        if (_bufferView.getTarget().value() == BufferView::TargetArrayBuffer || _bufferView.getTarget().value() == BufferView::ElementArrayBuffer) {
            core::logError("gltf::SparseIndices: Referenced bufferView cannot have ARRAY_BUFFER or ELEMENT_ARRAY_BUFFER target.");
            return GltfError::InvalidInput;
        }
    }

    m_bufferView = &_bufferView;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::SparseIndices::setByteOffset(uint32_t _byteOffset)
{
    m_byteOffset = _byteOffset;
    return ux3d::gltf::GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::SparseIndices::setComponentType(ComponentType _componentType)
{
    m_componentType = _componentType;
    return GltfResult::Success;
}

bool ux3d::gltf::SparseIndices::isValid() const
{
    return !(m_componentType == ComponentType::None);
}

ux3d::gltf::GltfResult ux3d::gltf::SparseIndices::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    if (!_invalidProperties.reserve(1)) {
        return GltfError::AllocationFailure;
    }

    if (m_componentType == ComponentType::None) {
        (void)_invalidProperties.emplace_back("componentType");
    }

    return GltfResult::Success;
}
