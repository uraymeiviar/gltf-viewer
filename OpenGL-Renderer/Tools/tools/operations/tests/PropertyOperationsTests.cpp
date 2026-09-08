
#include "gtest/gtest.h"

#include "Gltfs.h"
#include "Helpers.h"

#include "operations/Library.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"

#include <glm.hpp>

using namespace ux3d;
using namespace ux3d::operations;

class PropertyOperationsTests : public OperationsTests {
};

TEST_F(PropertyOperationsTests, addNode)
{
    auto taskDescription = R"(
        {
            "add_node": {
                "path": "/gltf/nodes/0",
                "pathSrc": "/gltf/nodes/2"}
        }
    )"_json;

    auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

    EXPECT_EQ(state["gltf"]["nodes"][0]["children"].size(), 2);
}

TEST_F(PropertyOperationsTests, addNewNode)
{
    auto taskDescription = R"(
        {
            "add_new_node": {
                "path": "/gltf/nodes/0"
            }
        }
    )"_json;

    int nodeCount = nlohmann::json(m_box)["gltf"]["nodes"].size();
    auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

    EXPECT_EQ(state["gltf"]["nodes"][0]["children"][1], nodeCount);
}

TEST_F(PropertyOperationsTests, clone)
{
    {
        auto taskDescription = R"(
        {
            "clone_property": {
                "path": "/gltf/nodes/0",
                "cloneModeShallow": true,
                "target": "parent"
            }
        }
        )"_json;

        int elementCount = nlohmann::json(m_box)["gltf"]["scenes"][0]["nodes"].size();
        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        // added correctly to parent
        EXPECT_EQ(state["gltf"]["scenes"][0]["nodes"].size(), elementCount + 1);
    }
    {
        auto taskDescription = R"(
        {
            "clone_property": {
                "path": "/gltf/nodes/1",
                "cloneModeShallow": true,
                "target": "/gltf/nodes/2"
            }
        }
        )"_json;

        int elementCount = nlohmann::json(m_box)["gltf"]["scenes"][0]["nodes"].size();
        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        EXPECT_EQ(state["gltf"]["nodes"][3]["mesh"], 0); // new node is referencing original mesh
    }

    {
        auto taskDescription = R"(
        {
            "clone_property": {
                "path": "/gltf/nodes/1",
                "cloneModeShallow": false,
                "target": "/gltf/nodes/2"
            }
        }
        )"_json;

        int elementCount = nlohmann::json(m_box)["gltf"]["scenes"][0]["nodes"].size();
        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        EXPECT_NE(state["gltf"]["nodes"][3]["mesh"], 0); // new node is new mesh
    }
}

TEST_F(PropertyOperationsTests, cloneAvocadoGltf)
{

    ScenarioResource resource = Avocado;
    {
        auto taskDescription = nlohmann::json({ { "clone_property", nlohmann::json::object() } });
        taskDescription["clone_property"]["path"] = "/gltf/nodes/0";
        taskDescription["clone_property"]["cloneModeShallow"] = false;
        taskDescription["clone_property"]["target"] = "parent";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(PropertyOperationsTests, cloneAvocadoGlb)
{

    ScenarioResource resource = AvocadoGlb;
    {
        auto taskDescription = nlohmann::json({ { "clone_property", nlohmann::json::object() } });
        taskDescription["clone_property"]["path"] = "/gltf/nodes/0";
        taskDescription["clone_property"]["cloneModeShallow"] = false;
        taskDescription["clone_property"]["target"] = "parent";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    auto accessorA = resource.state.at(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/attributes/NORMAL")).get<uint64_t>();
    auto accessorB = resource.state.at(nlohmann::json::json_pointer("/gltf/meshes/1/primitives/0/attributes/NORMAL")).get<uint64_t>();

    EXPECT_NE(accessorA, accessorB);
}

TEST_F(PropertyOperationsTests, cloneWithoutParent)
{

    ScenarioResource resource = AvocadoGlb;
    {
        auto taskDescription = nlohmann::json({ { "remove", nlohmann::json::object() } });
        taskDescription["remove"]["path"] = "/gltf/scenes/0";
        resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage);
    }
    {
        auto taskDescription = nlohmann::json({ { "clone_property", nlohmann::json::object() } });
        taskDescription["clone_property"]["path"] = "/gltf/nodes/0";
        taskDescription["clone_property"]["cloneModeShallow"] = false;
        taskDescription["clone_property"]["target"] = "parent_optional";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    EXPECT_EQ(resource.state["gltf"]["nodes"].size(), 2);
}

TEST_F(PropertyOperationsTests, trimBuffer)
{
    auto taskDescription = R"(
    {
        "trim_buffer": {
            "path": "/gltf/buffers/1"
        }
    }
    )"_json;

    EXPECT_EQ(nlohmann::json(m_box)["gltf"]["buffers"][1]["byteLength"], 1000); // before trim

    auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

    EXPECT_EQ(state["gltf"]["buffers"][1]["byteLength"], 100); // size of referencing bufferview
}

TEST_F(PropertyOperationsTests, trimBufferAvocado)
{
    ScenarioResource resource = AvocadoGlb;

    uint32_t bufferViewsBefore = resource.state["gltf"]["bufferViews"].size();
    { // Remove bufferView to create test-hole in buffer
        std::string op = "remove";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = "/gltf/bufferViews/1";
        resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage);
    }
    uint32_t bufferViewsAfter = resource.state["gltf"]["bufferViews"].size();
    EXPECT_TRUE(bufferViewsBefore == bufferViewsAfter + 1);

    uint32_t sizeBefore = resource.state["gltf"]["buffers"][0]["byteLength"];
    { // Remove unreferenced data from the buffer
        std::string op = "trim_buffer";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = "/gltf/buffers/0";
        resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage);
    }
    uint32_t sizeAfter = resource.state["gltf"]["buffers"][0]["byteLength"];
    EXPECT_TRUE(sizeBefore > sizeAfter);
}

TEST_F(PropertyOperationsTests, mergeBuffers)
{
    auto taskDescription = R"(
    {
        "merge_buffers": {}
    }
    )"_json;
    taskDescription["merge_buffers"]["bufferPaths"] = nlohmann::json::array();
    taskDescription["merge_buffers"]["bufferPaths"].push_back("/gltf/buffers/0");
    taskDescription["merge_buffers"]["bufferPaths"].push_back("/gltf/buffers/1");

    uint32_t bufferSum = nlohmann::json(m_box)["gltf"]["buffers"][0]["byteLength"].get<uint32_t>() + nlohmann::json(m_box)["gltf"]["buffers"][1]["byteLength"].get<uint32_t>();

    auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

    EXPECT_EQ(state["gltf"]["buffers"][0]["byteLength"], bufferSum); // size of all buffers
    EXPECT_EQ(state["gltf"]["buffers"].size(), 1); // only one buffer remaining
}

