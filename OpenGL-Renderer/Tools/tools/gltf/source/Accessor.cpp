// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Accessor.h"
#include "core/MemoryRange.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Gltf.h"
#include <algorithm>

using namespace ux3d;
using namespace ux3d::gltf;

Accessor::Accessor(uint32_t _id)
    : ChildOfRoot(_id)
{
}

Accessor::Accessor(uint32_t _id, const Accessor& _source)
    : ChildOfRoot(_id, _source)
    , m_byteOffset(_source.m_byteOffset)
    , m_type(_source.m_type)
    , m_componentType(_source.m_componentType)
    , m_count(_source.m_count)
    , m_normalized(_source.m_normalized)
    , m_min(_source.m_min)
    , m_max(_source.m_max)
    , m_storageFormat(_source.m_storageFormat)
{
    if (_source.m_sparse) {
        m_sparse.emplace(_source.m_sparse.value());
    }
}

bool Accessor::isValid() const
{
    if (m_componentType == ComponentType::None) {
        return false;
    }
    if (m_type == AttributeType::None) {
        return false;
    }
    if (m_count == 0) {
        return false;
    }

    return true;
}

Extensions Accessor::getUsedExtensions() const
{
    ComponentType componentType = getComponentType();

    const ComponentTypeFlag vanillaFlags = getAllowedTypesVanilla(getUsageFlag());
    const ComponentTypeFlag meshQuantizationFlags = getAllowedTypesWithMeshQuantization(getUsageFlag());
    const ComponentTypeFlag componentTypeFlag = getFlagFromType(componentType, isNormalized());

    if (vanillaFlags.isSet(componentTypeFlag) == false && meshQuantizationFlags.isSet(componentTypeFlag)) {
        return Extension::MeshQuantization;
    }

    return Extension::None;
}

GltfResult Accessor::setBufferView(BufferView& _bufferView)
{
    m_pBufferView = &_bufferView;
    return GltfResult::Success;
}

GltfResult Accessor::setByteOffset(uint32_t _byteOffset)
{
    m_byteOffset = _byteOffset;
    return GltfResult::Success;
}

GltfResult Accessor::setType(AttributeType _type)
{
    m_type = _type;
    return GltfResult::Success;
}

GltfResult Accessor::setComponentType(ComponentType _componentType)
{
    m_componentType = _componentType;
    return GltfResult::Success;
}

GltfResult Accessor::setCount(uint32_t _count)
{
    if (_count < 1) {
        return GltfError::InvalidInput;
    }
    m_count = _count;
    return GltfResult::Success;
}

GltfResult Accessor::setNormalized(bool _normalized)
{
    m_normalized = _normalized;
    return GltfResult::Success;
}

GltfResult Accessor::getData(core::MemoryRange<const uint8_t>& _outData) const
{
    const size_t elementSize = getByteLengthFrom(m_componentType, m_type);
    if (m_pBufferView == nullptr) {
        //Spec: When undefined, the accessor MUST be initialized with zeros;
        _outData = core::makeRange(m_internalData.data(), getSizeInBytes());
        return GltfResult::Success;
    }

    core::MemoryRange<const uint8_t> bufferViewData;
    m_pBufferView->getData(bufferViewData);

    if (bufferViewData.empty()) {
        return GltfError::ObjectNotFound;
    }

    if (bufferViewData.size() < getByteOffset()) {
        return GltfError::ObjectNotFound;
    }

    const uint8_t* begin = bufferViewData.data() + getByteOffset();
    if (begin == nullptr) {
        return GltfError::ObjectNotFound;
    }

    const size_t length = static_cast<size_t>(m_pBufferView->getByteStride().value_or(elementSize)) * static_cast<size_t>(m_count);

    _outData = core::makeRange(begin, length);
    return GltfResult::Success;
}

GltfResult Accessor::setUsageFlag(UsageFlag _usageFlag)
{
    m_usageFlags = _usageFlag;
    return GltfResult::Success;
}

GltfResult Accessor::setStorageFormat(StorageFormat _storageFormat)
{
    m_storageFormat = _storageFormat;
    return GltfResult::Success;
}

GltfResult Accessor::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    if (_invalidProperties.reserve(3) == false) {
        return GltfError::AllocationFailure;
    }

    if (m_componentType == ComponentType::None) {
        (void)_invalidProperties.emplace_back("componentType");
    }
    if (m_type == AttributeType::None) {
        (void)_invalidProperties.emplace_back("type");
    }
    if (m_count == 0) {
        (void)_invalidProperties.push_back("count");
    }

    return GltfResult::Success;
}

size_t Accessor::getByteLengthFrom(ComponentType _componentType)
{
    size_t byteLength;

    switch (_componentType) {
    case ComponentType::Byte:
    case ComponentType::UnsignedByte:
        byteLength = 1;
        break;
    case ComponentType::Short:
    case ComponentType::UnsignedShort:
        byteLength = 2;
        break;
    case ComponentType::Float:
    case ComponentType::UnsignedInt:
        byteLength = 4;
        break;
    default:
        byteLength = 0;
    }

    return byteLength;
}

