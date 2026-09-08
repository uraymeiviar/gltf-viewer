#pragma once

#include "GlTypes.h"
#include "core/MemoryRange.h"
#include "core/Vector.h"
#include "gltf/Accessor.h"
#include "gltf/GltfResult.h"
#include "math/Math.h"
#include "math/VectorTypes.h"

#include <limits>

namespace ux3d::gltf {
class Gltf;

class BufferTranscoder {
public:
    BufferTranscoder(Gltf& _gltf, Accessor& _accessor);
    GltfResult transcode(ComponentType _newComponentType, bool _isNormalized);

    void convertByteToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer);
    void convertUnsignedByteToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer);
    void convertShortToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer);
    void convertUnsignedShortToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer);
    void convertFloatToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<float>& _outBuffer, bool _skipDataNormalization);

    void convertIntermediateToByte(core::MemoryRange<float>& _inBuffer, core::MemoryRange<uint8_t>& _outBuffer, bool _isNormalized);
    void convertIntermediateToUnsignedByte(core::MemoryRange<float>& _inBuffer, core::MemoryRange<uint8_t>& _outBuffer, bool _isNormalized);
    void convertIntermediateToShort(core::MemoryRange<float>& _inBuffer, core::MemoryRange<uint8_t>& _outBuffer, bool _isNormalized);
    void convertIntermediateToUnsignedShort(core::MemoryRange<float>& _inBuffer, core::MemoryRange<uint8_t>& _outBuffer, bool _isNormalized);
    void convertIntermediateToFloat(core::MemoryRange<float>& _inBuffer, core::MemoryRange<uint8_t>& _outBuffer, bool _wasSigned, const bool _skipDataNormalization);

private:
    Gltf& m_gltf;
    Accessor& m_accessor;

    // converts data stored in a MemoryRange from one format to uint64_t by copying it to another MemoryRange
    template <typename FromType>
    static void convertIndicesToIntermediate(const core::MemoryRange<const uint8_t>& _inBuffer, core::MemoryRange<uint32_t>& _outBuffer)
    {
        core::MemoryRange<const FromType> inBufferTyped = _inBuffer.viewAs<const FromType>();

        for (size_t i = 0; i < _outBuffer.size(); ++i) {
            _outBuffer[i] = static_cast<uint32_t>(inBufferTyped[i]);
        }
    }

    // converts data stored in a MemoryRange from uint64_t to another format by copying it to another MemoryRange
    template <typename ToType>
    static void convertIndicesFromIntermediate(core::MemoryRange<uint32_t>& _inBuffer, core::MemoryRange<uint8_t>& _outBuffer)
    {
        core::MemoryRange<ToType> outBufferTyped = _outBuffer.viewAs<ToType>();

        for (size_t i = 0; i < _inBuffer.size(); ++i) {
            outBufferTyped[i] = static_cast<ToType>(_inBuffer[i]);
        }
    }

    // converts data stored in a MemoryRange from one format to float by copying it to another MemoryRange
    template <typename FromType>
    void convertTypeToIntermediate(core::MemoryRange<const FromType>& _inBufferTyped, core::MemoryRange<float>& _outBuffer, const size_t _padding, const bool _isUnsigned)
    {
        const uint32_t componentCount = m_accessor.getComponentCountFrom(m_accessor.getType());

        const auto& [minimums, maximums] = m_accessor.getMinMaxFromData();
        const bool isNormalized = m_accessor.isNormalized();
        constexpr float maxTypeValue = static_cast<float>(std::numeric_limits<FromType>::max());

        size_t totalPadding = 0;
        for (size_t i = 0; i < _outBuffer.size(); ++i) {
            if ((i + totalPadding) % 4 != 0 && i % componentCount == 0) {
                totalPadding += _padding;
            }

            if (isNormalized) {
                if (_isUnsigned) {
                    _outBuffer[i] = static_cast<float>(_inBufferTyped[i + totalPadding] / maxTypeValue);
                } else {
                    _outBuffer[i] = (std::max<float>(_inBufferTyped[i + totalPadding] / maxTypeValue, -1.0f) + 1.0f) * 0.5f;
                }
            } else {
                _outBuffer[i] = normalizeUsingMinMax(static_cast<float>(_inBufferTyped[i + totalPadding]), minimums, maximums);
            }
        }
    }

    // converts data stored in a MemoryRange from float to another format by copying it to another MemoryRange
    template <typename ToType>
    void convertIntermediateToType(core::MemoryRange<float>& _inBuffer, core::MemoryRange<ToType>& _outBufferTyped, const size_t _padding, const bool _isNormalized, const bool _isUnsigned)
    {
        const uint32_t componentCount = m_accessor.getComponentCountFrom(m_accessor.getType());

        const auto& [minimums, maximums] = m_accessor.getMinMaxFromData();
        constexpr float maxTypeValue = static_cast<float>(std::numeric_limits<ToType>::max());
        constexpr float lowestTypeValue = static_cast<float>(std::numeric_limits<ToType>::lowest());

        size_t totalPadding = 0;
        for (size_t i = 0; i < _inBuffer.size(); ++i) {
            if ((i + totalPadding) % 4 != 0 && i % componentCount == 0) {
                totalPadding += _padding;
            }

            if (_isNormalized) {
                if (_isUnsigned) {
                    _outBufferTyped[i + totalPadding] = static_cast<ToType>(math::round(_inBuffer[i] * maxTypeValue));
                } else {
                    // Spec: [BYTE] c = round(f * 127.0)
                    // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md
                    _outBufferTyped[i + totalPadding] = static_cast<ToType>(math::round(((2.0f * _inBuffer[i]) - 1.0f) * maxTypeValue));
                }
            } else {
                float value = _inBuffer[i];
                if (m_accessor.isNormalized() == false) {
                    value = denormalizeUsingMinMax(value, minimums, maximums);
                    value = math::clamp(value, lowestTypeValue, maxTypeValue);
                }
                _outBufferTyped[i + totalPadding] = static_cast<ToType>(math::round(value));
            }
        }
    }

    size_t getPadding(ComponentType _type);
    float normalizeUsingMinMax(float _value, const core::Vector<double>& _minimums, const core::Vector<double>& _maximums);
    float denormalizeUsingMinMax(float _value, const core::Vector<double>& _minimums, const core::Vector<double>& _maximums);
};
}
