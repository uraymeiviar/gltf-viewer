// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#include "animation/Animation.h"

ux3d::animation::Animation::Animation(float duration)
    : duration(duration)
{
}

void ux3d::animation::Animation::updateChannels(float animationTime)
{
    if (!isPlaying) {
        return;
    }
    animationTime = math::clamp(animationTime, 0.0f, duration);

    for (auto& channel : channels) {
        std::visit([animationTime](auto& channel) {
            channel.update(animationTime);
        },
            channel);
    }
}
