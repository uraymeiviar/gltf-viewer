// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/AnimationsToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("AnimationsToJson", "[tools.gltf.AnimationsToJson]")
{
    Gltf gltf;

    Animation* pAnimation = gltf.create<Animation>();
    REQUIRE(pAnimation != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    AnimationsToJson converter(indices, options, gltf);

    GIVEN("nothing explicitly set")
    {
        WHEN("converted")
        {
            nlohmann::json jsonAnimation;
            converter.convert(*pAnimation, jsonAnimation, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonAnimation.contains("name"));
            }
            THEN("has no channels")
            {
                CHECK_FALSE(jsonAnimation.contains("channels"));
            }
            THEN("has no samplers")
            {
                CHECK_FALSE(jsonAnimation.contains("samplers"));
            }
        }
    }

    GIVEN("all values explicit set")
    {
        core::string name = "good riddance!";
        AnimationChannel* pChannel1 = pAnimation->createChannel();
        AnimationChannel* pChannel2 = pAnimation->createChannel();
        AnimationSampler* pSampler1 = pAnimation->createSampler();
        AnimationSampler* pSampler2 = pAnimation->createSampler();
        REQUIRE(pChannel1 != nullptr);
        REQUIRE(pChannel2 != nullptr);
        REQUIRE(pSampler1 != nullptr);
        REQUIRE(pSampler2 != nullptr);
        pAnimation->setName(name);

        WHEN("converted")
        {
            nlohmann::json jsonAnimation;
            converter.convert(*pAnimation, jsonAnimation, {});

            THEN("has correct name")
            {
                REQUIRE(jsonAnimation.contains("name"));
                CHECK(jsonAnimation["name"].get<core::string>() == name);
            }
            THEN("has correct count of channels")
            {
                // channel conversion happens in a different class with its own tests
                REQUIRE(jsonAnimation.contains("channels"));
                CHECK(jsonAnimation["channels"].size() == 2);
            }
            THEN("has correct count of samplers")
            {
                // sampler conversion happens in a different class with its own tests
                REQUIRE(jsonAnimation.contains("samplers"));
                CHECK(jsonAnimation["samplers"].size() == 2);
            }
        }
    }
}
}