TEST_F(PropertyOperationsTests, cleanTexCoordAttributes)
{
    {
        // basic test
        auto taskDescription = R"(
        {
            "clean_texture_coordinate_attributes": {
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoVariants;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 5);
    }
    {
        // basic test 2
        auto taskDescription = R"(
        {
            "clean_texture_coordinate_attributes": {
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoVariants;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 5);
    }
    {
        auto taskDescription = R"(
        {
            "clean_texture_coordinate_attributes": {
            }
        }
        )"_json;

        nlohmann::json state = m_box;
        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(state), m_dataStorage), std::runtime_error);

        EXPECT_EQ(state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 2);
    }
}

TEST_F(PropertyOperationsTests, createTexCoordAttribute)
{
    {
        // basic test
        auto taskDescription = R"(
        {
            "create_texture_coordinates": {
                "path": "/gltf/meshes/0/primitives/0",
                "proxyShape": "Sphere",
                "texCoordIndex": 1
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoGlb;
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 4);

        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 5);
        EXPECT_EQ(testData.state["gltf"]["accessors"].size(), 6);
    }

    {
        // basic test 2
        // substitute texCoord0
        // -> number of attributes and accessors does not change
        auto taskDescription = R"(
        {
            "create_texture_coordinates": {
                "path": "/gltf/meshes/0/primitives/0",
                "proxyShape": "Sphere",
                "texCoordIndex": 0
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoGlb;
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 4);
        uint32_t accessorCountBefore = testData.state["gltf"]["accessors"].size();

        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 4);
        EXPECT_EQ(testData.state["gltf"]["accessors"].size(), accessorCountBefore);
    }

    {
        // basic test 3
        auto taskDescription = R"(
        {
            "create_texture_coordinates": {
                "path": "/gltf/meshes/0/primitives/0",
                "proxyShape": "Sphere",
                "texCoordIndex": 0
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoVariants;
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 7);

        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 7);
    }

    {
        // basic test 4
        auto taskDescription = R"(
        {
            "create_texture_coordinates": {
                "path": "/gltf/meshes/0/primitives/0",
                "proxyShape": "Sphere",
                "texCoordIndex": 4,
                "normal": [1, 0 ,0 ]
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoVariants;
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 7);

        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 8);
    }

    {
        // basic test 5
        auto taskDescription = R"(
        {
            "create_texture_coordinates": {
                "path": "/gltf/meshes/0/primitives/0",
                "proxyShape": "Cube",
                "texCoordIndex": 1,
                "normal": [1, 0 ,0 ]
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoGlb;
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 4);

        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 5);
        EXPECT_EQ(testData.state["gltf"]["accessors"].size(), 6);
    }

    {
        // basic test 4
        auto taskDescription = R"(
        {
            "create_texture_coordinates": {
                "path": "/gltf/meshes/0/primitives/0",
                "proxyShape": "Plane",
                "texCoordIndex": 1,
                "normal": [1, 0 ,0 ]
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoGlb;
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 4);

        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 5);
        EXPECT_EQ(testData.state["gltf"]["accessors"].size(), 6);
    }

    {
        // basic test 4
        auto taskDescription = R"(
        {
            "create_texture_coordinates": {
                "path": "/gltf/meshes/0/primitives/0",
                "proxyShape": "Cylinder",
                "texCoordIndex": 1,
                "normal": [1, 0 ,0 ]
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoGlb;
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 4);

        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 5);
        EXPECT_EQ(testData.state["gltf"]["accessors"].size(), 6);
    }

    {
        // error test 1
        auto taskDescription = R"(
        {
            "create_texture_coordinates": {
                "path": "/gltf/meshes/0/primitives/0",
                "proxyShape": "Plane",
                "texCoordIndex": 5,
                "normal": [0, 0 ,0 ]
            }
        }
        )"_json;

        ScenarioResource testData = AvocadoGlb;

        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage), std::runtime_error);
    }
}

TEST_F(PropertyOperationsTests, deleteAttributes)
{
    {
        // basic tests
        auto taskDescription = R"(
        {
            "delete_attribute": {
                "path": "/gltf/meshes/0/primitives/0/attributes/NORMAL"
            }
        }
        )"_json;

        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        EXPECT_EQ(state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 1);
    }
    {
        auto taskDescription = R"(
        {
            "delete_attribute": {
                "path": "/gltf/meshes/0/primitives/0/attributes/NORMAL"
            }
        }
        )"_json;
        ScenarioResource testData = AvocadoVariants;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 6);
    }
    {
        // test for used texcoords can not be deleted
        auto taskDescription = R"(
        {
            "delete_attribute": {
                "path": "/gltf/meshes/0/primitives/0/attributes/TEXCOORD_0"
            }
        }
        )"_json;
        ScenarioResource testData = AvocadoGlb;
        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage), std::runtime_error);
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 4);
    }
    {
        // test for out of order
        auto taskDescription = R"(
        {
            "delete_attribute": {
                "path": "/gltf/meshes/0/primitives/0/attributes/TEXCOORD_2"
            }
        }
        )"_json;
        ScenarioResource testData = AvocadoVariants;
        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage), std::runtime_error);
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 7);
    }
    {
        // test for correct deletion
        auto taskDescription = R"(
        {
            "delete_attribute": {
                "path": "/gltf/meshes/0/primitives/0/attributes/TEXCOORD_3"
            }
        }
        )"_json;
        ScenarioResource testData = AvocadoVariants;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 6);
    }
    {
        // nonexistant texccords
        auto taskDescription = R"(
        {
            "delete_attribute": {
                "path": "/gltf/meshes/0/primitives/0/attributes/TEXCOORD_22"
            }
        }
        )"_json;
        ScenarioResource testData = AvocadoVariants;
        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage), std::runtime_error);
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 7);
    }
    {
        // nonexistant attribute
        auto taskDescription = R"(
        {
            "delete_attribute": {
                "path": "/gltf/meshes/0/primitives/0/attributes/IAMGROOT"
            }
        }
        )"_json;
        ScenarioResource testData = AvocadoVariants;
        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage), std::runtime_error);
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 7);
    }
    {
        // texcoords used in material variants
        auto taskDescription = R"(
        {
            "delete_attribute": {
                "path": "/gltf/meshes/0/primitives/0/attributes/TEXCOORD_1"
            }
        }
        )"_json;
        ScenarioResource testData = AvocadoVariants;
        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage), std::runtime_error);
        EXPECT_EQ(testData.state["gltf"]["meshes"][0]["primitives"][0]["attributes"].size(), 7);
    }
}

