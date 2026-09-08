
#include "gtest/gtest.h"

#include "Gltfs.h"
#include "Helpers.h"
#include "reactor/Utils.h"

#include "operations/Library.h"

using namespace ux3d;
using namespace ux3d::operations;

class FileOperationsTests : public OperationsTests {
};

TEST_F(FileOperationsTests, saveEmptyGLB)
{
    ScenarioResource resource = Avocado;
    {
        auto taskDescription = nlohmann::json({ { "create_empty_gltf", nlohmann::json::object() } });
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

        taskDescription = nlohmann::json({ { "create_scene", nlohmann::json::object() } });
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

        taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.glb";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(FileOperationsTests, loadGlb)
{
    auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
    taskDescription["load_file"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/Avocado/glb/Avocado.glb";

    ScenarioResource resource;
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/accessors/0/bufferView")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/asset/generator")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/asset/version")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/bufferViews/0/buffer")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/bufferViews/0/byteLength")));
    EXPECT_FALSE(resource.state.contains(nlohmann::json::json_pointer("/gltf/buffers/0/uri"))); // TODO
    EXPECT_FALSE(resource.state.contains(nlohmann::json::json_pointer("/gltf/images/0/uri")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/attributes/TEXCOORD_0")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/materials/0/pbrMetallicRoughness/baseColorTexture/index")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/nodes/0/rotation")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/scene")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/scenes/0/nodes")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/textures/0/source")));
}

TEST_F(FileOperationsTests, saveGlb)
{
    {
        ScenarioResource resource = Avocado;
        auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.glb";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    {
        ScenarioResource resource = VC;
        auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.glb";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    {
        ScenarioResource resource = DuckGlb;
        auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.glb";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(FileOperationsTests, saveGlbGarbage)
{
    auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
    taskDescription["save_file"]["filepath"] = "test.glb";

    ScenarioResource resource;
    EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);
}

TEST_F(FileOperationsTests, saveCleanGlb)
{
    ScenarioResource resource = AvocadoGlb;
    uint32_t sizeBefore = resource.state["gltf"]["buffers"][0]["byteLength"].get<uint32_t>();
    { // remove image
        auto taskDescription = nlohmann::json({ { "remove", nlohmann::json::object() } });
        taskDescription["remove"]["path"] = "/gltf/images/1";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    { // save
        auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.glb";
        taskDescription["save_file"]["overwrite"] = true;
        taskDescription["save_file"]["cleanupData"] = true;
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    {
        ScenarioResource resourceLoaded;
        auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescription["load_file"]["filepath"] = "test.glb";

        EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescription), std::move(resourceLoaded.state), resourceLoaded.dataStorage));
        uint32_t sizeAfter = resourceLoaded.state["gltf"]["buffers"][0]["byteLength"].get<uint32_t>();
        EXPECT_LT(sizeAfter, sizeBefore);
    }
}

TEST_F(FileOperationsTests, loadGltf)
{

    ScenarioResource resource;
    auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
    taskDescription["load_file"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/Avocado/gltf/Avocado.gltf";

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/accessors/0/bufferView")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/asset/generator")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/asset/version")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/bufferViews/0/buffer")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/bufferViews/0/byteLength")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/buffers/0/uri")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/images/0/uri")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/attributes/TEXCOORD_0")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/materials/0/pbrMetallicRoughness/baseColorTexture/index")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/nodes/0/rotation")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/scene")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/scenes/0/nodes")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/textures/0/source")));
}

TEST_F(FileOperationsTests, saveGltfGarbage)
{
    ScenarioResource resource;
    auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
    taskDescription["save_file"]["filepath"] = "test.gltf";
    taskDescription["save_file"]["overwrite"] = true;

    EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);
}

TEST_F(FileOperationsTests, loadGltfGarbage)
{
    ScenarioResource resource;
    auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
    taskDescription["load_file"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/Avocado/gltf/Avocado.bin";

    EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::runtime_error);
}

