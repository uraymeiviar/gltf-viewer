// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "reactor/Utils.h"
#include "gtest/gtest.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace ux3d;
using namespace ux3d::reactor;

TEST(Utils, splitString)
{
    std::string input = "apfelabcbirne";
    std::string delimiter = "abc";
    std::vector<std::string> output = reactor::utils::splitString(input, delimiter);

    EXPECT_EQ(output.size(), 2);
    EXPECT_EQ(output[0].compare("apfel"), 0);
    EXPECT_EQ(output[1].compare("birne"), 0);
}

TEST(Utils, mergeString)
{
    std::vector<std::string> input;
    input.push_back("apfel");
    input.push_back("birne");
    std::string separator = "abc";
    std::string output = reactor::utils::mergeString(input, separator);

    EXPECT_EQ(output.compare("apfelabcbirne"), 0);
}

TEST(Utils, hasElement)
{
    auto json = R"(
            {
                "arr": [
                    {
                        "obj0": {
                            "value0":0,
                            "value1":1
                        }
                    },
                    {}
                ]
            }
            )"_json;

    EXPECT_TRUE(utils::hasElement(json, { "arr", 0 }));
    EXPECT_TRUE(utils::hasElement(json, { "arr", 0, "obj0", "value1" }));
    EXPECT_FALSE(utils::hasElement(json, { "arr", 2, "obj0", "value1" }));
    EXPECT_FALSE(utils::hasElement(json, { "arr", 0, 0, "value1" }));
}

TEST(Utils, createSubElement)
{
    auto json = R"(
            {
                "arr": [
                    {
                        "obj0": {
                            "value0":0,
                            "value1":1
                        }
                    },
                    {}
                ]
            }
            )"_json;

    // check if function overwrites existing elements
    utils::createSubElement(json, { "arr", 0 }); // already exists
    EXPECT_TRUE(utils::hasElement(json, { "arr", 0, "obj0" })); // element is still there

    EXPECT_FALSE(utils::hasElement(json, { "arr2", 2, "abc" }));
    utils::createSubElement(json, { "arr2", 2, "abc" }); // new object and array
    EXPECT_TRUE(utils::hasElement(json, { "arr2", 2, "abc" }));
}

TEST(Utils, createURIStem)
{
    EXPECT_NO_THROW(utils::createURIStem(nlohmann::json::object()));

    nlohmann::json state = nlohmann::json::object();
    state["gltf"] = nlohmann::json::object();
    state["files"] = nlohmann::json::object();
    state["files"]["abc"] = "key";

    EXPECT_TRUE(utils::createURIStem(state).compare("file_0") == 0);

    state["files"]["file_0"] = "key";
    state["files"]["file_1.bin"] = "key";
    state["files"]["file_2.bin"] = "key";
    state["files"]["file_3.bak.bin"] = "key";

    EXPECT_TRUE(utils::createURIStem(state).compare("file_4") == 0);
}
