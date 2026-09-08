
#include "gtest/gtest.h"

#include "Gltfs.h"
#include "Helpers.h"
#include "operations/Library.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"

#include <array>
#include <nlohmann/json.hpp>

using namespace ux3d;
using namespace ux3d::operations;

class OptimizationOperationsTests : public OperationsTests {
};

TEST_F(OptimizationOperationsTests, applyDracoMeshCompressionDefaultParameters)
{
    auto taskDescription = nlohmann::json({ { "apply_draco", nlohmann::json::object() } });
    taskDescription["apply_draco"]["path"] = "/gltf/meshes/0/primitives/0";

    ScenarioResource resource = AvocadoGlb;
    size_t bufferCountBefore = resource.state.at(nlohmann::json::json_pointer("/gltf/buffers")).size();
    size_t sizeBefore = 0;
    for (auto buffer : resource.state.at(nlohmann::json::json_pointer("/gltf/buffers"))) {
        sizeBefore += buffer.at("byteLength").get<uint32_t>();
    }

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    size_t bufferCountAfter = resource.state.at(nlohmann::json::json_pointer("/gltf/buffers")).size();
    size_t sizeAfter = 0;
    for (auto buffer : resource.state.at(nlohmann::json::json_pointer("/gltf/buffers"))) {
        sizeAfter += buffer.at("byteLength").get<uint32_t>();
    }
    EXPECT_TRUE(sizeAfter < sizeBefore);
    EXPECT_EQ(bufferCountBefore, bufferCountAfter);
}

TEST_F(OptimizationOperationsTests, applyDracoMeshCompressionInterleaved)
{
    ScenarioResource resource = CesiumMilkTruck;

    auto taskDescription = nlohmann::json({ { "apply_draco", nlohmann::json::object() } });

    taskDescription["apply_draco"]["path"] = "/gltf/meshes/0/primitives/2";
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    taskDescription["apply_draco"]["path"] = "/gltf/meshes/0/primitives/1";
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    taskDescription["apply_draco"]["path"] = "/gltf/meshes/1/primitives/0";
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    taskDescription["apply_draco"]["path"] = "/gltf/meshes/0/primitives/0";
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
}

TEST_F(OptimizationOperationsTests, applyDracoMeshCompressionProperties)
{
    {
        auto compressionSettings = nlohmann::json::object();
        compressionSettings["sequentialEncoding"] = false;
        compressionSettings["targetCompressionLevel"] = 5;
        compressionSettings["quantizationBitsPosition"] = 14;
        compressionSettings["quantizationBitsNormal"] = 10;
        compressionSettings["quantizationBitsUv"] = 12;
        compressionSettings["quantizationBitsColor"] = 8;
        compressionSettings["quantizationBitsGeneric"] = 12;

        auto taskDescription = nlohmann::json({ { "apply_draco", nlohmann::json::object() } });
        taskDescription["apply_draco"] = compressionSettings;
        taskDescription["apply_draco"]["path"] = "/gltf/meshes/0/primitives/0";
        Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage);
    }

    EXPECT_TRUE(Avocado.state["gltf"]["meshes"][0]["primitives"][0].contains("extensions"));
    EXPECT_TRUE(Avocado.state["gltf"]["meshes"][0]["primitives"][0]["extensions"].contains("KHR_draco_mesh_compression"));
    auto dracoMeshCompression = Avocado.state["gltf"]["meshes"][0]["primitives"][0]["extensions"].at("KHR_draco_mesh_compression");
    EXPECT_TRUE(dracoMeshCompression.contains("attributes"));
    EXPECT_TRUE(dracoMeshCompression["attributes"].contains("NORMAL"));
    EXPECT_TRUE(dracoMeshCompression["attributes"].contains("POSITION"));
    EXPECT_TRUE(dracoMeshCompression["attributes"].contains("TANGENT"));
    EXPECT_TRUE(dracoMeshCompression["attributes"].contains("TEXCOORD_0"));
    EXPECT_TRUE(dracoMeshCompression.contains("bufferView"));

    EXPECT_TRUE(Avocado.state["gltf"].contains("extensionsRequired"));
    EXPECT_TRUE(Avocado.state["gltf"].contains("extensionsUsed"));

    auto containsValue = [&](const nlohmann::json& jsonArray, const std::string value) -> bool {
        for (const nlohmann::json& elm : jsonArray) {
            std::string testValue = elm.get<std::string>();
            if (testValue == value) {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(containsValue(Avocado.state["gltf"].at("extensionsUsed"), "KHR_draco_mesh_compression"));
    EXPECT_TRUE(containsValue(Avocado.state["gltf"].at("extensionsRequired"), "KHR_draco_mesh_compression"));
}

TEST_F(OptimizationOperationsTests, applyDracoMeshCompressionStride)
{
    ScenarioResource resource = BoxInterleaved;
    {
        auto taskDescription = nlohmann::json({ { "apply_draco", nlohmann::json::object() } });
        taskDescription["apply_draco"]["path"] = "/gltf/meshes/0/primitives/0";
        resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage);
    }
    for (auto& buffer : resource.state["gltf"]["buffers"]) {
        EXPECT_TRUE(buffer.contains("byteLength"));
        EXPECT_TRUE(buffer.at("byteLength").get<uint32_t>() > 0);
    }
}

TEST_F(OptimizationOperationsTests, applyDracoMeshCompressionBinaryData)
{
    auto compressionSettings = nlohmann::json::object();
    compressionSettings["sequentialEncoding"] = false;
    compressionSettings["targetCompressionLevel"] = 5;
    compressionSettings["quantizationBitsPosition"] = 14;
    compressionSettings["quantizationBitsNormal"] = 10;
    compressionSettings["quantizationBitsUv"] = 12;
    compressionSettings["quantizationBitsColor"] = 8;
    compressionSettings["quantizationBitsGeneric"] = 12;

    auto taskDescription = nlohmann::json({ { "apply_draco", nlohmann::json::object() } });
    taskDescription["apply_draco"] = compressionSettings;
    taskDescription["apply_draco"]["path"] = "/gltf/meshes/0/primitives/0";
    Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage);

    ScenarioResource& groundTruth = AvocadoDraco;
    auto dracoBufferViewPath = nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/extensions/KHR_draco_mesh_compression/bufferView");
    EXPECT_TRUE(Avocado.state.contains(dracoBufferViewPath));
    uint32_t groundTruthIndex = groundTruth.state.at(dracoBufferViewPath).get<uint32_t>();
    uint32_t testIndex = Avocado.state.at(dracoBufferViewPath).get<uint32_t>();

    auto bufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews");
    auto groundTruthData = reactor::utils::getBufferViewData(bufferViewPath / groundTruthIndex, groundTruth.state, groundTruth.dataStorage);
    auto testData = reactor::utils::getBufferViewData(bufferViewPath / testIndex, Avocado.state, Avocado.dataStorage);

    EXPECT_EQ(testData.size(), groundTruthData.size());
    bool dataIsEqual = true;
    if (testData.size() == groundTruthData.size()) {
        for (uint32_t i = 0; i < testData.size(); i++) {
            if (testData[i] != groundTruthData[i]) {
                dataIsEqual = false;
            }
        }
    }
    EXPECT_TRUE(dataIsEqual);
}

