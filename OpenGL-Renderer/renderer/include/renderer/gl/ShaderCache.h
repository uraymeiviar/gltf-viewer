// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <unordered_map>

#include "Program.h"
#include "core/String.h"

namespace ux3d::core {
template <class T>
class Vector;
}

namespace ux3d::filesystem {
class Dir;
}

namespace ux3d::renderer {
class Defines;
}

namespace ux3d::renderer::gl {
class ShaderCache {
public:
    ShaderCache(GL* gl, const ux3d::filesystem::Dir& directory);
    ~ShaderCache();

    // Because the shader cache owns GL handles, it is not trivial to implement copy/move operations.
    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;
    ShaderCache(ShaderCache&&) = delete;
    ShaderCache& operator=(ShaderCache&&) = delete;

    bool addShader(const core::string& shaderIdentfier, const core::string& shaderText);

    GLuint selectShader(const core::string& shaderIdentfier, const Defines& defines);

    Program* getProgram(GLuint vertex, GLuint fragment);

private:
    bool getIncludes(core::string& shaderText, core::Vector<const char*>& outShaderStrings);

private:
    GL* m_gl = nullptr;
    std::unordered_map<core::string, core::string> m_ShaderTexts;
    std::unordered_map<uint64_t, GLuint> m_CompiledShaders;
    std::unordered_map<uint64_t, Program> m_ShaderPermutations;
};
}
