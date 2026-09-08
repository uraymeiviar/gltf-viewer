// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "AttributeType.h"
#include "ChildOfRoot.h"
#include "GlTypes.h"
#include "GltfResult.h"
#include "accessor/AccessorFlags.h"
#include "accessor/Sparse.h"
#include "core/MemoryRange.h"
#include "core/Slice.h"
#include "gltf/StorageFormats.h"

#include <limits>
#include <optional>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#accessor

namespace ux3d::gltf {
class BufferView;
class Gltf;

class Accessor : public ChildOfRoot {
public:
    Accessor(uint32_t _id);
    Accessor(uint32_t _id, const Accessor& _source);

    Extensions getUsedExtensions() const override;

    GltfType getGltfType() const override { return GltfType::Accessor; }

    BufferView* getBufferView() { return m_pBufferView; }
    const BufferView* getBufferView() const { return m_pBufferView; }
    GltfResult setBufferView(BufferView& _bufferView);
    void unsetBufferView() { m_pBufferView = nullptr; }

    bool hasByteOffset() const { return m_byteOffset.has_value(); }
    uint32_t getByteOffset() const { return m_byteOffset.value_or(0); }
    GltfResult setByteOffset(uint32_t _byteOffset);

    AttributeType getType() const { return m_type; }
    GltfResult setType(AttributeType _type);

    ComponentType getComponentType() const { return m_componentType; }
    GltfResult setComponentType(ComponentType _componentType);

    uint32_t getCount() const { return m_count; }
    GltfResult setCount(uint32_t _count);

    bool hasNormalized() const { return m_normalized.has_value(); }
    bool isNormalized() const { return m_normalized.value_or(false); }
    GltfResult setNormalized(bool _normalized);
    void unsetNormalized() { m_normalized.reset(); }
    std::optional<bool> getNormalizedOptional() { return m_normalized; }

    core::Vector<double>& getMin() { return m_min; }
    const core::Vector<double>& getMin() const { return m_min; }

    core::Vector<double>& getMax() { return m_max; }
    const core::Vector<double>& getMax() const { return m_max; }
    // calculates min/max without setting it
    std::pair<core::Vector<double>, core::Vector<double>> getMinMaxFromData();
    template <typename T>
    std::pair<core::Vector<double>, core::Vector<double>> getMinMaxFromData();

    void recalculateMinMax(bool create = false);
    template <typename T>
    void recalculateMinMax(bool create);

    std::optional<Sparse>& getSparse() { return m_sparse; }
    const std::optional<Sparse>& getSparse() const { return m_sparse; }
    GltfResult createSparse();
    void destroySparse() { m_sparse.reset(); }

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const override;

    GltfResult getData(core::MemoryRange<const uint8_t>& _outData) const;

    template <typename T>
    GltfResult getDataAs(core::Slice<T>& _data) const;

    template <typename T>
    GltfResult getSparseDataAs(core::Slice<T>& _data) const;

    template <typename T>
    GltfResult getDataViewAs(core::MemoryRange<const T>& _data) const;

    StorageFormat getStorageFormat() const { return m_storageFormat; }
    GltfResult setStorageFormat(StorageFormat _storageFormat);

    UsageFlag getUsageFlag() const { return m_usageFlags; }
    GltfResult setUsageFlag(UsageFlag _usageFlag);

    size_t getSizeInBytes() const;
    size_t getTotalSizeInBytes() const;

    static size_t getByteLengthFrom(ComponentType _componentType);
    static size_t getByteLengthFrom(ComponentType _componentType, AttributeType _attributeType, bool _withPadding = false);
    static uint32_t getComponentCountFrom(AttributeType _attributeType);

    core::Vector<uint8_t> m_internalData;

private:
    GltfResult applySparse(core::MemoryRange<uint8_t> _data) const;