TEST_F(OptimizationOperationsTests, applyDracoMeshCompressionKeepUncompressedData)
{
    auto params = nlohmann::json::object();
    params["keepUncompressedData"] = true;
    params["path"] = "/gltf/meshes/0/primitives/0";
    auto taskDescription = nlohmann::json({ { "apply_draco", params } });
    Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage);

    EXPECT_TRUE(Avocado.state["gltf"]["meshes"][0]["primitives"][0]["extensions"].contains("KHR_draco_mesh_compression"));

    auto path = nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/attributes/POSITION");
    EXPECT_TRUE(Avocado.state.contains(path));
    uint32_t positionAccessorID = Avocado.state.at(path).get<uint32_t>();
    auto positionAccessor = Avocado.state.at(nlohmann::json::json_pointer("/gltf/accessors") / positionAccessorID);

    EXPECT_TRUE(positionAccessor.contains("max"));
    EXPECT_TRUE(positionAccessor.contains("min"));
    EXPECT_TRUE(positionAccessor.contains("bufferView"));
}

TEST_F(OptimizationOperationsTests, applyDracoMeshCompressionForEach)
{
    {
        auto compressionSettings = nlohmann::json::object();
        compressionSettings["sequentialEncoding"] = false;
        compressionSettings["targetCompressionLevel"] = 5;
        compressionSettings["quantizationBitsPosition"] = 14;
        compressionSettings["quantizationBitsNormal"] = 10;
        compressionSettings["quantizationBitsUv"] = 12;
        compressionSettings["quantizationBitsColor"] = 8;
        compressionSettings["quantizationBitsGeneric"] = 12;

        auto taskDescription = nlohmann::json({ { "apply_draco_for_each", nlohmann::json::object() } });
        taskDescription["apply_draco_for_each"] = compressionSettings;
        EXPECT_NO_THROW(CesiumMilkTruck.state = g_library.call(std::move(taskDescription), std::move(CesiumMilkTruck.state), CesiumMilkTruck.dataStorage));

        EXPECT_TRUE(CesiumMilkTruck.state["gltf"]["meshes"][0]["primitives"][0].contains("extensions"));
        EXPECT_TRUE(CesiumMilkTruck.state["gltf"]["meshes"][0]["primitives"][0]["extensions"].contains("KHR_draco_mesh_compression"));

        EXPECT_TRUE(CesiumMilkTruck.state["gltf"]["meshes"][1]["primitives"][0].contains("extensions"));
        EXPECT_TRUE(CesiumMilkTruck.state["gltf"]["meshes"][1]["primitives"][0]["extensions"].contains("KHR_draco_mesh_compression"));

        EXPECT_TRUE(CesiumMilkTruck.state["gltf"]["meshes"][0]["primitives"][1].contains("extensions"));
        EXPECT_TRUE(CesiumMilkTruck.state["gltf"]["meshes"][0]["primitives"][1]["extensions"].contains("KHR_draco_mesh_compression"));
    }
    {
        auto taskDescription = nlohmann::json({ { "apply_draco_for_each", nlohmann::json::object() } });
        EXPECT_NO_THROW(AlphaBlendModeTest.state = g_library.call(std::move(taskDescription), std::move(AlphaBlendModeTest.state), AlphaBlendModeTest.dataStorage));
        EXPECT_FALSE(AlphaBlendModeTest.state["gltf"]["accessors"][0].contains("byteOffset"));
    }
}

TEST_F(OptimizationOperationsTests, applyDracoMeshCompressionSharedAccessor)
{
    // This asset contains several primitives that share one accessor
    auto taskDescription = nlohmann::json({ { "apply_draco_for_each", nlohmann::json::object() } });
    EXPECT_NO_THROW(AttenuationTest.state = g_library.call(std::move(taskDescription), std::move(AttenuationTest.state), AttenuationTest.dataStorage));
    EXPECT_TRUE(AttenuationTest.state["gltf"]["accessors"].size() == 57);

    EXPECT_TRUE(AttenuationTest.state["gltf"]["meshes"][1]["primitives"][0].contains("indices"));
    EXPECT_TRUE(AttenuationTest.state["gltf"]["meshes"][1]["primitives"][0]["indices"] != 2);

    EXPECT_TRUE(AttenuationTest.state["gltf"]["meshes"][8]["primitives"][0]["attributes"]["POSITION"] != 0);
    EXPECT_TRUE(AttenuationTest.state["gltf"]["meshes"][8]["primitives"][0]["attributes"]["NORMAL"] != 1);

    EXPECT_TRUE(AttenuationTest.state["gltf"]["meshes"][9]["primitives"][0]["attributes"]["POSITION"] != 0);
    EXPECT_TRUE(AttenuationTest.state["gltf"]["meshes"][9]["primitives"][0]["attributes"]["NORMAL"] != 1);
}

