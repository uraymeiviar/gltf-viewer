
#include "gtest/gtest.h"

#include "Gltfs.h"
#include "Helpers.h"

#include "operations/Library.h"

#include <iostream>

using namespace ux3d;
using namespace ux3d::operations;

class GeneratedOperationsTests : public OperationsTests {
};

TEST_F(GeneratedOperationsTests, setGenerated)
{
    {
        auto taskDescription = R"(
        {
            "set_gltf_value": {
                "path": "/gltf/nodes/0/name",
                "value": "new name"
            }
        }
        )"_json;

        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);
        EXPECT_EQ(state["gltf"]["nodes"][0]["name"], "new name");
    }

    // Component type is read only for now
    /*{
        // try to set an valid value of an enum
        auto taskDescription = R"(
        {
            "set_gltf_value": {
                "path": "/gltf/accessors/0/componentType",
                "value": 5122
            }
        }
        )"_json;
        EXPECT_NO_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage));
    }*/

    {
        // try to set an valid value of an enum
        auto taskDescription = R"(
        {
            "set_gltf_value": {
                "path": "/gltf/materials/0/alphaMode",
                "value": "MASK"
            }
        }
        )"_json;
        EXPECT_NO_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage));
    }

    {
        auto taskDescription = R"(
        {
            "create_gltf_object": {
                "path": "/gltf/materials/0/extensions/KHR_materials_unlit"
            }
        }
        )"_json;
        nlohmann::json state;
        EXPECT_NO_THROW({
            state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);
            EXPECT_TRUE(state.contains("/gltf/materials/0/extensions/KHR_materials_unlit"_json_pointer));
            EXPECT_TRUE(state.at("/gltf/materials/0/extensions/KHR_materials_unlit"_json_pointer).is_object());
        });
    }

    {
        auto taskDescription = R"(
        {
            "set_gltf_value": {
                "path": "/gltf/materials/0/extensions/KHR_materials_emissive_strength/emissiveStrength",
                "value": 5.0
            }
        }
        )"_json;
        nlohmann::json state;
        EXPECT_NO_THROW({
            state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);
            EXPECT_TRUE(state.contains("/gltf/materials/0/extensions/KHR_materials_emissive_strength/emissiveStrength"_json_pointer));
            EXPECT_TRUE(state.at("/gltf/materials/0/extensions/KHR_materials_emissive_strength/emissiveStrength"_json_pointer) == 5.0);
        });
    }
}

TEST_F(GeneratedOperationsTests, invalidSetGenerated)
{

    // try to set the string value to a number
    auto taskDescription = R"(
        {
            "set_gltf_value": {
                "path": "/gltf/nodes/0/name",
                "value": 5.2
            }
        }
    )"_json;
    EXPECT_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage), std::invalid_argument);

    // try to set an invalid value of an enum
    taskDescription = R"(
        {
            "set_gltf_value": {
                "path": "/gltf/accessors/0/componentType",
                "value": 1000003
            }
        }
    )"_json;
    EXPECT_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage), std::invalid_argument);
}

TEST_F(GeneratedOperationsTests, setPointer)
{
    auto taskDescription = R"(
        {
            "set_gltf_pointer": {
                "path": "/gltf/nodes/0/mesh",
                "pointer": "/gltf/meshes/0"
            }
        }
    )"_json;

    auto box = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

    EXPECT_EQ(box["gltf"]["nodes"][0]["mesh"], 0);
}

TEST_F(GeneratedOperationsTests, invalidSetPointerArguments)
{
    // Setting to the wrong pointer path should throw
    EXPECT_THROW(g_library.call(R"(
        {
            "set_gltf_pointer": {
                "path": "/gltf/nodes/0/mesh",
                "pointer": "/gltf/accessors/0"
            }
        }
    )"_json,
                     nlohmann::json(m_box), m_dataStorage),
        std::invalid_argument);

    // Setting pointer to non pointer property should not be allowed
    EXPECT_THROW(g_library.call(R"(
        {
            "set_gltf_pointer": {
                "path": "/gltf/nodes/0/name",
                "pointer": "/gltf/meshes/0"
            }
        }
    )"_json,
                     nlohmann::json(m_box), m_dataStorage),
        std::invalid_argument);

    // Should not be possible to set values in node children array with generated function
    // as this could lead to loops in the node hierarchy
    EXPECT_THROW(g_library.call(R"(
        {
            "set_gltf_pointer": {
                "path": "/gltf/nodes/0/children/0",
                "pointer": "/gltf/nodes/0"
            }
        }
    )"_json,
                     nlohmann::json(m_box), m_dataStorage),
        std::invalid_argument);
}

