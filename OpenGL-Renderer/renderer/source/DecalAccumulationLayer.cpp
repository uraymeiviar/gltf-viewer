// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "renderer/DecalAccumulationLayer.h"
#include "renderer/Mesh.h"
#include "renderer/RenderPass.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/Sampler.h"
#include "renderer/gl/Texture.h"
#include "renderer/passes/DecalAccumulationPass.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

using namespace ux3d;
using namespace ux3d::renderer;

namespace {
const gltf::asobo::ExtensionData* decalExtension(const Material& material)
{
    if (material.asobo == nullptr) {
        return nullptr;
    }
    const gltf::asobo::ExtensionData* decal = material.asobo->find("ASOBO_material_geometry_decal");
    return decal != nullptr ? decal : material.asobo->find("ASOBO_material_blend_gbuffer");
}

float factor(const gltf::asobo::ExtensionData* decal, const char* key)
{
    if (decal == nullptr) {
        return 1.0f;
    }
    return std::clamp(static_cast<float>(decal->number(key).value_or(1.0)), 0.0f, 1.0f);
}

// A decal pass seed may be OPAQUE or BLEND in the glTF file, but never one of the
// transmission-path duplicates (those have the 2 suffix and share the source mesh).
bool isForwardDecalPass(const Material& material)
{
    if (material.renderPass == nullptr) {
        return false;
    }
    const uint32_t passId = material.renderPass->getId();
    return passId == RenderPassType::Opaque || passId == RenderPassType::Transparent;
}
}

DecalAccumulationLayer::DecalAccumulationLayer(gl::GL* gl)
    : m_gl(gl)
{
    m_pass = std::make_unique<DecalAccumulationPass>(gl);

    m_g0Texture = std::make_unique<gl::Texture>(gl, 0, gl::TextureTarget::Texture2d, false);
    m_g0SamplerState = std::make_unique<gl::SamplerState>();
    m_g0SamplerState->minFilter = gl::TextureMinFilter::Nearest;
    m_g0SamplerState->magFilter = gl::TextureMagFilter::Nearest;
    m_g0SamplerState->wrapS = gl::TextureWrap::ClampToEdge;
    m_g0SamplerState->wrapT = gl::TextureWrap::ClampToEdge;
    m_g0SamplerState->wrapR = gl::TextureWrap::ClampToEdge;

    m_depthTexture = std::make_unique<gl::Texture>(gl, 0, gl::TextureTarget::Texture2d, false);
    m_depthSamplerState = std::make_unique<gl::SamplerState>();
    m_depthSamplerState->minFilter = gl::TextureMinFilter::Nearest;
    m_depthSamplerState->magFilter = gl::TextureMagFilter::Nearest;
    m_depthSamplerState->wrapS = gl::TextureWrap::ClampToEdge;
    m_depthSamplerState->wrapT = gl::TextureWrap::ClampToEdge;
    m_depthSamplerState->wrapR = gl::TextureWrap::ClampToEdge;
}

DecalAccumulationLayer::~DecalAccumulationLayer() = default;

void DecalAccumulationLayer::setG0Target(GLuint texture)
{
    m_g0Texture->handle = texture;
}

void DecalAccumulationLayer::setDepthTarget(GLuint texture)
{
    m_depthTexture->handle = texture;
}

void DecalAccumulationLayer::setPassCallbacks(std::function<void()> prePass, std::function<void()> postPass)
{
    m_pass->prePass = std::move(prePass);
    m_pass->postPass = std::move(postPass);
}

bool DecalAccumulationLayer::appliesTo(const Material& material) const
{
    if (material.asobo == nullptr) {
        return false;
    }
    const gltf::asobo::ExtensionData* geometry = material.asobo->find("ASOBO_material_geometry_decal");
    const gltf::asobo::ExtensionData* blend = material.asobo->find("ASOBO_material_blend_gbuffer");
    const bool geometryActive = geometry != nullptr && geometry->boolean("enabled").value_or(true);
    const bool blendActive = blend != nullptr && blend->boolean("enabled").value_or(true);
    return (geometryActive && m_geometryDecalApplied) || (blendActive && m_blendGbufferApplied);
}