TEST_F(OptimizationOperationsTests, applyResolveDracoMeshCompressionStride)
{
    ScenarioResource resource = BoxInterleaved;
    {
        auto taskDescription = nlohmann::json({ { "apply_draco", nlohmann::json::object() } });
        taskDescription["apply_draco"]["path"] = "/gltf/meshes/0/primitives/0";
        resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage);
    }
    {
        auto taskDescription = nlohmann::json({ { "resolve_draco", nlohmann::json::object() } });
        taskDescription["resolve_draco"]["path"] = "/gltf/meshes/0/primitives/0";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(OptimizationOperationsTests, resolveDracoMeshCompressionNoThrow)
{
    auto taskDescription = nlohmann::json({ { "resolve_draco", nlohmann::json::object() } });
    taskDescription["resolve_draco"]["path"] = "/gltf/meshes/0/primitives/0";

    EXPECT_NO_THROW(AvocadoDraco.state = g_library.call(std::move(taskDescription), std::move(AvocadoDraco.state), AvocadoDraco.dataStorage));
}

TEST_F(OptimizationOperationsTests, resolveDracoMeshCompressionProperties)
{
    {
        auto taskDescription = nlohmann::json({ { "resolve_draco", nlohmann::json::object() } });
        taskDescription["resolve_draco"]["path"] = "/gltf/meshes/0/primitives/0";

        EXPECT_NO_THROW(AvocadoDraco.state = g_library.call(std::move(taskDescription), std::move(AvocadoDraco.state), AvocadoDraco.dataStorage));
    }

    auto dracoPath = nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/extensions/KHR_draco_mesh_compression");
    EXPECT_FALSE(AvocadoDraco.state.contains(dracoPath));

    auto& testData = AvocadoDraco.state.at(nlohmann::json::json_pointer("/gltf/bufferViews"));
    auto& groundTruthData = Avocado.state.at(nlohmann::json::json_pointer("/gltf/bufferViews"));

    EXPECT_EQ(testData.size(), groundTruthData.size());
    uint32_t testSum = 0;
    uint32_t groundTruthSum = 0;

    for (uint16_t i = 0; i < testData.size(); i++) {
        testSum += testData[i].at("byteLength").get<uint32_t>();
        groundTruthSum += groundTruthData[i].at("byteLength").get<uint32_t>();
    }

    EXPECT_TRUE(testSum == groundTruthSum);

    auto path = nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0/attributes/POSITION");
    EXPECT_TRUE(AvocadoDraco.state.contains(path));
    uint32_t positionAccessorID = AvocadoDraco.state.at(path).get<uint32_t>();
    auto positionAccessor = AvocadoDraco.state.at(nlohmann::json::json_pointer("/gltf/accessors") / positionAccessorID);
    EXPECT_TRUE(positionAccessor.contains("max"));
    EXPECT_TRUE(positionAccessor.contains("min"));

    EXPECT_TRUE(positionAccessor.at("max").size() == 3);
    EXPECT_TRUE(positionAccessor.at("min").size() == 3);

    std::vector<double> max {
        0.0212792307138443,
        0.0628480613231659,
        0.013809407129883766
    };

    std::vector<double> min {
        -0.021280910819768906,
        -4.773767068400048e-05,
        -0.01380900014191866
    };

    auto minResult = positionAccessor.at("min").get<std::vector<double>>();
    auto maxResult = positionAccessor.at("max").get<std::vector<double>>();

    for (uint32_t i = 0; i < min.size(); i++) {
        EXPECT_EQ(round(max[i] * 1000.0), round(maxResult[i] * 1000.0));
        EXPECT_EQ(round(min[i] * 1000.0), round(minResult[i] * 1000.0));
    }
}

TEST_F(OptimizationOperationsTests, resolveDracoMeshCompressionForEach)
{
    auto bufferCountBefore = AvocadoDraco.state.at(nlohmann::json::json_pointer("/gltf/buffers")).size();

    auto taskDescription = nlohmann::json({ { "resolve_draco_for_each", nlohmann::json::object() } });

    EXPECT_NO_THROW(AvocadoDraco.state = g_library.call(std::move(taskDescription), std::move(AvocadoDraco.state), AvocadoDraco.dataStorage));

    auto bufferCountAfter = AvocadoDraco.state.at(nlohmann::json::json_pointer("/gltf/buffers")).size();

    EXPECT_EQ(bufferCountBefore, bufferCountAfter);

    auto bufferSizeOriginal = Avocado.state.at(nlohmann::json::json_pointer("/gltf/buffers/0/byteLength")).get<uint32_t>();
    auto bufferSizeDecoded = AvocadoDraco.state.at(nlohmann::json::json_pointer("/gltf/buffers/0/byteLength")).get<uint32_t>();
    EXPECT_EQ(bufferSizeOriginal, bufferSizeDecoded);
}

TEST_F(OptimizationOperationsTests, resolveDracoMeshCompressionRedundantData)
{
    auto path = nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0");
    {
        auto taskDescription = nlohmann::json({ { "resolve_draco", nlohmann::json::object() } });
        taskDescription["resolve_draco"]["path"] = path.to_string();
        taskDescription["resolve_draco"]["deleteEncodedData"] = false;

        EXPECT_NO_THROW(AvocadoDraco.state = g_library.call(std::move(taskDescription), std::move(AvocadoDraco.state), AvocadoDraco.dataStorage));
    }
    // we set the flag "deleteEncodedData" to false so we expect the extension to still exist
    EXPECT_TRUE(AvocadoDraco.state.contains(path / "extensions" / "KHR_draco_mesh_compression"));

    for (auto& mesh : AvocadoDraco.state["gltf"].at("meshes")) {
        for (auto& primitive : mesh.at("primitives")) {
            for (auto& [attributeName, attribute] : primitive.at("attributes").items()) {
                nlohmann::json::json_pointer accessorPath("/gltf/accessors");
                uint32_t idx = attribute.get<uint32_t>();
                auto& accessor = AvocadoDraco.state.at(accessorPath / idx);

                // Expect that all uncompressed bufferViews are still available
                EXPECT_TRUE(accessor.contains("bufferView"));
            }
        }
    }

    {
        auto taskDescription = nlohmann::json({ { "remove_redundant_draco", nlohmann::json::object() } });
        EXPECT_NO_THROW(AvocadoDraco.state = g_library.call(std::move(taskDescription), std::move(AvocadoDraco.state), AvocadoDraco.dataStorage));
    }

    for (auto& mesh : AvocadoDraco.state["gltf"].at("meshes")) {
        for (auto& primitive : mesh.at("primitives")) {
            for (auto& [attributeName, attribute] : primitive.at("attributes").items()) {
                nlohmann::json::json_pointer accessorPath("/gltf/accessors");
                uint32_t idx = attribute.get<uint32_t>();
                auto& accessor = AvocadoDraco.state.at(accessorPath / idx);

                // Expect that all uncompressed bufferViews were deleted
                EXPECT_FALSE(accessor.contains("bufferView"));
            }
        }
    }

    EXPECT_EQ(AvocadoDraco.state.at(nlohmann::json::json_pointer("/gltf/buffers")).size(), 1);
    EXPECT_EQ(AvocadoDraco.state.at(nlohmann::json::json_pointer("/gltf/bufferViews")).size(), 1);
}

TEST_F(OptimizationOperationsTests, deinterleaveAccessorNoThrow)
{
    auto taskDescription = nlohmann::json({ { "deinterleave_accessor", nlohmann::json::object() } });
    taskDescription["deinterleave_accessor"]["path"] = "/gltf/accessors/0";

    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));
}

TEST_F(OptimizationOperationsTests, deinterleaveAccessorNoChange)
{
    auto taskDescription = nlohmann::json({ { "deinterleave_accessor", nlohmann::json::object() } });
    taskDescription["deinterleave_accessor"]["path"] = "/gltf/accessors/0";

    auto avocadoCopy = Avocado;
    Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage);

    EXPECT_TRUE(equal(Avocado, avocadoCopy));
}

