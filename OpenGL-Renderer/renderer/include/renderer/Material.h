// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Defines.h"
#include "gl/Sampler.h"
#include "gl/Uniform.h"
#include "gltf/extensions/AsoboExtensions.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace ux3d::renderer {
class RenderPass;

struct RenderSettings {
    bool enableSkinning = true;
    bool enableMorphing = true;
    bool enableSpecularGlossiness = true;
    bool enableUnlit = true;
    bool enableClearCoat = true;
    bool enableSheen = true;
    bool enableTransmission = true;
    bool enableVolume = true;
    bool enableIridescence = true;
    bool enableIOR = true;
    bool enableSpecular = true;
    bool enableEmissiveStrength = true;
};

class Material {
public:
    Material(gl::GL* gl);
    Material(const Material&) = default;
    Material(Material&&) = default;
    Material& operator=(const Material&) = default; // picking-material reuse (P4.6)

    /// Creates a new uniform or overwrites an old uniform with the same name.
    template <class... Ts>
    void setUniform(const core::string& name, const Ts&... data);

    Defines defines;
    bool isDoubleSided = false;
    // The material's declared ASOBO_* extensions, parsed with their defaults (A0.1). Shared, so the
    // per-frame material copies of the transmission path stay cheap; null when it has none.
    std::shared_ptr<const gltf::asobo::ExtensionSet> asobo;
    // ASOBO_material_invisible with "enabled" true (the schema default): the material is
    // not rendered, e.g. MSFS collision geometry. GltfRenderLayer leaves it out of scenes
    // unless showInvisibleMaterials is set.
    bool invisible = false;
    RenderPass* renderPass = nullptr;
    RenderSettings renderSettings;
    std::string fragmentShader;
    std::vector<gl::Sampler> samplers;
    std::vector<gl::Uniform> uniforms;

private:
    gl::GL* m_gl = nullptr;
};

template <class... Ts>
void Material::setUniform(const std::string& name, const Ts&... data)
{
    auto iter = std::find_if(uniforms.begin(), uniforms.end(), [&name](const gl::Uniform& uniform) {
        return uniform.name == name;
    });

    if (iter == uniforms.end()) {
        uniforms.emplace_back(name).write(data...);
    } else {
        iter->write(data...);
    }
}
}