TEST_F(PropertyOperationsTests, cleanAll)
{
    {
        // basic tests
        auto taskDescription = R"(
        {
            "clean_gltf": {
            }
        }
        )"_json;

        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        EXPECT_EQ(state["gltf"]["asset"].size(), 2);
        EXPECT_EQ(state["gltf"]["accessors"].size(), 3);
        EXPECT_EQ(state["gltf"]["buffers"].size(), 1);
        EXPECT_EQ(state["gltf"]["bufferViews"].size(), 2);
        EXPECT_EQ(state["gltf"]["buffers"].size(), 1);
        EXPECT_EQ(state["gltf"]["cameras"].size(), 2);
        EXPECT_EQ(state["gltf"]["materials"].size(), 1);
        EXPECT_EQ(state["gltf"]["meshes"].size(), 1);
        EXPECT_EQ(state["gltf"]["nodes"].size(), 2);
        EXPECT_EQ(state["gltf"]["scene"].size(), 1);
        EXPECT_EQ(state["gltf"]["scenes"].size(), 1);
    }

    {
        // remove node from scene -> should result in an almost empty gltf

        auto taskDescription = R"(
        {
            "remove_gltf_pointer_from_array": {
                "path": "/gltf/scenes/0/nodes/0"
            }
        }
        )"_json;
        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        taskDescription = R"(
        {
            "clean_gltf": {
            }
        }
        )"_json;

        state = g_library.call(std::move(taskDescription), nlohmann::json(state), m_dataStorage);

        EXPECT_EQ(state["gltf"]["asset"].size(), 2);
        EXPECT_FALSE(state["gltf"].contains("accessors"));
        EXPECT_FALSE(state["gltf"].contains("buffers"));
        EXPECT_FALSE(state["gltf"].contains("bufferViews"));
        EXPECT_FALSE(state["gltf"].contains("buffers"));
        EXPECT_FALSE(state["gltf"].contains("cameras"));
        EXPECT_FALSE(state["gltf"].contains("materials"));
        EXPECT_FALSE(state["gltf"].contains("meshes"));
        EXPECT_FALSE(state["gltf"].contains("nodes"));
        EXPECT_EQ(state["gltf"]["scene"].size(), 1);
        EXPECT_EQ(state["gltf"]["scenes"].size(), 1);
    }
}

TEST_F(PropertyOperationsTests, cleanAllNoChange)
{
    ScenarioResource resource = MorphStressTest;
    auto taskDescription = nlohmann::json({ { "clean_gltf", nlohmann::json::object() } });
    EXPECT_ANY_THROW(g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
}

TEST_F(PropertyOperationsTests, calculateMinMax)
{
    auto positionAccessorPath = (nlohmann::json::json_pointer("/gltf/accessors/3"));
    auto& accessor = Avocado.state.at(positionAccessorPath);

    auto min = accessor.at("min").get<std::vector<double>>(); // backup values
    auto max = accessor.at("max").get<std::vector<double>>();

    accessor.erase("min");
    accessor.erase("max");

    auto taskDescription = nlohmann::json({ { "calculate_min_max", nlohmann::json::object() } });
    taskDescription["calculate_min_max"]["path"] = positionAccessorPath.to_string();

    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));

    auto& accessorResult = Avocado.state.at(positionAccessorPath);
    auto minResult = accessorResult.at("min").get<std::vector<double>>();
    auto maxResult = accessorResult.at("max").get<std::vector<double>>();

    for (uint32_t i = 0; i < min.size(); i++) {
        EXPECT_EQ(round(max[i] * 1000.0), round(maxResult[i] * 1000.0));
        EXPECT_EQ(round(min[i] * 1000.0), round(minResult[i] * 1000.0));
    }
}

TEST_F(PropertyOperationsTests, cleanNodes)
{
    // simple test
    {
        auto taskDescription = R"(
        {
            "clean_nodes": {
            }
        }
        )"_json;
        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);
        EXPECT_EQ(state["gltf"]["nodes"].size(), 2);
    }

    {
        nlohmann::json box = m_box;
        box["gltf"]["nodes"].push_back({});
        box["gltf"]["nodes"].push_back({ { "children", { 2, 3 } } });
        box["gltf"]["nodes"].push_back({ { "children", { 4 } } });
        box["gltf"]["nodes"].push_back({ { "mesh", 0 } });

        auto taskDescription = R"(
        {
            "clean_nodes": {
            }
        }
        )"_json;
        auto state = g_library.call(std::move(taskDescription), nlohmann::json(box), m_dataStorage);
        EXPECT_EQ(state["gltf"]["nodes"].size(), 3);
    }

    {
        nlohmann::json box = m_box;
        box["gltf"]["scenes"] = { {} };

        auto taskDescription = R"(
        {
            "clean_nodes": {
            }
        }
        )"_json;
        auto state = g_library.call(std::move(taskDescription), nlohmann::json(box), m_dataStorage);
        EXPECT_EQ(state["gltf"]["nodes"].size(), 2);
    }
}
TEST_F(PropertyOperationsTests, removeProperties)
{
    {
        // clean names
        auto taskDescription = R"(
        {
            "remove_properties_by_name": {
                "property" : "/name"
            }
        }
        )"_json;

        auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

        EXPECT_FALSE(state["gltf"]["meshes"][0].contains("name"));
        EXPECT_FALSE(state["gltf"]["nodes"][0].contains("name"));
        EXPECT_FALSE(state["gltf"]["cameras"][0].contains("name"));
        EXPECT_FALSE(state["gltf"]["cameras"][1].contains("name"));
        EXPECT_FALSE(state["gltf"]["materials"][0].contains("name"));
    }
    {
        // clean extras
        auto taskDescription = R"(
        {
            "remove_properties_by_name": {
                "property" : "/extras"
            }
        }
        )"_json;

        ScenarioResource testData = Avocado;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["asset"].size(), 2);
    }
    {
        // delete animations
        auto taskDescription = R"(
        {
            "remove_properties_by_name": {
                "property" : "/animations"
            }
        }
        )"_json;

        ScenarioResource testData = FoxGlb;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);
        EXPECT_FALSE(testData.state["gltf"].contains("animations"));
    }
}

