// Copyright (c) 2019 UX3D GmbH. All rights reserved.
// Authors: David Labode

#pragma once

#include "AL/al.h"
#include "math/VectorTypes.h"
#include <vector>

namespace ux3d::renderer {
struct AudioEmitter {
    enum class Type : int32_t {
        Invalid = ~0,
        Positional = 0,
        Global = 1
    };
    enum class DistanceModel : int32_t {
        Linear = 0,
        Inverse = 1,
        Exponential = 2
    };

    // general emitter data
    Type type = Type::Invalid;
    ALfloat gain = 1.0;
    ALboolean playing = false;
    ALboolean loop = false;
    ALuint source = 0;
    float durInSec = 0.0f; // duration in seconds

    // positional data
    ALfloat coneInnerAngle = 6.283185307179586;
    ALfloat coneOuterAngle = 6.283185307179586;
    ALfloat coneOuterGain = 0.0;
    ALfloat maxDistance = 1000.0;
    ALfloat refDistance = 1.0;
    ALfloat rollofFactor = 1.0;
    DistanceModel distanceModel = DistanceModel::Inverse;

    // transform data
    math::vec3 position = math::vec3(0);
    math::vec3 orientation = math::vec3(0.0, 0.0, 1.0); // Z+ as per spec

    void updateTransform(const math::mat4& transform);

    ALuint buffer = 0;

    // State keeping

    // used so that non looping emitters do not repeat
    bool hasPlayed = false;

    // used to get paused tracks into playing mode again
    bool isPaused = false;
};
}
