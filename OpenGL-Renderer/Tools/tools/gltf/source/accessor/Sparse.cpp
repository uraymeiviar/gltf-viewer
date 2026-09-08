#include "gltf/accessor/Sparse.h"
#include "gltf/Accessor.h"

using namespace ux3d;
using namespace ux3d::gltf;

Sparse::Sparse(const Sparse& _source)
    : GltfObject(_source)
    , m_count(_source.m_count)
    , m_indices(_source.m_indices)
    , m_values(_source.m_values)
{
}

GltfResult Sparse::setCount(uint32_t _count)
{
    if (_count < 1) {
        return GltfError::InvalidInput;
    }
    m_count = _count;
    return GltfResult::Success;
}

bool Sparse::isValid() const
{
    return true;
}

GltfResult Sparse::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}

size_t Sparse::getSizeInBytes(size_t _elementSize) const
{
    return m_count * _elementSize + m_count * Accessor::getByteLengthFrom(m_indices.getComponentType());
}