TEST_F(GeneratedOperationsTests, invalidSetPointerOperation)
{
    // Only allow setting valid pointers. This can only be catched at runtime of the operation
    EXPECT_THROW(g_library.call(R"(
        {
            "set_gltf_pointer": {
               "path": "/gltf/nodes/0/mesh",
               "pointer": "/gltf/meshes/15"
            }
        }
    )"_json,
                     nlohmann::json(m_box), m_dataStorage),
        std::invalid_argument);
}

TEST_F(GeneratedOperationsTests, removeGltfPointerFromArray)
{
    {
        auto taskDescription = R"(
            {
                "remove_gltf_pointer_from_array": {
                    "path": "/gltf/scenes/0/nodes/0"
                }
            }
        )"_json;

        auto box = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        EXPECT_EQ(box["gltf"]["scenes"][0]["nodes"].size(), 0);
    }

    {
        // Non-existend pointers should not be removable
        auto taskDescription = R"(
            {
                "remove_gltf_pointer_from_array": {
                    "path": "/gltf/scenes/0/nodes/1"
                }
            }
        )"_json;

        EXPECT_THROW(g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage), std::invalid_argument);
    }
}

TEST_F(GeneratedOperationsTests, insertGltfPointer)
{
    {
        // first remove the reference to the parent node, so that reparenting is allowed
        auto taskDescriptionRemove = R"(
            {
                "remove_gltf_pointer_from_array": {
                    "path": "/gltf/nodes/0/children/0"
                }
            }
        )"_json;

        auto preparedbox = g_library.call(std::move(taskDescriptionRemove), nlohmann::json(m_box), m_dataStorage);

        // actual test
        auto taskDescriptionInsert = R"(
            {
                "insert_gltf_pointer": {
                    "path": "/gltf/scenes/0/nodes",
                    "pointer": "/gltf/nodes/1"
                }
            }
        )"_json;

        auto box = g_library.call(std::move(taskDescriptionInsert), nlohmann::json(preparedbox), m_dataStorage);

        EXPECT_EQ(box["gltf"]["scenes"][0]["nodes"][1], 1);
    }

    {
        // Forming loops with adding nodes to children should not be allowed
        auto taskDescription = R"(
            {
                "insert_gltf_pointer": {
                    "path": "/gltf/nodes/1/children",
                    "pointer": "/gltf/nodes/0"
                }
            }
        )"_json;

        EXPECT_THROW(g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage), std::invalid_argument);
    }
}

TEST_F(GeneratedOperationsTests, remove)
{
    {
        EXPECT_EQ(nlohmann::json(m_box)["gltf"]["nodes"][0]["children"].size(), 1);
        auto taskDescription = R"(
            {
                "remove": {
                    "path": "/gltf/nodes/1"
                }
            }
        )"_json;
        auto box = g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage);
        EXPECT_FALSE(box["gltf"]["nodes"][0].contains("children"));
    }

    {
        auto taskDescription = R"(
        {
            "remove": {
                "path": "/gltf/accessors/1"
            }
        }
        )"_json;
        auto box = g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage);
        // Ensure the subsequent indices are decremented after erasing
        EXPECT_EQ(box["gltf"]["meshes"][0]["primitives"][0]["attributes"]["POSITION"], 1);
    }

    {
        auto taskDescription = R"(
        {
            "remove": {
                "path": "/gltf/meshes/0"
            }
        }
        )"_json;
        auto box = g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage);
        // Ensure the subsequent indices are decremented after erasing
        EXPECT_FALSE(box["gltf"]["nodes"][1].contains("mesh"));
    }

    {
        ScenarioResource resource = CesiumMilkTruck;

        auto taskDescription = R"(
        {
            "remove": {
                "path": "/gltf/animations/0"
            }
        }
        )"_json;
        auto stateAfterRemove = g_library.call(taskDescription, nlohmann::json(resource.state), resource.dataStorage);

        EXPECT_EQ(stateAfterRemove["gltf"]["animations"].size(), 0);
    }
}

