
#include "gtest/gtest.h"

#include "Gltfs.h"
#include "Helpers.h"

#include "operations/Library.h"

using namespace ux3d;
using namespace ux3d::operations;

class DangerousOperationsTests : public OperationsTests {
};

TEST_F(DangerousOperationsTests, setValue)
{
    auto taskDescription = R"(
        {
            "dangerous.setValue": {
                "path": "/gltf/nodes/0/name",
                "value": "new name"
            }
        }
    )"_json;

    auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

    EXPECT_EQ(state["gltf"]["nodes"][0]["name"], "new name");
}

TEST_F(DangerousOperationsTests, removeValue)
{
    auto taskDescription = R"(
        {
            "dangerous.removeValue": {
                "path": "/gltf/nodes/0/name"
            }
        }
    )"_json;

    auto state = g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage);

    EXPECT_FALSE(state["gltf"]["nodes"][0].contains("name"));
}

TEST_F(DangerousOperationsTests, combineOperations)
{
    nlohmann::json taskDescriptions = nlohmann::json::array();
    auto box = nlohmann::json(m_box);
    size_t nodeCount = box.at("/gltf/nodes"_json_pointer).size();
    ASSERT_EQ(nodeCount, 3);
    auto taskDescription = R"(
        {
            "remove": {
                "path": "/gltf/nodes/0"
            }
        }
    )"_json;
    taskDescriptions.push_back(taskDescription);
    taskDescriptions.push_back(taskDescription);
    taskDescription = { { "dangerous.combineOperations", taskDescriptions } };
    EXPECT_NO_THROW(box = g_library.call(std::move(taskDescription), std::move(box), m_dataStorage));
    EXPECT_EQ(box.at("/gltf/nodes"_json_pointer).size(), nodeCount - 2);
    EXPECT_ANY_THROW(g_library.call(std::move(taskDescription), std::move(box), m_dataStorage));
}
