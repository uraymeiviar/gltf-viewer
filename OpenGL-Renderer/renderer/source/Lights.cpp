// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/Lights.h"
#include "core/LinkedList.h"

#include <algorithm>

void ux3d::renderer::Light::updateTransform(const math::mat4& transform)
{
    position = math::vec3(transform[3]);
    direction = math::normalize(math::vec3(transform * math::vec4(0, 0, -1, 0)));
}

ux3d::renderer::LightData::LightData(gl::GL* gl)
    : m_gl(gl)
{
}

void ux3d::renderer::LightData::update(const std::vector<Light*>& lights)
{
    m_uniforms.clear();

    for (size_t i = 0; i < lights.size(); ++i) {
        const auto& light = lights[i];
        m_uniforms.emplace_back("u_Lights[" + std::to_string(i) + "].direction").write(light->direction);
        m_uniforms.emplace_back("u_Lights[" + std::to_string(i) + "].range").write(light->range);
        m_uniforms.emplace_back("u_Lights[" + std::to_string(i) + "].color").write(light->color);
        m_uniforms.emplace_back("u_Lights[" + std::to_string(i) + "].intensity").write(light->intensity);
        m_uniforms.emplace_back("u_Lights[" + std::to_string(i) + "].position").write(light->position);
        m_uniforms.emplace_back("u_Lights[" + std::to_string(i) + "].innerConeCos").write(light->innerConeCos);
        m_uniforms.emplace_back("u_Lights[" + std::to_string(i) + "].outerConeCos").write(light->outerConeCos);
        m_uniforms.emplace_back("u_Lights[" + std::to_string(i) + "].type").write(light->type);
    }
}

size_t ux3d::renderer::LightData::getLightCount() const
{
    return m_uniforms.size() / 8;
}
