// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/gltf/GltfResourceManager.h"
#include "filesystem/Dir.h"
#include "filesystem/File.h"
#include "filesystem/Path.h"
#include "gltf/AlphaMode.h"
#include "imageformats/imageformats.h"
#include "renderer/passes/FullscreenCopyPass.h"
#include "renderer/passes/OpaquePass.h"
#include "renderer/passes/TransmissiveBlitPass.h"
#include "renderer/passes/TransmissivePass.h"
#include "renderer/passes/TransparentPass.h"

using namespace ux3d;
using namespace ux3d::renderer;

GltfResourceManager::GltfResourceManager(gl::GL* gl)
    : ResourceManager(gl)
{
    // create billboard resources

    core::Vector<uint32_t> quadIndices = { 0, 1, 2, 2, 1, 3 };
    core::MemoryRange<const uint8_t> quadIndicesData(reinterpret_cast<uint8_t*>(quadIndices.data()), quadIndices.getMemoryRange().byteSize());
    m_billboardIndexBuffer = createWithGLContext<gl::Buffer>(std::move(quadIndicesData));

    core::Vector<float> quadVertices = {
        -0.5, -0.5, 0,
        0.5, -0.5, 0,
        -0.5, 0.5, 0,
        0.5, 0.5, 0
    };
    core::MemoryRange<const uint8_t> quadVerticesData(reinterpret_cast<uint8_t*>(quadVertices.data()), quadVertices.getMemoryRange().byteSize());
    m_billboardVertexBuffer = createWithGLContext<gl::Buffer>(std::move(quadVerticesData));

    core::Vector<float> quadUvCoords = {
        0, 1,
        1, 1,
        0, 0,
        1, 0
    };
    core::MemoryRange<const uint8_t> quadUvCoordsData(reinterpret_cast<uint8_t*>(quadUvCoords.data()), quadUvCoords.getMemoryRange().byteSize());
    m_billboardTexCoordBuffer = createWithGLContext<gl::Buffer>(std::move(quadUvCoordsData));

    // shared passes
    m_opaquePass = create<OpaquePass>(gl);
    m_transmissiveBlitPass = create<TransmissiveBlitPass>(gl);
    m_transmissivePass = create<TransmissivePass>(gl);
    m_transparentPass = create<TransparentPass>(gl);
    m_fullscreenCopyPass = create<FullscreenCopyPass>();

    initCubeMap();
    initGround();
    initEmptyTransmissionTuple();
}

void GltfResourceManager::initEmptyTransmissionTuple()
{
    // Create empty drawable to enable TransmissiveBlitPass
    // Shaders have to be set otherwise an error occurs. Therefore, we use the cubemap shaders.
    auto emptyMaterial = createWithGLContext<Material>();
    emptyMaterial->fragmentShader = "cubemap.frag";
    emptyMaterial->renderPass = m_transmissiveBlitPass;
    auto emptyMesh = createWithGLContext<Mesh>();
    emptyMesh->vertexShader = "cubemap.vert";
    m_emptyTransmissionTuple = { emptyMesh, emptyMaterial };
}

void GltfResourceManager::initCubeMap()
{
    std::vector<float> vertices = {
        -1, -1, -1,
        1, -1, -1,
        1, 1, -1,
        -1, 1, -1,
        -1, -1, 1,
        1, -1, 1,
        1, 1, 1,
        -1, 1, 1
    };
    std::vector<uint16_t> indices = {
        1, 2, 0,
        2, 3, 0,
        6, 2, 1,
        1, 5, 6,
        6, 5, 4,
        4, 7, 6,
        6, 3, 2,
        7, 3, 6,
        3, 7, 0,
        7, 4, 0,
        5, 1, 0,
        4, 5, 0
    };

    core::MemoryRange<const uint8_t> cubeVerticesData(reinterpret_cast<uint8_t*>(vertices.data()), vertices.size() * sizeof(float));
    auto vertexBuffer = createWithGLContext<gl::Buffer>(cubeVerticesData);

    core::MemoryRange<const uint8_t> cubeIndicesData(reinterpret_cast<uint8_t*>(indices.data()), indices.size() * sizeof(uint16_t));
    auto indexBuffer = createWithGLContext<gl::Buffer>(cubeIndicesData);

    gl::Attribute attribute("a_position");
    attribute.dimension = 3;
    attribute.type = gl::ComponentType::Float;
    attribute.buffer = vertexBuffer;

    m_cubeMapMesh = createWithGLContext<Mesh>();
    m_cubeMapMesh->vertexShader = "cubemap.vert";
    m_cubeMapMesh->primitiveTopology = gl::PrimitiveTopology::TriangleList;
    m_cubeMapMesh->indexFormat = gl::PrimitiveType::UnsignedShort;
    m_cubeMapMesh->indexCount = indices.size();
    m_cubeMapMesh->indexBuffer = indexBuffer;
    m_cubeMapMesh->vertexCount = vertices.size();
    m_cubeMapMesh->attributes.emplace_back(attribute);

    m_cubeMapMaterial = createWithGLContext<Material>();
    m_cubeMapMaterial->fragmentShader = "cubemap.frag";
    m_cubeMapMaterial->renderPass = m_opaquePass;
    m_cubeMapMaterial->defines.define("LINEAR_OUTPUT", 1);
    m_cubeMapMaterial->isDoubleSided = true;
}

