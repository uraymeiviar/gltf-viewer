#pragma once
#include "AttributeType.h"
#include "core/Vector.h"

#include <limits>

namespace ux3d::gltf {
class AnimationSampler;
class Gltf;

class AnimationKeyframeResolver {
public:
    AnimationKeyframeResolver(Gltf& _gltf);
    void resolve(AnimationSampler& _sampler);

private:
    Gltf& m_gltf;

    core::Vector<float> m_newKeyframeData;
    core::Vector<float> m_newKeyframeTimes;
    uint32_t m_newKeyframeCount = 0;

    // this magic number epsilon seems to give good results when comparing keyframe data
    static constexpr float epsilon = std::numeric_limits<float>::epsilon();

    bool isRotationOver90(core::MemoryRange<float>& keyframeData, size_t index) const;
    void resetData();

    // math helper functions, we use fast approximations, because we are not interested in detailed results
    float approxAtan(float z) const;
    float approxAtan2(float x, float y) const;

    template <typename T>
    void removeKeyframes(const T* compareFunction, core::MemoryRange<float>& keyframeData, core::MemoryRange<const float>& keyframeTimes, AttributeType type)
    {
        switch (type) {
        case gltf::AttributeType::Scalar:
            for (size_t i = 0; i < keyframeData.size(); i += 1) {
                if (i == 0 || i == keyframeData.size() - 1) {
                    // always save first and last values
                    m_newKeyframeData.push_back(keyframeData[i]);
                    m_newKeyframeTimes.push_back(keyframeTimes[i]);
                } else if ((*compareFunction)(i, 1) == false) {
                    // only save in-between values that differ from the value before
                    m_newKeyframeData.push_back(keyframeData[i]);
                    m_newKeyframeTimes.push_back(keyframeTimes[i]);
                }
            }

            m_newKeyframeCount = static_cast<uint32_t>(m_newKeyframeData.size());
            break;
        case gltf::AttributeType::Vec3:
            for (size_t i = 0; i < keyframeData.size(); i += 3) {
                if (i == 0 || i == keyframeData.size() - 3) {
                    // always save first and last values
                    m_newKeyframeData.push_back(keyframeData[i]);
                    m_newKeyframeData.push_back(keyframeData[i + 1]);
                    m_newKeyframeData.push_back(keyframeData[i + 2]);

                    m_newKeyframeTimes.push_back(keyframeTimes[i / 3]);
                } else if ((*compareFunction)(i, 3) == false) {
                    // only save in-between values that differ from the value before
                    m_newKeyframeData.push_back(keyframeData[i]);
                    m_newKeyframeData.push_back(keyframeData[i + 1]);
                    m_newKeyframeData.push_back(keyframeData[i + 2]);

                    m_newKeyframeTimes.push_back(keyframeTimes[i / 3]);
                }
            }

            m_newKeyframeCount = static_cast<uint32_t>(m_newKeyframeData.size() / 3);
            break;
        case gltf::AttributeType::Vec4:
            for (size_t i = 0; i < keyframeData.size(); i += 4) {
                if (i == 0 || i == keyframeData.size() - 4) {
                    // always save first and last values
                    m_newKeyframeData.push_back(keyframeData[i]);
                    m_newKeyframeData.push_back(keyframeData[i + 1]);
                    m_newKeyframeData.push_back(keyframeData[i + 2]);
                    m_newKeyframeData.push_back(keyframeData[i + 3]);

                    m_newKeyframeTimes.push_back(keyframeTimes[i / 4]);
                } else if (isRotationOver90(keyframeData, i) || (*compareFunction)(i, 4) == false) {
                    // only save in-between values that differ from the value before
                    // and values that describe a rotation over 90 degrees to make sure that we always turn in the correct direction
                    m_newKeyframeData.push_back(keyframeData[i]);
                    m_newKeyframeData.push_back(keyframeData[i + 1]);
                    m_newKeyframeData.push_back(keyframeData[i + 2]);
                    m_newKeyframeData.push_back(keyframeData[i + 3]);

                    m_newKeyframeTimes.push_back(keyframeTimes[i / 4]);
                }
            }

            m_newKeyframeCount = static_cast<uint32_t>(m_newKeyframeData.size() / 4);
            break;
        default:
            return;
        }
    }
};
}
