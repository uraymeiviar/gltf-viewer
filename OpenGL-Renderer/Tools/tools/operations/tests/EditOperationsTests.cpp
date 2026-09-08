
#include "gtest/gtest.h"

#include "Gltfs.h"
#include "Helpers.h"
#include "reactor/Utils.h"

#include "operations/Library.h"

#include <string>

using namespace ux3d;
using namespace ux3d::operations;

class EditOperationsTests : public OperationsTests {
};

TEST_F(EditOperationsTests, mergeGltf)
{
    auto taskDescription = nlohmann::json({ { "merge_gltf", nlohmann::json::object() } });
    taskDescription["merge_gltf"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/GlamVelvetSofa/glTF/GlamVelvetSofa.gltf";

    ScenarioResource resource = Fox;
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t foxNodeCount = Fox.state.at(nlohmann::json::json_pointer("/gltf/nodes")).size();

    // child of root properties
    auto accessorPath = nlohmann::json::json_pointer("/gltf/accessors");
    uint32_t accessorCount = Fox.state.at(accessorPath).size() + GlamVelvetSofa.state.at(accessorPath).size();
    EXPECT_EQ(resource.state.at(accessorPath).size(), accessorCount);

    // extensions
    auto lightPath = nlohmann::json::json_pointer("/gltf/extensions/KHR_lights_punctual/lights");
    uint32_t lightCount = 0 + GlamVelvetSofa.state.at(lightPath).size(); // Fox sample asset has no lights
    EXPECT_EQ(resource.state.at(lightPath).size(), lightCount);

    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/scenes")).size(), 2);

    // references
    auto sceneNodesPath = nlohmann::json::json_pointer("/gltf/scenes/1/nodes");
    EXPECT_EQ(resource.state.at(sceneNodesPath / 0).get<uint64_t>(), foxNodeCount + 0);
    EXPECT_EQ(resource.state.at(sceneNodesPath / 1).get<uint64_t>(), foxNodeCount + 1);
    EXPECT_EQ(resource.state.at(sceneNodesPath / 2).get<uint64_t>(), foxNodeCount + 2);
    EXPECT_EQ(resource.state.at(sceneNodesPath / 3).get<uint64_t>(), foxNodeCount + 3);

    auto sofaMaterialPath = nlohmann::json::json_pointer("/gltf/materials/1");
    EXPECT_EQ(resource.state.at(sofaMaterialPath / "occlusionTexture" / "index").get<uint64_t>(), 1);

    // data
    EXPECT_EQ(resource.state.at("files").size(), 5);
}

TEST_F(EditOperationsTests, mergeGlb)
{
    ScenarioResource resource = FoxGlb;

    auto taskDescription = nlohmann::json({ { "merge_gltf", nlohmann::json::object() } });
    taskDescription["merge_gltf"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/Avocado/glb/Avocado.glb";
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    EXPECT_FALSE(resource.state["gltf"]["buffers"][0].contains("uri"));
    EXPECT_TRUE(resource.state["gltf"]["buffers"][1].contains("uri"));
}

TEST_F(EditOperationsTests, mergeBufferAlignment)
{
    ScenarioResource resource = BarramundiFish;
    {
        auto taskDescription = nlohmann::json({ { "merge_gltf", nlohmann::json::object() } });
        taskDescription["merge_gltf"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/Avocado/glb/Avocado.glb";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    // Merge all buffers into one big buffer
    if (resource.state["gltf"]["buffers"].size() > 1) {
        auto taskDescription = nlohmann::json({ { "merge_buffers", nlohmann::json::object() } });
        taskDescription["merge_buffers"]["bufferPaths"] = nlohmann::json::array();
        // Add all buffers
        for (int i = 0; i < resource.state["gltf"]["buffers"].size(); i++) {
            taskDescription["merge_buffers"]["bufferPaths"].push_back("/gltf/buffers/" + std::to_string(i));
        }
        resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage);
    }

    for (auto& accessor : resource.state["gltf"]["accessors"]) {
        auto bufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews") / accessor["bufferView"].get<uint64_t>();
        auto& bufferView = resource.state.at(bufferViewPath);
        uint64_t totalOffset = 0;
        if (bufferView.contains("byteOffset")) {
            totalOffset += bufferView["byteOffset"].get<uint64_t>();
        }
        if (accessor.contains("byteOffset")) {
            totalOffset += accessor["byteOffset"].get<uint64_t>();
        }
        EXPECT_EQ(totalOffset % 4, 0);
    }
}

TEST_F(EditOperationsTests, importAsset)
{
    ScenarioResource resource = FoxGlb;

    auto taskDescription = nlohmann::json({ { "import_asset", nlohmann::json::object() } });
    taskDescription["import_asset"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/Lemon/fbx/Lemon.fbx";
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/images")).size(), 3);
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/images/1/uri")).get<std::string>(), "Textures/Lemon_Base_color.png");
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/1/name")).get<std::string>(), "Lemon");
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/1/pbrMetallicRoughness/baseColorTexture/index")).get<uint64_t>(), 1);
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/meshes/1/name")).get<std::string>(), "citrus.005");
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/meshes/1/primitives/0/attributes/POSITION")).get<uint64_t>(), 71);
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/scenes/1/nodes/0")).get<uint64_t>(), 26);
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/textures")).size(), 3);
}

TEST_F(EditOperationsTests, blendImages)
{
    ScenarioResource resource = Avocado;
    { // Import additional image
        auto taskDescription = nlohmann::json({ { "import_image", nlohmann::json::object() } });
        taskDescription["import_image"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/GlamVelvetSofa/glTF/GlamVelvetSofa_occlusion.png";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    auto taskDescription = nlohmann::json({ { "blend_images", nlohmann::json::object() } });
    taskDescription["blend_images"]["imageA"] = "/gltf/images/0";
    taskDescription["blend_images"]["imageB"] = "/gltf/images/3";
    taskDescription["blend_images"]["weight"] = 0.7;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore + 1, imageCountAfterwards);
}

TEST_F(EditOperationsTests, applyNodeTransform)
{
    ScenarioResource resource = Avocado;

    uint32_t bufferViewCountBefore = resource.state["/gltf/bufferViews"_json_pointer].size();
    uint32_t bufferCountBefore = resource.state["/gltf/buffers"_json_pointer].size();

    auto taskDescription = nlohmann::json({ { "apply_node_transform", nlohmann::json::object() } });
    taskDescription["apply_node_transform"]["path"] = "/gltf/nodes/0";

    resource.state["/gltf/nodes/0/translation"_json_pointer] = { 1.0, 1.0, 1.0 };

    resource.state["/gltf/nodes/0"_json_pointer].erase("rotation");
    std::string path = "/gltf/accessors/" + std::to_string(resource.state.at("/gltf/meshes/0/primitives/0/attributes/POSITION"_json_pointer).get<int>());
    nlohmann::json::json_pointer accessorPath(path);
    auto vecData = reactor::utils::getAccessorData<glm::vec3>(accessorPath, resource.state, resource.dataStorage);

    ASSERT_NO_THROW(resource.state = g_library.call(taskDescription, std::move(resource.state), resource.dataStorage));

    auto newVecData = reactor::utils::getAccessorData<glm::vec3>(accessorPath, resource.state, resource.dataStorage);

    for (int i = 0; i < vecData.size(); ++i) {
        EXPECT_EQ(vecData[i].x + 1, newVecData[i].x);
        EXPECT_EQ(vecData[i].y + 1, newVecData[i].y);
        EXPECT_EQ(vecData[i].z + 1, newVecData[i].z);
    }

    uint32_t bufferViewCountAfter = resource.state["/gltf/bufferViews"_json_pointer].size();
    uint32_t bufferCountAfter = resource.state["/gltf/buffers"_json_pointer].size();

    EXPECT_EQ(bufferViewCountBefore, bufferViewCountAfter);
    EXPECT_EQ(bufferCountAfter, bufferCountBefore);
}
