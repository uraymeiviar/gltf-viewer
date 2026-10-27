// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/ShaderCache.h"
#include "core/Hash.h"
#include "core/LoadProgress.h"
#include "filesystem/Dir.h"
#include "filesystem/File.h"
#include "filesystem/Path.h"
#include "renderer/Defines.h"
#include "renderer/gl/Util.h"

#include <regex>
#include <sstream>

ux3d::renderer::gl::ShaderCache::ShaderCache(GL* gl, const ux3d::filesystem::Dir& directory)
    : m_gl(gl)
{
    core::string shaderText;
    for (const auto& file : directory.listFiles()) {
        if (file.toString().find(".glsl") != core::string::npos || file.toString().find(".frag") != core::string::npos || file.toString().find(".vert") != core::string::npos || file.toString().find(".compute") != core::string::npos) {
            shaderText.clear();
            if (file.load(shaderText)) {
                addShader(file.getName(), shaderText);
            }
        }
    }
}

ux3d::renderer::gl::ShaderCache::~ShaderCache()
{
    for (const auto& [_, handle] : m_CompiledShaders) {
        if (handle != invalid) {
            m_gl->deleteShader(handle);
        }
    }
}

bool ux3d::renderer::gl::ShaderCache::addShader(const core::string& shaderIdentfier, const core::string& shaderText)
{
    if (m_ShaderTexts.find(shaderIdentfier) == m_ShaderTexts.end()) {
        return m_ShaderTexts.emplace(shaderIdentfier, std::string("// Source file: ") + shaderIdentfier + std::string("\n\n") + shaderText).second;
    } else {
        core::debugLog(core::LogLevel::Warning, "%s already cached", shaderIdentfier.c_str());
        return false;
    }
}

GLuint ux3d::renderer::gl::ShaderCache::selectShader(const core::string& shaderIdentfier, const Defines& defines)
{
    core::Hasher shaderTextHash;

    shaderTextHash << shaderIdentfier << defines.hash();

    const uint64_t hash = shaderTextHash;

    // check if we already compiled this shader variant
    auto shader = m_CompiledShaders.find(hash);

    if (shader != m_CompiledShaders.end()) {
        return shader->second;
    }

    // find shader text and compile with defines

    auto text = m_ShaderTexts.find(shaderIdentfier);

    if (text == m_ShaderTexts.end()) {
        core::logError("Shader text for %s not found in cache", shaderIdentfier.c_str());
        return invalid;
    }

    GLenum type = invalid;

    if (shaderIdentfier.find(".vert") != core::string::npos) {
        type = GL_VERTEX_SHADER;
    } else if (shaderIdentfier.find(".frag") != core::string::npos) {
        type = GL_FRAGMENT_SHADER;
    } else if (shaderIdentfier.find(".tess") != core::string::npos) {
        type = GL_TESS_CONTROL_SHADER;
    } else if (shaderIdentfier.find(".eval") != core::string::npos) {
        type = GL_TESS_EVALUATION_SHADER;
    } else if (shaderIdentfier.find(".geo") != core::string::npos) {
        type = GL_GEOMETRY_SHADER;
    }

    if (type == invalid) {
        core::logError("Unsupported shader type %s", shaderIdentfier.c_str());
        return invalid;
    }

    GLuint glShader = m_gl->createShader(type);
    if (!checkGLError(m_gl, "Could not create gl shader")) {
        return invalid;
    }

    core::Vector<const char*> strings;
    strings.emplace_back("#version 410\nprecision highp float;\nprecision highp int;\n");

    core::string defineLines = defines.toLines();
    strings.emplace_back(defineLines.c_str());

    core::string shaderText(text->second);

    // prepend included sources
    if (!getIncludes(shaderText, strings)) {
        return invalid;
    }

    // append the shader text we want to compile now
    strings.push_back(shaderText.c_str());

    m_gl->shaderSource(glShader, static_cast<GLsizei>(strings.size()), strings.data(), nullptr);
    if (!checkGLError(m_gl, "Could not load shader source")) {
        return invalid;
    }

    const ux3d::core::loading::Timer compileTimer(ux3d::core::loading::Shaders);
    m_gl->compileShader(glShader);
    GLint compiled = GL_FALSE;
    m_gl->getShaderiv(glShader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE && checkGLError(m_gl, "Failed to compile shader")) {
        m_CompiledShaders.emplace(hash, glShader);
        return glShader;
    }

#ifndef NDEBUG
    // Error occured --> construct a message from the opengl output
    GLchar msg[1024];
    GLsizei length = 0;
    m_gl->getShaderInfoLog(glShader, sizeof(msg), &length, msg);

    // extract lines of the shader
    std::vector<std::string> shaderLines;
    std::string fullShaderText;
    int preambleLineOffset = 0;
    for (auto subtext : strings) {
        // extract lines of the substring
        auto ss = std::stringstream { std::string(subtext) };
        for (std::string line; std::getline(ss, line, '\n');) {
            shaderLines.push_back(line);
            if (subtext != strings.back()) {
                // count line offset as long as we are not in the actual shader text
                ++preambleLineOffset;
            }
        }
    }

    // Extract some information from the shader to provide more meaningful error messages
    std::string errorString(msg);
    std::smatch matches;
    std::regex regex("(?:[A-Z]: 0:([0-9]+):)|(?:[0-9]+\\(([0-9]+)\\) : )(.*)"); // TODO: on different platforms this regex might need to be extended

    // loop over regex matches, each matches tuple will contain the whole matched
    // string, the first capture group (for line number) and the second capture group (for messsage)
    while (std::regex_search(errorString, matches, regex)) {
        int lineNum = -1;
        if (matches[1].matched) {
            lineNum = stoi(matches[1]);
        } else if (matches[2].matched) {
            lineNum = stoi(matches[2]);
        }
        if (lineNum < 0 || shaderLines.size() < static_cast<unsigned int>(lineNum)) {
            // something most likely went wrong with regex matching, fall back to basic logging
            std::string message = matches[0];
            core::logError("%s: %s", shaderIdentfier.c_str(), message.c_str());
            continue;
        }

        // extract the line for which the error was shown and construct a more helpful error message
        std::string line = shaderLines[lineNum - 1];
        std::string message = matches[3];
        core::logError("%s:%d %s\n%s", shaderIdentfier.c_str(), lineNum - preambleLineOffset, line.c_str(), message.c_str());

        // proceed search after the current match
        errorString = matches.suffix().str();
    }
    errorString = "Could not select shader: " + shaderIdentfier + "\n" + errorString;
    core::debugLog(core::LogLevel::Error, errorString.c_str());
#else
    GLchar msg[1024];
    GLsizei length = 0;
    m_gl->getShaderInfoLog(glShader, sizeof(msg), &length, msg);
    core::logError("Shader %s could not be compiled:\n%s", shaderIdentfier.c_str(), msg);
#endif

    return invalid;
}

