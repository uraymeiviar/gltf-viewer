// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/AudioEmitter.h"
#include <algorithm>

using namespace ux3d;
using namespace ux3d::renderer;

void AudioEmitter::updateTransform(const math::mat4& transform)
{
    position = math::vec3(transform[3]);
    orientation = math::normalize(math::vec3(transform * math::vec4(0, 0, -1, 0)));
}