void GltfResourceManager::initGround()
{
    // A3.6: four corners of the screen; ground.vert unprojects them to the near and far plane and
    // ground.frag intersects each pixel's ray with the plane (the ray-grid path of GridRenderer).
    std::vector<float> vertices = {
        -1, -1, 0,
        1, -1, 0,
        1, 1, 0,
        -1, 1, 0
    };
    std::vector<uint16_t> indices = { 0, 1, 2, 0, 2, 3 };

    core::MemoryRange<const uint8_t> verticesData(reinterpret_cast<uint8_t*>(vertices.data()), vertices.size() * sizeof(float));
    auto vertexBuffer = createWithGLContext<gl::Buffer>(verticesData);
    core::MemoryRange<const uint8_t> indicesData(reinterpret_cast<uint8_t*>(indices.data()), indices.size() * sizeof(uint16_t));
    auto indexBuffer = createWithGLContext<gl::Buffer>(indicesData);

    gl::Attribute attribute("a_position");
    attribute.dimension = 3;
    attribute.type = gl::ComponentType::Float;
    attribute.buffer = vertexBuffer;

    m_groundMesh = createWithGLContext<Mesh>();
    m_groundMesh->vertexShader = "ground.vert";
    m_groundMesh->primitiveTopology = gl::PrimitiveTopology::TriangleList;
    m_groundMesh->indexFormat = gl::PrimitiveType::UnsignedShort;
    m_groundMesh->indexCount = indices.size();
    m_groundMesh->indexBuffer = indexBuffer;
    m_groundMesh->vertexCount = vertices.size();
    m_groundMesh->attributes.emplace_back(attribute);
    // Opaque draws are sorted by the distance of the mesh position to the camera; the shader ignores the model
    // matrix, so a far position only makes the ground the last opaque draw (the sky is overdrawn once and the
    // ground shader does not run before the model has written its depth).
    m_groundMesh->updateTransform(math::translate(math::mat4x4(1.0f), math::vec3(0.0f, 1.0e9f, 0.0f)));

    m_groundMaterial = createWithGLContext<Material>();
    m_groundMaterial->fragmentShader = "ground.frag";
    m_groundMaterial->renderPass = m_opaquePass;
    m_groundMaterial->defines.define("LINEAR_OUTPUT", 1);
    m_groundMaterial->isDoubleSided = true;
    // Every uniform exists from the start, so later writes never move the storage the commands point at.
    m_groundMaterial->setUniform("u_GroundHeight", 0.0f);
    m_groundMaterial->setUniform("u_GroundAlbedo", math::vec3(0.5f));
    m_groundMaterial->setUniform("u_GroundRoughness", 0.9f);
    m_groundMaterial->setUniform("u_GroundDetail", math::vec3(0.0f, 1.0f, 0.0f));
    m_groundMaterial->setUniform("u_GroundHaze", math::vec2(0.0f, 0.0f));
    m_groundMaterial->setUniform("u_GroundHazeColor", math::vec3(0.0f));
}

std::tuple<Mesh*, Material*> GltfResourceManager::getCubeMapTuple() const
{
    return { m_cubeMapMesh, m_cubeMapMaterial };
}

