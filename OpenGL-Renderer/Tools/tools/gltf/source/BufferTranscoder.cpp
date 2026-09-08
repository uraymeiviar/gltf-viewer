#include "gltf/BufferTranscoder.h"
#include "gltf/Gltf.h"

using namespace ux3d;
using namespace ux3d::gltf;

BufferTranscoder::BufferTranscoder(Gltf& _gltf, Accessor& _accessor)
    : m_gltf(_gltf)
    , m_accessor(_accessor)
{
}

// transcodes a buffer from one component type to another by creating a new buffer and copying the data in the new format
GltfResult BufferTranscoder::transcode(ComponentType _newComponentType, bool _isNormalized)
{
    if (m_accessor.getComponentType() == _newComponentType && m_accessor.isNormalized() == _isNormalized) {
        return GltfResult::Success;
    }

    BufferView* bufferView = m_accessor.getBufferView();
    if (bufferView == nullptr) {
        return GltfError::ObjectNotFound;
    }

    Buffer* buffer = bufferView->getBuffer();
    if (buffer == nullptr) {
        return GltfError::ObjectNotFound;
    }

    core::MemoryRange<const uint8_t> accessorData;
    m_accessor.getData(accessorData);
    ComponentType accessorComponentType = m_accessor.getComponentType();
    m_accessor.recalculateMinMax();

    Buffer* newBuffer = m_gltf.create<Buffer>();
    newBuffer->setName(buffer->getName());
    newBuffer->setExtras(buffer->getExtras());
    newBuffer->setStorageFormat(buffer->getStorageFormat());

    size_t newStride = Accessor::getByteLengthFrom(_newComponentType, m_accessor.getType());
    // calculate stride with padding
    if (newStride % 4 != 0) {
        newStride += 4 - (newStride % 4);
    }
    newBuffer->setByteLength(m_accessor.getCount() * static_cast<uint32_t>(newStride));

    core::Vector<uint8_t> newDataVector;
    if (newDataVector.resize(m_accessor.getCount() * static_cast<size_t>(newStride)) == false) {
        return GltfError::AllocationFailure;
    }
    core::MemoryRange<uint8_t> newData = newDataVector.getMemoryRange();

    // does mesh quantization encoding for attribute accessors and regular int conversion indices accessors and joints (also indices).
    if (m_accessor.getUsageFlag().isSet(UsageFlags::MeshIndices) || m_accessor.getUsageFlag().isSet(UsageFlags::MeshJoints)) {
        core::Vector<uint32_t> intermediateDataVector;
        if (intermediateDataVector.resize(static_cast<size_t>(m_accessor.getCount()) * Accessor::getComponentCountFrom(m_accessor.getType())) == false) {
            return GltfError::AllocationFailure;
        }
        core::MemoryRange<uint32_t> intermediateData = intermediateDataVector.getMemoryRange();

        switch (accessorComponentType) {
        case ComponentType::UnsignedByte:
            convertIndicesToIntermediate<uint8_t>(accessorData, intermediateData);
            break;
        case ComponentType::UnsignedShort:
            convertIndicesToIntermediate<uint16_t>(accessorData, intermediateData);
            break;
        case ComponentType::UnsignedInt:
            convertIndicesToIntermediate<uint32_t>(accessorData, intermediateData);
            break;
        default:
            return GltfError::InvalidInput;
            break;
        }

        switch (_newComponentType) {
        case ComponentType::UnsignedByte:
            convertIndicesFromIntermediate<uint8_t>(intermediateData, newData);
            break;
        case ComponentType::UnsignedShort:
            convertIndicesFromIntermediate<uint16_t>(intermediateData, newData);
            break;
        case ComponentType::UnsignedInt:
            convertIndicesFromIntermediate<uint32_t>(intermediateData, newData);
            break;
        default:
            return GltfError::InvalidInput;
            break;
        }
    } else {
        core::Vector<float> intermediateDataVector;
        if (intermediateDataVector.resize(static_cast<size_t>(m_accessor.getCount()) * Accessor::getComponentCountFrom(m_accessor.getType())) == false) {
            return GltfError::AllocationFailure;
        }

        bool skipDataNormalization = false;
        if (m_accessor.getUsageFlag().isSet(UsageFlags::MeshNormals) || m_accessor.getUsageFlag().isSet(UsageFlags::MeshTangents) || m_accessor.getUsageFlag().isSet(UsageFlags::AnimationRotations) || m_accessor.getUsageFlag().isSet(UsageFlags::AnimationWeights)) {
            skipDataNormalization = true;
            // Normals and Tangents are normalized per vector. We do not want to break this vector normalization by doing a data (complete buffer) normalization
            // This makes a difference when the values -1 and +1 are not included in the whole data
        }

        core::MemoryRange<float> intermediateData = intermediateDataVector.getMemoryRange();

        bool wasSigned = false;

        switch (accessorComponentType) {
        case ComponentType::Byte:
            wasSigned = true;
            convertByteToIntermediate(accessorData, intermediateData);
            break;
        case ComponentType::UnsignedByte:
            convertUnsignedByteToIntermediate(accessorData, intermediateData);
            break;
        case ComponentType::Short:
            wasSigned = true;
            convertShortToIntermediate(accessorData, intermediateData);
            break;
        case ComponentType::UnsignedShort:
            convertUnsignedShortToIntermediate(accessorData, intermediateData);
            break;
        case ComponentType::Float:
            wasSigned = true;
            convertFloatToIntermediate(accessorData, intermediateData, skipDataNormalization);
            break;
        default:
            return GltfError::InvalidInput;
            break;
        }

        switch (_newComponentType) {
        case ComponentType::Byte:
            convertIntermediateToByte(intermediateData, newData, _isNormalized);
            break;
        case ComponentType::UnsignedByte:
            convertIntermediateToUnsignedByte(intermediateData, newData, _isNormalized);
            break;
        case ComponentType::Short:
            convertIntermediateToShort(intermediateData, newData, _isNormalized);
            break;
        case ComponentType::UnsignedShort:
            convertIntermediateToUnsignedShort(intermediateData, newData, _isNormalized);
            break;
        case ComponentType::Float:
            convertIntermediateToFloat(intermediateData, newData, wasSigned, skipDataNormalization);
            break;
        default:
            return GltfError::InvalidInput;
            break;
        }
    }

    newBuffer->setData(newData);

    BufferView* newBufferView = m_gltf.create<BufferView>();
    newBufferView->setBuffer(*newBuffer);
    newBufferView->setByteOffset(0u);
    newBufferView->setByteLength(newBuffer->getByteLength());

    if (!m_accessor.getUsageFlag().isSet(UsageFlags::MeshIndices) && !m_accessor.getUsageFlag().isSet(UsageFlags::AnimationWeights)) {
        // Byte stride is not allowed for mesh indices and animation weights (they have to be tightly packed)
        newBufferView->setByteStride(static_cast<uint32_t>(newStride));
    }

    m_accessor.setBufferView(*newBufferView);
    m_accessor.setByteOffset(0u);
    m_accessor.setComponentType(_newComponentType);
    m_accessor.recalculateMinMax();

    GltfResult normalizationResult = m_accessor.setNormalized(_isNormalized);
    if (core::Succeeded(normalizationResult) == false) {
        return normalizationResult;
    }

    return GltfResult::Success;
}

