#include "gltf/AnimationKeyframeResolver.h"
#include "core/MemoryRange.h"
#include "gltf/Accessor.h"
#include "gltf/AccessorCreation.h"
#include "gltf/BufferTranscoder.h"
#include "gltf/animation/AnimationSampler.h"
#include "math/VectorFunctions.h"

using namespace ux3d;
using namespace ux3d::gltf;

AnimationKeyframeResolver::AnimationKeyframeResolver(Gltf& _gltf)
    : m_gltf(_gltf)
{
}

void AnimationKeyframeResolver::resolve(AnimationSampler& _sampler)
{
    resetData();

    Accessor* timeAccessor = _sampler.getInputAccessor();
    Accessor* dataAccessor = _sampler.getOutputAccessor();

    if (timeAccessor == nullptr || dataAccessor == nullptr) {
        return;
    }

    AttributeType dataAttributeType = dataAccessor->getType();
    ComponentType dataComponentType = dataAccessor->getComponentType();

    // get the keyframe data as floats

    core::MemoryRange<const float> keyframeTimes;
    timeAccessor->getDataViewAs<const float>(keyframeTimes);

    core::Vector<float> keyFrameDataVector;
    if (keyFrameDataVector.resize(dataAccessor->getCount() * static_cast<size_t>(Accessor::getComponentCountFrom(dataAttributeType))) == false) {
        return;
    }
    core::MemoryRange<float> keyframeData = keyFrameDataVector.getMemoryRange();

    core::MemoryRange<const uint8_t> byteData;
    dataAccessor->getData(byteData);

    BufferTranscoder transcoder(m_gltf, *dataAccessor);

    switch (dataComponentType) {
    case ComponentType::Float: {
        core::MemoryRange<const float> tempDataFloat = byteData.viewAs<const float>();
        for (size_t i = 0; i < tempDataFloat.size(); ++i) {
            keyframeData[i] = tempDataFloat[i];
        }
    } break;
    case ComponentType::Byte:
        transcoder.convertByteToIntermediate(byteData, keyframeData);
        break;
    case ComponentType::UnsignedByte:
        transcoder.convertUnsignedByteToIntermediate(byteData, keyframeData);
        break;
    case ComponentType::Short:
        transcoder.convertShortToIntermediate(byteData, keyframeData);
        break;
    case ComponentType::UnsignedShort:
        transcoder.convertUnsignedShortToIntermediate(byteData, keyframeData);
        break;
    default:
        return;
    }

    // remove unneeded keyframes

    auto stepCompare = [&keyframeData, dataAttributeType](size_t index, size_t offset) {
        if (index < offset) {
            return false;
        }

        bool isEqual = true;
        const size_t numberOfEntries = static_cast<size_t>(Accessor::getComponentCountFrom(dataAttributeType));
        for (size_t i = 0; i < numberOfEntries; ++i) {
            isEqual &= math::epsilonEqual<float>(keyframeData[index - offset + i], keyframeData[index + i], epsilon);
        }
        return isEqual;
    };

    // this is used for all linear interpolations except for rotations, so for translation, scale and weights
    auto linearCompare = [&keyframeData, &keyframeTimes, dataAttributeType](size_t index, size_t offset) {
        if (index < offset) {
            return false;
        }

        bool isEqual = true;
        const size_t numberOfEntries = static_cast<size_t>(Accessor::getComponentCountFrom(dataAttributeType));

        const float previousTime = keyframeTimes[(index - offset) / numberOfEntries];
        const float currentTime = keyframeTimes[index / numberOfEntries];
        const float nextTime = keyframeTimes[(index + offset) / numberOfEntries];

        const float timePreviousToCurrent = currentTime - previousTime;
        const float timePreviousToNext = nextTime - previousTime;

        for (size_t i = 0; i < numberOfEntries; ++i) {
            const float previousData = keyframeData[index - offset + i];
            const float currentData = keyframeData[index + i];
            const float nextData = keyframeData[index + offset + i];

            const float linearInterpolationData = math::mix(previousData, nextData, timePreviousToCurrent / timePreviousToNext);

            isEqual &= math::epsilonEqual<float>(linearInterpolationData, currentData, epsilon);
        }

        return isEqual;
    };

    // this is only used for linear interpolation of rotations (quaternions)
    auto sphericalLinearCompare = [&keyframeData, &keyframeTimes](size_t index, size_t offset) {
        if (index < offset) {
            return false;
        }

        const math::quat previousData = math::quat(keyframeData[index - offset], keyframeData[index - offset + 1], keyframeData[index - offset + 2], keyframeData[index - offset + 3]);
        const math::quat currentData = math::quat(keyframeData[index], keyframeData[index + 1], keyframeData[index + 2], keyframeData[index + 3]);
        const math::quat nextData = math::quat(keyframeData[index + offset], keyframeData[index + offset + 1], keyframeData[index + offset + 2], keyframeData[index + offset + 3]);

        const float previousTime = keyframeTimes[(index - offset) / 4];
        const float currentTime = keyframeTimes[index / 4];
        const float nextTime = keyframeTimes[(index + offset) / 4];

        const float timePreviousToCurrent = currentTime - previousTime;
        const float timePreviousToNext = nextTime - previousTime;

        const math::quat linearInterpolationData = math::slerp(previousData, nextData, timePreviousToCurrent / timePreviousToNext);

        return math::epsilonEqual<float>(linearInterpolationData.x, currentData.x, epsilon)
            && math::epsilonEqual<float>(linearInterpolationData.y, currentData.y, epsilon)
            && math::epsilonEqual<float>(linearInterpolationData.z, currentData.z, epsilon)
            && math::epsilonEqual<float>(linearInterpolationData.w, currentData.w, epsilon);
    };

    switch (_sampler.getInterpolation()) {
    case AnimationInterpolation::Step:
        removeKeyframes(&stepCompare, keyframeData, keyframeTimes, dataAttributeType);
        break;
    case AnimationInterpolation::Linear:
        if (dataAttributeType == AttributeType::Vec4) {
            removeKeyframes(&sphericalLinearCompare, keyframeData, keyframeTimes, dataAttributeType);
        } else {
            removeKeyframes(&linearCompare, keyframeData, keyframeTimes, dataAttributeType);
        }
        break;
    default:
        return;
    }

    // save the results in the original format

    core::Vector<uint8_t> resultDataVector;
    if (resultDataVector.resize(m_newKeyframeCount * static_cast<size_t>(Accessor::getComponentCountFrom(dataAttributeType))) == false) {
        return;
    }

    core::MemoryRange<uint8_t> resultData = resultDataVector.getMemoryRange();
    bool isAccessorNormalized = false;

    core::MemoryRange<float> newKeyFrameDataRange = m_newKeyframeData.getMemoryRange();

    switch (dataComponentType) {
    case ComponentType::Float:
        resultData = m_newKeyframeData.getMemoryRange().viewAs<uint8_t>();
        break;
    case ComponentType::Byte:
        isAccessorNormalized = true;
        transcoder.convertIntermediateToByte(newKeyFrameDataRange, resultData, true);
        break;
    case ComponentType::UnsignedByte:
        isAccessorNormalized = true;
        transcoder.convertIntermediateToUnsignedByte(newKeyFrameDataRange, resultData, true);
        break;
    case ComponentType::Short:
        isAccessorNormalized = true;
        transcoder.convertIntermediateToShort(newKeyFrameDataRange, resultData, true);
        break;
    case ComponentType::UnsignedShort:
        isAccessorNormalized = true;
        transcoder.convertIntermediateToUnsignedShort(newKeyFrameDataRange, resultData, true);
        break;
    default:
        return;
    }

    // create new Accessors if the optimization was successful

    if (m_newKeyframeCount < dataAccessor->getCount()) {
        AccessorCreation creator(m_gltf);
        Accessor* newTimeAccessor = creator.create(timeAccessor->getType(), timeAccessor->getComponentType(), m_newKeyframeTimes.getMemoryRange().viewAs<const uint8_t>(), m_newKeyframeCount);
        newTimeAccessor->recalculateMinMax();
        Accessor* newDataAccessor = creator.create(dataAttributeType, dataComponentType, resultData, m_newKeyframeCount);
        newDataAccessor->setNormalized(isAccessorNormalized);

        _sampler.setInputAccessor(*newTimeAccessor);
        _sampler.setOutputAccessor(*newDataAccessor);
    }
}