TEST_F(OptimizationOperationsTests, deinterleaveAccessor)
{
    auto taskDescription = nlohmann::json({ { "deinterleave_accessor", nlohmann::json::object() } });
    taskDescription["deinterleave_accessor"]["path"] = "/gltf/accessors/2";

    auto gltfBefore = BoxInterleaved.state["gltf"];
    BoxInterleaved.state = g_library.call(std::move(taskDescription), std::move(BoxInterleaved.state), BoxInterleaved.dataStorage);
    auto gltfAfter = BoxInterleaved.state["gltf"];

    EXPECT_TRUE(gltfBefore["accessors"].size() == gltfAfter["accessors"].size());

    // Expect no change besides byteOffset and new bufferView
    gltfBefore["accessors"][2].at("byteOffset") = 0;
    gltfBefore["accessors"][2].at("bufferView") = gltfAfter["accessors"][2].at("bufferView");
    auto jsonDiff = nlohmann::json::diff(gltfBefore["accessors"][2], gltfAfter["accessors"][2]);
    EXPECT_TRUE(jsonDiff.size() == 0);
}

TEST_F(OptimizationOperationsTests, convertImageThrow)
{
    auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });

    auto resource = Avocado;
    EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);

    taskDescription["convert_image"]["path"] = "/gltf/images/0";
    resource = Avocado;
    EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);
    taskDescription["convert_image"]["mimeType"] = "jepg";
    resource = Avocado;
    EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);
    taskDescription["convert_image"]["mimeType"] = "image/jpeg";
    taskDescription["convert_image"]["width"] = 100;
    EXPECT_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage), std::invalid_argument);
}

TEST_F(OptimizationOperationsTests, convertImageJPEG)
{
    auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
    taskDescription["convert_image"]["path"] = "/gltf/images/0";
    taskDescription["convert_image"]["mimeType"] = "image/jpeg";
    taskDescription["convert_image"]["width"] = 100;
    taskDescription["convert_image"]["height"] = 100;
    taskDescription["convert_image"]["quality"] = 20;
    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));
    auto uri = Avocado.state.at(nlohmann::json::json_pointer("/gltf/images/0/uri")).get<std::string>();
    EXPECT_EQ(uri, "Avocado_baseColor.jpg");
}

TEST_F(OptimizationOperationsTests, convertImagePNG)
{
    auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
    taskDescription["convert_image"]["path"] = "/gltf/images/0";
    taskDescription["convert_image"]["mimeType"] = "image/png";
    taskDescription["convert_image"]["width"] = 50;
    taskDescription["convert_image"]["height"] = 50;
    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));
    auto uri = Avocado.state.at(nlohmann::json::json_pointer("/gltf/images/0/uri")).get<std::string>();
    EXPECT_EQ(uri, "Avocado_baseColor.png");
}

TEST_F(OptimizationOperationsTests, convertImageEmbedded)
{
    ScenarioResource resource = loadResource("/Avocado/StorageTypeEmbedded/Avocado.gltf");
    auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
    taskDescription["convert_image"]["path"] = "/gltf/images/0";
    taskDescription["convert_image"]["mimeType"] = "image/png";
    taskDescription["convert_image"]["width"] = 50;
    taskDescription["convert_image"]["height"] = 50;
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    auto uri = resource.state.at(nlohmann::json::json_pointer("/gltf/images/0/uri")).get<std::string>();
    EXPECT_TRUE(reactor::utils::isHashedDataUri(uri));
}

TEST_F(OptimizationOperationsTests, convertImageBufferView)
{
    ScenarioResource resource = AvocadoGlb;
    auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
    taskDescription["convert_image"]["path"] = "/gltf/images/0";
    taskDescription["convert_image"]["mimeType"] = "image/png";
    taskDescription["convert_image"]["width"] = 50;
    taskDescription["convert_image"]["height"] = 50;
    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    EXPECT_FALSE(resource.state.contains(nlohmann::json::json_pointer("/gltf/images/0/uri")));
    EXPECT_TRUE(resource.state.contains(nlohmann::json::json_pointer("/gltf/images/0/bufferView")));
}

TEST_F(OptimizationOperationsTests, decodeKTX2)
{
    auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
    taskDescription["convert_image"]["path"] = "/gltf/images/0";
    taskDescription["convert_image"]["mimeType"] = "image/jpeg";
    taskDescription["convert_image"]["width"] = 100;
    taskDescription["convert_image"]["height"] = 100;
    taskDescription["convert_image"]["quality"] = 20;

    auto ktxResource = BarramundiFish;
    EXPECT_NO_THROW(ktxResource.state = g_library.call(std::move(taskDescription), std::move(ktxResource.state), ktxResource.dataStorage));

    auto texture = ktxResource.state.at(nlohmann::json::json_pointer("/gltf/textures/0"));

    EXPECT_FALSE(texture.contains("extensions"));
    EXPECT_EQ(texture.at("source").get<uint32_t>(), 0);
}

