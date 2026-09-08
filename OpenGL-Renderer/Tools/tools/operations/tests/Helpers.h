

#include "gtest/gtest.h"

#include "Gltfs.h"
#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "reactor/Operations.h"
#include "reactor/Schema.h"
#include <gltf_schema/gltf_single_json.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <map>
#include <string>

// Use a single library for all tests to reduce test exectuion time
extern ux3d::operations::Library g_library;

class OperationsTests : public ::testing::Test {
protected:
    nlohmann::json m_box;
    ux3d::datastorage::DataStorage m_dataStorage;

    struct ScenarioResource {
        nlohmann::json state;
        ux3d::datastorage::DataStorage dataStorage;
    };

    ScenarioResource AlphaBlendModeTest;
    ScenarioResource AttenuationTest; // contains shared accessors
    ScenarioResource Avocado;
    ScenarioResource AvocadoGlb;
    ScenarioResource AvocadoDraco;
    ScenarioResource AvocadoVariants;
    ScenarioResource AvocadoWebP;
    ScenarioResource BoxInterleaved;
    ScenarioResource BarramundiFish;
    ScenarioResource CesiumMilkTruck;
    ScenarioResource CesiumMilkTruckVariant;
    ScenarioResource CustomXmpModel;
    ScenarioResource MaterialsVariantsShoe;
    ScenarioResource GlamVelvetSofa;
    ScenarioResource MultiPrimitive;
    ScenarioResource Fox;
    ScenarioResource FoxGlb;
    ScenarioResource MorphStressTest;
    ScenarioResource BoxWithSpaces;
    ScenarioResource EnvironmentTest;
    ScenarioResource VC;
    ScenarioResource DuckGlb;
    ScenarioResource SpecularGlossiness;
    ScenarioResource AvocadoMeshQuant;
    ScenarioResource SimpleMorphMeshQuant;

    static bool equal(ScenarioResource a, ScenarioResource b)
    {
        auto jsonDiff = nlohmann::json::diff(a.state["gltf"], b.state["gltf"]);

        // Remove timestamp diff
        for (uint32_t i = 0; i < jsonDiff.size(); i++) {
            if (jsonDiff[i].contains("path")) {
                if (jsonDiff[i]["path"].get<std::string>().compare("/asset/extras/created") == 0) {
                    jsonDiff.erase(i);
                    break;
                }
            }
        }

        return (jsonDiff.size() == 0);
    }

