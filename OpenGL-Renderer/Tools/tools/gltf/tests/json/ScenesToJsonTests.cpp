#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/ScenesToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("ScenesToJson", "[tools.gltf.ScenesToJson]")
{
    Gltf gltf;

    Scene* pScene = gltf.create<Scene>();
    REQUIRE(pScene != nullptr);

    Node* pNode1 = gltf.create<Node>();
    Node* pNode2 = gltf.create<Node>();
    Node* pNode3 = gltf.create<Node>();
    REQUIRE(pNode1 != nullptr);
    REQUIRE(pNode2 != nullptr);
    REQUIRE(pNode3 != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    ScenesToJson converter(indices, options, gltf);

    GIVEN("empty scene")
    {
        WHEN("converted")
        {
            nlohmann::json jsonScene;
            converter.convert(*pScene, jsonScene, {});

            THEN("json scene has nothing")
            {
                CHECK(jsonScene.empty());
            }
        }
    }

    GIVEN("scene with name")
    {
        core::string name = "good scene name";
        pScene->setName(name);

        WHEN("converted")
        {
            nlohmann::json jsonScene;
            converter.convert(*pScene, jsonScene, {});

            THEN("json scene has name")
            {
                REQUIRE(jsonScene.contains("name"));

                AND_THEN("name is correct")
                {
                    CHECK(jsonScene["name"].get<core::string>() == name);
                }
            }
        }
    }

    GIVEN("scene with nodes")
    {
        REQUIRE(pScene->addNode(*pNode1, gltf));
        REQUIRE(pScene->addNode(*pNode2, gltf));
        REQUIRE(pScene->addNode(*pNode3, gltf));

        WHEN("converted")
        {
            nlohmann::json jsonScene;
            converter.convert(*pScene, jsonScene, {});

            THEN("json scene has nodes")
            {
                REQUIRE(jsonScene.contains("nodes"));
                nlohmann::json jsonNodes = jsonScene["nodes"];

                AND_THEN("node count is correct")
                {
                    REQUIRE(jsonNodes.size() == 3);

                    AND_THEN("node indices are correct")
                    {
                        size_t index1 = indices[pNode1];
                        size_t index2 = indices[pNode2];
                        size_t index3 = indices[pNode3];

                        CHECK(jsonNodes[0].get<uint32_t>() == index1);
                        CHECK(jsonNodes[1].get<uint32_t>() == index2);
                        CHECK(jsonNodes[2].get<uint32_t>() == index3);
                    }
                }
            }
        }
    }
}
}