TEST_F(OptimizationOperationsTests, encodeKTX2)
{
    auto resource = Avocado;
    {
        auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
        taskDescription["convert_image"]["path"] = "/gltf/images/0";
        taskDescription["convert_image"]["mimeType"] = "image/ktx2";
        taskDescription["convert_image"]["width"] = 100;
        taskDescription["convert_image"]["height"] = 100;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    auto texture = resource.state.at(nlohmann::json::json_pointer("/gltf/textures/0"));

    EXPECT_TRUE(texture.contains("extensions"));
    EXPECT_FALSE(texture.contains("source"));
    auto uri = resource.state.at(nlohmann::json::json_pointer("/gltf/images/0/uri")).get<std::string>();
    EXPECT_EQ(uri, "Avocado_baseColor.ktx2");

    {
        auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
        taskDescription["convert_image"]["path"] = "/gltf/images/1";
        taskDescription["convert_image"]["mimeType"] = "image/ktx2";
        taskDescription["convert_image"]["width"] = 64;
        taskDescription["convert_image"]["height"] = 100;
        taskDescription["convert_image"]["compression"] = "UASTC";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    {
        auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
        taskDescription["convert_image"]["path"] = "/gltf/images/2";
        taskDescription["convert_image"]["mimeType"] = "image/ktx2";
        taskDescription["convert_image"]["width"] = 128;
        taskDescription["convert_image"]["height"] = 128;
        taskDescription["convert_image"]["compression"] = "UASTC";
        taskDescription["convert_image"]["superCompression"] = "ZSTANDARD";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(OptimizationOperationsTests, decodeWebP)
{
    auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
    taskDescription["convert_image"]["path"] = "/gltf/images/0";
    taskDescription["convert_image"]["mimeType"] = "image/jpeg";
    taskDescription["convert_image"]["width"] = 100;
    taskDescription["convert_image"]["height"] = 100;
    taskDescription["convert_image"]["quality"] = 20;

    auto webpResource = AvocadoWebP;
    EXPECT_NO_THROW(webpResource.state = g_library.call(std::move(taskDescription), std::move(webpResource.state), webpResource.dataStorage));

    auto texture = webpResource.state.at(nlohmann::json::json_pointer("/gltf/textures/0"));

    EXPECT_FALSE(texture.contains("extensions"));
    EXPECT_EQ(texture.at("source").get<uint32_t>(), 0);
    EXPECT_EQ(webpResource.state.at(nlohmann::json::json_pointer("/gltf/images/0/uri")).get<std::string>(), "Avocado_baseColor.jpg");
}

TEST_F(OptimizationOperationsTests, encodeWebP)
{
    auto resource = Avocado;
    {
        auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
        taskDescription["convert_image"]["path"] = "/gltf/images/0";
        taskDescription["convert_image"]["mimeType"] = "image/webp";
        taskDescription["convert_image"]["width"] = 100;
        taskDescription["convert_image"]["height"] = 100;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    auto texture = resource.state.at(nlohmann::json::json_pointer("/gltf/textures/0"));

    EXPECT_TRUE(texture.contains("extensions"));
    EXPECT_TRUE(texture["extensions"].contains("EXT_texture_webp"));
    EXPECT_TRUE(texture["extensions"]["EXT_texture_webp"].contains("source"));
    EXPECT_EQ(texture["extensions"]["EXT_texture_webp"]["source"], 0);
    EXPECT_FALSE(texture.contains("source"));
    auto uri = resource.state.at(nlohmann::json::json_pointer("/gltf/images/0/uri")).get<std::string>();
    EXPECT_EQ(uri, "Avocado_baseColor.webp");

    {
        auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
        taskDescription["convert_image"]["path"] = "/gltf/images/1";
        taskDescription["convert_image"]["mimeType"] = "image/webp";
        taskDescription["convert_image"]["width"] = 64;
        taskDescription["convert_image"]["height"] = 100;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    {
        auto taskDescription = nlohmann::json({ { "convert_image", nlohmann::json::object() } });
        taskDescription["convert_image"]["path"] = "/gltf/images/2";
        taskDescription["convert_image"]["mimeType"] = "image/webp";
        taskDescription["convert_image"]["width"] = 128;
        taskDescription["convert_image"]["height"] = 128;

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
}

TEST_F(OptimizationOperationsTests, convertImageForEach)
{
    auto taskDescription = nlohmann::json({ { "convert_image_for_each", nlohmann::json::object() } });
    taskDescription["convert_image_for_each"]["mimeType"] = "image/png";
    taskDescription["convert_image_for_each"]["width"] = 100;
    taskDescription["convert_image_for_each"]["height"] = 100;
    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));

    EXPECT_EQ(Avocado.state.at(nlohmann::json::json_pointer("/gltf/images/0/uri")).get<std::string>(), "Avocado_baseColor.png");
    EXPECT_EQ(Avocado.state.at(nlohmann::json::json_pointer("/gltf/images/1/uri")).get<std::string>(), "Avocado_roughnessMetallic.png");
    EXPECT_EQ(Avocado.state.at(nlohmann::json::json_pointer("/gltf/images/2/uri")).get<std::string>(), "Avocado_normal.png");
}

TEST_F(OptimizationOperationsTests, changeTRSinvalidParameters)
{
    auto taskDescription = nlohmann::json({ { "change_transformation_format", nlohmann::json::object() } });
    taskDescription["change_transformation_format"]["path"] = "/gltf/nodes/0";
    taskDescription["change_transformation_format"]["storageType"] = "TRS_INVALID";

    EXPECT_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage), std::invalid_argument);
}

TEST_F(OptimizationOperationsTests, changeTRSvalidParameters)
{
    auto taskDescription = nlohmann::json({ { "change_transformation_format", nlohmann::json::object() } });
    taskDescription["change_transformation_format"]["path"] = "/gltf/nodes/0";
    taskDescription["change_transformation_format"]["storageType"] = "Matrix";

    EXPECT_NO_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage));
}

TEST_F(OptimizationOperationsTests, changeTRStoMatrix)
{
    auto taskDescription = nlohmann::json({ { "change_transformation_format", nlohmann::json::object() } });
    taskDescription["change_transformation_format"]["path"] = "/gltf/nodes/0";
    taskDescription["change_transformation_format"]["storageType"] = "Matrix";

    auto resource = Avocado;

    auto node = resource.state.at(nlohmann::json::json_pointer("/gltf/nodes/0"));

    EXPECT_FALSE(node.contains("matrix"));

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    // Get a new reference to /gltf/nodes/0 as the previous one now references garbage memory
    node = resource.state.at(nlohmann::json::json_pointer("/gltf/nodes/0"));

    EXPECT_FALSE(node.contains("rotation"));

    EXPECT_TRUE(node.contains("matrix"));

    EXPECT_TRUE(node.at("matrix").size() == 16);

    EXPECT_TRUE(node.at("matrix")[0].get<double>() == -1.0);
}

TEST_F(OptimizationOperationsTests, changeMatrixtoTRS)
{
    auto taskDescription = nlohmann::json({ { "change_transformation_format", nlohmann::json::object() } });
    taskDescription["change_transformation_format"]["path"] = "/gltf/nodes/0";
    taskDescription["change_transformation_format"]["storageType"] = "TRS";

    auto resource = Avocado;

    auto& node = resource.state.at(nlohmann::json::json_pointer("/gltf/nodes/0"));

    EXPECT_FALSE(node.contains("matrix"));

    node.erase("rotation");
    node["matrix"] = R"([
				-1.0,
				0.0,
				0.0,
				0.0,
				0.0,
				1.0,
				0.0,
				0.0,
				0.0,
				0.0,
				-1.0,
				0.0,
				0.0,
				0.0,
				0.0,
				1.0
			])"_json;

    EXPECT_TRUE(node.at("matrix")[0].get<double>() == -1.0);

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), Avocado.dataStorage));

    // Get a new reference to /gltf/nodes/0 as the previous one now references garbage memory
    node = resource.state.at(nlohmann::json::json_pointer("/gltf/nodes/0"));

    EXPECT_FALSE(node.contains("matrix"));

    EXPECT_TRUE(node.contains("rotation"));
    EXPECT_TRUE(node.at("rotation")[1].get<double>() == 1.0);
}

