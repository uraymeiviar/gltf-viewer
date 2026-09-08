#include "PropertyOperations.h"
#include "StorageOperations.h"
#include "operations/Library.h"
#include "operations/Schema.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"

#include <glm.hpp>
#include <gtc/quaternion.hpp>

#include <vector>

using namespace ux3d::operations;

const Operation gltf::createAnimation = {
    .name = "create_animation",

    .schema = schema::operation(
        "Add New Animation",
        "Adds a newly generated animation.",
        {
            { "type", "object" },
            {
                "properties",
                {
                    schema::parameter::path("/gltf/nodes/\\d+"),
                    { "start", schema::number() },
                    { "duration", schema::number() },
                    { "bounce", schema::boolean() },
                    {
                        "translation",
                        {
                            { "type", "array" },
                            { "items", schema::number() },
                            { "minContains", 3 },
                            { "maxContains", 3 },
                        },
                    },
                    {
                        "rotationAxis",
                        {
                            { "type", "array" },
                            { "items", schema::number() },
                            { "minContains", 3 },
                            { "maxContains", 3 },
                        },
                    },
                    { "degrees", { { "type", "number" } } },
                },
            },
            {
                "required",
                { "path", "duration" },
            },
            {
                "oneOf",
                {
                    { { "required", { "translation" } } },
                    { { "required", { "rotationAxis", "degrees" } } },
                },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"]);
        float start = params.value("start", 0.0f);
        float duration = params["duration"];
        bool bounce = params.value("bounce", false);

        std::vector<float> input;
        std::vector<float> output;
        const char* targetPath = nullptr;
        reactor::gltf::Type type;

        if (params.contains("translation")) {
            targetPath = "translation";
            type = reactor::gltf::Type::VEC3;

            glm::vec3 nodeTranslation { 0 };
            if (state[path].contains("translation")) {
                nodeTranslation = {
                    state[path]["translation"][0],
                    state[path]["translation"][1],
                    state[path]["translation"][2],
                };
            }

            glm::vec3 translation {
                params["translation"][0],
                params["translation"][1],
                params["translation"][2],
            };

            size_t count = 2;
            if (bounce) {
                count += 1;
            }

            // Keyframe times
            float divisor = static_cast<float>(count - 1);
            input.resize(count);
            for (size_t i = 0; i < count; i++) {
                input[i] = start + (duration * static_cast<float>(i) / divisor);
            }

            // Keyframe data
            output.resize(count * 3);
            output[0] = nodeTranslation.x;
            output[1] = nodeTranslation.y;
            output[2] = nodeTranslation.z;

            output[3] = nodeTranslation.x + translation.x;
            output[4] = nodeTranslation.y + translation.y;
            output[5] = nodeTranslation.z + translation.z;

            if (bounce) {
                output[6] = nodeTranslation.x;
                output[7] = nodeTranslation.y;
                output[8] = nodeTranslation.z;
            }
        } else if (params.contains("rotationAxis") && params.contains("degrees")) {
            targetPath = "rotation";
            type = reactor::gltf::Type::VEC4;

            glm::quat nodeRotation = glm::identity<glm::quat>();
            if (state[path].contains("matrix")) {
                auto taskDescription = nlohmann::json({ { "change_transformation_format", nlohmann::json::object() } });
                taskDescription["change_transformation_format"]["path"] = path;
                taskDescription["change_transformation_format"]["storageType"] = "TRS";
                state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
                callback({ { "message", "Transformed matrix to TRS for animation target" } });
            }
            if (state[path].contains("rotation")) {
                // Quad init with w,x,y,z
                nodeRotation = {
                    state[path]["rotation"][3],
                    state[path]["rotation"][0],
                    state[path]["rotation"][1],
                    state[path]["rotation"][2]
                };
            }

            glm::vec3 rotationAxis = glm::normalize(glm::vec3 {
                params["rotationAxis"][0],
                params["rotationAxis"][1],
                params["rotationAxis"][2],
            });

            float degrees = params["degrees"];

            size_t count = static_cast<size_t>(glm::floor(glm::abs(degrees) / 90.0f)) + 1;
            count = glm::max<size_t>(count, 2);
            if (bounce) {
                count += count - 1;
            }

            float timeDivisor = static_cast<float>(count - 1);

            // Keyframe times
            input.resize(count);
            for (size_t i = 0; i < count; ++i) {
                input[i] = start + (duration * static_cast<float>(i) / timeDivisor);
            }

            // Keyframe data
            float dataDivisor = timeDivisor;
            if (bounce) {
                dataDivisor = glm::ceil(dataDivisor / 2.0f);
            }

            output.resize(count * 4);
            for (size_t i = 0; i < count; ++i) {
                float dividend = static_cast<float>(i);

                if (bounce && dividend * 2.0f > static_cast<float>(count)) {
                    dividend = static_cast<float>(count) - (dividend + 1);
                }

                float fraction = dividend / dataDivisor;

                glm::quat q = glm::angleAxis(glm::radians(degrees * fraction), rotationAxis);
                q *= nodeRotation;

                output[4 * i + 0] = q.x;
                output[4 * i + 1] = q.y;
                output[4 * i + 2] = q.z;
                output[4 * i + 3] = q.w;
            }
        }

        {
            std::vector<uint8_t> bytes;
            bytes.resize(input.size() * sizeof(float));
            memcpy(bytes.data(), input.data(), bytes.size());

            auto bufferView = reactor::utils::createBufferView(state, reactor::utils::createBuffer(state, dataStorage, bytes));
            state["gltf"]["accessors"].emplace_back(nlohmann::json({
                { "type", reactor::gltf::Type::SCALAR },
                { "componentType", reactor::gltf::ComponentType::Float },
                { "count", input.size() },
                { "bufferView", std::stoi(bufferView.back()) },
                { "min", { start } },
                { "max", { start + duration } },
            }));
        }
        size_t inputAccessorIndex = state["gltf"]["accessors"].size() - 1;

        {
            std::vector<uint8_t> bytes;
            bytes.resize(output.size() * sizeof(float));
            memcpy(bytes.data(), output.data(), bytes.size());

            auto bufferView = reactor::utils::createBufferView(state, reactor::utils::createBuffer(state, dataStorage, bytes));
            state["gltf"]["accessors"].emplace_back(nlohmann::json({
                { "type", type },
                { "componentType", reactor::gltf::ComponentType::Float },
                { "count", input.size() },
                { "bufferView", std::stoi(bufferView.back()) },
            }));
        }
        size_t outputAccessorIndex = state["gltf"]["accessors"].size() - 1;

        nlohmann::json sampler {
            { "interpolation", "LINEAR" },
            { "input", inputAccessorIndex },
            { "output", outputAccessorIndex }
        };

        nlohmann::json channel {
            {
                "target",
                {
                    { "path", targetPath },
                    { "node", std::stoi(path.back()) },
                },
            },
            { "sampler", 0 }
        };

        nlohmann::json animation {
            { "channels", { channel } },
            { "samplers", { sampler } },
        };

        if (!state["gltf"].contains("animations")) {
            state["gltf"]["animations"] = nlohmann::json::array();
        }
        state["gltf"]["animations"].push_back(animation);
        return state;
    }
};
