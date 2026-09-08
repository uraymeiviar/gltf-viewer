#include "gltf/AccessorCreation.h"
using namespace ux3d;
using namespace ux3d::gltf;

Accessor* AccessorCreation::create(AttributeType _attributeType,
    ComponentType _componentType,
    core::MemoryRange<const uint8_t> _accessorData,
    uint32_t _count,
    std::optional<std::string> _name)
{
    Buffer* newBuffer = m_gltf.create<Buffer>();
    newBuffer->setStorageFormat(StorageFormat::File);
    newBuffer->setData(_accessorData);

    BufferView* newBufferView = m_gltf.create<BufferView>();
    newBufferView->setBuffer(*newBuffer);
    newBufferView->setByteOffset(0u);
    newBufferView->setByteLength(newBuffer->getByteLength());

    Accessor* newAccessor = m_gltf.create<Accessor>();
    newAccessor->setBufferView(*newBufferView);
    newAccessor->setByteOffset(0u);
    if (_name.has_value()) {
        newAccessor->setName(_name.value());
    }
    newAccessor->setStorageFormat(StorageFormat::File);

    newAccessor->setCount(_count);
    newAccessor->setComponentType(_componentType);
    newAccessor->setType(_attributeType);

    std::pair<core::Vector<double>, core::Vector<double>> minmax = newAccessor->getMinMaxFromData();
    newAccessor->getMin() = minmax.first;
    newAccessor->getMax() = minmax.second;

    return newAccessor;
}