TEST_F(FileOperationsTests, roundtripGltf)
{
    {
        // save Avocado.gltf
        ScenarioResource resource = Avocado;
        auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.gltf";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    {
        // load
        ScenarioResource resource;
        auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescription["load_file"]["filepath"] = "test.gltf";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

        EXPECT_TRUE(equal(resource, Avocado));
    }

    //////// Box With Spaces
    {
        // save Box With Spaces.gltf
        ScenarioResource resource = BoxWithSpaces;
        auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.gltf";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    {
        // load
        ScenarioResource resource;
        auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescription["load_file"]["filepath"] = "test.gltf";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

        EXPECT_TRUE(equal(resource, BoxWithSpaces));
    }

    //////// URI containing subfolders
    {
        ScenarioResource resource = EnvironmentTest;
        auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.gltf";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    {
        // load
        ScenarioResource resource;
        auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescription["load_file"]["filepath"] = "test.gltf";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

        EXPECT_TRUE(equal(resource, EnvironmentTest));
    }

    //////// Glb->gltf
    {
        // save Box With Spaces.gltf
        ScenarioResource resource = AvocadoGlb;
        auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.gltf";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    {
        // load
        ScenarioResource resource;
        auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescription["load_file"]["filepath"] = "test.gltf";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

        EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/buffers/0/uri")));
        EXPECT_TRUE(resource.state.at(nlohmann::json::json_pointer("/gltf/buffers/0/uri")).size() > 0);
    }
}

TEST_F(FileOperationsTests, saveParameters)
{
    auto taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });

    {
        taskDescription["save_file"]["filepath"] = "test123.png";
        taskDescription["save_file"]["overwrite"] = true;
        ScenarioResource resource;
        EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);
    }
    { // overwrite test
        taskDescription["save_file"]["filepath"] = "test123.gltf";
        taskDescription["save_file"]["overwrite"] = true;
        ScenarioResource resource = Avocado;
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
        resource = Avocado;
        taskDescription["save_file"]["overwrite"] = false;
        EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);
    }
    {
        taskDescription["save_file"]["filepath"] = "test123.glb";
        taskDescription["save_file"]["overwrite"] = true;
        ScenarioResource resource = Avocado;
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    {
        taskDescription["save_file"]["filepath"] = "test123.gltf";
        taskDescription["save_file"]["overwrite"] = true;
        ScenarioResource resource = Avocado;
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(FileOperationsTests, saveSelectionCesiumMilkTruck)
{
    // save a selected part of a glTF
    ScenarioResource resourceSaved = CesiumMilkTruck;
    auto taskDescriptionSave = nlohmann::json();
    taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionCesiumMilkTruck.gltf";
    taskDescriptionSave["save_properties"]["overwrite"] = true;
    taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
    taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/nodes/1");
    taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/nodes/3");

    resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage);

    // load again
    ScenarioResource resourceLoaded;
    auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
    taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionCesiumMilkTruck.gltf";

    EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

    // test removing animations
    EXPECT_FALSE(resourceLoaded.state["gltf"].contains("animations"));
    // test keeping nodes
    EXPECT_EQ(resourceLoaded.state["gltf"]["nodes"].size(), 4);
    // test that the default scene has been removed
    EXPECT_FALSE(resourceLoaded.state["gltf"].contains("scene"));
}

TEST_F(FileOperationsTests, saveSelectionOverlappingSubtrees)
{
    // save a selection that contains strongly overlapping subtrees
    ScenarioResource resourceSaved = CesiumMilkTruck;
    auto taskDescriptionSave = nlohmann::json();
    taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionOverlappingSubtrees.glb";
    taskDescriptionSave["save_properties"]["overwrite"] = true;
    taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
    taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/meshes/1");
    taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/nodes/2");
    taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/scenes/0");

    EXPECT_NO_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage));

    // load again
    ScenarioResource resourceLoaded;
    auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
    taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionOverlappingSubtrees.glb";

    EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

    // test keeping multiple overlapping subtrees including scenes, meshes and nodes
    EXPECT_EQ(resourceLoaded.state["gltf"]["scenes"].size(), 1);
    EXPECT_EQ(resourceLoaded.state["gltf"]["meshes"].size(), 2);
    EXPECT_EQ(resourceLoaded.state["gltf"]["nodes"].size(), 5);
    // test that the default scene is still present
    EXPECT_TRUE(resourceLoaded.state["gltf"].contains("scene"));
}

