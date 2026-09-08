// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#pragma once

#include "Channel.h"
#include "math/VectorTypes.h"

#include <variant>
#include <vector>

namespace ux3d::animation {
class Animation {
public:
    using ChannelVariant = std::variant<
        Channel<math::vec2>,
        Channel<math::vec3>,
        Channel<math::vec4>,
        Channel<math::quat>,
        Channel<float>,
        Channel<std::vector<float>>>;

    Animation(float duration);

    void updateChannels(float animationTime);

    const float duration;
    bool isPlaying = false;
    std::vector<ChannelVariant> channels;
};
}
