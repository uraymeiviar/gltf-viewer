// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "gtest/gtest.h"

#include "Gltfs.h"
#include "reactor/Gltf.h"
#include "reactor/Json.h"
#include <gltf_schema/gltf_single_json.h>

#include <gsl/span>
#include <iostream>
#include <uuid.h>

using namespace ux3d;
using namespace ux3d::reactor;

TEST(Gltf, validateNodeHierarchies)
{
    auto boxGltf = nlohmann::json::parse(box);
    auto validationResults = gltf::validateNodeHierarchies(boxGltf);
    EXPECT_EQ(validationResults.size(), 0);
}

TEST(Gltf, propertyUUIDs)
{
    auto previousGltf = nlohmann::json::object();
    auto boxGltf = nlohmann::json::parse(box);
    auto uuids = nlohmann::json::object();

    auto minimalDiff = json::minimal_diff(previousGltf, boxGltf);

    uuids = gltf::propertyUUIDs(std::move(uuids), minimalDiff);

    auto node1UUID = uuids["nodes"][1]["uuid"];

    previousGltf = boxGltf;
    boxGltf["nodes"].erase(0);

    minimalDiff = json::minimal_diff(previousGltf, boxGltf);

    uuids = gltf::propertyUUIDs(std::move(uuids), minimalDiff);

    // UUIDs should survive a remove of a node
    EXPECT_EQ(node1UUID, uuids["nodes"][0]["uuid"]);

    // UUIDs must be parsable by stduuid
    auto id = uuids::uuid::from_string(uuids["nodes"][0]["uuid"].get<std::string>());
    EXPECT_TRUE(id.has_value());
    EXPECT_EQ(uuids::to_string(id.value()), uuids["nodes"][0]["uuid"].get<std::string>());
}

TEST(Gltf, gatherUnusedNodes)
{
    auto boxGltf = nlohmann::json::parse(box);
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");
    auto linearizedSchema = schema::linearize(schemaJson);
    auto refs = gltf::schema::backwardReferences(linearizedSchema);

    // In the box glTF, all nodes are used so the result should be empty
    // (tests that node and scene parents are handled correctly)
    auto unusedNodes = gltf::gatherUnusedNodes(boxGltf, refs);
    EXPECT_TRUE(unusedNodes.empty());

    // Add a empty node to the gltf, this should be detected as an unused node.
    auto unusedNode = R"({ "name": "Unused Node" })"_json;
    boxGltf["nodes"].push_back(unusedNode);
    unusedNodes = gltf::gatherUnusedNodes(boxGltf, refs);
    ASSERT_FALSE(unusedNodes.empty());
    EXPECT_EQ(boxGltf[unusedNodes.back()], unusedNode);

    // A node that is used in a skin as a joint should not be detected as unused
    boxGltf = nlohmann::json::parse(box);
    auto jointNode = R"({ "name": "Joint Node" })"_json;
    boxGltf["nodes"].push_back(jointNode);
    boxGltf["skins"] = R"([{
        "joints": [2]
    }])"_json;
    unusedNodes = gltf::gatherUnusedNodes(boxGltf, refs);
    EXPECT_TRUE(unusedNodes.empty());

    // This should all still work when number refs are replaced with resolved refs
    boxGltf = resolveGltfReferences(linearizedSchema, boxGltf);
    unusedNodes = gltf::gatherUnusedNodes(boxGltf, refs);
    EXPECT_TRUE(unusedNodes.empty());
}

TEST(Gltf, schema_backwardReferences)
{
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");
    auto linear = schema::linearize(schemaJson);

    auto refs = gltf::schema::backwardReferences(linear);

    auto& meshRefs = refs.at("/meshes/\\d+");
    auto it = std::find(meshRefs.begin(), meshRefs.end(), "/nodes/\\d+/mesh");
    EXPECT_TRUE(it != meshRefs.end());
}

TEST(Gltf, pathPrefix)
{
    auto result = gltf::applyParentToReference("/random/string", "/accessors");
    EXPECT_EQ(result, "/accessors");
    result = gltf::applyParentToReference("/animations/2/channels/0/sampler", "/animations/\\d+/samplers");
    EXPECT_EQ(result, "/animations/2/samplers");
    result = gltf::applyParentToReference("/animations/10/samplers/5/other/path/which/0/is/unrelated", "/animations/\\d+/samplers/\\d+/tests");
    EXPECT_EQ(result, "/animations/10/samplers/5/tests");
}

TEST(Gltf, getGlobalPath)
{
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");
    auto linearizedSchema = schema::linearize(schemaJson);
    auto linearizedSchemaResolved = reactor::json::resolveAllOf(linearizedSchema);
    {
        auto result = gltf::schema::getGlobalPath(linearizedSchemaResolved, "/gltf/nodes/1");
        EXPECT_EQ(result, "/gltf/nodes/1");
    }
    {
        auto result = gltf::schema::getGlobalPath(linearizedSchemaResolved, "/gltf/nodes/1/name");
        EXPECT_EQ(result, "/gltf/nodes/1");
    }
    {
        auto result = gltf::schema::getGlobalPath(linearizedSchemaResolved, "/gltf/scenes/1/nodes/1");
        EXPECT_EQ(result, "/gltf/scenes/1");
    }
    {
        auto result = gltf::schema::getGlobalPath(linearizedSchemaResolved, "/gltf/meshes/1/primitives/1/mode");
        EXPECT_EQ(result, "/gltf/meshes/1/primitives/1");
    }
    {
        auto result = gltf::schema::getGlobalPath(linearizedSchemaResolved, "/gltf/extensions/KHR_lights_punctual/lights/1/color");
        EXPECT_EQ(result, "/gltf/extensions/KHR_lights_punctual/lights/1");
    }
    {
        auto result = gltf::schema::getGlobalPath(linearizedSchemaResolved, "/gltf/nodes/1/extensions/KHR_lights_punctual/light");
        EXPECT_EQ(result, "/gltf/nodes/1/extensions/KHR_lights_punctual");
    }
}

TEST(Gltf, getLocalPath)
{
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");
    auto linearizedSchema = schema::linearize(schemaJson);
    auto linearizedSchemaResolved = reactor::json::resolveAllOf(linearizedSchema);

    {
        auto result = gltf::schema::getLocalPath(linearizedSchemaResolved, "/gltf/nodes/1");
        EXPECT_EQ(result, "");
    }
    {
        auto result = gltf::schema::getLocalPath(linearizedSchemaResolved, "/gltf/nodes/1/name");
        EXPECT_EQ(result, "/name");
    }
    {
        auto result = gltf::schema::getLocalPath(linearizedSchemaResolved, "/gltf/scenes/1/nodes/1");
        EXPECT_EQ(result, "/nodes/1");
    }
    {
        auto result = gltf::schema::getLocalPath(linearizedSchemaResolved, "/gltf/meshes/1/primitives/1/mode");
        EXPECT_EQ(result, "/mode");
    }
    {
        auto result = gltf::schema::getLocalPath(linearizedSchemaResolved, "/gltf/extensions/KHR_lights_punctual/lights/1/color");
        EXPECT_EQ(result, "/color");
    }
    {
        auto result = gltf::schema::getLocalPath(linearizedSchemaResolved, "/gltf/nodes/1/extensions/KHR_lights_punctual/light");
        EXPECT_EQ(result, "/light");
    }
}
