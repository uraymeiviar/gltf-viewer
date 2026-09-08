// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/AnimationSamplersToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("AnimationSamplerToJson", "[tools.gltf.AnimationSamplerToJson]")
{
    Gltf gltf;

    Animation* pAnimation = gltf.create<Animation>();
    Accessor* pInputAccessor = gltf.create<Accessor>();
    Accessor* pOutputAccessor = gltf.create<Accessor>();
    REQUIRE(pAnimation != nullptr);
    REQUIRE(pInputAccessor != nullptr);
    REQUIRE(pOutputAccessor != nullptr);

    AnimationSampler* pSampler = pAnimation->createSampler();
    REQUIRE(pSampler != nullptr);

    IndexLookup indices(gltf);
    ExportOptions exportOptions;
    AnimationSamplersToJson converter(indices, exportOptions);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonSampler;
            converter.convert(*pSampler, jsonSampler);

            THEN("has default interpolation")
            {
                REQUIRE(jsonSampler.contains("interpolation"));
                core::string defaultInterpolation = animationInterpolationToString(AnimationInterpolation());
                CHECK(jsonSampler["interpolation"].get<core::string>() == defaultInterpolation);
            }
            THEN("has no input accessor")
            {
                CHECK_FALSE(jsonSampler.contains("input"));
            }
            THEN("has no output accessor")
            {
                CHECK_FALSE(jsonSampler.contains("output"));
            }
        }
    }

    GIVEN("all values explicitly set")
    {
        AnimationInterpolation interpolation = AnimationInterpolation::CubicSpline;
        core::string interpolationString = animationInterpolationToString(interpolation);
        pSampler->setInputAccessor(*pInputAccessor);
        pSampler->setOutputAccessor(*pOutputAccessor);
        pSampler->setInterpolation(interpolation);

        WHEN("converted")
        {
            nlohmann::json jsonSampler;
            converter.convert(*pSampler, jsonSampler);

            THEN("has correct interpolation")
            {
                REQUIRE(jsonSampler.contains("interpolation"));
                CHECK(jsonSampler["interpolation"].get<core::string>() == interpolationString);
            }
            THEN("has correct input accessor index")
            {
                REQUIRE(jsonSampler.contains("input"));
                size_t index = indices[pInputAccessor];
                CHECK(jsonSampler["input"].get<uint32_t>() == index);
            }
            THEN("has correct output accessor index")
            {
                REQUIRE(jsonSampler.contains("output"));
                size_t index = indices[pOutputAccessor];
                CHECK(jsonSampler["output"].get<uint32_t>() == index);
            }
        }
    }
}
}
