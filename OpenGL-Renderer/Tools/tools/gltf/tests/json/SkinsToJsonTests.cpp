// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/SkinsToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("SkinsToJson", "[tools.gltf.SkinsToJson]")
{
    Gltf gltf;

    Skin* pSkin = gltf.create<Skin>();
    REQUIRE(pSkin != nullptr);

    Accessor* pInverseBindMatrices = gltf.create<Accessor>();
    Node* pSkeletonRoot = gltf.create<Node>();
    Node* pJoint1 = gltf.create<Node>();
    Node* pJoint2 = gltf.create<Node>();
    REQUIRE(pInverseBindMatrices != nullptr);
    REQUIRE(pSkeletonRoot != nullptr);
    REQUIRE(pJoint1 != nullptr);
    REQUIRE(pJoint2 != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    SkinsToJson converter(indices, options);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonSkin;
            converter.convert(*pSkin, jsonSkin, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonSkin.contains("name"));
            }
            THEN("has no inverse bind matrices accessor")
            {
                CHECK_FALSE(jsonSkin.contains("inverseBindMatrices"));
            }
            THEN("has no skeleton root")
            {
                CHECK_FALSE(jsonSkin.contains("skeleton"));
            }
            THEN("has no joints")
            {
                CHECK_FALSE(jsonSkin.contains("joints"));
            }
        }
    }

    GIVEN("all values explicitly set")
    {
        core::string name = "hello sailor!";
        pSkin->setName(name);
        pSkin->setInverseBindMatricesAccessor(*pInverseBindMatrices);
        pSkin->setSkeletonRoot(*pSkeletonRoot);
        pSkin->addJoint(*pJoint1);
        pSkin->addJoint(*pJoint2);

        WHEN("converted")
        {
            nlohmann::json jsonSkin;
            converter.convert(*pSkin, jsonSkin, {});

            THEN("has correct name")
            {
                REQUIRE(jsonSkin.contains("name"));
                CHECK(jsonSkin["name"].get<core::string>() == name);
            }
            THEN("has correct inverse bind matrices accessor index")
            {
                REQUIRE(jsonSkin.contains("inverseBindMatrices"));
                size_t index = indices[pInverseBindMatrices];
                CHECK(jsonSkin["inverseBindMatrices"].get<uint32_t>() == index);
            }
            THEN("has correct skeleton root node index")
            {
                REQUIRE(jsonSkin.contains("skeleton"));
                size_t index = indices[pSkeletonRoot];
                CHECK(jsonSkin["skeleton"].get<uint32_t>() == index);
            }
            THEN("has correct joint indices")
            {
                REQUIRE(jsonSkin.contains("joints"));
                nlohmann::json jsonJoints = jsonSkin["joints"];
                REQUIRE(jsonJoints.size() == 2);

                size_t index1 = indices[pJoint1];
                size_t index2 = indices[pJoint2];

                CHECK(jsonJoints[0].get<uint32_t>() == index1);
                CHECK(jsonJoints[1].get<uint32_t>() == index2);
            }
        }
    }
}
}
