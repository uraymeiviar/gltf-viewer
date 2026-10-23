// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "gl/GL.h"

#include "core/Hash.h"
#include "core/LinkedList.h"
#include "core/TupleHash.h"
#include <cstdint>
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
using MeshMaterialTuple = std::tuple<Mesh*, Material*>;
class CommandMapping {
public:
    CommandMapping(gl::GL* gl, CommandGenerator& scene, gl::ShaderCache& shaderCache);
    void update(const core::LinkedList<MeshMaterialTuple>& drawables, std::vector<CmdDesc>& commands);

    // Program-selection cache (P4.6). The key holds every input of the define
    // assembly and shader selection, so a hit returns exactly the program the
    // full path would build. Disabling it restores the uncached path (A/B checks).
    void setProgramCacheEnabled(bool enabled) { m_programCacheEnabled = enabled; }
    uint64_t getProgramCacheHits() const { return m_programCacheHits; }
    uint64_t getProgramCacheMisses() const { return m_programCacheMisses; }

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

    std::optional<CmdDesc> makeCommand(MeshMaterialTuple meshMat);
    ProgramKey makeProgramKey(const Mesh& mesh, const Material& material) const;
    gl::Program* selectProgram(const Mesh& mesh, const Material& material);
    gl::Program* buildProgram(const Mesh& mesh, const Material& material);

protected:
    gl::GL* m_gl;
    gl::ShaderCache& m_shaderCache;
    CommandGenerator& m_scene; // TODO: Rename

    std::unordered_map<ProgramKey, gl::Program*, ProgramKeyHash> m_programCache;
    bool m_programCacheEnabled = true;
    uint64_t m_programCacheHits = 0;
    uint64_t m_programCacheMisses = 0;
};
}
