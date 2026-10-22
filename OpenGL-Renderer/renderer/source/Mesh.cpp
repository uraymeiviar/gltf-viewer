// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/Mesh.h"
#include "renderer/gl/Texture.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

using namespace ux3d;
using namespace ux3d::renderer;

namespace {

static std::atomic<uint64_t> s_nextMeshId{ 1 };
static std::vector<std::pair<void*, Mesh::MeshDestroyCallback>> s_destroyCallbacks;
static std::mutex s_callbacksMutex;

void notifyMeshDestroyed(uint64_t meshId)
{
    std::lock_guard<std::mutex> lock(s_callbacksMutex);
    for (const auto& [_, cb] : s_destroyCallbacks) {
        if (cb) {
            cb(meshId);
        }
    }
}

} // namespace

void Mesh::addMeshDestroyCallback(void* ownerKey, MeshDestroyCallback cb)
{
    if (ownerKey == nullptr || !cb) {
        return;
    }
    std::lock_guard<std::mutex> lock(s_callbacksMutex);
    auto it = std::find_if(s_destroyCallbacks.begin(), s_destroyCallbacks.end(),
        [ownerKey](const auto& pair) { return pair.first == ownerKey; });
    if (it != s_destroyCallbacks.end()) {
        it->second = std::move(cb);
    } else {
        s_destroyCallbacks.emplace_back(ownerKey, std::move(cb));
    }
}

void Mesh::removeMeshDestroyCallback(void* ownerKey)
{
    if (ownerKey == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(s_callbacksMutex);
    s_destroyCallbacks.erase(
        std::remove_if(s_destroyCallbacks.begin(), s_destroyCallbacks.end(),
            [ownerKey](const auto& pair) { return pair.first == ownerKey; }),
        s_destroyCallbacks.end());
}

Mesh::Mesh(gl::GL* gl)
    : m_gl(gl)
    , m_morphTargetSampler("u_MorphTargetsSampler")
    , m_id(s_nextMeshId++)
    , m_generation(1)
{
    m_morphTargetSamplerState.magFilter = gl::TextureMagFilter::Nearest;
    m_morphTargetSamplerState.minFilter = gl::TextureMinFilter::Nearest;
    m_morphTargetSamplerState.wrapS = gl::TextureWrap::ClampToEdge;
    m_morphTargetSamplerState.wrapT = gl::TextureWrap::ClampToEdge;
    m_morphTargetSamplerState.wrapR = gl::TextureWrap::ClampToEdge;
    m_morphTargetSampler.samplerState = &m_morphTargetSamplerState;
}

Mesh::Mesh(Mesh&& other)
    : indexCount(other.indexCount)
    , indexByteOffset(other.indexByteOffset)
    , vertexCount(other.vertexCount)
    , primitiveTopology(other.primitiveTopology)
    , polygonMode(other.polygonMode)
    , frontFace(other.frontFace)
    , indexFormat(other.indexFormat)
    , attributes(std::move(other.attributes))
    , defines(std::move(other.defines))
    , uniforms(std::move(other.uniforms))
    , uniformBlocks(std::move(other.uniformBlocks))
    , vertexShader(std::move(other.vertexShader))
    , indexBuffer(other.indexBuffer)
    , m_transform(other.m_transform)
    , m_gl(other.m_gl)
    , m_morphTargetSamplerState(other.m_morphTargetSamplerState)
    , m_morphTargetSampler(other.m_morphTargetSampler)
    , m_morphTargetTexture(std::move(other.m_morphTargetTexture))
    , jointsSampler(other.jointsSampler)
    , m_id(other.m_id)
    , m_generation(other.m_generation)
{
    other.jointsSampler = nullptr;
    other.m_morphTargetTexture = nullptr;
    other.indexBuffer = nullptr;
    other.vertexCount = 0u;
    other.indexCount = 0u;
    other.indexByteOffset = 0u;
    other.m_id = 0;
    other.m_generation = 0;
}

Mesh::Mesh(const Mesh& other)
    : indexCount(other.indexCount)
    , indexByteOffset(other.indexByteOffset)
    , vertexCount(other.vertexCount)
    , primitiveTopology(other.primitiveTopology)
    , polygonMode(other.polygonMode)
    , frontFace(other.frontFace)
    , indexFormat(other.indexFormat)
    , attributes(other.attributes)
    , defines(other.defines)
    , uniforms(other.uniforms)
    , uniformBlocks(other.uniformBlocks)
    , vertexShader(other.vertexShader)
    , indexBuffer(other.indexBuffer)
    , m_transform(other.m_transform)
    , m_gl(other.m_gl)
    , m_morphTargetSamplerState(other.m_morphTargetSamplerState)
    , m_morphTargetSampler(other.m_morphTargetSampler)
    , m_morphTargetTexture(other.m_morphTargetTexture)
    , jointsSampler(other.jointsSampler)
    , m_id(s_nextMeshId++)
    , m_generation(1)
{
}

Mesh::~Mesh()
{
    if (m_id != 0) {
        notifyMeshDestroyed(m_id);
    }
}

void Mesh::updateTransform(const math::mat4x4& transform)
{
    m_transform = transform;
    setUniform("u_ModelMatrix", transform);
    setUniform("u_NormalMatrix", math::transpose(math::inverse(transform)));
}

void Mesh::resetWeightsUpdated()
{
    weightsUpdated = false;
}

void Mesh::updateWeights(const std::vector<float>& weights)
{
    // we check whether the weights have already been updated this frame,
    // to make sure that we do not override animated mesh weights with e.g. existing non-animated node weight data
    if (weightsUpdated) {
        return;
    } else {
        weightsUpdated = true;
    }

    bool morphingIsUsed = weights.empty() == false;
    int count = std::max(static_cast<int>(weights.size()), 1);
    defines.defineIf(morphingIsUsed, "USE_MORPHING");
    defines.defineIf(morphingIsUsed, "WEIGHT_COUNT", count);
    if (morphingIsUsed) {
        for (int i = 0; i < count; ++i) {
            setUniform("u_morphWeights[" + std::to_string(i) + "]", weights[i]);
        }
    }
}

bool Mesh::setUniformBlock(const std::string& name, const uint8_t* data, size_t byteSize)
{
    auto it = std::find_if(uniformBlocks.begin(), uniformBlocks.end(), [&name](const gl::UniformBlock& _uniform) {
        return _uniform.name == name;
    });

    if (it == uniformBlocks.end()) {
        uniformBlocks.emplace_back(m_gl, name).write(byteSize, data);
    } else {
        it->write(byteSize, data);
    }
    return true;
}

void Mesh::setMorphTargetTexture(uint32_t width, std::vector<float>& data, size_t textureCount)
{
    GLuint targetTexture;
    m_gl->genTextures(1, &targetTexture);
    m_gl->bindTexture(GL_TEXTURE_2D_ARRAY, targetTexture);
    m_gl->texImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA32F, width, width, textureCount, 0, GL_RGBA, GL_FLOAT, data.data());
    m_gl->texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_gl->texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_gl->texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_gl->texParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_morphTargetTexture = std::make_shared<gl::Texture>(m_gl, targetTexture, gl::TextureTarget::Texture2dArray);
    m_morphTargetSampler.texture = m_morphTargetTexture.get();
}
