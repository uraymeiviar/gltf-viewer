// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "gtest/gtest.h"

#include "Gltfs.h"
#include "operations/Library.h"
#include "operations/Schema.h"
#include "reactor/Operations.h"
#include <gltf_schema/gltf_single_json.h>

#include <iostream>

using namespace ux3d;

class ReactorOperationsTests : public ::testing::Test {
protected:
    nlohmann::json gltfSchema;
    nlohmann::json linearizedSchema;

    static void SetUpTestSuite()
    {
        auto schemas = gltf_schema::merged();
        s_gltfSchema = reactor::schema::resolveRefs(gltf_schema::merged(), "glTF.schema.json");
        s_linearizedSchema = reactor::schema::linearize(s_gltfSchema);
    }

    virtual void SetUp()
    {
        gltfSchema = s_gltfSchema;
        linearizedSchema = s_linearizedSchema;
    }

    virtual void TearDown()
    {
    }

    inline static nlohmann::json s_gltfSchema;
    inline static nlohmann::json s_linearizedSchema;
};

TEST_F(ReactorOperationsTests, getOperationsByPath)
{
    auto schemaJson = gltf_schema::merged();
    ux3d::operations::Library library;

    library.loadPlugin({ ux3d::operations::Operation {
        .name = "OperationName",

        .schema = ux3d::operations::schema::operation(
            "Some Operation Title",
            "Some Operation Description",
            ux3d::operations::schema::object({ ux3d::operations::schema::parameter::path("/some/path/\\d+"), { "value", ux3d::operations::schema::any() } })),

        .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const ux3d::operations::Library&, std::function<void(const nlohmann::json&)>) -> nlohmann::json {
            return state;
        } } });

    auto byNameJson = reactor::operations::byName(library.schema);
    auto byPathPattern = reactor::operations::byPathPattern(byNameJson);
    std::string path = "/some/path/0";
    nlohmann::json operationsByPath = reactor::operations::byPath(path, byPathPattern);

    EXPECT_TRUE(std::find(operationsByPath.begin(), operationsByPath.end(), "OperationName") != operationsByPath.end());

    EXPECT_FALSE(std::find(operationsByPath.begin(), operationsByPath.end(), "abcedasdf") != operationsByPath.end());
}

TEST_F(ReactorOperationsTests, byName)
{
    ux3d::operations::Library library;
    library.loadPlugin({ ux3d::operations::Operation {
        .name = "OperationName",

        .schema = ux3d::operations::schema::operation(
            "Some Operation Title",
            "Some Operation Description",
            ux3d::operations::schema::object({ ux3d::operations::schema::parameter::path(".*"), { "value", ux3d::operations::schema::any() } })),

        .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const ux3d::operations::Library&, std::function<void(const nlohmann::json&)>) -> nlohmann::json {
            return state;
        } } });

    auto operationsByName = reactor::operations::byName(library.schema);

    EXPECT_TRUE(operationsByName.contains("OperationName"));
}

TEST_F(ReactorOperationsTests, byPathPattern)
{
    ux3d::operations::Library library;
    library.loadPlugin({ ux3d::operations::Operation {
        .name = "OperationName",

        .schema = ux3d::operations::schema::operation(
            "Some Operation Title",
            "Some Operation Description",
            ux3d::operations::schema::object({ ux3d::operations::schema::parameter::path("/some/path/\\d+"), { "value", ux3d::operations::schema::any() } })),

        .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const ux3d::operations::Library&, std::function<void(const nlohmann::json&)>) -> nlohmann::json {
            return state;
        } } });

    auto operationsByPathPattern = reactor::operations::byPathPattern(reactor::operations::byName(library.schema));

    auto pathPointer = "/some/path/\\d+";
    EXPECT_TRUE(operationsByPathPattern.contains(pathPointer));
    EXPECT_TRUE(operationsByPathPattern[pathPointer].contains("OperationName"));
}
