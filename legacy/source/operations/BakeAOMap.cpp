#include "operations/BakeAOMap.h"

#include "operations/Library.h"
#include "operations/Schema.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"

#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions_4_1_Core>
#include <QSurfaceFormat>

#include <memory>

#include <fmt/format.h>

#include "thirdparty/lightmapper.h"

using namespace ux3d;

const operations::Operation operations::bake_ao_map = {
    .name = "bake_ao_map",

    .schema = schema::operation(
        "Ambient Occlusion",
        "Bake an ambient occlusion or thickness map",
        {
            { "type", "object" },
            { "additionalProperties", false },
            { "required", { "path" } },
            {
                "properties",
                {
                    schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+"),
                    { "width", schema::integer(512) },
                    { "height", schema::integer(512) },
                    { "thickness", schema::boolean(false) },
                    { "texCoordSet", schema::integer(0) },
                    { "occluderDistance", schema::number(0.2) },
                    { "attachToMaterial", schema::boolean(true) },
                },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::unique_ptr<QOffscreenSurface> surface(new QOffscreenSurface);
        surface->create();

        std::unique_ptr<QOpenGLContext> context(new QOpenGLContext);
        context->setScreen(QGuiApplication::primaryScreen());
        context->create();
        if (!context->makeCurrent(surface.get())) {
            throw std::runtime_error { "Cannot create background OpenGL context" };
        }

        std::unique_ptr<QOpenGLFunctions_4_1_Core> gl { new QOpenGLFunctions_4_1_Core };
        gl->initializeOpenGLFunctions();

        const auto& primitive = state[nlohmann::json::json_pointer { params["path"] }];
        size_t width = params["width"];
        size_t height = params["height"];
        bool invert = params["thickness"];
        size_t texCoordSet = params["texCoordSet"];
        float occluderDistance = params["occluderDistance"];
        bool attachToMaterial = params["attachToMaterial"];

        // Compile shader

        const char* vp = "#version 150 core\n"
                         "in vec3 a_position;\n"
                         "in vec2 a_texcoord;\n"
                         "uniform mat4 u_view;\n"
                         "uniform mat4 u_projection;\n"
                         "out vec2 v_texcoord;\n"

                         "void main()\n"
                         "{\n"
                         "gl_Position = u_projection * (u_view * vec4(a_position, 1.0));\n"
                         "v_texcoord = a_texcoord;\n"
                         "}\n";

        const char* fp = "#version 150 core\n"
                         "in vec2 v_texcoord;\n"
                         "uniform sampler2D u_lightmap;\n"
                         "out vec4 o_color;\n"

                         "void main()\n"
                         "{\n"
                         "o_color = vec4(texture(u_lightmap, v_texcoord).rrr, 1.0);\n"
                         "}\n";

        GLint status;

        GLuint vertexShader = gl->glCreateShader(GL_VERTEX_SHADER);
        gl->glShaderSource(vertexShader, 1, &vp, nullptr);
        gl->glCompileShader(vertexShader);
        gl->glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            throw std::runtime_error { "Could not compile thickness baker vertex shader" };
        }

        GLuint fragmentShader = gl->glCreateShader(GL_FRAGMENT_SHADER);
        gl->glShaderSource(fragmentShader, 1, &fp, nullptr);
        gl->glCompileShader(fragmentShader);
        gl->glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            throw std::runtime_error { "Could not compile thickness baker fragment shader" };
        }

        GLuint program = gl->glCreateProgram();
        gl->glAttachShader(program, vertexShader);
        gl->glAttachShader(program, fragmentShader);
        gl->glLinkProgram(program);
        gl->glDeleteShader(vertexShader);
        gl->glDeleteShader(fragmentShader);
        gl->glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            throw std::runtime_error { "Could not link thickness baker shader program" };
        }

        GLint u_view = gl->glGetUniformLocation(program, "u_view");
        GLint u_projection = gl->glGetUniformLocation(program, "u_projection");
        GLint u_lightmap = gl->glGetUniformLocation(program, "u_lightmap");
        GLint a_position = gl->glGetAttribLocation(program, "a_position");
        GLint a_texcoord = gl->glGetAttribLocation(program, "a_texcoord");

        // Create vertex buffers and optionally an index buffer

        std::string texCoordAttribute = "TEXCOORD_" + std::to_string(texCoordSet);
        if (!primitive["attributes"].contains(texCoordAttribute)) {
            throw std::runtime_error { fmt::format("TexCoord set {} required to bake thickness map", texCoordSet) };
        }

        nlohmann::json::json_pointer indexAccessor;
        if (primitive.contains("indices")) {
            indexAccessor = nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["indices"].get<int>();
        }
        auto positionAccessor = nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["POSITION"].get<int>();
        auto texcoordAccessor = nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"][texCoordAttribute].get<int>();

        double maxExtent = 0.0;
        for (int i = 0; i < state[positionAccessor]["max"].size(); ++i) {
            maxExtent = std::max(state[positionAccessor]["max"][i].get<double>() - state[positionAccessor]["min"][i].get<double>(), maxExtent);
        }

        std::vector<uint8_t> indexBytes;
        auto positions = reactor::utils::getAccessorData<glm::vec3>(positionAccessor, state, dataStorage);
        auto texcoords = reactor::utils::getAccessorData<glm::vec2>(texcoordAccessor, state, dataStorage);

        uint32_t count;
        lightmapper::Type indexType;
        if (!indexAccessor.empty()) {
            indexBytes = reactor::utils::getAccessorByteDataPacked(indexAccessor, state, dataStorage);
            count = state[indexAccessor]["count"];
            switch (state[indexAccessor]["componentType"].get<reactor::gltf::ComponentType>()) {
            case reactor::gltf::ComponentType::UnsignedByte:
                indexType = lightmapper::Type::UnsignedByte;
                break;
            case reactor::gltf::ComponentType::UnsignedShort:
                indexType = lightmapper::Type::UnsignedShort;
                break;
            case reactor::gltf::ComponentType::UnsignedInt:
                indexType = lightmapper::Type::UnsignedInt;
                break;
            default:
                throw std::runtime_error { "Mesh uses unsupported index type. Supported types are: unsigned byte, unsigned short, unsigned int" };
            }
        } else {
            count = state[positionAccessor]["count"];
            indexType = lightmapper::Type::None;
        }

        GLuint vao;
        gl->glGenVertexArrays(1, &vao);
        gl->glBindVertexArray(vao);

        if (!indexAccessor.empty()) {
            GLuint ebo;
            gl->glGenBuffers(1, &ebo);
            gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexBytes.size(), indexBytes.data(), GL_STATIC_DRAW);
        }

        GLuint vboPosition;
        gl->glGenBuffers(1, &vboPosition);
        gl->glBindBuffer(GL_ARRAY_BUFFER, vboPosition);
        gl->glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);
        gl->glEnableVertexAttribArray(a_position);
        gl->glVertexAttribPointer(a_position, 3, GL_FLOAT, GL_FALSE, 0, reinterpret_cast<void*>(0));

        GLenum type = GL_FLOAT;
        auto lmPositionsType = lightmapper::Type::Float;
        switch (state[positionAccessor]["componentType"].get<reactor::gltf::ComponentType>()) {
        case reactor::gltf::ComponentType::Float:
            type = GL_FLOAT;
            lmPositionsType = lightmapper::Type::Float;
            break;
        case reactor::gltf::ComponentType::UnsignedShort:
            type = GL_UNSIGNED_SHORT;
            lmPositionsType = lightmapper::Type::UnsignedShort;
            break;
        case reactor::gltf::ComponentType::UnsignedByte:
            type = GL_UNSIGNED_BYTE;
            lmPositionsType = lightmapper::Type::UnsignedByte;
            break;
        case reactor::gltf::ComponentType::UnsignedInt:
            type = GL_UNSIGNED_INT;
            lmPositionsType = lightmapper::Type::UnsignedInt;
            break;
        default:
            throw std::runtime_error { "Position accessor has unsupported component type" };
        };

        gl->glVertexAttribPointer(a_position, 3, type, GL_FALSE, 0, nullptr);

        GLuint vboTexcoord;
        gl->glGenBuffers(1, &vboTexcoord);
        gl->glBindBuffer(GL_ARRAY_BUFFER, vboTexcoord);
        gl->glBufferData(GL_ARRAY_BUFFER, texcoords.size() * sizeof(glm::vec2), texcoords.data(), GL_STATIC_DRAW);
        gl->glEnableVertexAttribArray(a_texcoord);

        lightmapper::Type lmTexcoordsType;
        switch (state[texcoordAccessor]["componentType"].get<reactor::gltf::ComponentType>()) {
        case reactor::gltf::ComponentType::Float:
            type = GL_FLOAT;
            lmTexcoordsType = lightmapper::Type::Float;
            break;
        case reactor::gltf::ComponentType::UnsignedShort:
            type = GL_UNSIGNED_SHORT;
            lmTexcoordsType = lightmapper::Type::UnsignedShort;
            break;
        case reactor::gltf::ComponentType::UnsignedByte:
            type = GL_UNSIGNED_BYTE;
            lmTexcoordsType = lightmapper::Type::UnsignedByte;
            break;
        case reactor::gltf::ComponentType::UnsignedInt:
            type = GL_UNSIGNED_INT;
            lmTexcoordsType = lightmapper::Type::UnsignedInt;
            break;
        default:
            throw std::runtime_error { "TexCoord accessor has unsupported component type" };
        };

        gl->glVertexAttribPointer(a_texcoord, 2, type, GL_FALSE, 0, nullptr);

        // Create lightmap texture

        std::unique_ptr<float[]> lightmap { new float[width * height]() };
        GLuint texture;
        gl->glGenTextures(1, &texture);
        gl->glBindTexture(GL_TEXTURE_2D, texture);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        double zFar = maxExtent * occluderDistance;
        float zNear = 0.0001f;
        lightmapper::Context* ctx = create(gl.get(), invert ? lightmapper::Mode::Thickness : lightmapper::Mode::AmbientOcclusion, 256, zNear, zFar, 1, 1, 1, 2, 0.005f);
        if (ctx == nullptr) {
            throw std::runtime_error { "Could not create lightmapper context" };
        }

        gl->glBindTexture(GL_TEXTURE_2D, texture);
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, lightmap.get());
        lightmapper::setTargetLightmap(ctx, lightmap.get(), width, height, 1);

        lightmapper::setGeometry(ctx, nullptr,
            lmPositionsType, positions.data(), 0,
            lightmapper::Type::None, nullptr, 0,
            lmTexcoordsType, texcoords.data(), 0,
            count, indexType, indexBytes.data());

        int viewport[4];
        float view[16];
        float projection[16];

        while (lightmapper::begin(ctx, viewport, view, projection)) {
            gl->glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            gl->glEnable(GL_DEPTH_TEST);
            gl->glUseProgram(program);
            gl->glUniform1i(u_lightmap, 0);
            gl->glUniformMatrix4fv(u_projection, 1, GL_FALSE, projection);
            gl->glUniformMatrix4fv(u_view, 1, GL_FALSE, view);
            gl->glActiveTexture(GL_TEXTURE0);
            gl->glBindTexture(GL_TEXTURE_2D, texture);
            gl->glBindVertexArray(vao);
            if (!indexAccessor.empty()) {
                gl->glDrawElements(static_cast<GLenum>(primitive.value("mode", 4)), count, static_cast<GLenum>(state[indexAccessor]["componentType"]), nullptr);
            } else {
                gl->glDrawArrays(static_cast<GLenum>(primitive.value("mode", 4)), 0, count);
            }

            // Progress in [0, 1] can be retrieved with `progress(ctx)`

            end(ctx);
        }

        // Postprocess lightmap to remove artifacts
        std::unique_ptr<float[]> lightmapCounterpart { new float[width * height] };
        for (int i = 0; i < 6; i++) {
            lightmapper::imageDilate(lightmap.get(), lightmapCounterpart.get(), width, height, 1);
            lightmapper::imageDilate(lightmapCounterpart.get(), lightmap.get(), width, height, 1);
        }

        lightmapper::destroy(ctx);

        std::vector<uint8_t> lightmapPixelsNorm(width * height);
        lightmapper::imageFtoUB(lightmap.get(), lightmapPixelsNorm.data(), width, height, 1);

        ux3d::reactor::image::ImageData imageData {
            .data = std::move(lightmapPixelsNorm),
            .info = {
                { "width", width },
                { "height", height },
                { "channels", 1 },
                { "pixelByteSize", 1 },
            },
        };

        auto name = std::string { invert ? "Thickness" : "AO" } + "_Map";

        auto image = nlohmann::json::object();
        image["uri"] = reactor::utils::createURIStem(state) + ".png";
        image["name"] = name;
        state["gltf"]["images"].emplace_back(image);

        auto imagePath = reactor::utils::getDataIDPath(image, state);
        state[imagePath] = dataStorage.insert(reactor::image::encode(imageData, { { "mimeType", "image/png" } }));

        state["gltf"]["textures"].emplace_back(nlohmann::json {
            { "source", state["gltf"]["images"].size() - 1 },
            { "name", name },
        });

        if (attachToMaterial && primitive.contains("material")) {
            nlohmann::json::json_pointer texturePointer = nlohmann::json::json_pointer {} / "gltf" / "materials" / primitive["material"].get<int>();
            if (invert) {
                texturePointer = texturePointer / "extensions" / "KHR_materials_volume" / "thicknessTexture";
            } else {
                texturePointer = texturePointer / "occlusionTexture";
            }
            nlohmann::json taskDescription;
            taskDescription["set_gltf_pointer"]["path"] = (texturePointer / "index").to_string();
            taskDescription["set_gltf_pointer"]["pointer"] = (nlohmann::json::json_pointer("/gltf/textures") / (state["gltf"]["textures"].size() - 1)).to_string();
            state = library.call(taskDescription, std::move(state), dataStorage, callback);

            taskDescription = nlohmann::json();
            taskDescription["set_gltf_value"]["path"] = (texturePointer / "texCoord").to_string();
            taskDescription["set_gltf_value"]["value"] = texCoordSet;
            state = library.call(taskDescription, std::move(state), dataStorage, callback);
        }

        return state;
    }
};
