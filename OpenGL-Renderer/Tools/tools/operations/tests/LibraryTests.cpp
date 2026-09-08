#include "gtest/gtest.h"

#include "Gltfs.h"
#include "Helpers.h"

#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"

using namespace ux3d;
using namespace ux3d::operations;

class LibraryTests : public OperationsTests {
};

TEST_F(LibraryTests, scheduleOperation)
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

TEST_F(LibraryTests, invalidParameterName)
{
    auto taskDescription = R"(
        {
            "set_gltf_value": {
                "blarf": "/gltf/nodes/0/name",
                "value": "new name"
            }
        }
    )"_json;

    EXPECT_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage), std::invalid_argument);
}

TEST_F(LibraryTests, invalidJsonPointerArgument)
{
    auto taskDescription = R"(
        {
            "set_gltf_value": {
                "path": "/glterf/noedels/0/name",
                "value": "new name"
            }
        }
    )"_json;

    EXPECT_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage), std::invalid_argument);
}

TEST_F(LibraryTests, invalidOperationName)
{
    auto taskDescription = R"(
        {
            "göneräted.setGlerftValue 🙀": {
                "path": "/gltf/nodes/0/name",
                "value": "new name"
            }
        }
    )"_json;

    EXPECT_THROW(g_library.call(std::move(taskDescription), nlohmann::json(m_box), m_dataStorage), std::invalid_argument);
}

TEST_F(LibraryTests, loadLibraryPlugin)
{
    // Create a new library so that the library shared between tests is not modified by this test
    // (a copy doesn't work because the json_validator is not copyable)
    auto library = Library();
    auto state = nlohmann::json::object({ { "message", "" } });

    std::vector<Operation> pluginOperations = {
        Operation {
            .name = "test.setString",
            .schema = schema::operation(
                "TestOperation",
                "Test operation description",
                schema::object({ { "value", schema::string() } })),
            .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library&, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
                state["message"] = params["value"];
                nlohmann::json info;
                info["message"] = "Value changed";
                callback(info);
                return state;
            } }
    };

    library.loadPlugin(pluginOperations);

    auto taskDescription = R"(
        {
            "test.setString": {
                "value": "Hello Plugin"
            }
        }
    )"_json;

    std::string successMessage = "";
    auto callback = [&successMessage](const nlohmann::json& input) { successMessage = input.at("message").get<std::string>(); };

    state = library.call(std::move(taskDescription), std::move(state), m_dataStorage, callback);

    EXPECT_EQ("Hello Plugin", state["message"]);
    EXPECT_TRUE(successMessage == "Value changed");
}