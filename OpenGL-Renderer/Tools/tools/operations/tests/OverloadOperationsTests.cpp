#include "gtest/gtest.h"

#include "Helpers.h"
#include "reactor/GltfEnums.h"

using namespace ux3d;
using namespace ux3d::operations;

class OverloadOperationsTests : public OperationsTests {
};

TEST_F(OverloadOperationsTests, setTRS)
{
    nlohmann::json box = m_box;
    ASSERT_TRUE(box.contains("/gltf/nodes/0/matrix"_json_pointer));
    ASSERT_FALSE(box.contains("/gltf/nodes/0/translation"_json_pointer));
    {
        std::vector<std::string> keys = { "translation", "scale" };
        for (auto& key : keys) {
            auto taskDescription = R"(
            {
                "set_gltf_value": {
                    "value": [1.0, 1.0, 1.0]
                }
            }
            )"_json;
            taskDescription["set_gltf_value"]["path"] = "/gltf/nodes/0/" + key;
            auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);
            EXPECT_FALSE(state.contains("/gltf/nodes/0/matrix"_json_pointer));
            EXPECT_TRUE(state.contains("/gltf/nodes/0"_json_pointer / key));
        }
    }

    {
        std::vector<std::string> keys = { "rotation" };
        for (auto& key : keys) {
            auto taskDescription = R"(
            {
                "set_gltf_value": {
                    "value": [1.0, 1.0, 1.0, 1.0]
                }
            }
            )"_json;
            taskDescription["set_gltf_value"]["path"] = "/gltf/nodes/0/" + key;
            auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);
            EXPECT_FALSE(state.contains("/gltf/nodes/0/matrix"_json_pointer));
            EXPECT_TRUE(state.contains("/gltf/nodes/0"_json_pointer / key));
        }
    }
}

TEST_F(OverloadOperationsTests, joinPrimitives)
{
    auto taskDescription = R"(
        {
            "join_primitives": {
            }
        }
        )"_json;
    taskDescription["join_primitives"]["path"] = "/gltf/meshes/0";

    MultiPrimitive.state = g_library.call(std::move(taskDescription), std::move(MultiPrimitive.state), MultiPrimitive.dataStorage);

    const auto& gltf = MultiPrimitive.state["gltf"];

    const auto& prims = gltf["meshes"][0]["primitives"];
    EXPECT_EQ(prims.size(), 1);
}

TEST_F(OverloadOperationsTests, joinPrimitivesInvalidMesh)
{
    auto taskDescription = nlohmann::json({ { "join_primitives", { { "path", "/gltf/meshes/0" } } } });
    EXPECT_ANY_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage));
}

TEST_F(OverloadOperationsTests, setTexCoord)
{
    {
        auto taskDescription = R"(
        {
            "set_gltf_value": {
                "value": 0
            }
        }
        )"_json;
        taskDescription["set_gltf_value"]["path"] = "/gltf/materials/0/normalTexture/texCoord";
        EXPECT_NO_THROW(g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));
    }

    {
        auto taskDescription = R"(
        {
            "set_gltf_value": {
                "value": 0
            }
        }
        )"_json;
        taskDescription["set_gltf_value"]["path"] = "/gltf/materials/0/normalTexture/texCoord";
        EXPECT_ANY_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage));
    }
}

TEST_F(OverloadOperationsTests, setCameraValues)
{
    nlohmann::json box = m_box;
    ASSERT_EQ(box.at("/gltf/cameras/0/type"_json_pointer).get<std::string>(), "perspective");
    {
        auto taskDescription = R"(
        {
            "set_gltf_value": {
                "value": 1.0
            }
        }
        )"_json;
        taskDescription["set_gltf_value"]["path"] = "/gltf/cameras/0/orthographic/xmag";
        nlohmann::json state;
        EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(box), m_dataStorage));
        EXPECT_EQ(state.at("/gltf/cameras/0/type"_json_pointer).get<std::string>(), "orthographic");
        EXPECT_EQ(state.at("/gltf/cameras/0/orthographic/xmag"_json_pointer).get<double>(), 1.0);
        EXPECT_FALSE(state.contains("/gltf/cameras/0/type/perspective"_json_pointer));

        taskDescription = R"(
        {
            "set_gltf_value": {
                "value": 1
            }
        }
        )"_json;
        taskDescription["set_gltf_value"]["path"] = "/gltf/cameras/0/perspective/aspectRatio";
        EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), m_dataStorage));
        EXPECT_EQ(state.at("/gltf/cameras/0/type"_json_pointer).get<std::string>(), "perspective");
        EXPECT_EQ(state.at("/gltf/cameras/0/perspective/aspectRatio"_json_pointer).get<double>(), 1.0);
        EXPECT_FALSE(state.contains("/gltf/cameras/0/type/orthographic"_json_pointer));
    }
}

