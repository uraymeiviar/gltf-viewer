// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gl/Attribute.h"
#include "gl/Buffer.h"
#include "gl/Uniform.h"
#include "gl/UniformBlock.h"
#include "math/VectorTypes.h"
#include "renderer/Defines.h"
#include "renderer/gl/Sampler.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ux3d::renderer::gl {
class Texture;
}

namespace ux3d::renderer {
class Mesh {
public:
    Mesh(gl::GL*);
    Mesh(Mesh&&);
    Mesh(const Mesh&);
    ~Mesh();

    void updateTransform(const math::mat4x4& transform);
    math::vec3 getPosition() const { return math::vec3(m_transform[3]); }
    const math::mat4x4& getTransform() const { return m_transform; }

    void updateWeights(const std::vector<float>& weights);

    template <class... Args>
    void setUniform(const std::string& name, const Args&... data);

    bool setUniformBlock(const std::string& name, const uint8_t* data, size_t byteSize);

    void setMorphTargetTexture(uint32_t width, std::vector<float>& data, size_t textureCount);
    const gl::Sampler& getMorphTargetSampler() const { return m_morphTargetSampler; }

    void resetWeightsUpdated();

    uint64_t getId() const noexcept { return m_id; }
    uint64_t getGeneration() const noexcept { return m_generation; }
    void bumpGeneration() noexcept { ++m_generation; }

    using MeshDestroyCallback = std::function<void(uint64_t meshId)>;
    static void addMeshDestroyCallback(void* ownerKey, MeshDestroyCallback cb);
    static void removeMeshDestroyCallback(void* ownerKey);

    std::vector<gl::Attribute> attributes;
    std::vector<gl::Uniform> uniforms;
    std::vector<gl::UniformBlock> uniformBlocks;
    uint32_t indexCount = 0;
    uint32_t indexByteOffset = 0;
    uint32_t vertexCount = 0;
    gl::PolygonMode polygonMode = gl::PolygonMode::Fill;
    gl::Winding frontFace = gl::Winding::CounterClockwise;
    gl::PrimitiveTopology primitiveTopology = gl::PrimitiveTopology::TriangleList;
    gl::PrimitiveType indexFormat = gl::PrimitiveType::UnsignedShort;
    size_t morphTargetAttributeCount = 0;
    bool nukeRotation = false;
    bool nukeScale = false;
    const gl::Sampler* jointsSampler = nullptr;
    Defines defines;
    std::string vertexShader;
    gl::Buffer* indexBuffer = nullptr;

private:
    gl::GL* m_gl = nullptr;
    std::shared_ptr<gl::Texture> m_morphTargetTexture = nullptr;
    gl::Sampler m_morphTargetSampler;
    gl::SamplerState m_morphTargetSamplerState;
    math::mat4x4 m_transform = math::mat4x4(1.0f);

    uint64_t m_id = 0;
    uint64_t m_generation = 1;

    bool weightsUpdated = false; // indicates whether the weights have been updated already this frame
};

template <class... Args>
void Mesh::setUniform(const std::string& name, const Args&... data)
{
    auto it = std::find_if(uniforms.begin(), uniforms.end(), [&name](const gl::Uniform& uniform) {
        return uniform.name == name;
    });

    if (it == uniforms.end()) {
        uniforms.emplace_back(name).write(data...);
    } else {
        it->write(data...);
    }
}
}
