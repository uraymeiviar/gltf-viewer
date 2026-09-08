// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/AnimationChannelsToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("AnimationChannelToJson", "[tools.gltf.AnimationChannelToJson]")
{
    Gltf gltf;

    Animation* pAnimation = gltf.create<Animation>();
    Node* pNode = gltf.create<Node>();
    REQUIRE(pAnimation != nullptr);
    REQUIRE(pNode != nullptr);

    AnimationChannel* pChannel = pAnimation->createChannel();
    AnimationSampler* pSampler = pAnimation->createSampler();
    REQUIRE(pChannel != nullptr);
    REQUIRE(pSampler != nullptr);

    IndexLookup indices(gltf);
    ExportOptions exportOptions;
    AnimationChannelsToJson converter(indices, exportOptions);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonChannel;
            converter.convert(*pChannel, jsonChannel);

            THEN("has no sampler")
            {
                CHECK_FALSE(jsonChannel.contains("sampler"));
            }
            THEN("has target")
            {
                REQUIRE(jsonChannel.contains("target"));
                nlohmann::json jsonTarget = jsonChannel["target"];

                AND_THEN("target has no node")
                {
                    CHECK_FALSE(jsonTarget.contains("node"));
                }
                AND_THEN("target has default path")
                {
                    REQUIRE(jsonTarget.contains("path"));
                    core::string defaultPath = animationPathToString(AnimationPath());
                    CHECK(jsonTarget["path"].get<core::string>() == defaultPath);
                }
            }
        }
    }

    GIVEN("all values explicitly set")
    {
        AnimationPath path = AnimationPath::Scale;
        pChannel->setSampler(*pSampler);
        pChannel->getTarget().setNode(*pNode);
        pChannel->getTarget().setPath(path);

        WHEN("converted")
        {
            nlohmann::json jsonChannel;
            converter.convert(*pChannel, jsonChannel);

            THEN("has correct sampler index")
            {
                REQUIRE(jsonChannel.contains("sampler"));
                int32_t index = static_cast<size_t>(core::findIndexOf(pAnimation->getSamplers(), pSampler));
                CHECK(jsonChannel["sampler"].get<int32_t>() == index);
            }
            THEN("has target")
            {
                REQUIRE(jsonChannel.contains("target"));
                nlohmann::json jsonTarget = jsonChannel["target"];

                AND_THEN("target has correct node index")
                {
                    REQUIRE(jsonTarget.contains("node"));
                    size_t index = indices[pNode];
                    CHECK(jsonTarget["node"].get<uint32_t>() == index);
                }
                AND_THEN("target has correct path")
                {
                    REQUIRE(jsonTarget.contains("path"));
                    CHECK(jsonTarget["path"].get<core::string>() == animationPathToString(path));
                }
            }
        }
    }
}
}
