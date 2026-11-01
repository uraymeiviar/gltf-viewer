// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#pragma once

#include "IAnimationTarget.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace ux3d::animation {

template <class T>
class Channel {
public:
    using SamplerFunction = std::function<T(const T&, const T&, const T&, const T&, float)>;
    struct Keyframe {
        float time = 0;
        T inTangent {};
        T value {};
        T outTangent {};
    };

    Channel(IAnimationTarget<T>& animationTarget, const SamplerFunction& sampler);

    void addKeyframe(float time, const T& value);
    void addKeyframe(float time, const T& inTangent, const T& value, const T& outTangent);
    void update(float animationTime);

private:
    IAnimationTarget<T>& m_animationTarget;
    std::vector<Keyframe> m_keyframes;
    SamplerFunction m_sampler;
};

template <class T>
Channel<T>::Channel(
    IAnimationTarget<T>& target,
    const SamplerFunction& sampler)
    : m_animationTarget(target)
    , m_keyframes()
    , m_sampler(sampler)
{
}

template <class T>
void Channel<T>::addKeyframe(float time, const T& value)
{
    m_keyframes.emplace_back(Keyframe { time, {}, value, {} });
}

template <class T>
void Channel<T>::addKeyframe(float time, const T& inTangent, const T& value, const T& outTangent)
{
    m_keyframes.emplace_back(Keyframe { time, inTangent, value, outTangent });
}

// Scales a CUBICSPLINE tangent by the keyframe interval (glTF: m = (t_k+1 - t_k) * tangent); STEP and LINEAR
// keyframes carry empty / zero tangents, which stay unchanged.
template <class T>
T scaleTangent(const T& tangent, float delta)
{
    return tangent * delta;
}

template <>
inline std::vector<float> scaleTangent(const std::vector<float>& tangent, float delta)
{
    std::vector<float> scaled(tangent);
    for (float& value : scaled) {
        value *= delta;
    }
    return scaled;
}

template <class T>
void Channel<T>::update(float animationTime)
{
    if (m_keyframes.empty()) {
        return;
    }

    // glTF: the keyframe interval of time t starts at the last key with time <= t. Find the first key after t.
    auto higherFrame = std::find_if(m_keyframes.begin(), m_keyframes.end(),
        [&animationTime](const Keyframe& keyframe) -> bool {
            return keyframe.time > animationTime;
        });

    // The animation time is before the first frame: the first value holds
    if (higherFrame == m_keyframes.begin()) {
        m_animationTarget.update(higherFrame->value);
        return;
    }

    // The animation time is at or behind the last frame: the last value holds
    if (higherFrame == m_keyframes.end()) {
        m_animationTarget.update(m_keyframes.back().value);
        return;
    }

    auto lowerFrame = higherFrame - 1;

    // Exactly on a key: its value (STEP: the key's own output, not the previous one; LINEAR / CUBICSPLINE: the key
    // value exactly, without the interpolation's rounding)
    if (animationTime == lowerFrame->time) {
        m_animationTarget.update(lowerFrame->value);
        return;
    }

    // The standard case between two frames
    const float delta = higherFrame->time - lowerFrame->time;
    const float interpolant = (animationTime - lowerFrame->time) / delta;
    m_animationTarget.update(m_sampler(lowerFrame->value, scaleTangent(lowerFrame->outTangent, delta), higherFrame->value,
        scaleTangent(higherFrame->inTangent, delta), interpolant));
}
}
