// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/TexturesToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("TexturesToJson: Directional", "[tools.gltf.TexturesToJson]")
{

    GIVEN("no explicit values")
    {
        Gltf gltf;

        Texture* pTexture = gltf.create<Texture>();
        REQUIRE(pTexture != nullptr);

        IndexLookup indices(gltf);
        ExportOptions options;
        TexturesToJson converter(indices, options);

        WHEN("converted")
        {
            nlohmann::json jsonTexture;
            converter.convert(*pTexture, jsonTexture, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonTexture.contains("name"));
            }
            THEN("has no sampler")
            {
                CHECK_FALSE(jsonTexture.contains("sampler"));
            }
            THEN("has no source")
            {
                CHECK_FALSE(jsonTexture.contains("source"));
            }
            THEN("has no extensions")
            {
                CHECK_FALSE(jsonTexture.contains("extensions"));
            }
        }
    }

    GIVEN("all values explicitly set")
    {
        Gltf gltf;

        Texture* pTexture = gltf.create<Texture>();
        REQUIRE(pTexture != nullptr);

        core::string name = "good name for a texture";
        pTexture->setName(name);

        Sampler* pSampler = gltf.create<Sampler>();
        pTexture->setSampler(*pSampler);

        Image* pSource = gltf.create<Image>();
        pTexture->setSource(*pSource);

        Image* pImageBasisU = gltf.create<Image>();
        REQUIRE(pImageBasisU != nullptr);
        pTexture->createBasisU();
        pTexture->getBasisU()->setSource(pImageBasisU);

        IndexLookup indices(gltf);
        ExportOptions options;
        TexturesToJson converter(indices, options);

        WHEN("converted")
        {
            nlohmann::json jsonTexture;
            converter.convert(*pTexture, jsonTexture, {});

            THEN("has correct name")
            {
                REQUIRE(jsonTexture.contains("name"));
                CHECK(jsonTexture["name"].get<core::string>() == name);
            }
            THEN("has correct sampler")
            {
                REQUIRE(jsonTexture.contains("sampler"));

                size_t jsonSamplerIndex = jsonTexture["sampler"];
                size_t testSamplerIndex = indices[pSampler];
                CHECK(jsonSamplerIndex == testSamplerIndex);
            }
            THEN("has correct source")
            {
                REQUIRE(jsonTexture.contains("source"));

                size_t jsonSourceIndex = jsonTexture["source"];
                size_t testSourceIndex = indices[pSource];
                CHECK(jsonSourceIndex == testSourceIndex);
            }

            REQUIRE(jsonTexture.contains("extensions"));

            THEN("has correct KHR_texture_basisu")
            {
                REQUIRE(jsonTexture["extensions"].contains("KHR_texture_basisu"));
                size_t jsonIndex = jsonTexture["extensions"]["KHR_texture_basisu"]["source"];
                size_t testIndex = indices[pImageBasisU];
                CHECK(jsonIndex == testIndex);
            }
        }
    }
}
}
