// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "gtest/gtest.h"

#include "Gltfs.h"
#include "reactor/Json.h"
#include "reactor/Reactor.h"
#include <gltf_schema/gltf_single_json.h>

#include <chrono>
#include <iostream>

using namespace ux3d;
using namespace ux3d::reactor;

// TEST_CASE("validateGltf")
//{
//     auto schemaJson = nlohmann::json::parse(gltfSchemas);
//     auto boxGltf = nlohmann::json::parse(box);
//     CHECK_NOTHROW(validate(schemaJson, boxGltf));
// }

TEST(Reactor, zipGltf)
{
    auto schemaJson = gltf_schema::merged();
    auto boxGltf = nlohmann::json::parse(box);
    auto zipped = zipSchema(schemaJson, boxGltf);

    auto nodeRefPath = nlohmann::json::json_pointer("/scenes/0/nodes/0");
    auto prefixPath = nlohmann::json::json_pointer(schemaJson.at(nlohmann::json::json_pointer(zipped.at("/scenes/0/nodes/0"))).at("ux3d_pathPrefix"));
    EXPECT_EQ(boxGltf.at(prefixPath / boxGltf.at(nodeRefPath).get<int>()).at("name"), "first");
}

TEST(Reactor, resolveGltfReferences)
{
    auto schemaJson = ux3d::reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    auto boxGltf = nlohmann::json::parse(box);

    auto linearizedSchema = reactor::schema::linearize(schemaJson);

    auto referencesGltf = resolveGltfReferences(linearizedSchema, boxGltf);

    EXPECT_EQ(referencesGltf["nodes"][0]["children"][0].get<std::string>(), "/nodes/1");
}

TEST(Reactor, resolveGltfHierarchy)
{
    auto schemaJson = ux3d::reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    auto linearizedSchema = reactor::schema::linearize(schemaJson);
    nlohmann::json filteredSchema;
    for (auto item : linearizedSchema.items()) {
        if (item.value().contains("ux3d_pathPrefix")) {
            filteredSchema[item.key()] = item.value();
        }
    }

    auto boxGltf = nlohmann::json::parse(box);
    EXPECT_NO_THROW(resolveGltfHierarchy(filteredSchema, linearizedSchema, boxGltf));
}

TEST(Reactor, translateIDsToUUIDs)
{
    auto schemaJson = ux3d::reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    auto linearizedSchema = reactor::schema::linearize(schemaJson);
    nlohmann::json filteredSchema;
    for (auto item : linearizedSchema.items()) {
        if (item.value().contains("ux3d_pathPrefix")) {
            filteredSchema[item.key()] = item.value();
        }
    }

    auto boxGltf = nlohmann::json::parse(box);

    auto previousGltf = nlohmann::json::object();
    auto uuids = nlohmann::json::object();
    auto minimalDiff = json::minimal_diff(previousGltf, boxGltf);
    uuids = gltf::propertyUUIDs(std::move(uuids), minimalDiff);

    auto hierarchy = resolveGltfHierarchy(filteredSchema, linearizedSchema, boxGltf);

    EXPECT_NO_THROW(translateIDsToUUIDs(hierarchy, uuids));
}

TEST(Reactor, extractSubtree)
{
    auto schemaJson = ux3d::reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    auto linearizedSchema = reactor::schema::linearize(schemaJson);

    auto boxGltf = nlohmann::json::parse(box);

    {
        std::set<nlohmann::json::json_pointer> subTree;
        nlohmann::json::json_pointer from("/nodes/0");

        EXPECT_NO_THROW(subTree = extractSubtree(boxGltf, from, linearizedSchema));
        EXPECT_EQ(subTree.size(), 16);
    }

    {
        std::set<nlohmann::json::json_pointer> subTree;
        nlohmann::json::json_pointer from("/scenes/0");

        EXPECT_NO_THROW(subTree = extractSubtree(boxGltf, from, linearizedSchema, false));
        EXPECT_EQ(subTree.size(), 16);
    }

    {
        std::set<nlohmann::json::json_pointer> subTree;
        nlohmann::json::json_pointer from("/scenes/0");

        EXPECT_NO_THROW(subTree = extractSubtree(boxGltf, from, linearizedSchema, true));
        EXPECT_EQ(subTree.size(), 18);
    }

    // xmp test
    {
        std::set<nlohmann::json::json_pointer> subTree;
        nlohmann::json::json_pointer from("/asset");

        EXPECT_NO_THROW(subTree = extractSubtree(boxGltf, from, linearizedSchema));
        EXPECT_EQ(subTree.size(), 1);
    }
}

TEST(Reactor, cleanSubTreeElementsFromArray)
{
    auto schemaJson = ux3d::reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
    auto linearizedSchema = reactor::schema::linearize(schemaJson);

    auto boxGltf = nlohmann::json::parse(box);

    {
        nlohmann::json selectionArray = nlohmann::json::array();
        selectionArray.push_back({ { "absolutePath", "/nodes/0" } });
        nlohmann::json cleanedArray = cleanSubTreeElementsFromArray(selectionArray, boxGltf, linearizedSchema);

        EXPECT_EQ(cleanedArray.size(), 1);
    }

    {
        nlohmann::json selectionArray = nlohmann::json::array();
        selectionArray.push_back({ { "absolutePath", "/nodes/0" } });
        selectionArray.push_back({ { "absolutePath", "/nodes/1" } });
        nlohmann::json cleanedArray = cleanSubTreeElementsFromArray(selectionArray, boxGltf, linearizedSchema);

        EXPECT_EQ(cleanedArray.size(), 1);
    }

    {
        nlohmann::json selectionArray = nlohmann::json::array();
        selectionArray.push_back({ { "absolutePath", "/nodes/0" } });
        selectionArray.push_back({ { "absolutePath", "/nodes/1" } });
        selectionArray.push_back({ { "absolutePath", "/meshes/0" } });
        selectionArray.push_back({ { "absolutePath", "/meshes/0/primitives/0" } });
        nlohmann::json cleanedArray = cleanSubTreeElementsFromArray(selectionArray, boxGltf, linearizedSchema);

        EXPECT_EQ(cleanedArray.size(), 1);
    }

    {
        nlohmann::json selectionArray = nlohmann::json::array();
        selectionArray.push_back({ { "absolutePath", "/nodes/0" } });
        selectionArray.push_back({ { "absolutePath", "/nodes/1" } });
        selectionArray.push_back({ { "absolutePath", "/meshes/0" } });
        selectionArray.push_back({ { "absolutePath", "/asset" } });
        nlohmann::json cleanedArray = cleanSubTreeElementsFromArray(selectionArray, boxGltf, linearizedSchema);

        EXPECT_EQ(cleanedArray.size(), 2);
        EXPECT_FALSE(cleanedArray[1].is_null());
    }
}

TEST(Reactor, getReferenceCandidates)
{
    auto boxGltf = nlohmann::json::parse(box);
    auto references = getReferenceCandidates(boxGltf, "/nodes"_json_pointer);
    ASSERT_TRUE(references.size() == 2);
    EXPECT_TRUE(std::get<0>(references[0]) == "/nodes/0");
    EXPECT_TRUE(std::get<1>(references[0]) == "'first'");
    EXPECT_TRUE(std::get<0>(references[1]) == "/nodes/1");
    EXPECT_TRUE(std::get<1>(references[1]) == "");
}