    static ScenarioResource loadResource(std::string path)
    {
        auto taskDescription = nlohmann::json({ { "load_file", nlohmann::json::object() } });
        taskDescription["load_file"]["filepath"] = std::string(UX3D_OPERATIONS_TEST_RESOURCES) + path;

        ScenarioResource resource;
        resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage);
        return resource;
    }

    static void SetUpTestSuite()
    {
        m_scenarioResources["AlphaBlendModeTest.gltf"] = loadResource("/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf");
        m_scenarioResources["AttenuationTest.gltf"] = loadResource("/AttenuationTest/glTF/AttenuationTest.gltf");
        m_scenarioResources["Avocado.gltf"] = loadResource("/Avocado/gltf/Avocado.gltf");
        m_scenarioResources["Avocado.glb"] = loadResource("/Avocado/glb/Avocado.glb");
        m_scenarioResources["AvocadoVariants.gltf"] = loadResource("/Avocado/variants/Avocado.gltf");
        m_scenarioResources["AvocadoDraco.gltf"] = loadResource("/Avocado/Draco/Avocado.gltf");
        m_scenarioResources["AvocadoWebP.gltf"] = loadResource("/Avocado/gltfWebP/Avocado.gltf");
        m_scenarioResources["BarramundiFish.mixed.glb"] = loadResource("/BarramundiFish/ktx2/BarramundiFish.mixed.glb");
        m_scenarioResources["Box With Spaces.gltf"] = loadResource("/Box With Spaces/glTF/Box With Spaces.gltf");
        m_scenarioResources["BoxInterleaved.gltf"] = loadResource("/BoxInterleaved/gltf/BoxInterleaved.gltf");
        m_scenarioResources["CesiumMilkTruck.gltf"] = loadResource("/CesiumMilkTruck/gltf/CesiumMilkTruck.gltf");
        m_scenarioResources["CesiumMilkTruckVariant.gltf"] = loadResource("/CesiumMilkTruck/gltfVariant/CesiumMilkTruckVariant.gltf");
        m_scenarioResources["CustomXmpModel.gltf"] = loadResource("/CustomXmpModel/glTF/CustomXmpModel.glTF");
        m_scenarioResources["Duck.glb"] = loadResource("/Duck/glTF-Binary/Duck.glb");
        m_scenarioResources["EnvironmentTest.gltf"] = loadResource("/EnvironmentTest/glTF/EnvironmentTest.gltf");
        m_scenarioResources["Fox.gltf"] = loadResource("/Fox/gltf/Fox.gltf");
        m_scenarioResources["Fox.glb"] = loadResource("/Fox/glb/Fox.glb");
        m_scenarioResources["GlamVelvetSofa.gltf"] = loadResource("/GlamVelvetSofa/glTF/GlamVelvetSofa.gltf");
        m_scenarioResources["MaterialsVariantsShoe.glb"] = loadResource("/MaterialsVariantsShoe/glb/MaterialsVariantsShoe.glb");
        m_scenarioResources["MorphStressTest.gltf"] = loadResource("/MorphStressTest/glTF/MorphStressTest.gltf");
        m_scenarioResources["MultiPrimitive.gltf"] = loadResource("/MultiPrimitive/glTF/MultiPrimitive.gltf");
        m_scenarioResources["VC.gltf"] = loadResource("/VC/glTF/VC.gltf");
        m_scenarioResources["SpecularGlossiness.gltf"] = loadResource("/SpecularGlossiness/SpecGlossVsMetalRough.gltf");
        m_scenarioResources["AvocadoMeshQuant.glb"] = loadResource("/Avocado/MeshQuantization/Avocado.glb");
        m_scenarioResources["SimpleMorphMeshQuant.glb"] = loadResource("/SimpleMorph/MeshQuantization/SimpleMorph.glb");

        // Setup manually created asset
        ScenarioResource box;
        box.state = nlohmann::json::object({ { "gltf", nlohmann::json::parse(boxGltf) } });
        box.state["files"] = nlohmann::json::object();
        box.dataStorage = ux3d::datastorage::DataStorage();
        uint32_t bufferSize = box.state["gltf"]["buffers"][0]["byteLength"].get<uint32_t>();
        uint64_t id = box.dataStorage.insert(std::vector<uint8_t>(bufferSize));
        box.state["files"][box.state["gltf"]["buffers"][0]["uri"].get<std::string>()] = id;

        id = box.dataStorage.insert(std::vector<uint8_t>(1000)); // Dummy buffer
        box.state["files"][box.state["gltf"]["buffers"][1]["uri"].get<std::string>()] = id;

        m_scenarioResources["box"] = box;
    }

    virtual void SetUp()
    {
        // Refresh test resources every test run with our cached resources
        m_dataStorage = m_scenarioResources["box"].dataStorage;
        m_box = m_scenarioResources["box"].state;

        AlphaBlendModeTest = m_scenarioResources["AlphaBlendModeTest.gltf"];
        AttenuationTest = m_scenarioResources["AttenuationTest.gltf"];
        Avocado = m_scenarioResources["Avocado.gltf"];
        AvocadoGlb = m_scenarioResources["Avocado.glb"];
        AvocadoDraco = m_scenarioResources["AvocadoDraco.gltf"];
        AvocadoWebP = m_scenarioResources["AvocadoWebP.gltf"];
        BoxInterleaved = m_scenarioResources["BoxInterleaved.gltf"];
        BarramundiFish = m_scenarioResources["BarramundiFish.mixed.glb"];
        CesiumMilkTruck = m_scenarioResources["CesiumMilkTruck.gltf"];
        CesiumMilkTruckVariant = m_scenarioResources["CesiumMilkTruckVariant.gltf"];
        CustomXmpModel = m_scenarioResources["CustomXmpModel.gltf"];
        MaterialsVariantsShoe = m_scenarioResources["MaterialsVariantsShoe.glb"];
        GlamVelvetSofa = m_scenarioResources["GlamVelvetSofa.gltf"];
        MultiPrimitive = m_scenarioResources["MultiPrimitive.gltf"];
        MorphStressTest = m_scenarioResources["MorphStressTest.gltf"];
        FoxGlb = m_scenarioResources["Fox.glb"];
        Fox = m_scenarioResources["Fox.gltf"];
        AvocadoVariants = m_scenarioResources["AvocadoVariants.gltf"];
        BoxWithSpaces = m_scenarioResources["Box With Spaces.gltf"];
        EnvironmentTest = m_scenarioResources["EnvironmentTest.gltf"];
        VC = m_scenarioResources["VC.gltf"];
        DuckGlb = m_scenarioResources["Duck.glb"];
        SpecularGlossiness = m_scenarioResources["SpecularGlossiness.gltf"];
        AvocadoMeshQuant = m_scenarioResources["AvocadoMeshQuant.glb"];
        SimpleMorphMeshQuant = m_scenarioResources["SimpleMorphMeshQuant.glb"];
    }

    virtual void TearDown()
    {
    }

private:
    static inline std::map<std::string, ScenarioResource> m_scenarioResources;
};

inline void saveGltf(const std::string& filePath, nlohmann::json& state, ux3d::datastorage::DataStorage dataStorage)
{
    nlohmann::json taskDescription {
        {
            "save_file",
            {
                { "filepath", filePath },
                { "overwrite", true },
            },
        }
    };
    state = g_library.call(std::move(taskDescription), std::move(state), dataStorage);
}