TEST_F(PropertyOperationsTests, removePropertiesFail)
{

    // wrong parameter
    auto taskDescription = R"(
        {
            "remove_properties_by_name": {
                "property" : "meshes"
            }
        }
        )"_json;

    ScenarioResource testData = Avocado;
    EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage), std::invalid_argument);
}

TEST_F(PropertyOperationsTests, removeExtrasProperties)
{
    ScenarioResource resource = Avocado;

    resource.state.at(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0"))["extras"] = nlohmann::json::object();
    resource.state.at(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0"))["extras"]["no_delete"] = nlohmann::json::object();
    resource.state.at(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0"))["extras"]["please_delete"] = nlohmann::json::object();

    auto params = nlohmann::json::object();
    params["property"] = "/extras/please_delete";
    auto taskDescription = nlohmann::json({ { "remove_properties_by_name", params } });
    resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage);

    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/extras")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/extras/no_delete")));
    EXPECT_FALSE(resource.state.contains(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/extras/please_delete")));
}

TEST_F(PropertyOperationsTests, removeKnownExtensions)
{
    {
        // delete KHR_lights_punctual
        auto taskDescription = R"(
        {
            "remove_extension": {
                "property" : "KHR_lights_punctual"
            }
        }
        )"_json;

        ScenarioResource testData = Fox;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        auto it = std::find(testData.state["gltf"]["extensionsUsed"].begin(), testData.state["gltf"]["extensionsUsed"].end(), "KHR_lights_punctual");
        EXPECT_TRUE(it == testData.state["gltf"]["extensionsUsed"].end());
        EXPECT_FALSE(testData.state["gltf"]["extensions"].contains("KHR_lights_punctual"));
        EXPECT_FALSE(testData.state["gltf"]["nodes"][26].contains("KHR_lights_punctual"));
    }
    {
        // delete KHR_materials_clearcoat
        auto taskDescription = R"(
        {
            "remove_extension": {
                "property" : "KHR_materials_clearcoat"
            }
        }
        )"_json;

        ScenarioResource testData = Fox;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        auto it = std::find(testData.state["gltf"]["extensionsUsed"].begin(), testData.state["gltf"]["extensionsUsed"].end(), "KHR_materials_clearcoat");
        EXPECT_TRUE(it == testData.state["gltf"]["extensionsUsed"].end());
        EXPECT_FALSE(testData.state["gltf"]["materials"][0]["extensions"].contains("KHR_materials_clearcoat"));
    }
    {
        // delete KHR_materials_clearcoat and KHR_lights_punctual
        auto taskDescription = R"(
        {
            "remove_extension": {
                "properties" : ["KHR_materials_clearcoat", "KHR_lights_punctual"]
            }
        }
        )"_json;

        ScenarioResource testData = Fox;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        auto it = std::find(testData.state["gltf"]["extensionsUsed"].begin(), testData.state["gltf"]["extensionsUsed"].end(), "KHR_materials_clearcoat");
        EXPECT_TRUE(it == testData.state["gltf"]["extensionsUsed"].end());
        EXPECT_FALSE(testData.state["gltf"]["materials"][0]["extensions"].contains("KHR_materials_clearcoat"));

        it = std::find(testData.state["gltf"]["extensionsUsed"].begin(), testData.state["gltf"]["extensionsUsed"].end(), "KHR_lights_punctual");
        EXPECT_TRUE(it == testData.state["gltf"]["extensionsUsed"].end());
        EXPECT_FALSE(testData.state["gltf"]["extensions"].contains("KHR_lights_punctual"));
        EXPECT_FALSE(testData.state["gltf"]["nodes"][26].contains("KHR_lights_punctual"));
    }
}

TEST_F(PropertyOperationsTests, removeUnknownExtensions)
{
    {
        // delete KHR_lights_punctual
        auto taskDescription = R"(
        {
            "remove_unknown_extension": {
            }
        }
        )"_json;

        ScenarioResource testData = Fox;
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        auto it = std::find(testData.state["gltf"]["extensionsUsed"].begin(), testData.state["gltf"]["extensionsUsed"].end(), "UX3D_unknown_extension");
        EXPECT_TRUE(it == testData.state["gltf"]["extensionsUsed"].end());
        EXPECT_FALSE(testData.state["gltf"]["extensions"].contains("UX3D_unknown_extension"));
        EXPECT_FALSE(testData.state["gltf"]["materials"][0]["extensions"].contains("UX3D_unknown_extension"));
    }
}

TEST_F(PropertyOperationsTests, convertPrimitivesToMeshes)
{
    {
        // basic test
        auto taskDescription = R"(
        {
            "split_up_primitives": {
                "path": "/gltf/meshes/1" 
            }
        }
        )"_json;

        ScenarioResource testData = CesiumMilkTruck;
        uint32_t meshCountBefore = testData.state["gltf"]["meshes"].size(); // Wheels mesh (1) has one primitive -> mesh count should stay the same
        EXPECT_ANY_THROW(g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage));
    }
    {
        // basic test
        auto taskDescription = R"(
        {
            "split_up_primitives": {
                "path": "/gltf/meshes/0"
            }
        }
        )"_json;

        ScenarioResource testData = CesiumMilkTruck;

        uint32_t meshCountBefore = testData.state["gltf"]["meshes"].size();
        // Cesium_Milk_Truck mesh has 3 primitives.
        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"].size(), meshCountBefore + 3);
    }
    {
        // node creation test
        auto taskDescription = R"(
        {
            "split_up_primitives": {
                "path": "/gltf/meshes/0"
            }
        }
        )"_json;

        ScenarioResource testData = CesiumMilkTruck;
        uint32_t meshCountBefore = testData.state["gltf"]["meshes"].size();
        uint32_t nodeCountBefore = testData.state["gltf"]["nodes"].size();

        testData.state = g_library.call(std::move(taskDescription), std::move(testData.state), testData.dataStorage);

        EXPECT_EQ(testData.state["gltf"]["meshes"].size(), meshCountBefore + 3);
        EXPECT_EQ(testData.state["gltf"]["nodes"].size(), nodeCountBefore + 3);
        EXPECT_EQ(testData.state["gltf"]["meshes"][meshCountBefore + 0]["primitives"].size(), 1);
        EXPECT_EQ(testData.state["gltf"]["meshes"][meshCountBefore + 1]["primitives"].size(), 1);
        EXPECT_EQ(testData.state["gltf"]["meshes"][meshCountBefore + 2]["primitives"].size(), 1);
        EXPECT_TRUE(testData.state["gltf"]["nodes"][nodeCountBefore + 0].contains("mesh"));
        EXPECT_TRUE(testData.state["gltf"]["nodes"][nodeCountBefore + 1].contains("mesh"));
        EXPECT_TRUE(testData.state["gltf"]["nodes"][nodeCountBefore + 2].contains("mesh"));
    }
}