image::Image GltfResourceManager::loadImage(const filesystem::Path& path)
{
    core::Vector<uint8_t> imageData;
    filesystem::File textureFile = path;
    if (!textureFile.load(imageData)) {
        core::logError("Could not load image %s", textureFile.toString().c_str());
    }
    image::Image image;
    if (!imageformats::decodeImage(imageData, image)) {
        core::logError("Could not decode image %s", textureFile.toString().c_str());
    }

    return image;
}

gl::Texture* GltfResourceManager::loadTexture(const filesystem::Path& path)
{
    return createWithGLContext<gl::Texture>(loadImage(path));
}

std::tuple<gl::Texture*, uint32_t> GltfResourceManager::loadCubeMapTexture(const filesystem::Path& path)
{
    core::Vector<uint8_t> imageData;
    filesystem::File cubemapFile = path;
    if (!cubemapFile.load(imageData)) {
        core::logError("Could not load cube map image %s", cubemapFile.toString().c_str());
    }
    image::Image image;
    if (!imageformats::decodeImage(imageData, image)) {
        core::logError("Could not decode cube map image %s", cubemapFile.toString().c_str());
    }

    gl::Texture* ggxTexture = createWithGLContext<gl::Texture>(image);

    return { ggxTexture, 10 };
}

EnvironmentMap* GltfResourceManager::loadEnvironmentMap(const IBLSampler& _iblSampler)
{
    renderer::EnvironmentMap envMap(m_gl);

    auto lambertianTexture = create<gl::Texture>(m_gl, _iblSampler.lambertianTextureID, gl::TextureTarget::TextureCubeMap);
    auto ggxTexture = create<gl::Texture>(m_gl, _iblSampler.ggxTextureID, gl::TextureTarget::TextureCubeMap);
    auto sheenTexture = create<gl::Texture>(m_gl, _iblSampler.sheenTextureID, gl::TextureTarget::TextureCubeMap);
    auto ggxLUTTexture = create<gl::Texture>(m_gl, _iblSampler.ggxLUTTextureID, gl::TextureTarget::Texture2d);
    auto charlieLUTTexture = create<gl::Texture>(m_gl, _iblSampler.charlieLUTTextureID, gl::TextureTarget::Texture2d);
    auto sheenLUTTexture = create<gl::Texture>(m_gl, _iblSampler.sheenLUTTextureID, gl::TextureTarget::Texture2d);

    renderer::gl::SamplerState samplerState;
    samplerState.minFilter = renderer::gl::TextureMinFilter::Linear;
    samplerState.magFilter = renderer::gl::TextureMagFilter::Linear;
    samplerState.wrapS = renderer::gl::TextureWrap::ClampToEdge;
    samplerState.wrapT = renderer::gl::TextureWrap::ClampToEdge;
    samplerState.wrapR = renderer::gl::TextureWrap::ClampToEdge;
    envMap.setDiffuse(lambertianTexture, create<renderer::gl::SamplerState>(samplerState));

    samplerState.minFilter = renderer::gl::TextureMinFilter::LinearMipmapLinear;
    samplerState.magFilter = renderer::gl::TextureMagFilter::Linear;
    samplerState.wrapS = renderer::gl::TextureWrap::ClampToEdge;
    samplerState.wrapT = renderer::gl::TextureWrap::ClampToEdge;
    samplerState.wrapR = renderer::gl::TextureWrap::ClampToEdge;
    envMap.setSpecular(ggxTexture, create<renderer::gl::SamplerState>(samplerState), _iblSampler.getMipmapLevels());

    samplerState.minFilter = renderer::gl::TextureMinFilter::LinearMipmapLinear;
    samplerState.magFilter = renderer::gl::TextureMagFilter::Linear;
    samplerState.wrapS = renderer::gl::TextureWrap::ClampToEdge;
    samplerState.wrapT = renderer::gl::TextureWrap::ClampToEdge;
    samplerState.wrapR = renderer::gl::TextureWrap::ClampToEdge;
    envMap.setSheen(sheenTexture, create<renderer::gl::SamplerState>(samplerState), _iblSampler.getMipmapLevels());

    samplerState.minFilter = renderer::gl::TextureMinFilter::Linear;
    samplerState.magFilter = renderer::gl::TextureMagFilter::Linear;
    samplerState.wrapS = renderer::gl::TextureWrap::ClampToEdge;
    samplerState.wrapT = renderer::gl::TextureWrap::ClampToEdge;
    envMap.setGGXLUT(ggxLUTTexture, create<renderer::gl::SamplerState>(samplerState));
    envMap.setCharieLUT(charlieLUTTexture, create<renderer::gl::SamplerState>(samplerState));
    envMap.setSheenLUT(sheenLUTTexture, create<renderer::gl::SamplerState>(samplerState));

    return create<EnvironmentMap>(std::move(envMap));
}