TEST_F(FileOperationsTests, saveSelectionGarbageParameters)
{
    // save a selected part of a glTF
    ScenarioResource resourceSaved = CesiumMilkTruck;
    auto taskDescriptionSave = nlohmann::json();
    taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionGarbageParameter.gltf";
    taskDescriptionSave["save_properties"]["overwrite"] = true;
    taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
    // test handling these properties that do not exist
    taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/scene/3");
    taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/fakeProperty/fakeThing/0");

    EXPECT_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage), std::invalid_argument);
}

TEST_F(FileOperationsTests, saveSelectionAnimation)
{
    // save a selected part of a glTF
    ScenarioResource resourceSaved = CesiumMilkTruck;
    auto taskDescriptionSave = nlohmann::json();
    taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionAnimation.gltf";
    taskDescriptionSave["save_properties"]["overwrite"] = true;
    taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
    taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/animations/0");

    EXPECT_NO_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage));

    // load again
    ScenarioResource resourceLoaded;
    auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
    taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionAnimation.gltf";

    EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

    // test saving explicitly mentioned animations and the required references
    EXPECT_EQ(resourceLoaded.state["gltf"]["animations"].size(), 1);
    EXPECT_EQ(resourceLoaded.state["gltf"]["nodes"].size(), 2);
    EXPECT_EQ(resourceLoaded.state["gltf"]["meshes"].size(), 1);
    EXPECT_EQ(resourceLoaded.state["gltf"]["materials"].size(), 1);
}

TEST_F(FileOperationsTests, saveSelectionMaterialsVariants)
{
    {
        // save a glTF containing materials variants
        ScenarioResource resourceSaved = MaterialsVariantsShoe;
        auto taskDescriptionSave = nlohmann::json();
        taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionMaterialsVariants.gltf";
        taskDescriptionSave["save_properties"]["overwrite"] = true;
        taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/extensions/KHR_materials_variants/variants/0");

        EXPECT_NO_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage));

        // load again
        ScenarioResource resourceLoaded;
        auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionMaterialsVariants.gltf";

        EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

        // test saving explicitly mentioned materials variants
        EXPECT_EQ(resourceLoaded.state["gltf"].size(), 3);
        EXPECT_TRUE(resourceLoaded.state["gltf"].contains("extensions"));
        EXPECT_TRUE(resourceLoaded.state["gltf"].contains("extensionsUsed"));
        EXPECT_TRUE(resourceLoaded.state["gltf"].contains("asset"));
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensionsUsed"].size(), 1);
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensionsUsed"][0], "KHR_materials_variants");
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensions"]["KHR_materials_variants"]["variants"].size(), 1);
    }

    {
        // save a subtree that does not need materials variants
        ScenarioResource resourceSaved = MaterialsVariantsShoe;
        auto taskDescriptionSave = nlohmann::json();
        taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionNoMaterialsVariants.gltf";
        taskDescriptionSave["save_properties"]["overwrite"] = true;
        taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/materials/0");
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/materials/1");
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/materials/2");

        EXPECT_NO_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage));

        // load again
        ScenarioResource resourceLoaded;
        auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionNoMaterialsVariants.gltf";

        EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

        // test that materials variants are gone
        EXPECT_FALSE(resourceLoaded.state["gltf"].contains("extensions"));
        EXPECT_FALSE(resourceLoaded.state["gltf"].contains("extensionsUsed"));
        EXPECT_FALSE(resourceLoaded.state["gltf"].contains("meshes"));
        EXPECT_EQ(resourceLoaded.state["gltf"]["materials"].size(), 3);
        EXPECT_EQ(resourceLoaded.state["gltf"]["textures"].size(), 5);
        EXPECT_EQ(resourceLoaded.state["gltf"]["images"].size(), 5);
        EXPECT_EQ(resourceLoaded.state["gltf"]["samplers"].size(), 3);
    }
}