TEST_F(PropertyOperationsTests, joinMeshes)
{
    {
        // supply operation with invalid arguments, needs at least two mesh pointers
        auto taskDescription = R"(
        {
            "join_meshes": {
                "meshPaths": [
                    "/gltf/meshes/0"
                ]
            }
        }
        )"_json;

        ScenarioResource resource = CesiumMilkTruckVariant;
        EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);
    }

    {
        // join two meshes, one having multiple primitives
        auto taskDescription = R"(
        {
            "join_meshes": {
                "meshPaths": [
                    "/gltf/meshes/0",
                    "/gltf/meshes/1"
                ]
            }
        }
        )"_json;

        ScenarioResource resource = CesiumMilkTruckVariant;
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
        EXPECT_EQ(resource.state["gltf"]["meshes"].size(), 3); // the new joined mesh has been added
        EXPECT_EQ(resource.state["gltf"]["meshes"][0]["primitives"].size(), 3); // original meshes should keep their primitives
        EXPECT_EQ(resource.state["gltf"]["meshes"][1]["primitives"].size(), 1); // original meshes should keep their primitives
        EXPECT_EQ(resource.state["gltf"]["meshes"][2]["primitives"].size(), 4); // new meshes should have all primitives present in the joined meshes
        EXPECT_EQ(resource.state["gltf"]["meshes"][2]["primitives"][1]["material"], 1); // primitive's material has not changed after copying
        EXPECT_EQ(resource.state["gltf"]["meshes"][2]["name"], "Joined Mesh (\"Cesium_Milk_Truck\", \"Wheels\")"); // name is set correctly
    }

    {
        // join two meshes, with mismatching morph targets
        auto taskDescription = R"(
        {
            "join_meshes": {
                "meshPaths": [
                    "/gltf/meshes/0",
                    "/gltf/meshes/1"
                ]
            }
        }
        )"_json;

        ScenarioResource resource = MorphStressTest;
        EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::runtime_error);
    }
}

TEST_F(PropertyOperationsTests, transformNode)
{
    ScenarioResource resource = Avocado;
    {
        auto taskDescription = nlohmann::json({ { "transform_node", nlohmann::json::object() } });
        taskDescription["transform_node"]["path"] = "/gltf/nodes/0";
        taskDescription["transform_node"]["translation"] = nlohmann::json({ 1.0, 0.0, 0.0 });
        taskDescription["transform_node"]["rotation"] = nlohmann::json({ 90.0, 0.0, 0.0 });
        taskDescription["transform_node"]["scale"] = nlohmann::json({ 1.0, 2.0, 1.0 });
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
        EXPECT_EQ(resource.state["gltf"]["nodes"][0]["translation"][0].get<double>(), 1.0);
        EXPECT_EQ(resource.state["gltf"]["nodes"][0]["translation"][1].get<double>(), 0.0);
        EXPECT_EQ(resource.state["gltf"]["nodes"][0]["translation"][2].get<double>(), 0.0);
        EXPECT_EQ(resource.state["gltf"]["nodes"][0]["scale"][0].get<double>(), 1.0);
        EXPECT_EQ(resource.state["gltf"]["nodes"][0]["scale"][1].get<double>(), 2.0);
        EXPECT_EQ(resource.state["gltf"]["nodes"][0]["scale"][2].get<double>(), 1.0);
    }
}

TEST_F(PropertyOperationsTests, transformNodeRotation)
{
    {
        // Zero rotation
        ScenarioResource resource = DuckGlb;
        auto taskDescription = nlohmann::json({ { "transform_node", nlohmann::json::object() } });
        taskDescription["transform_node"]["path"] = "/gltf/nodes/2";
        taskDescription["transform_node"]["rotation"] = nlohmann::json({ 0.0, 0.0, 0.0 });
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
        EXPECT_EQ(resource.state["gltf"]["nodes"][2]["rotation"][0].get<double>(), 0.0);
        EXPECT_EQ(resource.state["gltf"]["nodes"][2]["rotation"][1].get<double>(), 0.0);
        EXPECT_EQ(resource.state["gltf"]["nodes"][2]["rotation"][2].get<double>(), 0.0);
        EXPECT_EQ(resource.state["gltf"]["nodes"][2]["rotation"][3].get<double>(), 1.0);
    }
    {
        // Rotation 90 degrees x-axis
        ScenarioResource resource = DuckGlb;
        auto taskDescription = nlohmann::json({ { "transform_node", nlohmann::json::object() } });
        taskDescription["transform_node"]["path"] = "/gltf/nodes/2";
        taskDescription["transform_node"]["rotation"] = nlohmann::json({ 90.0, 0.0, 0.0 });
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

        ASSERT_NEAR(resource.state["gltf"]["nodes"][2]["rotation"][0].get<double>(), 0.7071067811865476, 0.0000001);
        ASSERT_NEAR(resource.state["gltf"]["nodes"][2]["rotation"][1].get<double>(), 0.0, 0.0000001);
        ASSERT_NEAR(resource.state["gltf"]["nodes"][2]["rotation"][2].get<double>(), 0.0, 0.0000001);
        ASSERT_NEAR(resource.state["gltf"]["nodes"][2]["rotation"][3].get<double>(), 0.7071067811865476, 0.0000001);
    }
}

