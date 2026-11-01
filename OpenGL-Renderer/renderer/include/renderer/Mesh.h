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
    // A1.4: world-space centre of the primitive's POSITION bounds (the node pivot when the
    // bounds cannot be computed), refreshed by GltfRenderLayer::createScene.
    math::vec3 getBoundingCenter() const { return m_boundingCenter; }
    void setBoundingCenter(const math::vec3& center) { m_boundingCenter = center; }

    void updateWeights(const std::vector<float>& weights);

    // EXT_mesh_gpu_instancing: the mesh is drawn once per instance (glDraw*Instanced, in every pass). Each instance's
    // model matrix is the node world matrix x T * R * S of the instance, composed exactly like a child node's world
    // matrix (TransformHierarchyNode::traverse), and its normal matrix like updateTransform's. Both are recomputed
    // on every updateTransform and streamed to primitive.vert (USE_INSTANCING) from a per-instance vertex buffer
    // (u_ModelMatrix then u_NormalMatrix, 128 bytes per instance, the layout of the P8.3 instanced draws).
    // `boxValid`, `boxLo`, `boxHi`: the primitive's static POSITION box in model space, for the world bounds.
    struct Instance {
        math::vec3 translation = math::vec3(0.0f);
        math::mat4x4 rotation = math::mat4x4(1.0f);
        math::vec3 scale = math::vec3(1.0f);
    };
    void setInstances(std::vector<Instance> instances, bool boxValid, const math::vec3& boxLo, const math::vec3& boxHi);
    void clearInstances();
    bool isInstanced() const { return m_instancing != nullptr && !m_instancing->instances.empty(); }
    uint32_t getInstanceCount() const { return isInstanced() ? static_cast<uint32_t>(m_instancing->instances.size()) : 0u; }
    const gl::Buffer* getInstanceBuffer() const { return m_instancing != nullptr ? m_instancing->buffer.get() : nullptr; }
    // The world-space union of every instance's POSITION box (false without a static box).
    bool getInstanceWorldBounds(math::vec3& lo, math::vec3& hi) const;

    template <class... Args>
    void setUniform(const std::string& name, const Args&... data);

    bool setUniformBlock(const std::string& name, const uint8_t* data, size_t byteSize);

    void setMorphTargetTexture(uint32_t width, std::vector<float>& data, size_t textureCount);
    const gl::Sampler& getMorphTargetSampler() const { return m_morphTargetSampler; }

    void resetWeightsUpdated();

    uint64_t getId() const noexcept { return m_id; }
    uint64_t getGeneration() const noexcept { return m_generation; }
    void bumpGeneration() noexcept { ++m_generation; }

    // A1.9: ASOBO_asset_optimized stores every triangle clockwise (Direct3D). The
    // mirrored-node rule and a viewport's invert-winding override are already in
    // `frontFace`; this applies the packaged storage convention on top, so the two flips
    // cancel for a mirrored packaged node. Undeclared files return `frontFace` unchanged.
    gl::Winding effectiveFrontFace() const
    {
        if (!assetOptimizedWinding) {
            return frontFace;
        }
        return frontFace == gl::Winding::Clockwise ? gl::Winding::CounterClockwise : gl::Winding::Clockwise;
    }

    using MeshDestroyCallback = std::function<void(uint64_t meshId)>;
    static void addMeshDestroyCallback(void* ownerKey, MeshDestroyCallback cb);
    static void removeMeshDestroyCallback(void* ownerKey);

    std::vector<gl::Attribute> attributes;
    std::vector<gl::Uniform> uniforms;
    std::vector<gl::UniformBlock> uniformBlocks;
    uint32_t indexCount = 0;
    uint32_t indexByteOffset = 0;
    // Packaged MSFS primitives may address a later vertex block with 16-bit indices.
    GLint baseVertex = 0;
    uint32_t vertexCount = 0;
    gl::PolygonMode polygonMode = gl::PolygonMode::Fill;
    // Winding of the node's geometry after the mirrored-node rule and a viewport's
    // invert-winding override. The packaged ASOBO layout stores the opposite winding,
    // which effectiveFrontFace() applies on top, so every pass that draws a glTF mesh
    // gets the same answer. Later passes (the A2 decal prepass) must query this, not
    // `frontFace` directly.
    gl::Winding frontFace = gl::Winding::CounterClockwise;
    bool assetOptimizedWinding = false;
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
    math::vec3 m_boundingCenter = math::vec3(0.0f);

    uint64_t m_id = 0;
    uint64_t m_generation = 1;

    struct Instancing {
        std::vector<Instance> instances;
        std::unique_ptr<gl::Buffer> buffer;
        std::vector<math::mat4x4> data; // per instance: model matrix, normal matrix
        bool boxValid = false;
        math::vec3 boxLo = math::vec3(0.0f);
        math::vec3 boxHi = math::vec3(0.0f);
        bool worldValid = false;
        math::vec3 worldLo = math::vec3(0.0f);
        math::vec3 worldHi = math::vec3(0.0f);
    };
    // Shared by copies (they draw the same instances); set by the mesh mapping, refreshed by updateTransform.
    std::shared_ptr<Instancing> m_instancing;
    void updateInstances();

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
