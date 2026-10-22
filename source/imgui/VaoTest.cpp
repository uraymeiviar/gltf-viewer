// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/VaoTest.h"
#include <glad/glad.h>
#include "imgui/GladGL.h"
#include "imgui/stb_image_write.h"
#include "renderer/Renderer.h"
#include "renderer/CommandGenerator.h"
#include "renderer/CommandExecutor.h"
#include "renderer/Command.h"
#include "renderer/Mesh.h"
#include "renderer/gl/Buffer.h"
#include "renderer/gl/Program.h"
#include "renderer/gl/VertexLayout.h"
#include "renderer/gl/VertexLayoutCache.h"

#include <array>
#include <sstream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cmath>

namespace ux3d::imgui {

namespace {

GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

template <typename T>
ux3d::renderer::gl::Buffer makeGlBuffer(ux3d::renderer::gl::GL* gl, const std::vector<T>& data)
{
    return ux3d::renderer::gl::Buffer(gl, ux3d::core::MemoryRange<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(T)));
}

} // namespace

SelfCheckResult runVaoSelfCheck(
    ux3d::renderer::gl::GL* gl,
    bool forceFailure,
    const std::string& testCase,
    ux3d::renderer::Renderer* glRenderer,
    ux3d::renderer::CommandGenerator* commandGenerator,
    std::function<bool(const std::string&)> reloadAndDraw
)
{
    SelfCheckResult result;
    result.success = true;
    std::ostringstream report;
    report << "P3.3 Vertex Array Object (VAO) reuse and lifecycle self-check\n";
    report << "Test case filter: " << (testCase.empty() ? "all" : testCase) << "\n";

    size_t checks = 0;
    std::vector<std::string> failures;

    auto check = [&](bool condition, const std::string& desc) {
        ++checks;
        if (!condition) {
            result.success = false;
            failures.push_back(desc);
            report << "  FAIL: " << desc << "\n";
        } else {
            report << "  PASS: " << desc << "\n";
        }
    };

    if (forceFailure) {
        check(false, "forced VAO self-check failure requested");
        report << "\nTotal assertions evaluated: " << checks << "\n";
        report << "Failed assertions: " << failures.size() << "\n";
        report << "Result: FAIL\n";
        result.success = false;
        result.report = report.str();
        return result;
    }

    if (gl == nullptr) {
        check(false, "GL wrapper pointer is null");
        report << "\nTotal assertions evaluated: " << checks << "\n";
        report << "Failed assertions: " << failures.size() << "\n";
        report << "Result: FAIL\n";
        result.success = false;
        result.report = report.str();
        return result;
    }

    const bool runAll = testCase.empty() || testCase == "all";

    // -------------------------------------------------------------
    // Shaders and Program Fixtures
    // -------------------------------------------------------------
    const char* vsBasicSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec3 a_position;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_position, 1.0);\n"
        "}\n";

    const char* fsColorSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(0.2, 0.8, 0.4, 1.0);\n"
        "}\n";

    const char* vsMultiAttrSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec3 a_position;\n"
        "layout(location = 1) in vec3 a_normal;\n"
        "layout(location = 2) in vec4 a_tangent;\n"
        "layout(location = 3) in vec4 a_color_0;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_position, 1.0) + vec4(a_normal * 0.0001 + a_tangent.xyz * 0.0001 + a_color_0.xyz * 0.0001, 0.0);\n"
        "}\n";

    // Program Norm: active a_position (loc 0) and active a_normal (loc 1)
    const char* vsNormSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec3 a_position;\n"
        "layout(location = 1) in vec3 a_normal;\n"
        "out vec4 v_color;\n"
        "void main() {\n"
        "    v_color = vec4(a_normal, 1.0);\n"
        "    gl_Position = vec4(a_position, 1.0);\n"
        "}\n";

    const char* fsVaryingColorSrc =
        "#version 330 core\n"
        "in vec4 v_color;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = v_color;\n"
        "}\n";

    // Program Col: active a_position (loc 0) and active a_color_0 (loc 2)
    const char* vsColSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec3 a_position;\n"
        "layout(location = 2) in vec4 a_color_0;\n"
        "out vec4 v_color;\n"
        "void main() {\n"
        "    v_color = a_color_0;\n"
        "    gl_Position = vec4(a_position, 1.0);\n"
        "}\n";

    GLuint vsBasic = compileShader(GL_VERTEX_SHADER, vsBasicSrc);
    GLuint fsColor = compileShader(GL_FRAGMENT_SHADER, fsColorSrc);
    GLuint progBasicHandle = linkProgram(vsBasic, fsColor);

    GLuint vsMulti = compileShader(GL_VERTEX_SHADER, vsMultiAttrSrc);
    GLuint progMultiHandle = linkProgram(vsMulti, fsColor);

    GLuint fsVarying = compileShader(GL_FRAGMENT_SHADER, fsVaryingColorSrc);
    GLuint vsNorm = compileShader(GL_VERTEX_SHADER, vsNormSrc);
    GLuint progNormHandle = linkProgram(vsNorm, fsVarying);

    GLuint vsCol = compileShader(GL_VERTEX_SHADER, vsColSrc);
    GLuint progColHandle = linkProgram(vsCol, fsVarying);

    check(progBasicHandle != 0, "Basic test shader program linked successfully");
    check(progMultiHandle != 0, "Multi-attribute test shader program linked successfully");
    check(progNormHandle != 0, "Normal-varying test shader program linked successfully");
    check(progColHandle != 0, "Color-varying test shader program linked successfully");

    ux3d::renderer::gl::Program progBasic(gl, progBasicHandle);
    ux3d::renderer::gl::Program progMulti(gl, progMultiHandle);
    ux3d::renderer::gl::Program progNorm(gl, progNormHandle);
    ux3d::renderer::gl::Program progCol(gl, progColHandle);

    check(progBasic.getId() != 0, "progBasic has nonzero monotonic id");
    check(progMulti.getId() != 0, "progMulti has nonzero monotonic id");
    check(progNorm.getId() != 0, "progNorm has nonzero monotonic id");
    check(progCol.getId() != 0, "progCol has nonzero monotonic id");
    check(progNorm.getId() != progCol.getId(), "progNorm and progCol have distinct program IDs");

    // Common geometry buffers
    std::vector<float> posData = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    auto posBuffer = makeGlBuffer(gl, posData);

    std::vector<float> normData = {
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };
    auto normBuffer = makeGlBuffer(gl, normData);

    std::vector<float> tangData = {
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 1.0f
    };
    auto tangBuffer = makeGlBuffer(gl, tangData);

    std::vector<float> colData = {
        1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f
    };
    auto colBuffer = makeGlBuffer(gl, colData);

    std::vector<uint16_t> idxData = { 0, 1, 2 };
    auto idxBuffer = makeGlBuffer(gl, idxData);

    // =============================================================
    // Section 0: Nonzero HDR Rotation Behavior (Degree-to-Radian Verification)
    // =============================================================
    if ((runAll || testCase == "hdr-rotation") && commandGenerator != nullptr) {
        report << "\n--- Section 0: Nonzero HDR Rotation Behavior ---\n";

        // Set rotation to 90 degrees: cos(90 deg) ~ 0.0, sin(90 deg) ~ 1.0
        commandGenerator->setEnvironmentMapRotation(90.0f);
        const glm::mat3x3& rot90 = commandGenerator->getEnvironmentMapRotation();

        const bool rot90DiagonalCos = (std::abs(rot90[0][0]) < 1e-4f) &&
                                      (std::abs(rot90[1][1] - 1.0f) < 1e-4f) &&
                                      (std::abs(rot90[2][2]) < 1e-4f);
        check(rot90DiagonalCos, "setEnvironmentMapRotation(90.0f) produces cos(90 deg) ~ 0.0 on X/Z diagonals");

        const bool rot90OffDiagonalSin = (std::abs(std::abs(rot90[0][2]) - 1.0f) < 1e-4f) ||
                                         (std::abs(std::abs(rot90[2][0]) - 1.0f) < 1e-4f);
        check(rot90OffDiagonalSin, "setEnvironmentMapRotation(90.0f) produces sin(90 deg) ~ 1.0 on off-diagonals");

        // Set rotation to 180 degrees: cos(180 deg) = -1.0, sin(180 deg) = 0.0
        commandGenerator->setEnvironmentMapRotation(180.0f);
        const glm::mat3x3& rot180 = commandGenerator->getEnvironmentMapRotation();
        const bool rot180Cos = (std::abs(rot180[0][0] - (-1.0f)) < 1e-4f) &&
                               (std::abs(rot180[2][2] - (-1.0f)) < 1e-4f);
        check(rot180Cos, "setEnvironmentMapRotation(180.0f) produces cos(180 deg) ~ -1.0 on X/Z diagonals");

        // Reset rotation to 0.0f (identity)
        commandGenerator->setEnvironmentMapRotation(0.0f);
        const glm::mat3x3& rot0 = commandGenerator->getEnvironmentMapRotation();
        const bool rot0Identity = (std::abs(rot0[0][0] - 1.0f) < 1e-4f) &&
                                  (std::abs(rot0[1][1] - 1.0f) < 1e-4f) &&
                                  (std::abs(rot0[2][2] - 1.0f) < 1e-4f);
        check(rot0Identity, "setEnvironmentMapRotation(0.0f) restores identity rotation matrix");
    }

    // =============================================================
    // Section 1: Steady-State Reuse Across Successive Queries
    // =============================================================
    if (runAll || testCase == "steady-state") {
        report << "\n--- Section 1: Steady-State VAO Reuse ---\n";
        ux3d::renderer::gl::VertexLayoutCache localCache(gl);

        ux3d::renderer::Mesh mesh(gl);
        ux3d::renderer::gl::Attribute attrPos("a_position");
        attrPos.buffer = &posBuffer;
        attrPos.dimension = 3;
        attrPos.type = ux3d::renderer::gl::ComponentType::Float;
        mesh.attributes.push_back(attrPos);

        ux3d::renderer::gl::Attribute attrNorm("a_normal");
        attrNorm.buffer = &normBuffer;
        attrNorm.dimension = 3;
        attrNorm.type = ux3d::renderer::gl::ComponentType::Float;
        mesh.attributes.push_back(attrNorm);

        check(mesh.getId() != 0, "Mesh assigned monotonic id");
        check(mesh.getGeneration() == 1, "Mesh initial generation is 1");

        // Query 1 (cold miss)
        auto layout1 = localCache.getOrCreate(&mesh, &progBasic);
        check(layout1 != nullptr, "Query 1 returns valid VertexLayout");
        check(layout1->handle != 0, "Query 1 VertexLayout has valid GL handle");
        check(glIsVertexArray(layout1->handle) == GL_TRUE, "Query 1 VAO handle is valid GL vertex array");
        check(localCache.size() == 1, "Cache size is 1 after Query 1");

        // Query 2 (warm hit)
        auto layout2 = localCache.getOrCreate(&mesh, &progBasic);
        check(layout2 != nullptr, "Query 2 returns valid VertexLayout");
        check(layout1 == layout2, "Query 2 returns identical std::shared_ptr instance (pointer equality)");
        check(layout1->handle == layout2->handle, "Query 2 retains identical driver VAO handle");
        check(localCache.size() == 1, "Cache size remains 1 (0 spurious growth)");

        // 10 successive queries
        bool allMatched = true;
        for (int q = 0; q < 10; ++q) {
            auto layoutNext = localCache.getOrCreate(&mesh, &progBasic);
            if (layoutNext != layout1 || layoutNext->handle != layout1->handle) {
                allMatched = false;
                break;
            }
        }
        check(allMatched, "10 successive queries in steady state return cached layout without recreation");
        check(localCache.size() == 1, "Cache size is exactly 1 after 10 queries");
    }

    // =============================================================
    // Section 1B: Deterministic Cache Scaling & Work Counters
    // =============================================================
    if (runAll || testCase == "scaling") {
        report << "\n--- Section 1B: Deterministic Cache Scaling & Work Counters ---\n";
        ux3d::renderer::gl::VertexLayoutCache scalingCache(gl);

        const size_t numMeshes = 50;
        std::vector<std::unique_ptr<ux3d::renderer::Mesh>> meshList;
        meshList.reserve(numMeshes);

        for (size_t i = 0; i < numMeshes; ++i) {
            auto m = std::make_unique<ux3d::renderer::Mesh>(gl);
            ux3d::renderer::gl::Attribute aPos("a_position");
            aPos.buffer = &posBuffer;
            aPos.dimension = 3;
            aPos.type = ux3d::renderer::gl::ComponentType::Float;
            m->attributes.push_back(aPos);
            meshList.push_back(std::move(m));
        }

        // Insertion of 50 distinct meshes
        scalingCache.resetStats();
        for (size_t i = 0; i < numMeshes; ++i) {
            scalingCache.getOrCreate(meshList[i].get(), &progBasic);
        }
        check(scalingCache.size() == numMeshes, "Scaling cache has 50 entries after populating 50 meshes");
        check(scalingCache.getStats().unrelatedEntriesScanned == 0,
              "Populating 50 meshes scanned 0 unrelated entries across all insertions");

        // Insertion of 51st mesh: must visit 0 entries of the prior 50 meshes (O(1) insertion)
        auto mesh51 = std::make_unique<ux3d::renderer::Mesh>(gl);
        ux3d::renderer::gl::Attribute aPos51("a_position");
        aPos51.buffer = &posBuffer;
        aPos51.dimension = 3;
        aPos51.type = ux3d::renderer::gl::ComponentType::Float;
        mesh51->attributes.push_back(aPos51);

        scalingCache.resetStats();
        scalingCache.getOrCreate(mesh51.get(), &progBasic);
        check(scalingCache.getStats().entriesVisited == 0,
              "Populating 51st mesh visited 0 entries of prior 50 meshes (O(1) insertion)");
        check(scalingCache.getStats().unrelatedEntriesScanned == 0,
              "Populating 51st mesh scanned 0 unrelated entries");
        check(scalingCache.size() == 51, "Scaling cache size is exactly 51");

        // Generation bump of mesh 10: visits only mesh 10's own programs (never other 50 meshes)
        scalingCache.resetStats();
        meshList[10]->bumpGeneration();
        scalingCache.getOrCreate(meshList[10].get(), &progBasic);
        check(scalingCache.getStats().entriesVisited <= 2,
              "Generation bump visited only affected mesh's programs (entriesVisited <= 2)");
        check(scalingCache.getStats().unrelatedEntriesScanned == 0,
              "Generation bump scanned 0 unrelated entries of other meshes");

        // Invalidation of mesh 20: direct lookup and removal (visits 0 unrelated entries)
        scalingCache.resetStats();
        scalingCache.invalidateMesh(meshList[20]->getId());
        check(scalingCache.getStats().unrelatedEntriesScanned == 0,
              "Invalidating mesh scanned 0 unrelated entries of other meshes");
        check(scalingCache.size() == 50, "Cache size decremented to 50 after invalidating mesh");
        check(!scalingCache.hasMesh(meshList[20]->getId()), "Invalidated mesh is no longer present in cache");
    }

    // =============================================================
    // Section 2: Indexed vs Non-Indexed Element Array Buffer State
    // =============================================================
    if (runAll || testCase == "indexing") {
        report << "\n--- Section 2: Indexed vs Non-Indexed Core Profile EBO Invariant ---\n";
        ux3d::renderer::gl::VertexLayoutCache localCache(gl);

        ux3d::renderer::gl::Attribute attrPos("a_position");
        attrPos.buffer = &posBuffer;
        attrPos.dimension = 3;
        attrPos.type = ux3d::renderer::gl::ComponentType::Float;

        // Indexed Mesh
        ux3d::renderer::Mesh meshIndexed(gl);
        meshIndexed.attributes.push_back(attrPos);
        meshIndexed.indexBuffer = &idxBuffer;

        auto layoutIndexed = localCache.getOrCreate(&meshIndexed, &progBasic);
        check(layoutIndexed != nullptr, "Indexed layout created");

        glBindVertexArray(layoutIndexed->handle);
        GLint boundEboIndexed = 0;
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundEboIndexed);
        check(boundEboIndexed == static_cast<GLint>(idxBuffer.handle),
              "Indexed VAO explicitly holds element array buffer in its VAO state");

        // Non-Indexed Mesh
        ux3d::renderer::Mesh meshNonIndexed(gl);
        meshNonIndexed.attributes.push_back(attrPos);
        meshNonIndexed.indexBuffer = nullptr;

        auto layoutNonIndexed = localCache.getOrCreate(&meshNonIndexed, &progBasic);
        check(layoutNonIndexed != nullptr, "Non-indexed layout created");

        glBindVertexArray(layoutNonIndexed->handle);
        GLint boundEboNonIndexed = 0;
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundEboNonIndexed);
        check(boundEboNonIndexed == 0,
              "Non-indexed VAO explicitly has element array buffer 0 in its VAO state");

        // Sequence alternation test (switching between indexed and non-indexed VAOs)
        glBindVertexArray(layoutIndexed->handle);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundEboIndexed);
        check(boundEboIndexed == static_cast<GLint>(idxBuffer.handle),
              "Binding indexed VAO restores its element array buffer");

        glBindVertexArray(layoutNonIndexed->handle);
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundEboNonIndexed);
        check(boundEboNonIndexed == 0,
              "Switching to non-indexed VAO immediately unbinds element array buffer (zero contamination)");

        glBindVertexArray(0);
    }

    // =============================================================
    // Section 3: Differing Attribute Sets (Zero Attribute Contamination)
    // =============================================================
    if (runAll || testCase == "attributes") {
        report << "\n--- Section 3: Differing Attribute Sets & Contamination Isolation ---\n";
        ux3d::renderer::gl::VertexLayoutCache localCache(gl);

        ux3d::renderer::gl::Attribute attrPos("a_position");
        attrPos.buffer = &posBuffer;
        attrPos.dimension = 3;
        attrPos.type = ux3d::renderer::gl::ComponentType::Float;

        ux3d::renderer::gl::Attribute attrNorm("a_normal");
        attrNorm.buffer = &normBuffer;
        attrNorm.dimension = 3;
        attrNorm.type = ux3d::renderer::gl::ComponentType::Float;

        ux3d::renderer::gl::Attribute attrTang("a_tangent");
        attrTang.buffer = &tangBuffer;
        attrTang.dimension = 4;
        attrTang.type = ux3d::renderer::gl::ComponentType::Float;

        ux3d::renderer::gl::Attribute attrCol("a_color_0");
        attrCol.buffer = &colBuffer;
        attrCol.dimension = 4;
        attrCol.type = ux3d::renderer::gl::ComponentType::Float;

        // Mesh Full has all 4 attributes
        ux3d::renderer::Mesh meshFull(gl);
        meshFull.attributes.push_back(attrPos);
        meshFull.attributes.push_back(attrNorm);
        meshFull.attributes.push_back(attrTang);
        meshFull.attributes.push_back(attrCol);

        // Mesh Partial has only position and normal
        ux3d::renderer::Mesh meshPartial(gl);
        meshPartial.attributes.push_back(attrPos);
        meshPartial.attributes.push_back(attrNorm);

        auto layoutFull = localCache.getOrCreate(&meshFull, &progMulti);
        auto layoutPartial = localCache.getOrCreate(&meshPartial, &progMulti);

        check(layoutFull != nullptr && layoutPartial != nullptr, "Both full and partial layouts created");
        check(layoutFull != layoutPartial, "Full and partial meshes receive distinct layouts");
        check(layoutFull->handle != layoutPartial->handle, "Full and partial meshes receive distinct VAO handles");

        // Inspect enabled attribute arrays for Mesh Full
        glBindVertexArray(layoutFull->handle);
        GLint enPos = 0, enNorm = 0, enTang = 0, enCol = 0;
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enPos);
        glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enNorm);
        glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enTang);
        glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enCol);
        check(enPos == GL_TRUE && enNorm == GL_TRUE && enTang == GL_TRUE && enCol == GL_TRUE,
              "Mesh Full VAO has all 4 attribute locations enabled (0, 1, 2, 3)");

        // Inspect enabled attribute arrays for Mesh Partial
        glBindVertexArray(layoutPartial->handle);
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enPos);
        glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enNorm);
        glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enTang);
        glGetVertexAttribiv(3, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enCol);
        check(enPos == GL_TRUE && enNorm == GL_TRUE,
              "Mesh Partial VAO has position (0) and normal (1) enabled");
        check(enTang == GL_FALSE, "Mesh Partial VAO has tangent (2) explicitly DISABLED");
        check(enCol == GL_FALSE, "Mesh Partial VAO has color (3) explicitly DISABLED");

        // Alternating binding check
        glBindVertexArray(layoutFull->handle);
        glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enTang);
        check(enTang == GL_TRUE, "Rebinding Full VAO restores tangent enabled state");

        glBindVertexArray(layoutPartial->handle);
        glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enTang);
        check(enTang == GL_FALSE, "Rebinding Partial VAO isolates tangent (zero contamination)");

        glBindVertexArray(0);
    }

    // =============================================================
    // Section 4: Shader Variants Differentiation & Active Attribute Separation
    // =============================================================
    if (runAll || testCase == "shader-variants") {
        report << "\n--- Section 4: Shader Variants Layout Separation ---\n";

        // Query active attribute locations from GL driver to prove compiler did not optimize them out
        GLint normProgPosLoc = glGetAttribLocation(progNormHandle, "a_position");
        GLint normProgNormLoc = glGetAttribLocation(progNormHandle, "a_normal");
        GLint normProgColLoc = glGetAttribLocation(progNormHandle, "a_color_0");

        check(normProgPosLoc == 0, "progNorm has active a_position at location 0");
        check(normProgNormLoc == 1, "progNorm has active a_normal at location 1");
        check(normProgColLoc == -1, "progNorm does not have a_color_0 active (returns -1)");

        GLint colProgPosLoc = glGetAttribLocation(progColHandle, "a_position");
        GLint colProgColLoc = glGetAttribLocation(progColHandle, "a_color_0");
        GLint colProgNormLoc = glGetAttribLocation(progColHandle, "a_normal");

        check(colProgPosLoc == 0, "progCol has active a_position at location 0");
        check(colProgColLoc == 2, "progCol has active a_color_0 at location 2");
        check(colProgNormLoc == -1, "progCol does not have a_normal active (returns -1)");

        ux3d::renderer::gl::VertexLayoutCache localCache(gl);

        // Same mesh providing a_position, a_normal, and a_color_0
        ux3d::renderer::Mesh meshShared(gl);
        ux3d::renderer::gl::Attribute attrPos("a_position");
        attrPos.buffer = &posBuffer;
        attrPos.dimension = 3;
        attrPos.type = ux3d::renderer::gl::ComponentType::Float;
        meshShared.attributes.push_back(attrPos);

        ux3d::renderer::gl::Attribute attrNorm("a_normal");
        attrNorm.buffer = &normBuffer;
        attrNorm.dimension = 3;
        attrNorm.type = ux3d::renderer::gl::ComponentType::Float;
        meshShared.attributes.push_back(attrNorm);

        ux3d::renderer::gl::Attribute attrCol("a_color_0");
        attrCol.buffer = &colBuffer;
        attrCol.dimension = 4;
        attrCol.type = ux3d::renderer::gl::ComponentType::Float;
        meshShared.attributes.push_back(attrCol);

        // Same mesh evaluated against progNorm vs progCol
        auto layoutNorm = localCache.getOrCreate(&meshShared, &progNorm);
        auto layoutCol = localCache.getOrCreate(&meshShared, &progCol);

        check(layoutNorm != nullptr && layoutCol != nullptr, "Both progNorm and progCol layouts created");
        check(layoutNorm != layoutCol, "Same mesh with differing shader variants generates distinct layouts");
        check(layoutNorm->handle != layoutCol->handle, "Distinct driver VAOs assigned for different shader variants");
        check(localCache.size() == 2, "Cache size is 2 (indexed by meshId, gen, programId)");

        // Verify layoutNorm active attribute configuration
        glBindVertexArray(layoutNorm->handle);
        GLint en0 = 0, en1 = 0, en2 = 0;
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en0);
        glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en1);
        glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en2);
        check(en0 == GL_TRUE && en1 == GL_TRUE, "layoutNorm has locations 0 and 1 enabled");
        check(en2 == GL_FALSE, "layoutNorm has location 2 explicitly DISABLED");

        // Verify layoutCol active attribute configuration
        glBindVertexArray(layoutCol->handle);
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en0);
        glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en1);
        glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en2);
        check(en0 == GL_TRUE && en2 == GL_TRUE, "layoutCol has locations 0 and 2 enabled");
        check(en1 == GL_FALSE, "layoutCol has location 1 explicitly DISABLED");

        // Re-binding layoutNorm isolates locations without cross-program contamination
        glBindVertexArray(layoutNorm->handle);
        glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en1);
        glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en2);
        check(en1 == GL_TRUE && en2 == GL_FALSE, "Rebinding layoutNorm restores location 1 and keeps location 2 disabled");

        // Re-query returns respective cached layouts
        auto lNorm2 = localCache.getOrCreate(&meshShared, &progNorm);
        auto lCol2 = localCache.getOrCreate(&meshShared, &progCol);
        check(lNorm2 == layoutNorm, "Re-query progNorm returns cached layout");
        check(lCol2 == layoutCol, "Re-query progCol returns cached layout");
        check(localCache.size() == 2, "Cache size remains 2");
        glBindVertexArray(0);
    }

    // =============================================================
    // Section 5: Invalidation on Mesh Buffer Updates or Replacement
    // =============================================================
    if (runAll || testCase == "invalidation") {
        report << "\n--- Section 5: Invalidation on Buffer Updates & Generation Bump ---\n";
        ux3d::renderer::gl::VertexLayoutCache localCache(gl);

        ux3d::renderer::Mesh mutMesh(gl);
        ux3d::renderer::gl::Attribute attrPos("a_position");
        attrPos.buffer = &posBuffer;
        attrPos.dimension = 3;
        attrPos.type = ux3d::renderer::gl::ComponentType::Float;
        mutMesh.attributes.push_back(attrPos);

        auto lOrig = localCache.getOrCreate(&mutMesh, &progBasic);
        check(lOrig != nullptr, "Original mutable mesh layout created");
        GLuint origHandle = lOrig->handle;

        // Verify buffer binding on original VAO
        glBindVertexArray(origHandle);
        GLint boundBuf = 0;
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &boundBuf);
        check(boundBuf == static_cast<GLint>(posBuffer.handle), "Original VAO points to initial posBuffer");

        // Mutate mesh: replace buffer and bump generation
        std::vector<float> posData2 = {
            -1.0f, -1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,
             0.0f,  1.0f, 0.0f
        };
        auto posBuffer2 = makeGlBuffer(gl, posData2);
        mutMesh.attributes[0].buffer = &posBuffer2;
        mutMesh.bumpGeneration();
        check(mutMesh.getGeneration() == 2, "Mesh generation bumped to 2");

        auto lUpdated = localCache.getOrCreate(&mutMesh, &progBasic);
        check(lUpdated != nullptr, "Query after generation bump returns valid layout");
        check(lUpdated != lOrig, "Cache miss on bumped generation produces fresh VertexLayout");
        check(lUpdated->handle != origHandle, "Updated layout has fresh VAO handle");

        // Verify updated buffer binding on new VAO
        glBindVertexArray(lUpdated->handle);
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &boundBuf);
        check(boundBuf == static_cast<GLint>(posBuffer2.handle), "Updated VAO correctly points to replacement posBuffer2");

        // Verify old generation entry was pruned from cache
        check(localCache.size() == 1, "Cache size is 1 (stale generation entry was pruned)");
        glBindVertexArray(0);
    }

    // =============================================================
    // Section 6: Clean Destruction on Mesh Delete, Reload, and Teardown
    // =============================================================
    if (runAll || testCase == "lifecycle") {
        report << "\n--- Section 6: Clean Destruction & Lifecycle Invalidation ---\n";

        // Part A: Mesh Destructor Hook Invalidation
        {
            ux3d::renderer::gl::VertexLayoutCache localCache(gl);

            GLuint heapVaoHandle = 0;
            {
                auto* heapMesh = new ux3d::renderer::Mesh(gl);
                ux3d::renderer::gl::Attribute attrPos("a_position");
                attrPos.buffer = &posBuffer;
                attrPos.dimension = 3;
                attrPos.type = ux3d::renderer::gl::ComponentType::Float;
                heapMesh->attributes.push_back(attrPos);

                auto layout = localCache.getOrCreate(heapMesh, &progBasic);
                heapVaoHandle = layout->handle;
                check(heapVaoHandle != 0, "Heap mesh layout created");
                check(glIsVertexArray(heapVaoHandle) == GL_TRUE, "Driver recognizes heap mesh VAO");
                check(localCache.size() == 1, "Cache has 1 entry for heap mesh");

                // Destroy mesh
                delete heapMesh;
            }

            // Mesh::~Mesh() notified localCache to invalidate heapMesh->getId()
            check(localCache.size() == 0, "Deleting Mesh purged its entries from VertexLayoutCache");
            check(glIsVertexArray(heapVaoHandle) == GL_FALSE,
                  "Driver VAO deleted immediately upon Mesh destruction (glIsVertexArray == GL_FALSE)");
        }

        // Part B: Direct clear() Driver Deletion
        {
            ux3d::renderer::gl::VertexLayoutCache localCache(gl);

            ux3d::renderer::Mesh meshA(gl);
            ux3d::renderer::gl::Attribute attrPos("a_position");
            attrPos.buffer = &posBuffer;
            attrPos.dimension = 3;
            attrPos.type = ux3d::renderer::gl::ComponentType::Float;
            meshA.attributes.push_back(attrPos);

            ux3d::renderer::Mesh meshB(gl);
            meshB.attributes.push_back(attrPos);

            auto lA = localCache.getOrCreate(&meshA, &progBasic);
            auto lB = localCache.getOrCreate(&meshB, &progBasic);
            GLuint handleA = lA->handle;
            GLuint handleB = lB->handle;

            check(localCache.size() == 2, "Cache has 2 entries");
            check(glIsVertexArray(handleA) == GL_TRUE && glIsVertexArray(handleB) == GL_TRUE,
                  "Both VAOs are active on driver");

            // Release local references so cache holds only references
            lA.reset();
            lB.reset();

            localCache.clear();
            check(localCache.size() == 0, "localCache.clear() resets container size to 0");
            check(glIsVertexArray(handleA) == GL_FALSE, "handleA deleted on driver after clear()");
            check(glIsVertexArray(handleB) == GL_FALSE, "handleB deleted on driver after clear()");
        }

        // Part C: Production Model Reload & Teardown Integration (Inspected After Each Reload Cycle)
        if (reloadAndDraw && glRenderer && commandGenerator) {
            report << "\n--- Section 6C: Production Model Reload & Teardown Integration ---\n";

            const std::string modelPath = "resources/sampleGltf/HelloCube.glb";

            size_t baselineCycleCount = 0;
            bool reloadOk = true;

            for (int cycle = 1; cycle <= 3; ++cycle) {
                if (!reloadAndDraw(modelPath)) {
                    reloadOk = false;
                    break;
                }

                size_t cycleLayouts = commandGenerator->getVertexLayoutCache().size();
                check(cycleLayouts > 0, "Cycle " + std::to_string(cycle) + ": CommandGenerator cached vertex layouts after draw");

                if (cycle == 1) {
                    baselineCycleCount = cycleLayouts;
                } else {
                    check(cycleLayouts == baselineCycleCount,
                          "Cycle " + std::to_string(cycle) + ": Live layout count remains bounded (count=" +
                          std::to_string(cycleLayouts) + " == baseline=" + std::to_string(baselineCycleCount) + ")");
                }

                // Verify all current layouts are recognized as active by OpenGL driver
                auto currentLayouts = commandGenerator->getVertexLayoutCache().getAllLayouts();
                bool currentValid = true;
                for (const auto& l : currentLayouts) {
                    if (!l || glIsVertexArray(l->handle) != GL_TRUE) {
                        currentValid = false;
                    }
                }
                check(currentValid, "Cycle " + std::to_string(cycle) + ": All current layouts are active driver VAOs");
            }
            check(reloadOk, "3 production loadGltfModel + draw cycles completed successfully");

            // Collect active handles before teardown
            std::vector<GLuint> prodHandles;
            {
                auto prodLayouts = commandGenerator->getVertexLayoutCache().getAllLayouts();
                for (const auto& l : prodLayouts) {
                    if (l) prodHandles.push_back(l->handle);
                }
            } // prodLayouts released so only CommandGenerator holds references

            commandGenerator->clearVertexLayoutCache();
            check(commandGenerator->getVertexLayoutCache().size() == 0,
                  "Production CommandGenerator cache cleared successfully");

            bool allDeleted = true;
            for (GLuint h : prodHandles) {
                if (glIsVertexArray(h) != GL_FALSE) {
                    allDeleted = false;
                }
            }
            check(allDeleted, "All production VAO handles return glIsVertexArray == GL_FALSE after teardown");
        }
    }

    // =============================================================
    // Section 7: Production Draw Execution, Pixel Verification & Boundary Isolation
    // =============================================================
    if (runAll || testCase == "production-draw" || testCase == "boundary") {
        report << "\n--- Section 7: Production Draw Execution & FBO Readback ---\n";

        // Create offscreen FBO (64x64)
        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        GLuint colorTex = 0;
        glGenTextures(1, &colorTex);
        glBindTexture(GL_TEXTURE_2D, colorTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

        check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
              "Offscreen test FBO is complete");

        glViewport(0, 0, 64, 64);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Quad geometry covering full viewport
        std::vector<float> quadPos = {
            -1.0f, -1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f
        };
        auto quadPosBuf = makeGlBuffer(gl, quadPos);

        // Quad normal buffer: pure green (0, 1, 0)
        std::vector<float> quadNorm = {
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f
        };
        auto quadNormBuf = makeGlBuffer(gl, quadNorm);

        // Quad color buffer 1: pure magenta (1, 0, 1, 1)
        std::vector<float> quadCol1 = {
            1.0f, 0.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 1.0f
        };
        auto quadColBuf1 = makeGlBuffer(gl, quadCol1);

        // Quad color buffer 2: pure blue (0, 0, 1, 1)
        std::vector<float> quadCol2 = {
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 1.0f, 1.0f
        };
        auto quadColBuf2 = makeGlBuffer(gl, quadCol2);

        std::vector<uint16_t> quadIndices = { 0, 1, 2, 2, 3, 0 };
        auto quadIdxBuf = makeGlBuffer(gl, quadIndices);

        ux3d::renderer::Mesh quadMesh(gl);
        ux3d::renderer::gl::Attribute qAttrPos("a_position");
        qAttrPos.buffer = &quadPosBuf;
        qAttrPos.dimension = 3;
        qAttrPos.type = ux3d::renderer::gl::ComponentType::Float;
        quadMesh.attributes.push_back(qAttrPos);

        ux3d::renderer::gl::Attribute qAttrNorm("a_normal");
        qAttrNorm.buffer = &quadNormBuf;
        qAttrNorm.dimension = 3;
        qAttrNorm.type = ux3d::renderer::gl::ComponentType::Float;
        quadMesh.attributes.push_back(qAttrNorm);

        ux3d::renderer::gl::Attribute qAttrCol("a_color_0");
        qAttrCol.buffer = &quadColBuf1;
        qAttrCol.dimension = 4;
        qAttrCol.type = ux3d::renderer::gl::ComponentType::Float;
        quadMesh.attributes.push_back(qAttrCol);

        quadMesh.indexBuffer = &quadIdxBuf;
        quadMesh.indexCount = 6;
        quadMesh.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        quadMesh.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;

        ux3d::renderer::gl::VertexLayoutCache drawCache(gl);
        auto layoutNormDraw = drawCache.getOrCreate(&quadMesh, &progNorm);
        auto layoutColDraw = drawCache.getOrCreate(&quadMesh, &progCol);
        check(layoutNormDraw != nullptr && layoutColDraw != nullptr, "Created layouts for draw verification");

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        // --- Step A: Production Draw with progNorm (verifies location 1 active normal -> green) ---
        {
            glClear(GL_COLOR_BUFFER_BIT);
            ux3d::renderer::Command cmd(gl, ux3d::renderer::CommandTag::Draw);
            cmd.program = &progNorm;
            cmd.vertexLayout = layoutNormDraw;
            cmd.indexBuffer = &quadIdxBuf;
            cmd.primitiveCount = 6;
            cmd.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
            cmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
            cmd.culling = ux3d::renderer::gl::Culling::None;

            executor.execute(cmd);

            std::array<uint8_t, 4> pixel = { 0, 0, 0, 0 };
            glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            const bool greenMatch = (pixel[0] == 0) && (pixel[1] >= 250) && (pixel[2] == 0) && (pixel[3] == 255);
            check(greenMatch, "progNorm draw correctly rendered normal-derived green output [0, 255, 0, 255]");
        }

        // --- Step B: Production Draw with progCol (verifies location 2 active color -> magenta) ---
        {
            glClear(GL_COLOR_BUFFER_BIT);
            ux3d::renderer::Command cmd(gl, ux3d::renderer::CommandTag::Draw);
            cmd.program = &progCol;
            cmd.vertexLayout = layoutColDraw;
            cmd.indexBuffer = &quadIdxBuf;
            cmd.primitiveCount = 6;
            cmd.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
            cmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
            cmd.culling = ux3d::renderer::gl::Culling::None;

            executor.execute(cmd);

            std::array<uint8_t, 4> pixel = { 0, 0, 0, 0 };
            glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            const bool magentaMatch = (pixel[0] >= 250) && (pixel[1] == 0) && (pixel[2] >= 250) && (pixel[3] == 255);
            check(magentaMatch, "progCol draw correctly rendered color-derived magenta output [255, 0, 255, 255]");
        }

        // --- Step C: Indexed vs Non-Indexed Draw Sequence ---
        {
            // Non-indexed triangle geometry (3 vertices covering upper half)
            std::vector<float> triPos = {
                -1.0f,  0.0f, 0.0f,
                 1.0f,  0.0f, 0.0f,
                 0.0f,  1.0f, 0.0f
            };
            auto triPosBuf = makeGlBuffer(gl, triPos);

            ux3d::renderer::Mesh triNonIndexed(gl);
            ux3d::renderer::gl::Attribute tAttrPos("a_position");
            tAttrPos.buffer = &triPosBuf;
            tAttrPos.dimension = 3;
            tAttrPos.type = ux3d::renderer::gl::ComponentType::Float;
            triNonIndexed.attributes.push_back(tAttrPos);
            triNonIndexed.indexBuffer = nullptr;
            triNonIndexed.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;

            auto layoutNonIndexed = drawCache.getOrCreate(&triNonIndexed, &progBasic);
            check(layoutNonIndexed != nullptr, "Non-indexed triangle layout created");

            // Draw indexed quad
            ux3d::renderer::Command cmdIndexed(gl, ux3d::renderer::CommandTag::Draw);
            cmdIndexed.program = &progBasic;
            cmdIndexed.vertexLayout = layoutNormDraw;
            cmdIndexed.indexBuffer = &quadIdxBuf;
            cmdIndexed.primitiveCount = 6;
            cmdIndexed.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
            cmdIndexed.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
            cmdIndexed.culling = ux3d::renderer::gl::Culling::None;
            executor.execute(cmdIndexed);

            // Immediately draw non-indexed triangle
            ux3d::renderer::Command cmdNonIndexed(gl, ux3d::renderer::CommandTag::Draw);
            cmdNonIndexed.program = &progBasic;
            cmdNonIndexed.vertexLayout = layoutNonIndexed;
            cmdNonIndexed.indexBuffer = nullptr;
            cmdNonIndexed.primitiveCount = 3;
            cmdNonIndexed.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
            cmdNonIndexed.culling = ux3d::renderer::gl::Culling::None;
            executor.execute(cmdNonIndexed);

            GLint activeEbo = 0;
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &activeEbo);
            check(activeEbo == 0, "Non-indexed draw left element array buffer binding 0");

            // Draw indexed quad again
            executor.execute(cmdIndexed);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &activeEbo);
            check(activeEbo == static_cast<GLint>(quadIdxBuf.handle),
                  "Indexed draw re-established element array buffer binding");
        }

        // --- Step D: Warm Layout, Buffer Replacement, Draw Again & Unrelated Mesh Retention ---
        {
            // Unrelated mesh fixture
            ux3d::renderer::Mesh meshUnrelated(gl);
            meshUnrelated.attributes.push_back(qAttrPos);
            auto layoutUnrelatedBefore = drawCache.getOrCreate(&meshUnrelated, &progCol);
            check(layoutUnrelatedBefore != nullptr, "Unrelated mesh layout warmed in cache");

            // Replace color buffer on quadMesh with blue buffer (quadColBuf2)
            quadMesh.attributes[2].buffer = &quadColBuf2;
            quadMesh.bumpGeneration();

            auto layoutColDraw2 = drawCache.getOrCreate(&quadMesh, &progCol);
            check(layoutColDraw2 != layoutColDraw, "Buffer replacement + bumpGeneration created fresh layout");

            // Verify unrelated mesh retained its cached layout
            auto layoutUnrelatedAfter = drawCache.getOrCreate(&meshUnrelated, &progCol);
            check(layoutUnrelatedAfter == layoutUnrelatedBefore,
                  "Unrelated mesh retained its cached layout after other mesh's mutation");

            // Execute draw with updated layout
            glClear(GL_COLOR_BUFFER_BIT);
            ux3d::renderer::Command cmd(gl, ux3d::renderer::CommandTag::Draw);
            cmd.program = &progCol;
            cmd.vertexLayout = layoutColDraw2;
            cmd.indexBuffer = &quadIdxBuf;
            cmd.primitiveCount = 6;
            cmd.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
            cmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
            cmd.culling = ux3d::renderer::gl::Culling::None;

            executor.execute(cmd);

            std::array<uint8_t, 4> pixel = { 0, 0, 0, 0 };
            glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            const bool blueMatch = (pixel[0] == 0) && (pixel[1] == 0) && (pixel[2] >= 250) && (pixel[3] == 255);
            check(blueMatch, "Updated layout successfully rendered replaced blue buffer [0, 0, 255, 255]");
        }

        // --- Step E: External-Function VAO Invalidation Boundary ---
        {
            // Command 1: Draw quad with layoutColDraw2
            ux3d::renderer::Command cmdDraw1(gl, ux3d::renderer::CommandTag::Draw);
            cmdDraw1.program = &progCol;
            cmdDraw1.vertexLayout = drawCache.getOrCreate(&quadMesh, &progCol);
            cmdDraw1.indexBuffer = &quadIdxBuf;
            cmdDraw1.primitiveCount = 6;
            cmdDraw1.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
            cmdDraw1.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
            cmdDraw1.culling = ux3d::renderer::gl::Culling::None;
            executor.execute(cmdDraw1);

            // Command 2: ExternalFunction that binds a dummy foreign VAO
            GLuint dummyVao = 0;
            glGenVertexArrays(1, &dummyVao);

            ux3d::renderer::Command cmdExt(gl, ux3d::renderer::CommandTag::ExternalFunction);
            cmdExt.externalFunction = [dummyVao]() {
                glBindVertexArray(dummyVao);
            };
            executor.execute(cmdExt);

            GLint boundForeign = 0;
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundForeign);
            check(boundForeign == static_cast<GLint>(dummyVao),
                  "ExternalFunction successfully bound foreign VAO");

            // Command 3: Draw quad again with layoutColDraw2
            // Because ExternalFunction reset m_activeVertexArray to gl::invalid,
            // CommandExecutor must rebind layoutColDraw2->handle rather than assuming it remained bound.
            glClear(GL_COLOR_BUFFER_BIT);
            executor.execute(cmdDraw1);

            std::array<uint8_t, 4> pixel = { 0, 0, 0, 0 };
            glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            const bool blueMatch = (pixel[0] == 0) && (pixel[1] == 0) && (pixel[2] >= 250) && (pixel[3] == 255);
            check(blueMatch, "Draw after ExternalFunction rebound VAO cleanly and rendered correct blue output");

            GLenum glErr = glGetError();
            check(glErr == GL_NO_ERROR, "No GL errors during external-function boundary test");

            glBindVertexArray(0);
            glDeleteVertexArrays(1, &dummyVao);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteTextures(1, &colorTex);
        glDeleteFramebuffers(1, &fbo);
    }

    report << "\nTotal assertions evaluated: " << checks << "\n";
    report << "Failed assertions: " << failures.size() << "\n";
    report << "Result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";

    result.report = report.str();
    result.success = (failures.empty());
    return result;
}

} // namespace ux3d::imgui