TEST_F(PropertyOperationsTests, createEmpty)
{
    ScenarioResource resource = Avocado;
    {
        auto taskDescription = nlohmann::json({ { "create_empty_gltf", nlohmann::json::object() } });
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(PropertyOperationsTests, createAnimationTranslation)
{
    nlohmann::json state = {
        { "gltf", { { "nodes", { { { "translation", { 1, 2, 3 } } } } } } },
        { "files", {} }
    };
    datastorage::DataStorage dataStorage;

    auto taskDescription = R"({
        "create_animation": {
            "path": "/gltf/nodes/0",
            "start": 1.5,
            "duration": 2,
            "translation": [1, 2, 3]
        }
     })"_json;

    state = g_library.call(std::move(taskDescription), std::move(state), dataStorage);

    EXPECT_EQ(state["gltf"]["animations"][0]["channels"].size(), 1);
    EXPECT_EQ(state["gltf"]["animations"][0]["samplers"].size(), 1);
    EXPECT_EQ(state["gltf"]["accessors"].size(), 2);
}

TEST_F(PropertyOperationsTests, createAnimationRotation)
{
    nlohmann::json state = {
        { "gltf", { { "nodes", { { { "rotation", { 1, 2, 3, 4 } } } } } } },
        { "files", {} }
    };
    datastorage::DataStorage dataStorage;

    auto taskDescription = R"({
        "create_animation": {
            "path": "/gltf/nodes/0",
            "start": 1.5,
            "duration": 2,
            "rotationAxis": [4, 5, 6],
            "degrees": 45
        }
     })"_json;

    state = g_library.call(std::move(taskDescription), std::move(state), dataStorage);

    EXPECT_EQ(state["gltf"]["animations"][0]["channels"].size(), 1);
    EXPECT_EQ(state["gltf"]["animations"][0]["samplers"].size(), 1);
    EXPECT_EQ(state["gltf"]["accessors"].size(), 2);
}

TEST_F(PropertyOperationsTests, createAnimationFail)
{
    nlohmann::json state = {
        { "gltf", { { "nodes", {} } } },
        { "files", {} }
    };
    datastorage::DataStorage dataStorage;

    auto taskDescription = R"({
        "create_animation": {
            "path": "/gltf/nodes/0",
            "start": 1.5,
            "duration": 2,
            "translation": [1, 2, 3],
            "rotationAxis": [4, 5, 6],
            "degrees": 45
        }
     })"_json;

    EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(state), dataStorage), std::invalid_argument);
}

TEST_F(PropertyOperationsTests, createMesh)
{
    nlohmann::json state = {
        { "gltf", {
                      { "asset", { { "version", "2.0" } } },
                      { "nodes", nlohmann::json::array() },
                  } },
    };
    datastorage::DataStorage dataStorage;

    auto taskDescription = nlohmann::json({ { "create_mesh", nlohmann::json::object() } });
    taskDescription["create_mesh"]["path"] = "/gltf/nodes/0";
    taskDescription["create_mesh"]["shape"] = "cube";

    EXPECT_ANY_THROW(state = g_library.call(std::move(taskDescription), std::move(state), dataStorage));
    state["gltf"]["nodes"].emplace_back(nlohmann::json::object());
    EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), dataStorage));

    EXPECT_EQ(state["gltf"]["nodes"][0]["mesh"].get<int>(), 0);
    ASSERT_EQ(state["gltf"]["meshes"].size(), 1);
    EXPECT_TRUE(state["gltf"]["meshes"][0].contains("primitives"));
    ASSERT_EQ(state["gltf"]["meshes"][0]["primitives"].size(), 1);

    const auto& primitive = state["gltf"]["meshes"][0]["primitives"][0];
    ASSERT_TRUE(primitive.contains("indices"));
    ASSERT_TRUE(primitive.contains("attributes"));
    EXPECT_LT(primitive["indices"], state["gltf"]["accessors"].size());
    ASSERT_TRUE(primitive["attributes"].contains("POSITION"));
    ASSERT_LT(primitive["attributes"]["POSITION"], state["gltf"]["accessors"].size());
    EXPECT_TRUE(state["gltf"]["accessors"][primitive["attributes"]["POSITION"].get<int>()].contains("max"));
    EXPECT_TRUE(state["gltf"]["accessors"][primitive["attributes"]["POSITION"].get<int>()].contains("min"));
    ASSERT_TRUE(primitive["attributes"].contains("NORMAL"));
    EXPECT_LT(primitive["attributes"]["NORMAL"], state["gltf"]["accessors"].size());
    ASSERT_TRUE(primitive["attributes"].contains("TEXCOORD_0"));
    EXPECT_LT(primitive["attributes"]["TEXCOORD_0"], state["gltf"]["accessors"].size());

    taskDescription["create_mesh"]["shape"] = "plane";
    EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), dataStorage));
    taskDescription["create_mesh"]["shape"] = "sphere";
    EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), dataStorage));
    taskDescription["create_mesh"]["shape"] = "torus";
    EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), dataStorage));
    taskDescription["create_mesh"]["shape"] = "disk";
    EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), dataStorage));
}

TEST_F(PropertyOperationsTests, reassignSkinnedAnimation)
{
    {
        // fail test: reference same skin twice in task description
        nlohmann::json state = R"(
        {
            "gltf": {
                "asset": { "version": "2.0" },
                "nodes": [ { "name": "Node 0" }, { "name": "Node 1" } ],
                "animations": [ { "channels": [ { "sampler": 0, "target": { "node": 0, "path": "translation" } } ], "samplers": [ {"input": 0, "output": 1} ] } ],
                "skins": [ { "joints": [ 0 ] }, { "joints": [ 1 ] } ]
            }
        }
        )"_json;
        datastorage::DataStorage dataStorage;

        auto taskDescription = R"(
        {
            "reassign_skinned_animation": {
                "path": "/gltf/animations/0",
                "sourceSkin": "/gltf/skins/0",
                "targetSkin": "/gltf/skins/0"
            }
        }
        )"_json;

        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(state), dataStorage), std::invalid_argument);
    }
    {
        // fail test: animation index out of bounds
        nlohmann::json state = R"(
        {
            "gltf": {
                "asset": { "version": "2.0" },
                "nodes": [ { "name": "Node 0" }, { "name": "Node 1" } ],
                "animations": [ { "channels": [ { "sampler": 0, "target": { "node": 0, "path": "translation" } } ], "samplers": [ {"input": 0, "output": 1} ] } ],
                "skins": [ { "joints": [ 0 ] }, { "joints": [ 1 ] } ]
            }
        }
        )"_json;
        datastorage::DataStorage dataStorage;

        auto taskDescription = R"(
        {
            "reassign_skinned_animation": {
                "path": "/gltf/animations/1",
                "sourceSkin": "/gltf/skins/0",
                "targetSkin": "/gltf/skins/1"
            }
        }
        )"_json;

        EXPECT_THROW(g_library.call(std::move(taskDescription), std::move(state), dataStorage), std::invalid_argument);
    }
    {
        // apply operation to simple glTF
        nlohmann::json state = R"(
        {
            "gltf": {
                "asset": { "version": "2.0" },
                "nodes": [ { "name": "Node 0" }, { "name": "Node 1" } ],
                "animations": [ { "channels": [ { "sampler": 0, "target": { "node": 0, "path": "translation" } } ], "samplers": [ {"input": 0, "output": 1} ] } ],
                "skins": [ { "joints": [ 0 ] }, { "joints": [ 1 ] } ]
            }
        }
        )"_json;
        datastorage::DataStorage dataStorage;

        auto taskDescription = R"(
        {
            "reassign_skinned_animation": {
                "path": "/gltf/animations/0",
                "sourceSkin": "/gltf/skins/0",
                "targetSkin": "/gltf/skins/1"
            }
        }
        )"_json;

        state = g_library.call(std::move(taskDescription), std::move(state), dataStorage);
        EXPECT_TRUE(state["gltf"]["animations"][0]["channels"][0]["target"]["node"] == 1);
    }
}