void GltfResourceManager::createFramebufferTexture(GLuint textureID)
{
    if (m_framebufferTexture != nullptr) {
        m_framebufferTexture->handle = textureID;
        return;
    }

    renderer::gl::SamplerState samplerState;
    samplerState.minFilter = renderer::gl::TextureMinFilter::LinearMipmapLinear;
    samplerState.magFilter = renderer::gl::TextureMagFilter::Linear;
    samplerState.wrapS = renderer::gl::TextureWrap::ClampToEdge;
    samplerState.wrapT = renderer::gl::TextureWrap::ClampToEdge;

    m_framebufferSamplerState = create<renderer::gl::SamplerState>(std::move(samplerState));
    m_framebufferTexture = createWithGLContext<renderer::gl::Texture>(textureID, gl::TextureTarget::Texture2d, false);
}

void GltfResourceManager::setClearColor(math::vec4 color)
{
    m_transmissiveBlitPass->setClearColor(color);
}

void GltfResourceManager::setBlitFramebufferFunction(std::function<void()> _blitFramebuffer, math::vec4 _clearColor)
{
    m_transmissiveBlitPass->setBlitFramebufferFunction(_blitFramebuffer, _clearColor);
};

void GltfResourceManager::setTransmissiveTextureFunction(std::function<void()> _transmissiveTexture)
{
    m_transmissivePass->setTransmissiveTextureFunction(_transmissiveTexture);
};

Material* GltfResourceManager::createBillboardMaterial()
{
    renderer::Material* material = createWithGLContext<Material>();
    material->fragmentShader = "gizmo_material.frag";

    material->renderPass = m_transparentPass;

    material->setUniform<math::vec3>("u_EmissiveFactor", math::vec3(0.f, 0.f, 0.f));
    material->setUniform<float>("u_AlphaCutoff", 0.01f);

    material->defines.define("MATERIAL_UNLIT");
    material->defines.define("MATERIAL_METALLICROUGHNESS");

    material->setUniform("u_BaseColorFactor", math::vec4(1));
    material->defines.define("HAS_BASE_COLOR_MAP");

    material->defines.define("ALPHAMODE", static_cast<int>(gltf::AlphaMode::Blend));
    material->defines.define("ALPHAMODE_OPAQUE", static_cast<int>(gltf::AlphaMode::Opaque));
    material->defines.define("ALPHAMODE_MASK", static_cast<int>(gltf::AlphaMode::Mask));
    material->defines.define("ALPHAMODE_BLEND", static_cast<int>(gltf::AlphaMode::Blend));
    material->defines.define("DEBUG", 0);

    material->isDoubleSided = false;

    return material;
}

Mesh* GltfResourceManager::createBillboardMesh()
{
    gl::Attribute positionAttribute("a_position");
    positionAttribute.buffer = m_billboardVertexBuffer;
    positionAttribute.dimension = 3;
    positionAttribute.type = gl::ComponentType::Float;

    gl::Attribute texcoordAttribute("a_texcoord_0");
    texcoordAttribute.buffer = m_billboardTexCoordBuffer;
    texcoordAttribute.dimension = 2;
    texcoordAttribute.type = gl::ComponentType::Float;

    auto mesh = createWithGLContext<Mesh>();
    mesh->vertexShader = "gizmo.vert";
    mesh->primitiveTopology = gl::PrimitiveTopology::TriangleList;
    mesh->indexFormat = gl::PrimitiveType::UnsignedInt;
    mesh->indexCount = 6;
    mesh->indexBuffer = m_billboardIndexBuffer;
    mesh->defines.define("IS_BILLBOARD");
    mesh->defines.define("IS_FIXEDSIZE");
    mesh->setUniform("u_fixedSizeScale", 1);
    mesh->vertexCount = 4;
    mesh->defines.define("HAS_TEXCOORD_0_VEC2");
    mesh->attributes.emplace_back(positionAttribute);
    mesh->attributes.emplace_back(texcoordAttribute);
    return mesh;
}