TEST_F(GeneratedOperationsTests, moveGltfPointer)
{
    {
        auto taskDescription = R"(
            {
                "move_gltf_pointer": {
                    "targetPath": "/gltf/nodes/0/mesh",
                    "pointer": "/gltf/meshes/0",
                    "sourcePath": "/gltf/nodes/1/mesh"
                }
            }
        )"_json;

        auto box = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        EXPECT_EQ(box["gltf"]["nodes"][0]["mesh"], 0);
    }

    {
        auto taskDescription = R"(
            {
                "move_gltf_pointer": {
                    "targetPath": "/gltf/nodes/1/camera",
                    "pointer": "/gltf/cameras/1",
                    "sourcePath": "/gltf/nodes/0/camera"
                }
            }
        )"_json;

        auto box = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        EXPECT_FALSE(box["gltf"]["nodes"][0].contains("camera"));
        EXPECT_EQ(box["gltf"]["nodes"][1]["camera"], 1);
    }

    {
        auto taskDescription = R"(
            {
                "move_gltf_pointer": {
                    "targetPath": "/gltf/nodes/1/mesh",
                    "pointer": "/gltf/meshes/0",
                    "sourcePath": "/gltf/nodes/0/mesh"
                }
            }
        )"_json;

        // Moving a mesh from a node that does not have one, should not be possible
        EXPECT_THROW(g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage), std::invalid_argument);
    }
}

TEST_F(GeneratedOperationsTests, reset)
{
    {
        auto taskDescription = R"(
            {
                "reset": {
                    "path": "/gltf/nodes"
                }
            }
        )"_json;
        EXPECT_ANY_THROW(g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage));
    }

    {
        auto taskDescription = R"(
        {
            "reset": {
                "path": "/gltf/meshes/0/primitives"
            }
        }
        )"_json;

        EXPECT_ANY_THROW({ g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage); });
    }

    {
        auto taskDescription = R"(
        {
            "reset": {
                "path": "/gltf/cameras/0/perspective"
            }
        }
        )"_json;

        EXPECT_ANY_THROW({ g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage); });
    }

    {
        auto taskDescription = R"(
        {
            "reset": {
                "path": "/gltf/cameras/0/perspective/xmag"
            }
        }
        )"_json;

        EXPECT_ANY_THROW({ g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage); });
    }

    {
        auto taskDescription = R"(
        {
            "reset": {
                "path": "/gltf/materials/0/pbrMetallicRoughness"
            }
        }
        )"_json;

        auto box = g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage);
        EXPECT_FALSE(box.contains("/gltf/materials/0/pbrMetallicRoughness"));
    }

    {
        auto taskDescription = R"(
        {
            "reset": {
                "path": "/gltf/materials/0/name"
            }
        }
        )"_json;

        auto box = g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage);
        EXPECT_FALSE(box.contains("/gltf/materials/0/name"));
    }

    {
        auto taskDescription = R"(
        {
            "reset": {
                "path": "/gltf/nodes/0/matrix"
            }
        }
        )"_json;

        auto box = g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage);
        EXPECT_FALSE(box.contains("/gltf/materials/0/matrix"));
    }
}

TEST_F(GeneratedOperationsTests, setGLTFPointerForSkins)
{
    {
        nlohmann::json state = R"(
        {
            "gltf": {
                "asset": { "version": "2.0" },
                "nodes": [ { "name": "Node 0" }, { "name": "Node 1" } ],
                "skins": [ { "joints": [ 0 ] } ]
            }
        }
        )"_json;
        datastorage::DataStorage dataStorage;

        auto taskDescription = R"(
        {
            "insert_gltf_pointer": {
                "path": "/gltf/skins/0/joints/0",
                "pointer": "/gltf/nodes/2"
            }
        }
        )"_json;

        EXPECT_THROW(state = g_library.call(std::move(taskDescription), std::move(state), dataStorage), std::exception);
    }
    {
        nlohmann::json state = R"(
        {
            "gltf": {
                "asset": { "version": "2.0" },
                "nodes": [ { "name": "Node 0" }, { "name": "Node 1" } ],
                "skins": [ { "joints": [ 0 ] } ]
            }
        }
        )"_json;
        datastorage::DataStorage dataStorage;

        auto taskDescription = R"(
        {
            "insert_gltf_pointer": {
                "path": "/gltf/skins/0/joints",
                "pointer": "/gltf/nodes/1"
            }
        }
        )"_json;

        EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), dataStorage));
        EXPECT_EQ(state["gltf"]["skins"][0]["joints"].size(), 2);
    }
}