auto ux3d::renderer::gl::ShaderCache::getProgram(GLuint vertex, GLuint fragment) -> Program*
{
    const uint64_t hash = (uint64_t(vertex) << 32u) | fragment;

    auto perm = m_ShaderPermutations.find(hash);

    GLchar infoLog[1024];
    GLsizei infoLogLength;

    // shader permutation already compiled
    if (perm != m_ShaderPermutations.end()) {
        return &perm->second;
    }

    // link the program and cache the permutation

    const GLuint program = m_gl->createProgram();

    if (program == 0) {
        checkGLError(m_gl, "Could not create program");
        return nullptr;
    }

    m_gl->attachShader(program, vertex);
    if (!checkGLError(m_gl, "Could not attach vertex shader")) {
        m_gl->getShaderInfoLog(vertex, sizeof(infoLog), &infoLogLength, infoLog);
        core::logError(infoLog);
        return nullptr;
    }

    m_gl->attachShader(program, fragment);
    if (!checkGLError(m_gl, "Could not attach fragment shader")) {
        m_gl->getShaderInfoLog(fragment, sizeof(infoLog), &infoLogLength, infoLog);
        core::logError(infoLog);
        return nullptr;
    }

    const ux3d::core::loading::Timer linkTimer(ux3d::core::loading::ShaderLink);
    m_gl->linkProgram(program);
    GLint status;
    m_gl->getProgramiv(program, GL_LINK_STATUS, &status);

    if (status != GL_TRUE) {
        m_gl->getProgramInfoLog(program, sizeof(infoLog), &infoLogLength, infoLog);
        core::logError(infoLog);
        return nullptr;
    }

    m_gl->detachShader(program, vertex);
    m_gl->detachShader(program, fragment);

    if (!checkGLError(m_gl, "Could not detach shaders from gl program")) {
        return nullptr;
    }

    auto [iter, didInsert] = m_ShaderPermutations.emplace(hash, Program(m_gl, program));
    if (didInsert) {
        return &iter->second;
    } else {
        return nullptr;
    }
}

bool ux3d::renderer::gl::ShaderCache::getIncludes(core::string& shaderText, core::Vector<const char*>& outShaderStrings)
{
    size_t includePosition = shaderText.find("#include");

    core::string includePath;

    while (includePosition != core::string::npos) {
        size_t nameStart = shaderText.find_first_of("\"<", includePosition);
        size_t nameEnd = shaderText.find_first_of("\">", nameStart);

        if (nameStart != core::string::npos && nameEnd != core::string::npos) {
            includePath = shaderText.substr(nameStart + 1, nameEnd - nameStart - 1);

            if (auto iter = m_ShaderTexts.find(includePath);
                iter != m_ShaderTexts.end()) {
                outShaderStrings.push_back(iter->second.data());
            } else {
                core::logError("Shader include %s not cached", includePath.c_str());
                return false;
            }

            shaderText.insert(includePosition, "// ");
            includePosition += sizeof("// #include");
        }

        includePosition = shaderText.find("#include", includePosition);
    }

    return true;
}
