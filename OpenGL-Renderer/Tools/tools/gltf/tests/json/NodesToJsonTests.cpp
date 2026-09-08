#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/NodesToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("NodesToJson", "[tools.gltf.NodesToJson]")
{
    Gltf gltf;

    Node* pNode = gltf.create<Node>();
    REQUIRE(pNode != nullptr);

    Node* pNode1 = gltf.create<Node>();
    Node* pNode2 = gltf.create<Node>();
    Node* pNode3 = gltf.create<Node>();
    REQUIRE(pNode1 != nullptr);
    REQUIRE(pNode2 != nullptr);
    REQUIRE(pNode3 != nullptr);

    Camera* pCamera0 = gltf.create<Camera>(Camera::ProjectionType::Perspective);
    REQUIRE(pCamera0 != nullptr);

    Skin* pSkin = gltf.create<Skin>();
    REQUIRE(pSkin != nullptr);

    math::dmat4x4 transformMatrix = math::identity<math::dmat4x4>();
    transformMatrix[3][0] = 1;

    Mesh* pMesh = gltf.create<Mesh>();

    math::dquat rotation;
    rotation[0] = 0.0;
    rotation[1] = 0.0;
    rotation[2] = 1.0;
    rotation[3] = 0.0;

    math::dvec3 scale(0.0);
    scale[1] = 0.5;

    math::dvec3 translation(0.0);
    scale[1] = 0.3;

    Light* pLight = gltf.create<Light>(Light::Type::Point);

    IndexLookup indices(gltf);
    ExportOptions options;
    NodesToJson converter(indices, options, gltf);

    GIVEN("empty node")
    {
        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has nothing")
            {
                CHECK(jsonNode.empty());
            }
        }
    }

    GIVEN("node with name")
    {
        core::string name = "good node name";
        pNode->setName(name);

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has name")
            {
                REQUIRE(jsonNode.contains("name"));

                AND_THEN("name is correct")
                {
                    CHECK(jsonNode["name"].get<core::string>() == name);
                }
            }
        }
    }

    GIVEN("node with camera")
    {
        REQUIRE(pNode->setCamera(*pCamera0));

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has camera")
            {
                REQUIRE(jsonNode.contains("camera"));

                AND_THEN("camera index is correct")
                {
                    size_t index = indices[pCamera0];
                    CHECK(jsonNode["camera"].get<uint32_t>() == index);
                }
            }
        }
    }

    GIVEN("node with children")
    {
        REQUIRE(pNode->addChild(*pNode1, gltf));
        REQUIRE(pNode->addChild(*pNode2, gltf));
        REQUIRE(pNode->addChild(*pNode3, gltf));

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has children")
            {
                REQUIRE(jsonNode.contains("children"));
                nlohmann::json jsonChildren = jsonNode["children"];

                AND_THEN("children count is correct")
                {
                    REQUIRE(jsonChildren.size() == 3);

                    AND_THEN("children indices are correct")
                    {
                        size_t index1 = indices[pNode1];
                        size_t index2 = indices[pNode2];
                        size_t index3 = indices[pNode3];

                        CHECK(jsonChildren[0].get<uint32_t>() == index1);
                        CHECK(jsonChildren[1].get<uint32_t>() == index2);
                        CHECK(jsonChildren[2].get<uint32_t>() == index3);
                    }
                }
            }
        }
    }

    GIVEN("node with skin")
    {
        REQUIRE(pNode->setSkin(*pSkin));

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has skin")
            {
                REQUIRE(jsonNode.contains("skin"));

                AND_THEN("skin index is correct")
                {
                    size_t index = indices[pSkin];
                    CHECK(jsonNode["skin"].get<uint32_t>() == index);
                }
            }
        }
    }

    GIVEN("node with mesh")
    {
        REQUIRE(pNode->setMesh(*pMesh));

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has mesh")
            {
                REQUIRE(jsonNode.contains("mesh"));

                AND_THEN("mesh index is correct")
                {
                    size_t index = indices[pMesh];
                    CHECK(jsonNode["mesh"].get<uint32_t>() == index);
                }
            }
        }
    }

    GIVEN("node with rotation")
    {
        REQUIRE(pNode->setRotation(rotation));

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has rotation")
            {
                REQUIRE(jsonNode.contains("rotation"));
                nlohmann::json jsonArray = jsonNode["rotation"];

                AND_THEN("rotation values are correct")
                {
                    REQUIRE(jsonArray.size() == 4);

                    for (size_t i = 0; i < jsonArray.size(); i++) {
                        CHECK(jsonArray[i].get<double>() == rotation[math::quat::length_type(i)]);
                    }
                }
            }
        }
    }

    GIVEN("node with scale")
    {
        REQUIRE(pNode->setScale(scale));

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has scale")
            {
                REQUIRE(jsonNode.contains("scale"));
                nlohmann::json jsonArray = jsonNode["scale"];

                AND_THEN("scale values are correct")
                {
                    REQUIRE(jsonArray.size() == 3);

                    for (size_t i = 0; i < jsonArray.size(); i++) {
                        CHECK(jsonArray[i].get<double>() == scale[math::dvec3::length_type(i)]);
                    }
                }
            }
        }
    }

    GIVEN("node with translation")
    {
        REQUIRE(pNode->setTranslation(translation));

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has translation")
            {
                REQUIRE(jsonNode.contains("translation"));
                nlohmann::json jsonArray = jsonNode["translation"];

                AND_THEN("translation values are correct")
                {
                    REQUIRE(jsonArray.size() == 3);

                    for (size_t i = 0; i < jsonArray.size(); i++) {
                        CHECK(jsonArray[i].get<double>() == translation[math::dvec3::length_type(i)]);
                    }
                }
            }
        }
    }

    GIVEN("node with weights")
    {
        core::Vector<double>& weights = pNode->getWeights();
        REQUIRE(weights.resize(5));
        for (size_t i = 0; i < weights.size(); i++) {
            weights[i] = double(i);
        }

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has weights")
            {
                REQUIRE(jsonNode.contains("weights"));
                nlohmann::json jsonArray = jsonNode["weights"];

                AND_THEN("translation values are correct")
                {
                    REQUIRE(jsonArray.size() == weights.size());

                    for (size_t i = 0; i < jsonArray.size(); i++) {

                        CHECK(jsonArray[i].get<double>() == weights[i]);
                    }
                }
            }
        }
    }

    GIVEN("node with light")
    {
        REQUIRE(pNode->setLight(*pLight));

        WHEN("converted")
        {
            nlohmann::json jsonNode;
            converter.convert(*pNode, jsonNode, {});

            THEN("json node has light extension")
            {
                REQUIRE(jsonNode.contains("extensions"));
                REQUIRE(jsonNode["extensions"].contains("KHR_lights_punctual"));
                nlohmann::json extension = jsonNode["extensions"]["KHR_lights_punctual"];

                AND_THEN("light index is correct")
                {
                    REQUIRE(extension.contains("light"));
                    size_t index = indices[pLight];
                    CHECK(extension["light"].get<uint32_t>() == index);
                }
            }
        }
    }
}
}
