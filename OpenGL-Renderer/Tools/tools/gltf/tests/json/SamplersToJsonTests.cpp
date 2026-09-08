// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/SamplersToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("SamplersToJson", "[tools.gltf.SamplersToJson]")
{
    Gltf gltf;

    Sampler* pSampler = gltf.create<Sampler>();
    REQUIRE(pSampler != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    SamplersToJson converter(indices, options);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonSampler;
            converter.convert(*pSampler, jsonSampler, {});

            THEN("has nothing")
            {
                CHECK(jsonSampler.empty());
            }
        }
    }

    GIVEN("all values explicitly set")
    {
        core::string name = "good name for a light";

        pSampler->setName(name);

        TextureFilterType testMagFilterType = TextureFilterType::LinearMipmapLinear;
        pSampler->setMagFilter(testMagFilterType);

        TextureFilterType testMinFilterType = TextureFilterType::NearestMipmapLinear;
        pSampler->setMinFilter(testMinFilterType);

        TextureWrappingMode testWrapS = TextureWrappingMode::MirroredRepeat;
        pSampler->setWrapS(testWrapS);

        TextureWrappingMode testWrapT = TextureWrappingMode::ClampToEdge;
        pSampler->setWrapT(testWrapT);

        WHEN("converted")
        {
            nlohmann::json jsonSampler;
            converter.convert(*pSampler, jsonSampler, {});

            THEN("has correct magFilter")
            {
                REQUIRE(jsonSampler.contains("magFilter"));
                CHECK(jsonSampler["magFilter"].get<int32_t>() == static_cast<int32_t>(testMagFilterType));
            }
            THEN("has correct minFilter")
            {
                REQUIRE(jsonSampler.contains("minFilter"));
                CHECK(jsonSampler["minFilter"].get<int32_t>() == static_cast<int32_t>(testMinFilterType));
            }
            THEN("has correct wrapS")
            {
                REQUIRE(jsonSampler.contains("wrapS"));
                CHECK(jsonSampler["wrapS"].get<int32_t>() == static_cast<int32_t>(testWrapS));
            }
            THEN("has correct wrapT")
            {
                REQUIRE(jsonSampler.contains("wrapT"));
                CHECK(jsonSampler["wrapT"].get<int32_t>() == static_cast<int32_t>(testWrapT));
            }
        }
    }
}
}