void BufferTranscoder::convertByteToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer)
{
    core::MemoryRange<const int8_t> inBufferTyped = _inBuffer.viewAs<const int8_t>();
    const size_t padding = getPadding(ComponentType::Byte);

    convertTypeToIntermediate<int8_t>(inBufferTyped, _outBuffer, padding, false);
}

void BufferTranscoder::convertUnsignedByteToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer)
{
    core::MemoryRange<const uint8_t> inBufferTyped = _inBuffer.viewAs<const uint8_t>();
    const size_t padding = getPadding(ComponentType::UnsignedByte);

    convertTypeToIntermediate<uint8_t>(inBufferTyped, _outBuffer, padding, true);
}

void BufferTranscoder::convertShortToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer)
{
    core::MemoryRange<const int16_t> inBufferTyped = _inBuffer.viewAs<const int16_t>();
    const size_t padding = getPadding(ComponentType::Short);

    convertTypeToIntermediate<int16_t>(inBufferTyped, _outBuffer, padding, false);
}

void BufferTranscoder::convertUnsignedShortToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer)
{
    core::MemoryRange<const uint16_t> inBufferTyped = _inBuffer.viewAs<const uint16_t>();
    const size_t padding = getPadding(ComponentType::UnsignedShort);

    convertTypeToIntermediate<uint16_t>(inBufferTyped, _outBuffer, padding, true);
}

void BufferTranscoder::convertFloatToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer, bool _skipDataNormalization)
{
    core::MemoryRange<const float> inBufferTyped = _inBuffer.viewAs<const float>();

    const auto& [minimums, maximums] = m_accessor.getMinMaxFromData();
    for (size_t i = 0; i < inBufferTyped.size(); ++i) {
        if (_skipDataNormalization) {
            // necessary to preserve vector normalization
            _outBuffer[i] = (inBufferTyped[i] + 1.0f) / 2.0f;
        } else {
            _outBuffer[i] = normalizeUsingMinMax(inBufferTyped[i], minimums, maximums);
        }
    }
}

