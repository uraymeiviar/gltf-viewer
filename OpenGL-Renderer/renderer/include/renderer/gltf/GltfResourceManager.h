// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "renderer/ResourceManager.h"
#include "renderer/filtering/IBLSampler.h"

namespace ux3d::filesystem {
class Path;
}

namespace ux3d::renderer {
// forward delcs
class OpaquePass;
class TransparentPass;
class FullscreenCopyPass;
class TransmissivePass;
class TransmissiveBlitPass;

// GltfResourceManager holds all renderer resources that are used in the gltf renderer frontend.
// It provides helper functions to quickly create preconfigured resources (e.g. where all uniforms are set to the default value)
class GltfResourceManager : public ResourceManager {
public:
    GltfResourceManager(gl::GL* gl);
    virtual ~GltfResourceManager() = default;

    std::tuple<Mesh*, Material*> getCubeMapTuple() const;
    // A3.6: the ground plane drawable (a full-screen quad whose fragment shader intersects every pixel's
    // view ray with the horizontal plane; ground.vert / ground.frag). It is an Opaque-pass drawable, so the
    // application appends it to a scene only while a viewport's ground is the plane, and writes the
    // plane's parameters into the material's uniforms before each draw.
    std::tuple<Mesh*, Material*> getGroundTuple() const { return { m_groundMesh, m_groundMaterial }; }
    const std::tuple<Mesh*, Material*>& getEmptyTransmissionBlit() const { return m_emptyTransmissionTuple; }

    /// Loads and decodes image.
    static image::Image loadImage(const filesystem::Path& path);

    /// Loads and decodes image, creating a texture for that image.
    gl::Texture* loadTexture(const filesystem::Path& path);

    std::tuple<gl::Texture*, uint32_t> loadCubeMapTexture(const filesystem::Path& path);
    EnvironmentMap* loadEnvironmentMap(const IBLSampler& iblSampler);

    OpaquePass* getOpaquePass() const { return m_opaquePass; }
    TransmissiveBlitPass* getTransmissiveBlitPass() const { return m_transmissiveBlitPass; }
    TransmissivePass* getTransmissivePass() const { return m_transmissivePass; }
    TransparentPass* getTransparentPass() const { return m_transparentPass; }
    FullscreenCopyPass* getFullscreenCopyPass() const { return m_fullscreenCopyPass; }

    void createFramebufferTexture(GLuint textureID);
    void setClearColor(math::vec4 color);
    void setBlitFramebufferFunction(std::function<void()> _blitFramebuffer, math::vec4 _clearColor);
    void setTransmissiveTextureFunction(std::function<void()> _transmissiveTexture);
    gl::Texture* getFramebufferTexture() { return m_framebufferTexture; };
    gl::SamplerState* getFramebufferSamplerState() { return m_framebufferSamplerState; };

    Mesh* createBillboardMesh();
    Material* createBillboardMaterial();

protected:
    OpaquePass* m_opaquePass = nullptr;
    TransmissiveBlitPass* m_transmissiveBlitPass = nullptr;
    TransmissivePass* m_transmissivePass = nullptr;
    TransparentPass* m_transparentPass = nullptr;
    FullscreenCopyPass* m_fullscreenCopyPass = nullptr;

    void initEmptyTransmissionTuple();
    std::tuple<Mesh*, Material*> m_emptyTransmissionTuple;

    gl::Buffer* m_billboardIndexBuffer;
    gl::Buffer* m_billboardVertexBuffer;
    gl::Buffer* m_billboardTexCoordBuffer;

    void initCubeMap();
    Mesh* m_cubeMapMesh;
    Material* m_cubeMapMaterial;

    void initGround();
    Mesh* m_groundMesh = nullptr;
    Material* m_groundMaterial = nullptr;

    gl::Texture* m_framebufferTexture = nullptr;
    gl::SamplerState* m_framebufferSamplerState = nullptr;
};
}