RendererScene DecalAccumulationLayer::applyLayer(RendererScene&& scene, const std::function<bool(const Mesh*)>& isSceneMesh)
{
    m_decalDrawableCount = 0;

    // Collect and unlink first: the linked list's erase() mutates the iteration. Decals of
    // the forward passes are accumulated; transmission-path duplicates of a decal (the 2
    // passes) are removed without a second accumulation draw.
    std::vector<std::tuple<Mesh*, Material*>> decals;
    for (auto it = scene.drawables.begin(); it != scene.drawables.end();) {
        Mesh* mesh = std::get<Mesh*>(*it);
        Material* material = std::get<Material*>(*it);
        const bool decal = mesh != nullptr && material != nullptr
            && material->drawOrderClass == DrawOrderClass::Decal && appliesTo(*material) && isSceneMesh(mesh);
        if (decal) {
            if (isForwardDecalPass(*material)) {
                decals.emplace_back(mesh, material);
            }
            it = scene.drawables.erase(it);
        } else {
            ++it;
        }
    }

    for (const auto& meshMat : decals) {
        Mesh* mesh = std::get<Mesh*>(meshMat);
        Material* material = std::get<Material*>(meshMat);
        Material* pooled = nullptr;
        const auto existing = m_materials.find(material);
        if (existing != m_materials.end()) {
            pooled = existing->second.get();
            *pooled = *material;
        } else {
            auto copy = std::make_unique<Material>(*material);
            pooled = copy.get();
            m_materials.emplace(material, std::move(copy));
        }

        const gltf::asobo::ExtensionData* decal = decalExtension(*material);
        pooled->fragmentShader = "decal_accum.frag";
        pooled->renderPass = m_pass.get();
        // The decal shader evaluates the decal's own material, never the receiver path.
        pooled->defines.undefine("ASOBO_DECAL_RECEIVER");
        pooled->setUniform<float>("u_DecalBaseColorFactor", factor(decal, "baseColorBlendFactor"));
        pooled->setUniform<float>("u_DecalMetallicFactor", factor(decal, "metallicBlendFactor"));
        pooled->setUniform<float>("u_DecalRoughnessFactor", factor(decal, "roughnessBlendFactor"));
        pooled->setUniform<float>("u_DecalNormalFactor", factor(decal, "normalBlendFactor"));
        pooled->setUniform<float>("u_DecalEmissiveFactor", factor(decal, "emissiveBlendFactor"));
        pooled->setUniform<float>("u_DecalOcclusionFactor", factor(decal, "occlusionBlendFactor"));
        // normalOverrideFactor is geometry_decal-only; blend_gbuffer uses the decal mesh normal.
        const float normalOverride = decal != nullptr
                && material->asobo != nullptr
                && material->asobo->find("ASOBO_material_geometry_decal") != nullptr
            ? std::clamp(static_cast<float>(decal->number("normalOverrideFactor").value_or(1.0)), 0.0f, 1.0f)
            : 1.0f;
        pooled->setUniform<float>("u_DecalNormalOverride", normalOverride);
        // A2.3: mode "blendMasked" multiplies the coverage by the blend-mask ramp (semantics
        // §7). It needs the decal's blendMaskTexture sampler (created by GltfRenderLayer);
        // without it, and for "frosted", "default" and blend_gbuffer, the coverage stays the
        // A2.2 default one.
        const gltf::asobo::ExtensionData* geometry = material->asobo != nullptr
            ? material->asobo->find("ASOBO_material_geometry_decal") : nullptr;
        const std::string* mode = geometry != nullptr && decal == geometry ? geometry->string("mode") : nullptr;
        const bool hasBlendMask = std::any_of(pooled->samplers.begin(), pooled->samplers.end(),
            [](const gl::Sampler& sampler) { return sampler.name == "u_DecalBlendMaskSampler"; });
        const bool blendMasked = mode != nullptr && *mode == "blendMasked" && hasBlendMask;
        pooled->defines.defineIf(blendMasked, "ASOBO_DECAL_BLEND_MASKED");
        // Frost is weather: the SDK drives Geo Decal Frosted coverage by the icing state, and a clean
        // aircraft shows none. Glider uses its icing setting (0 = clean, the default) as that state.
        pooled->defines.defineIf(mode != nullptr && *mode == "frosted", "ASOBO_DECAL_FROSTED");
        if (blendMasked) {
            pooled->setUniform<float>("u_DecalBlendSharpness",
                std::clamp(static_cast<float>(geometry->number("blendSharpnessFactor").value_or(0.0)), 0.0f, 1.0f));
        }
        // The prepass G0 is not an attachment of this pass; it is read through this sampler.
        pooled->samplers.emplace_back("u_DecalG0", m_g0Texture.get(), m_g0SamplerState.get());
        // A2.2: the prepass depth, for the tolerant test that replaces GL_DEPTH_TEST here.
        pooled->samplers.emplace_back("u_DecalDepth", m_depthTexture.get(), m_depthSamplerState.get());

        scene.drawables.emplace_back(mesh, pooled);
        ++m_decalDrawableCount;
    }

    return std::move(scene);
}