TEST_F(OverloadOperationsTests, exclusiveMaterialExtensions)
{
    nlohmann::json box = m_box;
    {
        auto taskDescription = R"(
        {
            "set_gltf_value": {
                "value": 1.0
            }
        }
        )"_json;
        taskDescription["set_gltf_value"]["path"] = "/gltf/materials/0/extensions/KHR_materials_emissive_strength/emissiveStrength";
        nlohmann::json state;
        EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(box), m_dataStorage));

        taskDescription["set_gltf_value"]["path"] = "/gltf/materials/0/extensions/KHR_materials_pbrSpecularGlossiness/glossinessFactor";
        EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), m_dataStorage));
        EXPECT_TRUE(state.contains("/gltf/materials/0/extensions/KHR_materials_emissive_strength"_json_pointer));

        taskDescription["set_gltf_value"]["path"] = "/gltf/materials/0/extensions/KHR_materials_clearcoat/clearcoatFactor";
        EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), m_dataStorage));
        EXPECT_FALSE(state.contains("/gltf/materials/0/extensions/KHR_materials_pbrSpecularGlossiness"_json_pointer));
        EXPECT_TRUE(state.contains("/gltf/materials/0/extensions/KHR_materials_emissive_strength"_json_pointer));

        taskDescription = R"(
        {
            "create_gltf_object": {
                "path": "/gltf/materials/0/extensions/KHR_materials_unlit"
            }
        }
        )"_json;
        EXPECT_NO_THROW(state = g_library.call(std::move(taskDescription), std::move(state), m_dataStorage));
        EXPECT_FALSE(state.contains("/gltf/materials/0/extensions/KHR_materials_clearcoat"_json_pointer));
        EXPECT_FALSE(state.contains("/gltf/materials/0/extensions/KHR_materials_emissive_strength"_json_pointer));
    }
}

TEST_F(OverloadOperationsTests, animationSamplerReference)
{
    {
        ScenarioResource resource = FoxGlb;
        auto taskDescription = R"(
        {
            "set_gltf_pointer": {
                "pointer": "/gltf/animations/0/samplers/1",
                "path": "/gltf/animations/0/channels/0/sampler"
            }
        }
        )"_json;
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
        EXPECT_EQ(resource.state.at("/gltf/animations/0/channels/0/sampler"_json_pointer).get<int>(), 1);
    }
    {
        ScenarioResource resource = FoxGlb;
        auto taskDescription = R"(
        {
            "set_gltf_pointer": {
                "pointer": "/gltf/animations/1/samplers/1",
                "path": "/gltf/animations/0/channels/0/sampler"
            }
        }
        )"_json;
        EXPECT_ANY_THROW(g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(OverloadOperationsTests, resetDraco)
{
    {
        ScenarioResource resource = AvocadoDraco;
        auto taskDescription = R"(
        {
            "reset": {
                "path": "/gltf/meshes/0/primitives/0/extensions/KHR_draco_mesh_compression"
            }
        }
        )"_json;
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
        EXPECT_FALSE(resource.state["gltf"]["meshes"][0]["primitives"][0].contains("extensions"));
    }
}

TEST_F(OverloadOperationsTests, PrimitiveOperationWithMesh)
{
    ScenarioResource resource = MultiPrimitive;
    auto taskDescription = nlohmann::json({ { "reduce_triangle_count", nlohmann::json::object() } });
    taskDescription["reduce_triangle_count"]["path"] = "/gltf/meshes/0";
    taskDescription["reduce_triangle_count"]["targetSize"] = 0.9f;

    uint8_t indexAccessor0 = resource.state.at("/gltf/meshes/0/primitives/0/indices"_json_pointer);
    uint8_t indexAccessor1 = resource.state.at("/gltf/meshes/0/primitives/1/indices"_json_pointer);
    uint8_t indexAccessor2 = resource.state.at("/gltf/meshes/0/primitives/2/indices"_json_pointer);

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    EXPECT_TRUE(indexAccessor0 != resource.state.at("/gltf/meshes/0/primitives/0/indices"_json_pointer).get<std::uint8_t>());
    EXPECT_TRUE(indexAccessor1 != resource.state.at("/gltf/meshes/0/primitives/1/indices"_json_pointer).get<std::uint8_t>());
    EXPECT_TRUE(indexAccessor1 != resource.state.at("/gltf/meshes/0/primitives/2/indices"_json_pointer).get<std::uint8_t>());
}

TEST_F(OverloadOperationsTests, setImageURI)
{
    ScenarioResource resource = Avocado;

    auto taskDescription = nlohmann::json({ { "set_gltf_value", nlohmann::json::object() } });
    taskDescription["set_gltf_value"]["path"] = "/gltf/images/0/uri";
    taskDescription["set_gltf_value"]["value"] = "thisdoesnotexist.png";
    EXPECT_NO_THROW(resource.state = g_library.call(taskDescription, std::move(resource.state), resource.dataStorage));
    EXPECT_TRUE(resource.state.contains("/files/thisdoesnotexist.png"_json_pointer));
    EXPECT_TRUE(resource.state.at("/gltf/images/0/uri"_json_pointer).get<std::string>() == "thisdoesnotexist.png");

    auto taskDescription2 = nlohmann::json({ { "convert_storage_type", nlohmann::json::object() } });
    taskDescription2["convert_storage_type"]["path"] = "/gltf/images/0";
    taskDescription2["convert_storage_type"]["storageType"] = "Embedded";
    EXPECT_NO_THROW(resource.state = g_library.call(taskDescription2, std::move(resource.state), resource.dataStorage));
    EXPECT_ANY_THROW(resource.state = g_library.call(taskDescription, std::move(resource.state), resource.dataStorage));
    taskDescription2["convert_storage_type"]["storageType"] = "BufferView";
    EXPECT_NO_THROW(resource.state = g_library.call(taskDescription2, std::move(resource.state), resource.dataStorage));
    EXPECT_ANY_THROW(resource.state = g_library.call(taskDescription, std::move(resource.state), resource.dataStorage));
}