TEST_F(OptimizationOperationsTests, changeMatrixtoTRSdecomposefail)
{
    auto taskDescription = nlohmann::json({ { "change_transformation_format", nlohmann::json::object() } });
    taskDescription["change_transformation_format"]["path"] = "/gltf/nodes/0";
    taskDescription["change_transformation_format"]["storageType"] = "TRS";

    EXPECT_FALSE(Avocado.state["/gltf/nodes/0"_json_pointer].contains("matrix"));

    // the decompose function fails if the matrix has a 0 at position (3,3)
    Avocado.state["/gltf/nodes/0"_json_pointer].erase("rotation");
    Avocado.state["/gltf/nodes/0"_json_pointer]["matrix"] = R"([
				-1.0,
				0.0,
				0.0,
				0.0,
				0.0,
				1.0,
				0.0,
				0.0,
				0.0,
				0.0,
				-1.0,
				0.0,
				0.0,
				0.0,
				0.0,
				0.0 
			])"_json;

    auto node = Avocado.state["/gltf/nodes/0"_json_pointer];
    EXPECT_TRUE(node.at("matrix")[0].get<double>() == -1.0);

    EXPECT_THROW(Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage), std::invalid_argument);

    // Get a new reference to /gltf/nodes/0 as the previous one now references garbage memory
    node = Avocado.state.at(nlohmann::json::json_pointer("/gltf/nodes/0"));

    EXPECT_TRUE(node.contains("matrix"));

    EXPECT_FALSE(node.contains("rotation"));
}

TEST_F(OptimizationOperationsTests, changeTRSForAll)
{
    auto taskDescription = nlohmann::json({ { "change_transformation_format_for_each", nlohmann::json::object() } });
    taskDescription["change_transformation_format_for_each"]["storageType"] = "Matrix";

    auto& nodes = Avocado.state.at(nlohmann::json::json_pointer("/gltf/nodes"));

    // add new dummy node
    nlohmann::json newnode = { { "name", "Avocado" } };
    newnode["translation"] = R"([
				-1.0,
				0.0,
				0.0
			])"_json;

    nodes.push_back(newnode);

    auto node = Avocado.state.at(nlohmann::json::json_pointer("/gltf/nodes/0"));
    auto node1 = Avocado.state.at(nlohmann::json::json_pointer("/gltf/nodes/1"));

    EXPECT_TRUE(node1.contains("translation"));
    EXPECT_FALSE(node1.contains("matrix"));
    EXPECT_TRUE(node.contains("rotation"));
    EXPECT_FALSE(node.contains("matrix"));

    Avocado.state = g_library.call(std::move(taskDescription), std::move(Avocado.state), Avocado.dataStorage);

    // Get a new reference to /gltf/nodes/0 as the previous one now references garbage memory
    node = Avocado.state.at(nlohmann::json::json_pointer("/gltf/nodes/0"));
    node1 = Avocado.state.at(nlohmann::json::json_pointer("/gltf/nodes/1"));

    EXPECT_TRUE(node.contains("matrix"));
    EXPECT_FALSE(node.contains("rotation"));
    EXPECT_TRUE(node1.contains("matrix"));
    EXPECT_FALSE(node1.contains("translation"));
}

TEST_F(OptimizationOperationsTests, joinPrimitives)
{
    auto taskDescription = nlohmann::json({ { "join_primitives", nlohmann::json::object() } });
    taskDescription["join_primitives"]["paths"] = nlohmann::json::array();
    taskDescription["join_primitives"]["paths"].push_back("/gltf/meshes/0/primitives/0");
    taskDescription["join_primitives"]["paths"].push_back("/gltf/meshes/0/primitives/1");

    MultiPrimitive.state = g_library.call(std::move(taskDescription), std::move(MultiPrimitive.state), MultiPrimitive.dataStorage);

    const auto& gltf = MultiPrimitive.state["gltf"];

    const auto& prims = gltf["meshes"][0]["primitives"];
    EXPECT_EQ(prims.size(), 2) << "Joined two of three primitives";
    EXPECT_EQ(gltf["accessors"][prims[0]["indices"].get<int>()]["count"], 36) << "The joined primitive is appended";
    EXPECT_EQ(gltf["accessors"][prims[1]["indices"].get<int>()]["count"], 72) << "The joined primitive is appended";
    EXPECT_EQ(gltf["accessors"][prims[1]["indices"].get<int>()]["componentType"], reactor::gltf::ComponentType::UnsignedByte) << "Index type is narrowed";
    EXPECT_EQ(gltf["accessors"][prims[1]["attributes"]["POSITION"].get<int>()]["count"], 48) << "Vertices are concatenated";
    EXPECT_EQ(gltf["accessors"][prims[1]["attributes"]["POSITION"].get<int>()]["max"], (std::array { 1, 3, 1 })) << "\"max\" is combined";
    EXPECT_EQ(gltf["accessors"][prims[1]["attributes"]["POSITION"].get<int>()]["min"], (std::array { -3, -1, -1 })) << "\"min\" is combined";
    EXPECT_EQ(gltf["accessors"][prims[1]["attributes"]["NORMAL"].get<int>()]["count"], 48) << "Vertices are concatenated";
    EXPECT_EQ(gltf["accessors"][prims[1]["attributes"]["TEXCOORD_0"].get<int>()]["count"], 48) << "Vertices are concatenated";
    EXPECT_EQ(gltf["accessors"].size(), 2 + 2 * 3) << "Expected minimized accessor count";
    EXPECT_EQ(gltf["bufferViews"].size(), 2 + 2 * 3) << "Expected minimized buffer view count";
}

TEST_F(OptimizationOperationsTests, joinPrimitivesOnlyOneArgument)
{
    auto taskDescription = nlohmann::json({ { "join_primitives", nlohmann::json::array({ "/gltf/meshes/0/primitives/0" }) } });
    EXPECT_ANY_THROW(g_library.call(std::move(taskDescription), std::move(MultiPrimitive.state), MultiPrimitive.dataStorage));
}

TEST_F(OptimizationOperationsTests, joinPrimitivesNoArguments)
{
    auto taskDescription = nlohmann::json({ { "join_primitives", nlohmann::json::array({}) } });
    EXPECT_ANY_THROW(g_library.call(std::move(taskDescription), std::move(MultiPrimitive.state), MultiPrimitive.dataStorage));
}