uint32_t Accessor::getComponentCountFrom(AttributeType _attributeType)
{
    switch (_attributeType) {
    case AttributeType::Scalar:
        return 1u;
    case AttributeType::Mat2:
    case AttributeType::Vec4:
        return 4u;
    case AttributeType::Vec2:
        return 2u;
    case AttributeType::Vec3:
        return 3u;
    case AttributeType::Mat3:
        return 9u;
    case AttributeType::Mat4:
        return 16u;
    case AttributeType::None:
        return 0u;
    }

    return 0u;
}

size_t Accessor::getByteLengthFrom(ComponentType _componentType, AttributeType _attributeType, bool _withPadding)
{
    const size_t byteLength = getByteLengthFrom(_componentType);
    const size_t componentCount = getComponentCountFrom(_attributeType);
    size_t stride = byteLength * componentCount;
    if (_withPadding) {
        // calculate stride with padding
        if (stride % 4 != 0) {
            stride += 4 - (stride % 4);
        }
    }
    return stride;
}

void Accessor::recalculateMinMax(bool create)
{
    switch (m_componentType) {
    case ComponentType::Byte:
        recalculateMinMax<int8_t>(create);
        break;
    case ComponentType::UnsignedByte:
        recalculateMinMax<uint8_t>(create);
        break;
    case ComponentType::Short:
        recalculateMinMax<int16_t>(create);
        break;
    case ComponentType::UnsignedShort:
        recalculateMinMax<uint16_t>(create);
        break;
    case ComponentType::UnsignedInt:
        recalculateMinMax<uint32_t>(create);
        break;
    case ComponentType::Float:
        recalculateMinMax<float>(create);
        break;
    case ComponentType::None:
        break;
    default:
        break;
    }
}

std::pair<core::Vector<double>, core::Vector<double>> Accessor::getMinMaxFromData()
{
    std::pair<core::Vector<double>, core::Vector<double>> minMaxPair;

    switch (m_componentType) {
    case ComponentType::Byte:
        minMaxPair = getMinMaxFromData<int8_t>();
        break;
    case ComponentType::UnsignedByte:
        minMaxPair = getMinMaxFromData<uint8_t>();
        break;
    case ComponentType::Short:
        minMaxPair = getMinMaxFromData<int16_t>();
        break;
    case ComponentType::UnsignedShort:
        minMaxPair = getMinMaxFromData<uint16_t>();
        break;
    case ComponentType::UnsignedInt:
        minMaxPair = getMinMaxFromData<uint32_t>();
        break;
    case ComponentType::Float:
        minMaxPair = getMinMaxFromData<float>();
        break;
    case ComponentType::None:
        break;
    default:
        break;
    }

    return minMaxPair;
}

size_t Accessor::getSizeInBytes() const
{
    return m_count * getByteLengthFrom(m_componentType, m_type, true);
}

size_t Accessor::getTotalSizeInBytes() const
{
    size_t elementSize = getByteLengthFrom(m_componentType, m_type, true);
    size_t size = m_count * elementSize;
    if (m_sparse.has_value()) {
        size += m_sparse.value().getSizeInBytes(elementSize);
    }
    return size;
}

GltfResult Accessor::createSparse()
{
    if (m_sparse.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }

    m_sparse.emplace();
    return GltfResult::Success;
}

GltfResult Accessor::applySparse(core::MemoryRange<uint8_t> _data) const
{
    const uint32_t count = m_sparse->getCount();
    const size_t componentSize = getByteLengthFrom(m_componentType, m_type, true);

    const SparseIndices& indices = m_sparse->getIndices();
    const SparseValues& values = m_sparse->getValues();
    if (indices.getBufferView() == nullptr || values.getBufferView() == nullptr) {
        return GltfError::ObjectNotFound;
    }

    core::MemoryRange<const uint8_t> indicesData;
    if (auto result = indices.getBufferView()->getData(indicesData); core::Failed(result)) {
        return result;
    }

    core::MemoryRange<const uint8_t> valuesData;
    if (auto result = values.getBufferView()->getData(valuesData); core::Failed(result)) {
        return result;
    }

    if (valuesData.size() != count * componentSize) {
        return GltfError::InvalidInput;
    }

    auto apply = [&](auto indices) {
        if (indices.size() != count) {
            return GltfError::InvalidInput;
        }

        size_t valuesIndex = 0;
        for (const auto& i : indices) {
            // TODO: this looks super evil
            memcpy(_data.data() + (static_cast<size_t>(i) * componentSize), valuesData.data() + (valuesIndex++ * componentSize), componentSize);
        }

        return GltfResult::Success;
    };

    switch (indices.getComponentType()) {
    case ComponentType::Byte:
        return apply(indicesData.viewAs<const int8_t>());
    case ComponentType::UnsignedByte:
        return apply(indicesData.viewAs<const uint8_t>());
    case ComponentType::Short:
        return apply(indicesData.viewAs<const int16_t>());
    case ComponentType::UnsignedShort:
        return apply(indicesData.viewAs<const uint16_t>());
    case ComponentType::UnsignedInt:
        return apply(indicesData.viewAs<const uint32_t>());
    default:
        return GltfError::InvalidInput;
    }
}
