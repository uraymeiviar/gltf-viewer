// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <vector>

#include "gl/Uniform.h"
#include "gl/UniformBlock.h"
#include "math/VectorTypes.h"

namespace ux3d::renderer {
struct Light {
    enum class Type : int32_t {
        Invalid = ~0,
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    math::vec3 direction = math::vec3(0);
    float range = 0;

    math::vec3 color = math::vec3(1);
    float intensity = 1;

    math::vec3 position = math::vec3(0);
    float innerConeCos = 0;

    float outerConeCos = math::pi<float>() / 4.f;
    Type type = Type::Invalid;

    void updateTransform(const math::mat4& transform);
};

class LightData {
public:
    LightData(gl::GL* gl);

    void update(const std::vector<Light*>& lights);

    const std::vector<gl::Uniform>& getUniforms() const { return m_uniforms; }

    size_t getLightCount() const;

private:
    gl::GL* m_gl;
    std::vector<gl::Uniform> m_uniforms;
};
}
