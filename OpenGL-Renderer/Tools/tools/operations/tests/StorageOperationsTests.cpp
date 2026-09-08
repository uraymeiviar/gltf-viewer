
#include "gtest/gtest.h"

#include "Gltfs.h"
#include "Helpers.h"

#include "operations/Library.h"
#include "reactor/Utils.h"

using namespace ux3d;
using namespace ux3d::operations;

class StorageOperationsTests : public OperationsTests {
};

TEST_F(StorageOperationsTests, convertAllImagesFromEmbeddedToFile)
{
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/images";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "File";

    ScenarioResource avocado = loadResource("/Avocado/StorageTypeEmbedded/Avocado.gltf");
    EXPECT_NO_THROW(avocado.state = g_library.call(std::move(taskDescription), std::move(avocado.state), avocado.dataStorage));

    for (uint32_t i = 0; i < avocado.state["gltf"]["images"].size(); i++) {

        EXPECT_TRUE(avocado.state["gltf"]["images"][i].contains("uri"));
        EXPECT_FALSE(reactor::utils::isDataUri(avocado.state["gltf"]["images"][i].at("uri")));
        // No bufferView entry
        EXPECT_FALSE(avocado.state["gltf"]["images"][i].contains("bufferView"));
    }
}

TEST_F(StorageOperationsTests, convertAllImagesFromBufferViewToFile)
{
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/images";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "File";

    EXPECT_NO_THROW(AvocadoGlb.state = g_library.call(std::move(taskDescription), std::move(AvocadoGlb.state), AvocadoGlb.dataStorage));

    for (uint32_t i = 0; i < AvocadoGlb.state["gltf"]["images"].size(); i++) {

        EXPECT_TRUE(AvocadoGlb.state["gltf"]["images"][i].contains("uri"));
        // No bufferView entry
        EXPECT_FALSE(Avocado.state["gltf"]["images"][i].contains("bufferView"));
    }
}

TEST_F(StorageOperationsTests, convertAllImagesFromFileToEmbedded)
{
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/images";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "Embedded";

    ScenarioResource groundTruth = loadResource("/Avocado/StorageTypeEmbedded/Avocado.gltf");

    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));

    EXPECT_TRUE(Avocado.state["gltf"]["images"].size() == groundTruth.state["gltf"]["images"].size());

    for (uint32_t i = 0; i < Avocado.state["gltf"]["images"].size(); i++) {

        EXPECT_TRUE(Avocado.state["gltf"]["images"][i].contains("uri"));
        // Equal embedded string
        auto convertedUri = Avocado.state["gltf"]["images"][i]["uri"].get<std::string>();
        auto groundTruthUri = groundTruth.state["gltf"]["images"][i]["uri"].get<std::string>();

        EXPECT_TRUE(groundTruthUri.compare(convertedUri) == 0);

        // No bufferView entry
        EXPECT_FALSE(Avocado.state["gltf"]["images"][i].contains("bufferView"));
    }
}

TEST_F(StorageOperationsTests, convertAllImagesFromBufferViewToEmbedded)
{
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/images";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "Embedded";

    ScenarioResource groundTruth = loadResource("/Avocado/StorageTypeEmbedded/Avocado.gltf");

    EXPECT_NO_THROW(AvocadoGlb.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));

    EXPECT_TRUE(AvocadoGlb.state["gltf"]["images"].size() == groundTruth.state["gltf"]["images"].size());

    for (uint32_t i = 0; i < AvocadoGlb.state["gltf"]["images"].size(); i++) {

        EXPECT_TRUE(AvocadoGlb.state["gltf"]["images"][i].contains("uri"));
        // Equal embedded string
        auto convertedUri = AvocadoGlb.state["gltf"]["images"][i]["uri"].get<std::string>();
        auto groundTruthUri = groundTruth.state["gltf"]["images"][i]["uri"].get<std::string>();

        EXPECT_TRUE(groundTruthUri.compare(convertedUri) == 0);

        // No bufferView entry
        EXPECT_FALSE(AvocadoGlb.state["gltf"]["images"][i].contains("bufferView"));
    }
}

TEST_F(StorageOperationsTests, convertAllImagesFromFileToBufferView)
{
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/images";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "BufferView";

    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));

    for (auto& image : Avocado.state["gltf"]["images"]) {
        EXPECT_TRUE(image.contains("bufferView"));
        EXPECT_TRUE(image.contains("mimeType"));

        // No uri entry
        EXPECT_FALSE(image.contains("uri"));
    }
}

TEST_F(StorageOperationsTests, convertAllImagesFromEmbeddedToBufferView)
{
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/images";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "BufferView";

    ScenarioResource avocado = loadResource("/Avocado/StorageTypeEmbedded/Avocado.gltf");
    EXPECT_NO_THROW(avocado.state = g_library.call(std::move(taskDescription), std::move(avocado.state), avocado.dataStorage));

    for (auto& image : avocado.state["gltf"]["images"]) {
        EXPECT_TRUE(image.contains("bufferView"));
        EXPECT_TRUE(image.contains("mimeType"));

        // No uri entry
        EXPECT_FALSE(image.contains("uri"));
    }
}

TEST_F(StorageOperationsTests, convertAllBuffersToEmbedded)
{
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/buffers";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "Embedded";

    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));

    for (auto& buffer : Avocado.state["gltf"]["buffers"]) {
        EXPECT_TRUE(buffer.contains("uri"));
        EXPECT_TRUE(reactor::utils::isHashedDataUri(buffer.at("uri")));
    }
}

TEST_F(StorageOperationsTests, convertAllBuffersToFile)
{
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/buffers";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "File";

    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));

    for (auto& buffer : Avocado.state["gltf"]["buffers"]) {
        EXPECT_TRUE(buffer.contains("uri"));
    }
}

TEST_F(StorageOperationsTests, convertImageStorageSize)
{
    auto resource = AvocadoGlb;

    auto bufferSize = resource.state["gltf"]["buffers"][0]["byteLength"].get<uint32_t>();

    {
        std::string op = "convert_storage_type_for_each";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = "/gltf/images";
        taskDescription[op]["storageType"] = "File";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    auto bufferSizeAfter = resource.state["gltf"]["buffers"][0]["byteLength"].get<uint32_t>();

    EXPECT_TRUE(bufferSize > bufferSizeAfter);
}