TEST_F(OptimizationOperationsTests, clusterVerticesNoChange)
{
    auto getAcc = [](nlohmann::json& state, nlohmann::json::json_pointer attributePath) -> nlohmann::json {
        auto idx = state.at(attributePath).get<uint32_t>();
        return state["gltf"]["accessors"][idx];
    };

    auto testResource = Avocado;
    auto primitivePath = nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0");

    auto positionsAccessorBefore = getAcc(testResource.state, primitivePath / "attributes" / "POSITION");
    auto normalAccessorBefore = getAcc(testResource.state, primitivePath / "attributes" / "NORMAL");
    auto texcoordAccessorBefore = getAcc(testResource.state, primitivePath / "attributes" / "TEXCOORD_0");

    auto accessorCountBefore = testResource.state["gltf"]["accessors"].size();
    {
        std::string op = "apply_vertex_clustering";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = primitivePath;
        EXPECT_NO_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));
    }
    auto accessorCountAfter = testResource.state["gltf"]["accessors"].size();
    EXPECT_EQ(accessorCountBefore, accessorCountAfter);

    EXPECT_TRUE(testResource.state.at(primitivePath).contains("indices"));
    EXPECT_TRUE(testResource.state.at(primitivePath).at("attributes").contains("POSITION"));
    EXPECT_TRUE(testResource.state.at(primitivePath).at("attributes").contains("NORMAL"));
    EXPECT_TRUE(testResource.state.at(primitivePath).at("attributes").contains("TANGENT"));
    EXPECT_TRUE(testResource.state.at(primitivePath).at("attributes").contains("TEXCOORD_0"));

    auto positionsAccessorAfter = getAcc(testResource.state, primitivePath / "attributes" / "POSITION");
    auto normalAccessorAfter = getAcc(testResource.state, primitivePath / "attributes" / "NORMAL");
    auto texcoordAccessorAfter = getAcc(testResource.state, primitivePath / "attributes" / "TEXCOORD_0");

    EXPECT_EQ(positionsAccessorBefore["type"], positionsAccessorAfter["type"]);
    EXPECT_EQ(normalAccessorBefore["type"], normalAccessorAfter["type"]);
    EXPECT_EQ(texcoordAccessorBefore["type"], texcoordAccessorAfter["type"]);

    EXPECT_TRUE(positionsAccessorBefore["count"].get<uint32_t>() == positionsAccessorAfter["count"].get<uint32_t>());
    EXPECT_TRUE(normalAccessorBefore["count"].get<uint32_t>() == normalAccessorAfter["count"].get<uint32_t>());
    EXPECT_TRUE(texcoordAccessorBefore["count"].get<uint32_t>() == texcoordAccessorAfter["count"].get<uint32_t>());
}

TEST_F(OptimizationOperationsTests, clusterVertices)
{
    auto getAcc = [](nlohmann::json& state, nlohmann::json::json_pointer attributePath) -> nlohmann::json {
        auto idx = state.at(attributePath).get<uint32_t>();
        return state["gltf"]["accessors"][idx];
    };

    auto testResource = Avocado;
    auto primitivePath = nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0");

    auto positionsAccessorBefore = getAcc(testResource.state, primitivePath / "attributes" / "POSITION");
    auto normalAccessorBefore = getAcc(testResource.state, primitivePath / "attributes" / "NORMAL");
    auto texcoordAccessorBefore = getAcc(testResource.state, primitivePath / "attributes" / "TEXCOORD_0");

    auto accessorCountBefore = testResource.state["gltf"]["accessors"].size();
    {
        std::string op = "apply_vertex_clustering";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = primitivePath;
        taskDescription[op]["positionThreshold"] = 0.1;
        taskDescription[op]["normalThreshold"] = 0.4;
        taskDescription[op]["tangentThreshold"] = 0.4;
        taskDescription[op]["texCoordThreshold"] = 0.1;

        EXPECT_NO_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));
    }
    auto accessorCountAfter = testResource.state["gltf"]["accessors"].size();
    EXPECT_EQ(accessorCountBefore, accessorCountAfter);

    auto positionsAccessorAfter = getAcc(testResource.state, primitivePath / "attributes" / "POSITION");
    auto normalAccessorAfter = getAcc(testResource.state, primitivePath / "attributes" / "NORMAL");
    auto texcoordAccessorAfter = getAcc(testResource.state, primitivePath / "attributes" / "TEXCOORD_0");

    EXPECT_EQ(positionsAccessorBefore["type"], positionsAccessorAfter["type"]);
    EXPECT_EQ(normalAccessorBefore["type"], normalAccessorAfter["type"]);
    EXPECT_EQ(texcoordAccessorBefore["type"], texcoordAccessorAfter["type"]);

    EXPECT_TRUE(positionsAccessorBefore["count"].get<uint32_t>() > positionsAccessorAfter["count"].get<uint32_t>());
    EXPECT_TRUE(normalAccessorBefore["count"].get<uint32_t>() > normalAccessorAfter["count"].get<uint32_t>());
    EXPECT_TRUE(texcoordAccessorBefore["count"].get<uint32_t>() > texcoordAccessorAfter["count"].get<uint32_t>());
}

TEST_F(OptimizationOperationsTests, clusterVerticesFail)
{
    auto testResource = AvocadoDraco;
    auto primitivePath = nlohmann::json::json_pointer("/gltf/meshes/0/primitives/0");

    {
        std::string op = "apply_vertex_clustering";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = primitivePath;
        EXPECT_ANY_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));
    }
}

TEST_F(OptimizationOperationsTests, normalizeWeightsAccessor)
{
    auto testResource = Avocado;

    std::vector<glm::vec4> data;
    data.push_back(glm::vec4(2.0, 2.0, 4.0, 1.0));
    data.push_back(glm::vec4(0.0, 0.0, 0.0, 0.0));
    data.push_back(glm::vec4(0.0, 0.0, 0.0, 1.0));
    reactor::utils::createAccessorFromData(data, testResource.state, testResource.dataStorage);

    auto accessorPath = reactor::utils::createAccessorFromData(data, testResource.state, testResource.dataStorage);

    auto taskDescription = nlohmann::json({ { "normalize_weights", nlohmann::json::object() } });
    taskDescription["normalize_weights"]["path"] = accessorPath.to_string();
    EXPECT_NO_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));

    std::vector<glm::vec4> result = reactor::utils::getAccessorData<glm::vec4>(accessorPath, testResource.state, testResource.dataStorage);

    EXPECT_EQ(result.size(), data.size());
    EXPECT_EQ(data[2], result[2]);
    for (auto vec : result) {
        EXPECT_EQ(vec.x + vec.y + vec.z + vec.w, 1.0);
    }
}

TEST_F(OptimizationOperationsTests, clampAccessor)
{
    auto testResource = Avocado;

    std::vector<float> data({ -1.0, 0.0, 1.0, 2.0 });
    reactor::utils::createAccessorFromData(data, testResource.state, testResource.dataStorage);

    auto accessorPath = reactor::utils::createAccessorFromData(data, testResource.state, testResource.dataStorage);

    auto taskDescription = nlohmann::json({ { "clamp_accessor_values", nlohmann::json::object() } });
    taskDescription["clamp_accessor_values"]["path"] = accessorPath.to_string();
    taskDescription["clamp_accessor_values"]["min"] = 0.0;
    taskDescription["clamp_accessor_values"]["max"] = 1.0;
    EXPECT_NO_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));

    std::vector<float> result = reactor::utils::getAccessorData<float>(accessorPath, testResource.state, testResource.dataStorage);

    EXPECT_EQ(result.size(), data.size());
    for (auto scalar : result) {
        EXPECT_TRUE(scalar >= 0.0 && scalar <= 1.0);
    }
}

