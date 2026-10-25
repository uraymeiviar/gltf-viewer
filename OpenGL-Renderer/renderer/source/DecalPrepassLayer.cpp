// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "renderer/DecalPrepassLayer.h"
#include "renderer/Mesh.h"
#include "renderer/RenderPass.h"
#include "renderer/passes/OpaquePass.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/Sampler.h"
#include "renderer/gl/Texture.h"

#include <tuple>
#include <utility>
#include <vector>

using namespace ux3d;
using namespace ux3d::renderer;

namespace {
constexpr int kPreviewModeDepth = 0;
constexpr int kPreviewModeNormal = 1;
}

DecalPrepassLayer::DecalPrepassLayer(gl::GL* gl)
    : m_gl(gl)
{
    m_pass = std::make_unique<OpaquePass>(gl, RenderPassType::DecalPrepass, "DecalPrepass");
    // OpaquePass already sets depth test and disables blending. The external commands bind
    // the shared prepass target and restore the viewport target, exactly like picking.
    m_pass->getPreCommands().emplace_back(
        Command::makeExternalFunctionCommand(gl, [this] { if (prePass) prePass(); }));
    m_pass->getPostCommands().emplace_back(
        Command::makeExternalFunctionCommand(gl, [this] { if (postPass) postPass(); }));

    // Debug view: a fullscreen triangle that displays the prepass depth or G0. It runs in
    // the last pass of the scene, after every forward draw, so it covers the viewport.
    m_debugPass = std::make_unique<RenderPass>(RenderPassType::FullscreenCopy, "DecalPreview");
    m_debugPass->getPreCommands().emplace_back(Command::makeDepthTestCommand(gl, false));
    m_debugPass->getPreCommands().emplace_back(Command::makeBlendCommand(gl, false));

    m_debugMesh = std::make_unique<Mesh>(gl);
    m_debugMesh->vertexShader = "fullscreen.vert";
    m_debugMesh->primitiveTopology = gl::PrimitiveTopology::TriangleList;
    m_debugMesh->vertexCount = 3;

    // Borrowed views of the shared buffer's textures. Their handles are refreshed by
    // setDebugTargets whenever the buffer is created or resized; the material and its
    // samplers keep pointing at these stable wrapper objects.
    m_depthTexture = std::make_unique<gl::Texture>(gl, 0, gl::TextureTarget::Texture2d, false);
    m_normalTexture = std::make_unique<gl::Texture>(gl, 0, gl::TextureTarget::Texture2d, false);
    m_debugSamplerState = std::make_unique<gl::SamplerState>();
    m_debugSamplerState->minFilter = gl::TextureMinFilter::Nearest;
    m_debugSamplerState->magFilter = gl::TextureMagFilter::Nearest;
    m_debugSamplerState->wrapS = gl::TextureWrap::ClampToEdge;
    m_debugSamplerState->wrapT = gl::TextureWrap::ClampToEdge;
    m_debugSamplerState->wrapR = gl::TextureWrap::ClampToEdge;

    m_debugMaterial = std::make_unique<Material>(gl);
    m_debugMaterial->fragmentShader = "decal_preview.frag";
    m_debugMaterial->renderPass = m_debugPass.get();
    m_debugMaterial->isDoubleSided = true;
    m_debugMaterial->setUniform<int>("u_PreviewMode", kPreviewModeNormal);
    m_debugMaterial->setUniform<math::vec2>("u_PreviewUVScale", math::vec2(1.0f, 1.0f));
    m_debugMaterial->samplers.emplace_back("u_PreviewDepthSampler", m_depthTexture.get(), m_debugSamplerState.get());
    m_debugMaterial->samplers.emplace_back("u_PreviewNormalSampler", m_normalTexture.get(), m_debugSamplerState.get());
}

DecalPrepassLayer::~DecalPrepassLayer() = default;

void DecalPrepassLayer::setDebugTargets(GLuint depthTexture, GLuint normalTexture, int bufferWidth, int bufferHeight)
{
    m_depthTexture->handle = depthTexture;
    m_normalTexture->handle = normalTexture;
    m_bufferWidth = bufferWidth;
    m_bufferHeight = bufferHeight;
    updateDebugUVScale();
}

void DecalPrepassLayer::setDebugViewport(int width, int height)
{
    m_viewportWidth = width;
    m_viewportHeight = height;
    updateDebugUVScale();
}

void DecalPrepassLayer::updateDebugUVScale()
{
    const float scaleX = m_bufferWidth > 0 && m_viewportWidth > 0
        ? static_cast<float>(m_viewportWidth) / static_cast<float>(m_bufferWidth)
        : 1.0f;
    const float scaleY = m_bufferHeight > 0 && m_viewportHeight > 0
        ? static_cast<float>(m_viewportHeight) / static_cast<float>(m_bufferHeight)
        : 1.0f;
    m_debugMaterial->setUniform<math::vec2>("u_PreviewUVScale", math::vec2(scaleX, scaleY));
}

RendererScene DecalPrepassLayer::applyLayer(RendererScene&& scene, const std::function<bool(const Mesh*)>& isSceneMesh)
{
    m_prepassDrawableCount = 0;

    // The draw list is not modified while it is walked: collect first, append afterwards.
    std::vector<std::tuple<Mesh*, Material*>> prepassDraws;
    prepassDraws.reserve(scene.drawables.size());
    for (const auto& meshMat : scene.drawables) {
        Mesh* mesh = std::get<Mesh*>(meshMat);
        Material* material = std::get<Material*>(meshMat);
        if (mesh == nullptr || material == nullptr || material->renderPass == nullptr) {
            continue;
        }
        // Opaque and MASK primitives live in the Opaque pass. Decals are the A1.4
        // DrawOrderClass::Decal materials, whether or not the viewport applies draw order.
        if (material->renderPass->getId() != RenderPassType::Opaque) {
            continue;
        }
        if (material->drawOrderClass == DrawOrderClass::Decal) {
            continue;
        }
        if (!isSceneMesh(mesh)) {
            continue;
        }

        Material* pooled = nullptr;
        const auto existing = m_materials.find(material);
        if (existing != m_materials.end()) {
            pooled = existing->second.get();
            *pooled = *material;
            ++m_materialsReused;
        } else {
            auto copy = std::make_unique<Material>(*material);
            pooled = copy.get();
            m_materials.emplace(material, std::move(copy));
            ++m_materialsCreated;
        }
        pooled->fragmentShader = "decal_prepass.frag";
        pooled->renderPass = m_pass.get();
        prepassDraws.emplace_back(mesh, pooled);
    }

    for (const auto& meshMat : prepassDraws) {
        scene.drawables.emplace_back(meshMat);
    }
    m_prepassDrawableCount = prepassDraws.size();

    if (m_debugView != DebugView::None && m_depthTexture->handle != 0 && m_normalTexture->handle != 0) {
        m_debugMaterial->setUniform<int>("u_PreviewMode",
                                         m_debugView == DebugView::Depth ? kPreviewModeDepth : kPreviewModeNormal);
        scene.drawables.emplace_back(m_debugMesh.get(), m_debugMaterial.get());
    }

    return std::move(scene);
}