void BufferTranscoder::convertIntermediateToByte(
    core::MemoryRange<float>& _inBuffer,
    core::MemoryRange<uint8_t>& _outBuffer,
    const bool _isNormalized)
{
    core::MemoryRange<int8_t> outBufferTyped = _outBuffer.viewAs<int8_t>();
    const size_t padding = getPadding(ComponentType::Byte);

    convertIntermediateToType<int8_t>(_inBuffer, outBufferTyped, padding, _isNormalized, false);
}

void BufferTranscoder::convertIntermediateToUnsignedByte(
    core::MemoryRange<float>& _inBuffer,
    core::MemoryRange<uint8_t>& _outBuffer,
    const bool _isNormalized)
{
    const size_t padding = getPadding(ComponentType::UnsignedByte);

    convertIntermediateToType<uint8_t>(_inBuffer, _outBuffer, padding, _isNormalized, true);
}

void BufferTranscoder::convertIntermediateToShort(
    core::MemoryRange<float>& _inBuffer,
    core::MemoryRange<uint8_t>& _outBuffer,
    const bool _isNormalized)
{
    core::MemoryRange<int16_t> outBufferTyped = _outBuffer.viewAs<int16_t>();
    const size_t padding = getPadding(ComponentType::Short);

    convertIntermediateToType<int16_t>(_inBuffer, outBufferTyped, padding, _isNormalized, false);
}

void BufferTranscoder::convertIntermediateToUnsignedShort(
    core::MemoryRange<float>& _inBuffer,
    core::MemoryRange<uint8_t>& _outBuffer,
    const bool _isNormalized)
{
    core::MemoryRange<uint16_t> outBufferTyped = _outBuffer.viewAs<uint16_t>();
    const size_t padding = getPadding(ComponentType::UnsignedShort);

    convertIntermediateToType<uint16_t>(_inBuffer, outBufferTyped, padding, _isNormalized, true);
}

void BufferTranscoder::convertIntermediateToFloat(
    core::MemoryRange<float>& _inBuffer,
    core::MemoryRange<uint8_t>& _outBuffer,
    const bool _wasSigned, const bool skipDataNormalization)
{
    core::MemoryRange<float> outBufferTyped = _outBuffer.viewAs<float>();

    const auto& [minimums, maximums] = m_accessor.getMinMaxFromData();
    for (size_t i = 0; i < _inBuffer.size(); ++i) {
        if (_wasSigned) {
            if (skipDataNormalization) {
                outBufferTyped[i] = (_inBuffer[i] * 2.0f) - 1.0f;
            } else {
                outBufferTyped[i] = denormalizeUsingMinMax(_inBuffer[i], minimums, maximums);
            }
        } else {
            if (skipDataNormalization) {
                outBufferTyped[i] = _inBuffer[i];
            } else {
                outBufferTyped[i] = denormalizeUsingMinMax(_inBuffer[i], minimums, maximums);
            }
        }
    }
}

size_t BufferTranscoder::getPadding(const ComponentType _componentType)
{
    if (m_accessor.getUsageFlag().isSet(gltf::UsageFlags::AnimationWeights) || m_accessor.getUsageFlag().isSet(gltf::UsageFlags::MeshWeights)) {
        return 0;
    }

    const size_t attributeByteLength = m_accessor.getByteLengthFrom(_componentType, m_accessor.getType());
    const size_t componentByteLength = m_accessor.getByteLengthFrom(_componentType);
    size_t padding = 0; // padding per attribute measured in component width units
    if (attributeByteLength % 4 != 0) {
        padding = (4 - attributeByteLength % 4) / componentByteLength;
    }

    return padding;
}

float BufferTranscoder::normalizeUsingMinMax(const float _value, const core::Vector<double>& _minimums, const core::Vector<double>& _maximums)
{
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();

    for (size_t i = 0; i < _minimums.size(); ++i) {
        min = std::min(min, _minimums[i]);
        max = std::max(max, _maximums[i]);
    }

    return static_cast<float>((_value - min) / (max - min));
}

float BufferTranscoder::denormalizeUsingMinMax(const float _value, const core::Vector<double>& _minimums, const core::Vector<double>& _maximums)
{
    double min = std::numeric_limits<double>::max();
    double max = std::numeric_limits<double>::lowest();

    for (size_t i = 0; i < _minimums.size(); ++i) {
        min = std::min(min, _minimums[i]);
        max = std::max(max, _maximums[i]);
    }

    return static_cast<float>((_value * (max - min)) + min);
}