TEST_F(OptimizationOperationsTests, convertSpecularGlossinessToMetallicRoughness)
{
    ScenarioResource resource = SpecularGlossiness;
    {
        auto taskDescription = nlohmann::json({ { "convert_specular_glossiness_to_metallic_roughness", nlohmann::json::object() } });
        taskDescription["convert_specular_glossiness_to_metallic_roughness"]["path"] = "/gltf/materials/0";
        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }
    EXPECT_TRUE(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/0")).contains("pbrMetallicRoughness"));
    EXPECT_TRUE(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/0/extensions")).contains("KHR_materials_specular"));
    EXPECT_TRUE(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/0/extensions")).contains("KHR_materials_ior"));

    EXPECT_TRUE(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/0/pbrMetallicRoughness")).contains("baseColorTexture"));
    EXPECT_TRUE(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/0/pbrMetallicRoughness")).contains("metallicRoughnessTexture"));
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/0/pbrMetallicRoughness/metallicFactor")).get<double>(), 1.0);
    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/materials/0/pbrMetallicRoughness/roughnessFactor")).get<double>(), 1.0);
}

TEST_F(OptimizationOperationsTests, resolveMeshQuantization)
{
    {
        // test regular Attribute Accessors
        auto testResource = AvocadoMeshQuant;

        const size_t originalBufferViewCount = testResource.state.at(nlohmann::json::json_pointer("/gltf/bufferViews")).size();
        const size_t originalBufferCount = testResource.state.at(nlohmann::json::json_pointer("/gltf/buffers")).size();

        auto taskDescription = nlohmann::json({ { "resolve_mesh_quantization", { { "path", "/gltf/accessors/0" } } } });
        EXPECT_NO_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));

        EXPECT_TRUE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/0"))["componentType"].get<int>() == 5126); // 5126 = float
        EXPECT_FALSE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/1"))["componentType"].get<int>() == 5126);

        taskDescription = nlohmann::json({ { "resolve_mesh_quantization_for_each", nlohmann::json::object() } });
        EXPECT_NO_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));

        EXPECT_TRUE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/0"))["componentType"].get<int>() == 5126); // 5126 = float
        EXPECT_TRUE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/1"))["componentType"].get<int>() == 5126);
        EXPECT_TRUE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/2"))["componentType"].get<int>() == 5126);
        EXPECT_TRUE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/3"))["componentType"].get<int>() == 5126);

        // this componentType should still be unsigned short, since the Accessor does not use any mesh quantization
        EXPECT_TRUE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/4"))["componentType"].get<int>() == 5123); // 5123 = unsigned short

        // bufferView and buffer counts should not have changed during the operation
        const size_t newBufferViewCount = testResource.state.at(nlohmann::json::json_pointer("/gltf/bufferViews")).size();
        const size_t newBufferCount = testResource.state.at(nlohmann::json::json_pointer("/gltf/buffers")).size();

        EXPECT_TRUE(originalBufferViewCount == newBufferViewCount);
        EXPECT_TRUE(originalBufferCount == newBufferCount);
    }
    {
        // test morph target Accessors
        auto testResource = SimpleMorphMeshQuant;

        auto taskDescription = nlohmann::json({ { "resolve_mesh_quantization_for_each", nlohmann::json::object() } });
        EXPECT_NO_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));

        EXPECT_TRUE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/2"))["componentType"].get<int>() == 5126); // 5126 = float
        EXPECT_TRUE(testResource.state.at(nlohmann::json::json_pointer("/gltf/accessors/3"))["componentType"].get<int>() == 5126);
    }
}

TEST_F(OptimizationOperationsTests, reduceTriangleCount)
{
    auto testResource = Avocado;

    double targetSize = 0.5;

    nlohmann::json taskDescription { {
        "reduce_triangle_count",
        {
            { "path", "/gltf/meshes/0/primitives/0" },
            { "targetSize", targetSize },
            { "quality", 8 },
        },
    } };

    int initialIndexCount = testResource.state["gltf"]["accessors"][testResource.state["gltf"]["meshes"][0]["primitives"][0]["indices"].get<int>()]["count"];

    ASSERT_NO_THROW(testResource.state = g_library.call(std::move(taskDescription), std::move(testResource.state), testResource.dataStorage));

    int finalIndexCount = testResource.state["gltf"]["accessors"][testResource.state["gltf"]["meshes"][0]["primitives"][0]["indices"].get<int>()]["count"];

    double ratio = static_cast<double>(finalIndexCount) / static_cast<double>(initialIndexCount);
    EXPECT_LT(std::abs(ratio - std::min(targetSize, 1.0)), 0.1);
}

TEST_F(OptimizationOperationsTests, optimizeAnimation)
{
    auto derefAccessor = [](std::string path, nlohmann::json& state) -> nlohmann::json::json_pointer {
        auto index = state.at(nlohmann::json::json_pointer(path)).get<uint64_t>();
        return nlohmann::json::json_pointer("/gltf/accessors") / index;
    };

    auto resource = Fox;

    uint32_t timestepsBefore = 0;

    for (int i = 0; i < resource.state.at(nlohmann::json::json_pointer("/gltf/animations/0/samplers")).size(); i++) {
        auto timeAccessorPath = derefAccessor((nlohmann::json::json_pointer("/gltf/animations/0/samplers") / i / "input").to_string(), resource.state);
        auto timesteps = resource.state.at(timeAccessorPath / "count").get<uint32_t>();
        timestepsBefore += timesteps;
    }

    {
        auto taskDescription = nlohmann::json({ { "optimize_keyframes", nlohmann::json::object() } });
        taskDescription["optimize_keyframes"]["path"] = "/gltf/animations/0";

        EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));
    }

    uint32_t timestepsAfter = 0;

    for (int i = 0; i < resource.state.at(nlohmann::json::json_pointer("/gltf/animations/0/samplers")).size(); i++) {
        auto timeAccessorPath = derefAccessor((nlohmann::json::json_pointer("/gltf/animations/0/samplers") / i / "input").to_string(), resource.state);
        auto timesteps = resource.state.at(timeAccessorPath / "count").get<uint32_t>();
        timestepsAfter += timesteps;
    }

    EXPECT_LT(timestepsAfter, timestepsBefore);

    EXPECT_EQ(resource.state.at(nlohmann::json::json_pointer("/gltf/buffers")).size(), 1);
}