TEST_F(PropertyOperationsTests, changeCameraType)
{
    auto taskDescription = R"(
        {
            "change_camera_type": {
                "path": "/gltf/cameras/0",
                "type": "perspective"}
        }
    )"_json;

    EXPECT_ANY_THROW(g_library.call(taskDescription, nlohmann::json(m_box), m_dataStorage));

    taskDescription["change_camera_type"].erase("type");

    auto state = nlohmann::json(m_box);
    double aspectRatio = 1.3;
    state["/gltf/cameras/0/perspective/aspectRatio"_json_pointer] = aspectRatio;

    state = g_library.call(taskDescription, std::move(state), m_dataStorage);

    EXPECT_FALSE(state.contains("/gltf/cameras/0/perspective"_json_pointer));
    EXPECT_TRUE(state.contains("/gltf/cameras/0/orthographic"_json_pointer));
    EXPECT_TRUE(state.at("/gltf/cameras/0/type"_json_pointer) == "orthographic");
    EXPECT_TRUE(state.at("/gltf/cameras/0/orthographic/zfar"_json_pointer) == m_box.at("/gltf/cameras/0/perspective/zfar"_json_pointer));
    EXPECT_TRUE(state.at("/gltf/cameras/0/orthographic/znear"_json_pointer) == m_box.at("/gltf/cameras/0/perspective/znear"_json_pointer));
    EXPECT_EQ(state.at("/gltf/cameras/0/orthographic/xmag"_json_pointer).get<double>() / state.at("/gltf/cameras/0/orthographic/ymag"_json_pointer).get<double>(), aspectRatio);

    taskDescription["change_camera_type"]["type"] = "perspective";

    state = g_library.call(taskDescription, std::move(state), m_dataStorage);

    EXPECT_TRUE(state.contains("/gltf/cameras/0/perspective"_json_pointer));
    EXPECT_FALSE(state.contains("/gltf/cameras/0/orthographic"_json_pointer));
    EXPECT_TRUE(state.at("/gltf/cameras/0/type"_json_pointer) == "perspective");
    EXPECT_TRUE(state.at("/gltf/cameras/0/perspective/zfar"_json_pointer) == m_box.at("/gltf/cameras/0/perspective/zfar"_json_pointer));
    EXPECT_TRUE(state.at("/gltf/cameras/0/perspective/znear"_json_pointer) == m_box.at("/gltf/cameras/0/perspective/znear"_json_pointer));
    EXPECT_TRUE(state.at("/gltf/cameras/0/perspective/aspectRatio"_json_pointer) == aspectRatio);
    EXPECT_TRUE(state.at("/gltf/cameras/0/perspective/yfov"_json_pointer) == m_box.at("/gltf/cameras/0/perspective/yfov"_json_pointer));
}
TEST_F(PropertyOperationsTests, deleteAccessor)
{
    ScenarioResource resource = Avocado;

    uint32_t accessorCountBefore = resource.state.at(nlohmann::json::json_pointer("/gltf/accessors")).size();
    {
        std::string op = "delete_accessor";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = "/gltf/accessors/0";
        taskDescription[op]["skipIfReferenced"] = false;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    EXPECT_TRUE(resource.state["gltf"].contains("accessors"));
    EXPECT_TRUE(resource.state["gltf"].contains("bufferViews"));
    uint32_t accessorCountAfter = resource.state.at(nlohmann::json::json_pointer("/gltf/accessors")).size();
    EXPECT_EQ(accessorCountBefore, accessorCountAfter + 1);
}

TEST_F(PropertyOperationsTests, deleteAccessors)
{
    ScenarioResource resource = Avocado;
    // bufferView 2 is only referenced by accessor 2
    uint32_t accessor2Size = resource.state.at(nlohmann::json::json_pointer("/gltf/bufferViews/2/byteLength")).get<uint32_t>();

    {
        std::string op = "delete_accessor";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = nlohmann::json::array({ "/gltf/accessors/0", "/gltf/accessors/1", "/gltf/accessors/4", "/gltf/accessors/3" });
        taskDescription[op]["skipIfReferenced"] = false;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/accessors")).size(), 1);
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/bufferViews")).size(), 1);

    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/bufferViews/0/byteLength")).get<uint32_t>(), accessor2Size);
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/bufferViews/0/byteOffset")).get<uint32_t>(), 0);
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/buffers/0/byteLength")).get<uint32_t>(), accessor2Size);
}

TEST_F(PropertyOperationsTests, deleteAllAccessors)
{
    ScenarioResource resource = Avocado;
    {
        std::string op = "delete_accessor";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = nlohmann::json::array({ "/gltf/accessors/0", "/gltf/accessors/1", "/gltf/accessors/2", "/gltf/accessors/4", "/gltf/accessors/3" });
        taskDescription[op]["skipIfReferenced"] = false;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    EXPECT_FALSE(resource.state["gltf"].contains("accessors"));
    EXPECT_FALSE(resource.state["gltf"].contains("bufferViews"));
}

TEST_F(PropertyOperationsTests, updateExtensionsUsed)
{
    ScenarioResource resource = AvocadoDraco;

    {
        std::string op = "reset";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = "/gltf/meshes/0/primitives/0/extensions";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    EXPECT_TRUE(resource.state["gltf"].contains("extensionsUsed"));

    {
        std::string op = "update_extension_flags";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    EXPECT_FALSE(resource.state["gltf"].contains("extensionsUsed"));
}

TEST_F(PropertyOperationsTests, resolveSparseAccessor)
{
    nlohmann::json state { { "gltf", nlohmann::json::object() } };
    datastorage::DataStorage dataStorage;

    std::vector<uint16_t> indices { 3, 4, 5, 8, 9 };
    std::vector<glm::vec3> deviations {
        glm::vec3 { 1.0, 0.0, 0.0 },
        glm::vec3 { 0.0, 2.0, 0.0 },
        glm::vec3 { 0.0, 0.0, 3.0 },
        glm::vec3 { 4.0, 5.0, 0.0 },
        glm::vec3 { 0.0, 6.0, 7.0 },
    };

    std::vector<glm::vec3> values;
    values.resize(10, glm::vec3 { 42.0, 0.0, 0.0 });

    auto toByteVector = []<class T>(const std::vector<T>& vector, int offset) {
        std::vector<uint8_t> bytes;
        bytes.resize(offset + vector.size() * sizeof(T));
        memcpy(&bytes[offset], vector.data(), bytes.size());
        return bytes;
    };

    auto indexBuffer = reactor::utils::createBuffer(state, dataStorage, toByteVector(indices, 2));
    auto indexBufferView = reactor::utils::createBufferView(state, indexBuffer);

    auto deviationBuffer = reactor::utils::createBuffer(state, dataStorage, toByteVector(deviations, 5));
    auto deviationBufferView = reactor::utils::createBufferView(state, deviationBuffer);

    auto valueBuffer = reactor::utils::createBuffer(state, dataStorage, toByteVector(values, 3));
    auto valueBufferView = reactor::utils::createBufferView(state, valueBuffer);

    state["gltf"]["accessors"].push_back({
        { "count", 10 },
        { "type", "VEC3" },
        { "componentType", reactor::gltf::ComponentType::Float },
        { "bufferView", std::stoi(valueBufferView.back()) },
        { "byteOffset", 3 },
        {
            "sparse",
            {
                { "count", 5 },
                {
                    "indices",
                    {
                        { "bufferView", std::stoi(indexBufferView.back()) },
                        { "componentType", reactor::gltf::ComponentType::UnsignedShort },
                        { "byteOffset", 2 },
                    },
                },
                {
                    "values",
                    {
                        { "bufferView", std::stoi(deviationBufferView.back()) },
                        { "byteOffset", 5 },
                    },
                },
            },
        },
    });

    nlohmann::json task { { "resolve_sparse_accessor", { { "path", "/gltf/accessors/0" } } } };
    ASSERT_NO_THROW(state = g_library.call(std::move(task), std::move(state), dataStorage));

    ASSERT_EQ(state["gltf"]["bufferViews"].size(), 1);
    ASSERT_EQ(state["gltf"]["accessors"].size(), 1);
    ASSERT_FALSE(state["gltf"]["accessors"][0].contains("sparse"));
    ASSERT_EQ(state["gltf"]["accessors"][0]["count"], 10);
    ASSERT_EQ(state["gltf"]["accessors"][0]["type"], "VEC3");
    ASSERT_EQ(state["gltf"]["accessors"][0]["componentType"], reactor::gltf::ComponentType::Float);

    auto resolvedBytes = reactor::utils::getBufferViewData(nlohmann::json::json_pointer {} / "gltf" / "bufferViews" / state["gltf"]["accessors"][0]["bufferView"].get<int>(), state, dataStorage);

    ASSERT_EQ(resolvedBytes.size(), 10 * sizeof(glm::vec3));

    auto resolvedValues = reinterpret_cast<const glm::vec3*>(resolvedBytes.data());
    EXPECT_EQ(resolvedValues[0], (glm::vec3 { 42.0, 0.0, 0.0 }));
    EXPECT_EQ(resolvedValues[1], (glm::vec3 { 42.0, 0.0, 0.0 }));
    EXPECT_EQ(resolvedValues[2], (glm::vec3 { 42.0, 0.0, 0.0 }));
    EXPECT_EQ(resolvedValues[3], (glm::vec3 { 1.0, 0.0, 0.0 }));
    EXPECT_EQ(resolvedValues[4], (glm::vec3 { 0.0, 2.0, 0.0 }));
    EXPECT_EQ(resolvedValues[5], (glm::vec3 { 0.0, 0.0, 3.0 }));
    EXPECT_EQ(resolvedValues[6], (glm::vec3 { 42.0, 0.0, 0.0 }));
    EXPECT_EQ(resolvedValues[7], (glm::vec3 { 42.0, 0.0, 0.0 }));
    EXPECT_EQ(resolvedValues[8], (glm::vec3 { 4.0, 5.0, 0.0 }));
    EXPECT_EQ(resolvedValues[9], (glm::vec3 { 0.0, 6.0, 7.0 }));
}

TEST_F(PropertyOperationsTests, createTangents)
{

    ScenarioResource resource = Avocado;

    auto derefAccessor = [](std::string path, nlohmann::json& state) -> nlohmann::json::json_pointer {
        auto index = state.at(nlohmann::json::json_pointer(path)).get<uint64_t>();
        return nlohmann::json::json_pointer("/gltf/accessors") / index;
    };

    auto dataBefore = reactor::utils::getAccessorData<glm::vec4>(derefAccessor("/gltf/meshes/0/primitives/0/attributes/TANGENT", resource.state), resource.state, resource.dataStorage);

    {
        std::string op = "delete_attribute";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = "/gltf/meshes/0/primitives/0/attributes/TANGENT";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    EXPECT_FALSE(resource.state.contains(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/attributes/TANGENT")));

    {
        std::string op = "create_tangents";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = "/gltf/meshes/0/primitives/0";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/attributes/TANGENT")));

    auto dataAfter = reactor::utils::getAccessorData<glm::vec4>(derefAccessor("/gltf/meshes/0/primitives/0/attributes/TANGENT", resource.state), resource.state, resource.dataStorage);

    EXPECT_EQ(dataBefore.size(), dataAfter.size());
}