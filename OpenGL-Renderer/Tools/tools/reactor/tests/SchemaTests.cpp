// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "gtest/gtest.h"

#include "Gltfs.h"
#include "reactor/Schema.h"
#include <gltf_schema/gltf_single_json.h>

#include <iostream>

using namespace ux3d;
using namespace ux3d::reactor;

TEST(Schema, resolveRefs)
{
    auto schemaJson = gltf_schema::merged();
    EXPECT_NO_THROW(schema::resolveRefs(schemaJson, "glTF.schema.json"));
}

TEST(Schema, linearize)
{
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");

    EXPECT_NO_THROW(auto linear = schema::linearize(schemaJson));
}

TEST(Schema, patternForEach)
{
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");
    const auto boxGltf = nlohmann::json::parse(box);

    std::vector<std::string> names;
    schema::patternForEach("/nodes/\\d+/name", boxGltf, [&](const nlohmann::json::json_pointer& pointer) {
        names.push_back(boxGltf.at(pointer));
    });

    EXPECT_EQ(names[0], "first");
}

TEST(Schema, mapPointersToPatterns)
{
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");
    const auto boxGltf = nlohmann::json::parse(box);

    auto linear = schema::linearize(schemaJson);
    auto map = schema::mapPointersToPatterns(linear, boxGltf);

    EXPECT_EQ(map.at("/nodes/0/name"), "/nodes/\\d+/name");
}

TEST(Schema, mapPatternsToPointers)
{
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");
    const auto boxGltf = nlohmann::json::parse(box);

    auto linear = schema::linearize(schemaJson);
    auto map = schema::mapPatternsToPointers(linear, boxGltf);

    auto nodeNames = map.at("/nodes/\\d+/name");
    auto it = std::find(nodeNames.begin(), nodeNames.end(), "/nodes/0/name");

    EXPECT_TRUE(it != nodeNames.end());
}

TEST(Schema, mapToPattern)
{
    auto schemaJson = gltf_schema::merged();
    schemaJson = schema::resolveRefs(schemaJson, "glTF.schema.json");

    auto linear = schema::linearize(schemaJson);
    auto mapped = schema::mapToPattern(linear, "/nodes/0/name");

    EXPECT_EQ(mapped, "/nodes/\\d+/name");
}