bool AnimationKeyframeResolver::isRotationOver90(core::MemoryRange<float>& keyframeData, size_t index) const
{
    if (index < 4) {
        return true;
    }

    const math::quat previousData = math::quat(keyframeData[index - 4], keyframeData[index - 4 + 1], keyframeData[index - 4 + 2], keyframeData[index - 4 + 3]);
    const math::quat currentData = math::quat(keyframeData[index], keyframeData[index + 1], keyframeData[index + 2], keyframeData[index + 3]);

    math::quat mix = math::conjugate(previousData) * currentData;
    const float angle = math::degrees(2 * approxAtan2(math::length(math::vec3(mix.x, mix.y, mix.z)), mix.w));
    return math::ceil(angle) >= 90.0f;
}

void AnimationKeyframeResolver::resetData()
{
    m_newKeyframeData.clear();
    m_newKeyframeTimes.clear();
    m_newKeyframeCount = 0;
}

float AnimationKeyframeResolver::approxAtan(float z) const
{
    const float magic_number1 = 0.97239411f;
    const float magic_number2 = -0.19194795f;
    return (magic_number1 + magic_number2 * z * z) * z;
}

float AnimationKeyframeResolver::approxAtan2(float y, float x) const
{
    if (x != 0.0f) {
        if (fabsf(x) > fabsf(y)) {
            const float z = y / x;
            if (x > 0.0) {
                // atan2(y, x) = atan(y/x) if x > 0
                return approxAtan(z);
            } else if (y >= 0.0) {
                // atan2(y, x) = atan(y/x) + PI if x < 0, y >= 0
                return approxAtan(z) + math::pi<float>();
            } else {
                // atan2(y, x) = atan(y/x) - PI if x < 0, y < 0
                return approxAtan(z) - math::pi<float>();
            }
        } else // Use property atan(y/x) = PI/2 - atan(x/y) if |y/x| > 1.
        {
            const float z = x / y;
            if (y > 0.0) {
                // atan2(y, x) = PI/2 - atan(x/y) if |y/x| > 1, y > 0
                return -approxAtan(z) + math::half_pi<float>();
            } else {
                // atan2(y, x) = -PI/2 - atan(x/y) if |y/x| > 1, y < 0
                return -approxAtan(z) - math::half_pi<float>();
            }
        }
    } else {
        if (y > 0.0f) {
            // x = 0, y > 0
            return math::half_pi<float>();
        } else if (y < 0.0f) {
            // x = 0, y < 0
            return -math::half_pi<float>();
        }
    }

    // x, y = 0
    return 0.0f;
}
