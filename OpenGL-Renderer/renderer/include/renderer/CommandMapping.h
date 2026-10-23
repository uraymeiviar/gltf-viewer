// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "gl/GL.h"

#include "core/Hash.h"
#include "core/LinkedList.h"
#include "core/TupleHash.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ux3d::renderer::gl {
class ShaderCache;
class Program;
}

namespace ux3d::renderer {
class CommandGenerator;
struct CmdDesc;
class Mesh;
class Material;
class Camera;
using MeshMaterialTuple = std::tuple<Mesh*, Material*>;
class CommandMapping {
public:
    CommandMapping(gl::GL* gl, CommandGenerator& scene, gl::ShaderCache& shaderCache);
    // Fills commands with one command per drawable. Commands are owned by the
    // command cache and stay valid until the next update.
    void update(const core::LinkedList<MeshMaterialTuple>& drawables, std::vector<CmdDesc*>& commands);

    // Program-selection cache (P4.6). The key holds every input of the define
    // assembly and shader selection, so a hit returns exactly the program the
    // full path would build. Disabling it restores the uncached path (A/B checks).
    void setProgramCacheEnabled(bool enabled) { m_programCacheEnabled = enabled; }
    uint64_t getProgramCacheHits() const { return m_programCacheHits; }
    uint64_t getProgramCacheMisses() const { return m_programCacheMisses; }

    // Command-list cache (P4.6c). A drawable's command is reused when every
    // input makeCommand reads is unchanged: the same objects and vector storage
    // (commands hold pointers into them, so equal addresses give equal
    // commands), equal define content hashes, shader names and state. Only the
    // camera-distance sort key and the vertex layout lookup are refreshed.
    void setCommandCacheEnabled(bool enabled) { m_commandCacheEnabled = enabled; }
    uint64_t getCommandCacheHits() const { return m_commandCacheHits; }
    uint64_t getCommandCacheMisses() const { return m_commandCacheMisses; }
    void clearCommandCache() { m_commandCache.clear(); }

protected:
    struct ProgramKey {
        uint64_t meshDefines = 0;
        uint64_t materialDefines = 0;
        std::string vertexShader;
        std::string fragmentShader;
        uint32_t renderSettings = 0;
        uint32_t toneMapping = 0;
        uint32_t lightCount = 0;
        bool useIBL = false;
        bool usePunctualLights = false;
        bool operator==(const ProgramKey&) const = default;
    };
    struct ProgramKeyHash {
        size_t operator()(const ProgramKey& key) const;
    };

    struct CommandKey {
        const Mesh* mesh = nullptr;
        const Material* material = nullptr;
        const Camera* camera = nullptr;
        bool operator==(const CommandKey&) const = default;
    };
    struct CommandKeyHash {
        size_t operator()(const CommandKey& key) const;
    };
    struct CommandSignature {
        const void* renderPass = nullptr;
        const void* environment = nullptr;
        const void* lightUniforms = nullptr;
        size_t lightUniformCount = 0;
        const void* meshUniforms = nullptr;
        size_t meshUniformCount = 0;
        const void* meshBlocks = nullptr;
        size_t meshBlockCount = 0;
        const void* materialUniforms = nullptr;
        size_t materialUniformCount = 0;
        const void* materialSamplers = nullptr;
        size_t materialSamplerCount = 0;
        const void* morphTexture = nullptr;
        const void* jointsSampler = nullptr;
        const void* jointsTexture = nullptr;
        const void* indexBuffer = nullptr;
        uint64_t indexCount = 0;
        uint64_t indexByteOffset = 0;
        uint64_t vertexCount = 0;
        uint64_t meshDefines = 0;
        uint64_t materialDefines = 0;
        uint32_t indexFormat = 0;
        uint32_t renderSettings = 0;
        uint32_t toneMapping = 0;
        uint32_t lightCount = 0;
        uint32_t culling = 0;
        uint32_t winding = 0;
        uint32_t polygonMode = 0;
        uint32_t topology = 0;
        bool useIBL = false;
        bool usePunctualLights = false;
        bool operator==(const CommandSignature&) const = default;
    };
    struct CommandEntry {
        CommandSignature signature;
        std::string vertexShader;
        std::string fragmentShader;
        std::unique_ptr<CmdDesc> desc;
        uint64_t lastUse = 0;
    };

    std::optional<CmdDesc> makeCommand(MeshMaterialTuple meshMat);
    CommandSignature makeSignature(const Mesh& mesh, const Material& material) const;
    ProgramKey makeProgramKey(const Mesh& mesh, const Material& material) const;
    gl::Program* selectProgram(const Mesh& mesh, const Material& material);
    gl::Program* buildProgram(const Mesh& mesh, const Material& material);

protected:
    gl::GL* m_gl;
    gl::ShaderCache& m_shaderCache;
    CommandGenerator& m_scene; // TODO: Rename

    std::unordered_map<ProgramKey, gl::Program*, ProgramKeyHash> m_programCache;
    std::unordered_map<CommandKey, CommandEntry, CommandKeyHash> m_commandCache;
    bool m_commandCacheEnabled = true;
    uint64_t m_commandCacheHits = 0;
    uint64_t m_commandCacheMisses = 0;
    uint64_t m_updateSerial = 0;
    bool m_programCacheEnabled = true;
    uint64_t m_programCacheHits = 0;
    uint64_t m_programCacheMisses = 0;
};
}
