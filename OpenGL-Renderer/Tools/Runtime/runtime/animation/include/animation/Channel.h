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

template <class T>
void Channel<T>::update(float animationTime)
{
    if (m_keyframes.empty()) {
        return;
    }

    // Find the frame after the animation time
    auto higherFrame = std::find_if(m_keyframes.begin(), m_keyframes.end(),
        [&animationTime](const Keyframe& keyframe) -> bool {
            return keyframe.time >= animationTime;
        });

    // The animation time is before the first frame
    if (higherFrame == m_keyframes.begin()) {
        m_animationTarget.update(higherFrame->value);
        return;
    }

    // The animation time is behind the last frame
    if (higherFrame == m_keyframes.end()) {
        m_animationTarget.update(m_keyframes.back().value);
        return;
    }

    // The standard case between two frames
    auto lowerFrame = higherFrame - 1;
    float interpolant = (animationTime - lowerFrame->time) / (higherFrame->time - lowerFrame->time);
    m_animationTarget.update(m_sampler(lowerFrame->value, lowerFrame->outTangent, higherFrame->value, higherFrame->inTangent, interpolant));
}
}