TEST_F(FileOperationsTests, saveSelectionXMP)
{
    {
        // save a glTF that references xmp in multiple places
        ScenarioResource resourceSaved = CustomXmpModel;
        auto taskDescriptionSave = nlohmann::json();
        taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionXMP.gltf";
        taskDescriptionSave["save_properties"]["overwrite"] = true;
        taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/scenes/0");

        EXPECT_NO_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage));

        // load again
        ScenarioResource resourceLoaded;
        auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionXMP.gltf";

        EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

        // test that xmp is still present
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensionsUsed"].size(), 1);
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensionsUsed"][0], "KHR_xmp_json_ld");
        EXPECT_TRUE(resourceLoaded.state["gltf"]["extensions"].contains("KHR_xmp_json_ld"));
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensions"]["KHR_xmp_json_ld"]["packets"].size(), 1);
        EXPECT_EQ(resourceLoaded.state["gltf"]["scenes"][0]["extensions"]["KHR_xmp_json_ld"]["packet"], 0);
        EXPECT_EQ(resourceLoaded.state["gltf"]["nodes"][1]["extensions"]["KHR_xmp_json_ld"]["packet"], 0);
        EXPECT_EQ(resourceLoaded.state["gltf"]["meshes"][0]["extensions"]["KHR_xmp_json_ld"]["packet"], 0);
        EXPECT_EQ(resourceLoaded.state["gltf"]["materials"][0]["extensions"]["KHR_xmp_json_ld"]["packet"], 0);
        EXPECT_EQ(resourceLoaded.state["gltf"]["asset"]["extensions"]["KHR_xmp_json_ld"]["packet"], 0);
    }

    {
        // save a subtree that does not need xmp
        ScenarioResource resourceSaved = CustomXmpModel;
        auto taskDescriptionSave = nlohmann::json();
        taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionNoXMP.gltf";
        taskDescriptionSave["save_properties"]["overwrite"] = true;
        taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/accessors/0");

        EXPECT_NO_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage));

        // load again
        ScenarioResource resourceLoaded;
        auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionNoXMP.gltf";

        EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

        // test that xmp is not present anymore
        EXPECT_EQ(resourceLoaded.state["gltf"].size(), 4);
        EXPECT_TRUE(resourceLoaded.state["gltf"].contains("asset"));
        EXPECT_TRUE(resourceLoaded.state["gltf"].contains("accessors"));
        EXPECT_TRUE(resourceLoaded.state["gltf"].contains("bufferViews"));
        EXPECT_TRUE(resourceLoaded.state["gltf"].contains("buffers"));
    }
}

TEST_F(FileOperationsTests, saveSelectionLights)
{
    {
        // save a glTF that has a light
        ScenarioResource resourceSaved = GlamVelvetSofa;
        auto taskDescriptionSave = nlohmann::json();
        taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionLight.gltf";
        taskDescriptionSave["save_properties"]["overwrite"] = true;
        taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/scenes/0");

        EXPECT_NO_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage));

        // load again
        ScenarioResource resourceLoaded;
        auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionLight.gltf";

        EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

        // test that the light is still present
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensionsUsed"].size(), 5);
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensionsUsed"][0], "KHR_lights_punctual");
        EXPECT_TRUE(resourceLoaded.state["gltf"]["extensions"].contains("KHR_lights_punctual"));
        EXPECT_EQ(resourceLoaded.state["gltf"]["extensions"]["KHR_lights_punctual"]["lights"].size(), 1);
        EXPECT_EQ(resourceLoaded.state["gltf"]["nodes"][3]["extensions"]["KHR_lights_punctual"]["light"], 0);
    }

    {
        // save a subtree that does not need the light
        ScenarioResource resourceSaved = GlamVelvetSofa;
        auto taskDescriptionSave = nlohmann::json();
        taskDescriptionSave["save_properties"]["filepath"] = "saveSelectionNoLight.gltf";
        taskDescriptionSave["save_properties"]["overwrite"] = true;
        taskDescriptionSave["save_properties"]["paths"] = nlohmann::json::array();
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/nodes/0");
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/nodes/1");
        taskDescriptionSave["save_properties"]["paths"].push_back("/gltf/nodes/2");

        EXPECT_NO_THROW(resourceSaved.state = g_library.call(std::move(taskDescriptionSave), std::move(resourceSaved.state), resourceSaved.dataStorage));

        // load again
        ScenarioResource resourceLoaded;
        auto taskDescriptionLoad = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescriptionLoad["load_file"]["filepath"] = "saveSelectionNoLight.gltf";

        EXPECT_NO_THROW(resourceLoaded.state = g_library.call(std::move(taskDescriptionLoad), std::move(resourceLoaded.state), resourceLoaded.dataStorage));

        // test that the light is not present anymore
        EXPECT_EQ(resourceLoaded.state["gltf"]["nodes"].size(), 3);
        EXPECT_FALSE(resourceLoaded.state["gltf"]["nodes"][0].contains("extensions"));
        EXPECT_FALSE(resourceLoaded.state["gltf"]["nodes"][1].contains("extensions"));
        EXPECT_FALSE(resourceLoaded.state["gltf"]["nodes"][2].contains("extensions"));
        EXPECT_FALSE(resourceLoaded.state["gltf"]["extensions"].contains("KHR_lights_punctual"));
        EXPECT_FALSE(resourceLoaded.state["gltf"]["extensionsUsed"].contains("KHR_lights_punctual"));
    }
}