    BufferView* m_pBufferView = nullptr;
    std::optional<uint32_t> m_byteOffset;
    AttributeType m_type = AttributeType::None;
    ComponentType m_componentType = ComponentType::None;
    uint32_t m_count = 0u;
    std::optional<bool> m_normalized;
    core::Vector<double> m_min;
    core::Vector<double> m_max;
    std::optional<Sparse> m_sparse;
    StorageFormat m_storageFormat = StorageFormat::File;
    UsageFlag m_usageFlags = UsageFlags::None;
};

template <typename T>
inline GltfResult Accessor::getDataAs(core::Slice<T>& _data) const
{
    core::MemoryRange<const uint8_t> range;
    getData(range);

    if (range.empty()) {
        return GltfError::ObjectNotFound;
    }

    core::Slice<uint8_t> bytes;
    if (false == bytes.assign(range, _data.getAllocator())) {
        return GltfError::AllocationFailure;
    }

    _data = core::asSlice<T>(std::move(bytes));
    return GltfResult::Success;
}

template <typename T>
inline GltfResult Accessor::getSparseDataAs(core::Slice<T>& _data) const
{
    if (m_sparse.has_value() == false) {
        return GltfError::ObjectNotFound;
    }

    if (m_pBufferView != nullptr) {
        if (GltfResult result = getDataAs(_data); core::Failed(result)) {
            return result;
        }
    } else {
        const size_t elementSize = getByteLengthFrom(m_componentType, m_type);
        const size_t length = elementSize * m_count;

        const size_t elementCount = length / sizeof(T) + ((length % sizeof(T)) != 0 ? 1 : 0);
        if (_data.resizeWithValue(elementCount, {}) == false) {
            return GltfError::AllocationFailure;
        }
    }

    return applySparse(_data.getMemoryRange().template viewAs<uint8_t>());
}

template <typename T>
inline GltfResult Accessor::getDataViewAs(core::MemoryRange<const T>& _data) const
{
    core::MemoryRange<const uint8_t> range;
    GltfResult res = getData(range);

    if (res) {
        _data = range.viewAs<const T>();
    }

    return res;
}

template <typename T>
inline void Accessor::recalculateMinMax(bool create)
{
    if ((m_min.empty() || m_max.empty()) && create == false) {
        return;
    }

    m_min.clear();
    m_max.clear();

    uint32_t componentCount = getComponentCountFrom(m_type);

    m_min.resizeWithValue(componentCount, std::numeric_limits<double>::max());
    m_max.resizeWithValue(componentCount, -std::numeric_limits<double>::max());

    core::MemoryRange<const T> _data;
    GltfResult result = getDataViewAs(_data);
    if (result != GltfResult::Success) {
        return;
    }

    size_t attributeByteLength = getByteLengthFrom(m_componentType, m_type);
    size_t componentByteLength = getByteLengthFrom(m_componentType);
    size_t padding = 0; // padding per attribute measured in component width units
    if (attributeByteLength % 4 != 0 && m_type != AttributeType::Scalar) { // scalars have no padding
        padding = (4 - attributeByteLength % 4) / componentByteLength;
    }

    size_t totalPadding = 0;
    uint32_t component = 0;
    for (size_t i = 0; i < _data.size(); ++i) {
        if ((i + totalPadding) % 4 != 0 && i % componentCount == 0) {
            totalPadding += padding;
        }
        if (i + totalPadding >= _data.size()) {
            break;
        }

        m_min[component % componentCount] = std::min(m_min[component % componentCount], (double)_data[i + totalPadding]);
        m_max[component % componentCount] = std::max(m_max[component % componentCount], (double)_data[i + totalPadding]);

        component++;
    }
}

template <typename T>
inline std::pair<core::Vector<double>, core::Vector<double>> Accessor::getMinMaxFromData()
{
    core::Vector<double> resultMin;
    core::Vector<double> resultMax;
    uint32_t componentCount = getComponentCountFrom(m_type);

    resultMin.resizeWithValue(componentCount, std::numeric_limits<double>::max());
    resultMax.resizeWithValue(componentCount, -std::numeric_limits<double>::max());

    core::MemoryRange<const T> _data;
    GltfResult result = getDataViewAs(_data);
    if (result != GltfResult::Success) {
        return std::make_pair(resultMin, resultMax);
    }

    uint32_t component = 0;
    for (const auto& value : _data) {
        resultMin[component % componentCount] = std::min(resultMin[component % componentCount], (double)value);
        resultMax[component % componentCount] = std::max(resultMax[component % componentCount], (double)value);

        component++;
    }

    return std::make_pair(resultMin, resultMax);
}
}
