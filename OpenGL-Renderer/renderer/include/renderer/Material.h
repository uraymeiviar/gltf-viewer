// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Defines.h"
#include "DrawOrder.h"
#include "gl/Sampler.h"
#include "gl/Uniform.h"
#include "gltf/extensions/AsoboExtensions.h"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

namespace ux3d::renderer {
class RenderPass;

// Display policy only. Material IDs belong to the loaded document; the application
// clears this per-viewport state when replacing that document, like isolation.
struct MaterialExtensionOptions {
    std::unordered_map<uint32_t, std::vector<std::string>> disabled;

    bool applies(uint32_t material, std::string_view extension) const
    {
        const auto it = disabled.find(material);
        return it == disabled.end()
            || std::find(it->second.begin(), it->second.end(), extension) == it->second.end();
    }

    void setApplied(uint32_t material, std::string_view extension, bool applied)
    {
        if (applied) {
            const auto it = disabled.find(material);
            if (it == disabled.end()) return;
            std::erase(it->second, extension);
            if (it->second.empty()) disabled.erase(it);
        } else if (applies(material, extension)) {
            disabled[material].emplace_back(extension);
        }
    }

    bool operator==(const MaterialExtensionOptions&) const = default;
};

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
    bool enableAnisotropy = true;
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
    // Legacy MSFS material type in extras; exact SDK values only (no name guessing).
    enum class AsoboMaterialCode { None, Glass, Windshield };
    AsoboMaterialCode asoboMaterialCode = AsoboMaterialCode::None;
    // ASOBO_material_invisible with "enabled" true (the schema default): the material is
    // not rendered, e.g. MSFS collision geometry. GltfRenderLayer leaves it out of scenes
    // when the viewport does not apply ASOBO_material_invisible (GltfRenderLayer::asoboOptions).
    bool invisible = false;
    // ASOBO_material_environment_occluder (enabled, the default): hidden the same way, under its
    // own viewport switch (ASOBO_material_environment_occluder).
    bool environmentOccluder = false;
    // A1.4: ASOBO_material_draw_order. The class comes from the material type extensions and
    // the offset is read once per material sync; drawOrderActive is set per viewport by
    // GltfRenderLayer::createScene when the asset declares the extension and the viewport applies it.
    DrawOrderClass drawOrderClass = DrawOrderClass::None;
    int drawOrderOffset = 0;
    bool drawOrderActive = false;
    // A2.2: the viewport applies ASOBO_material_geometry_decal, so decal-class draws get their
    // depth bias. Set per viewport next to drawOrderActive; off renders the file as without ASOBO.
    bool decalBiasActive = false;
    // A7.1: the material compiles the anisotropic lobe. khrAnisotropy: KHR_materials_anisotropy with
    // a non-zero strength (its display switch is RenderSettings::enableAnisotropy).
    // asoboAnisotropy: ASOBO_material_anisotropic_v2 with its texture bound; createScene defines
    // ASOBO_ANISOTROPY per viewport when the viewport applies the extension.
    bool khrAnisotropy = false;
    bool asoboAnisotropy = false;
    bool overrideBlend = false;
    bool customBlend = false;
    bool blendingEnabled = false;
    GLenum blendSrcFactor = GL_SRC_ALPHA;
    GLenum blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
    GLenum blendEquation = GL_FUNC_ADD;
    bool logicOpEnabled = false;
    GLenum logicOp = GL_XOR;
    RenderPass* renderPass = nullptr;
    uint32_t gltfMaterialId = 0;
    RenderPass* nonTransmissivePass = nullptr;
    bool decalReceiverWithoutTransmission = false;
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