TEST_F(FileOperationsTests, exportAccessor)
{
    ScenarioResource resource = Avocado;
    auto taskDescription = nlohmann::json({ { "export_accessor", nlohmann::json::object() } });
    taskDescription["export_accessor"]["path"] = "/gltf/accessors/0";
    taskDescription["export_accessor"]["filepath"] = "accessor.txt";

    EXPECT_NO_THROW(g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
}

TEST_F(FileOperationsTests, importImageJpeg)
{
    ScenarioResource resource = Fox;
    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();

    auto taskDescription = nlohmann::json({ { "import_image", nlohmann::json::object() } });
    taskDescription["import_image"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/Avocado/gltf/Avocado_baseColor.jpg";

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore, imageCountAfterwards - 1);
}

TEST_F(FileOperationsTests, importImagePng)
{
    // Import an image into a file that has no images
    // Import path contains spaces
    // Image is of type png
    ScenarioResource resource = BoxInterleaved;
    EXPECT_FALSE(resource.state["gltf"].contains("images"));

    auto taskDescription = nlohmann::json({ { "import_image", nlohmann::json::object() } });
    taskDescription["import_image"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + "/Box With Spaces/glTF/Roughness Metallic.png";

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(1, imageCountAfterwards);

    EXPECT_TRUE(resource.state["gltf"]["images"][0].contains("uri"));
    EXPECT_EQ(resource.state["gltf"]["images"][0]["uri"].get<std::string>(), "Roughness Metallic.png");
}

TEST_F(FileOperationsTests, exportUsdz) {
#ifdef UX3D_IS_WINDOWS
    { ScenarioResource resource = Avocado;
auto taskDescription = nlohmann::json({ { "export_usdz", nlohmann::json::object() } });
taskDescription["export_usdz"]["filepath"] = "test.usdz";
taskDescription["export_usdz"]["overwrite"] = true;

EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
}
#endif
}

TEST_F(FileOperationsTests, loadSaveNonexistingImage)
{
    {
        // save Avocado.gltf
        ScenarioResource resource = Avocado;
        // remove image from files

        auto taskDescription = nlohmann::json({ { "set_gltf_value", nlohmann::json::object() } });
        taskDescription["set_gltf_value"]["path"] = "/gltf/images/0/uri";
        taskDescription["set_gltf_value"]["value"] = "thisdoesnotexist.png";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

        nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/images/0"), resource.state);
        resource.state.at(dataIDPath.parent_pointer()).erase(dataIDPath.back());

        taskDescription = nlohmann::json({ { "save_file", nlohmann::json::object() } });
        taskDescription["save_file"]["filepath"] = "test.gltf";
        taskDescription["save_file"]["overwrite"] = true;

        EXPECT_ANY_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
        // load
        taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescription["load_file"]["filepath"] = "test.gltf";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
        EXPECT_FALSE(resource.state.contains(dataIDPath));
    }
}

TEST_F(FileOperationsTests, exportImage)
{

    ScenarioResource resource = AvocadoGlb;
    {
        auto params = nlohmann::json::object();
        params["filepath"] = "img.jpg";
        params["path"] = "/gltf/images/0";
        auto taskDescription = nlohmann::json({ { "export_image", params } });
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}
