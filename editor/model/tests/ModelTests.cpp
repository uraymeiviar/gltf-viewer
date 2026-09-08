// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "gtest/gtest.h"

#include "Gltfs.h"

#include "model/EditorModel.h"
#include "model/GltfModel.h"
#include "model/ProjectManager.h"
#include "reactor/Json.h"

using namespace ux3d;
using namespace ux3d::model;

class ModelTests : public ::testing::Test {
protected:
    nlohmann::json m_box = nlohmann::json::object({ { "gltf", nlohmann::json::parse(boxGltf) } });
    Gltf m_model = Gltf(&Gltf::observeOnSameThread);

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
        m_model.clearHistory();
    }
};

TEST_F(ModelTests, projectManager)
{
    auto pm = ProjectManager::getInstance();
}

TEST_F(ModelTests, simpleExample)
{
    nlohmann::json state;
    bool hasBeenCalled = false;
    m_model.head.subscribe([&](nlohmann::json newState) {
        state = newState;
        hasBeenCalled = true;
    });

    m_model.apply(nlohmann::json(m_box));
    EXPECT_EQ(state["gltf"]["nodes"][0]["name"], "first");
    EXPECT_TRUE(hasBeenCalled);

    m_model.schedule(R"(
        {
            "set_gltf_value": {
                "path": "/gltf/nodes/0/name",
                "value": "new name"
            }
        }
    )"_json);

    EXPECT_EQ(state["gltf"]["nodes"][0]["name"], "new name");

    m_model.undo();
    EXPECT_EQ(state["gltf"]["nodes"][0]["name"], "first");
}

TEST_F(ModelTests, headWindowed)
{
    std::vector<std::string> values;

    auto subscription = m_model.headWindowed.subscribe([&values](const rxcpp::observable<nlohmann::json>& heads) {
        values.emplace_back("head_window_created");
        heads.subscribe(
            [&values](const nlohmann::json& head) {
                values.emplace_back("head_emitted");
            },
            [](std::exception_ptr ep) {
                std::rethrow_exception(ep);
            },
            [&values] {
                values.emplace_back("head_complete");
            }); },
        [](std::exception_ptr ep) {
            std::rethrow_exception(ep);
        });

    m_model.apply(nlohmann::json(m_box));
    m_model.clearHistory();
    m_model.apply(nlohmann::json(m_box));
    m_model.apply(nlohmann::json(m_box));

    std::vector<std::string> expected = { "head_window_created", "head_emitted", "head_window_created", "head_complete", "head_emitted", "head_emitted" };
    EXPECT_EQ(values, expected);

    subscription.unsubscribe();
}

TEST_F(ModelTests, diffs)
{
    // Accumulate diff patches in a json object to check if they add up to the same
    // gltf that is stored in head after applying some changes to the model

    std::vector<nlohmann::json> values;
    values.reserve(10); // make sure we don't need to reallocate
    auto subscription = m_model.diffs.subscribe(
        [&values](const rxcpp::observable<nlohmann::json>& diffs) {
            auto& json = values.emplace_back(nlohmann::json::object());
            diffs.subscribe(
                [&json](const nlohmann::json& diff) {
                    json = json.patch(diff);
                },
                [](std::exception_ptr ep) {
                    std::rethrow_exception(ep);
                });
        },
        [](std::exception_ptr ep) {
            std::rethrow_exception(ep);
        });

    // Subscribe to head so that we have something to compare against
    nlohmann::json gltf;
    auto headSubscription = m_model.headRaw.subscribe(
        [&gltf](const nlohmann::json& newHead) {
            gltf = newHead["gltf"];
        });

    // Make some changes to the model
    m_model.apply(nlohmann::json(m_box));
    m_model.schedule(R"(
        {
            "set_gltf_value": {
                "path": "/gltf/nodes/0/name",
                "value": "new name"
            }
        }
    )"_json);

    // Applying all diffs should give the same json as the current model head
    EXPECT_EQ(gltf, values[0]);

    // Cleanup subscriptions that would otherwise outlive the test
    subscription.unsubscribe();
    headSubscription.unsubscribe();
}

TEST_F(ModelTests, newGltfIndex)
{
    std::vector<int> values;

    auto subscription = m_model.newGltfIndex.subscribe([&values](int index) { values.emplace_back(index); },
        [](std::exception_ptr ep) {
            std::rethrow_exception(ep);
        });

    // subscribing to newGltfIndex without having loaded a gltf should not trigger the subscription
    // function
    EXPECT_TRUE(values.empty());

    m_model.apply(nlohmann::json(m_box));
    m_model.clearHistory();
    m_model.apply(nlohmann::json(m_box));
    m_model.apply(nlohmann::json(m_box));

    // The index in newGltf should increase iff the history was empty before pushing a new value
    std::vector<int> expected = { 1, 2 };
    EXPECT_EQ(values, expected);

    // Cleanup to avoid dangling subscriptions
    subscription.unsubscribe();
}

TEST_F(ModelTests, uuids)
{
    //

    std::vector<nlohmann::json> values;
    values.reserve(10); // make sure we don't need to reallocate

    auto subscription = m_model.uuids.subscribe(
        [&values](const nlohmann::json& uuids) { values.emplace_back(uuids); },
        [](std::exception_ptr ep) {
            std::rethrow_exception(ep);
        });

    // subscribing to uuids without having loaded a gltf should not trigger the subscription
    // function
    EXPECT_TRUE(values.empty());

    // Push a gltf (all new uuids)
    m_model.apply(nlohmann::json(m_box));

    // Making changes to primitive values should not change the UUIDs
    // TODO: we can even add a test to check that the observable doesn't fire in this case
    auto uuids = values.back();
    m_model.schedule(R"(
        {
            "set_gltf_value": {
                "path": "/gltf/nodes/0/name",
                "value": "new name"
            }
        }
    )"_json);
    EXPECT_EQ(uuids, values.back());

    // Creating an object (even at the start of a list) should lead to a diff of size 1
    m_model.schedule(R"(
        {
            "add_new_node": {
                "path": "/gltf/nodes/0"
            }
        }
    )"_json);
    EXPECT_EQ(reactor::json::minimal_diff(uuids, values.back()).size(), 1);

    // When loading a new gltf, it must not contain the UUIDs of the previous state
    m_model.clearHistory();
    m_model.apply(nlohmann::json(m_box));
    auto oldUUIDs = reactor::json::values(uuids.flatten());
    auto newUUIDs = reactor::json::values(values.back().flatten());
    for (const auto& oldUUID : oldUUIDs) {
        for (const auto& newUUID : newUUIDs) {
            // Check that no pair of oldUUID & newUUID satisfies the condition (oldUUID == newUUID)
            EXPECT_NE(oldUUID.get<std::string>(), newUUID.get<std::string>());
        }
    }

    // Cleanup subscriptions that would otherwise outlive the test
    subscription.unsubscribe();
}
