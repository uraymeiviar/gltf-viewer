// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/ResourceCollector.h"
#include "gltf/json/ImagesToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("ImagesToJson", "[tools.gltf.ImagesToJson]")
{
    GIVEN("no explicit values")
    {
        Gltf gltf;

        Image* pImage = gltf.create<Image>();
        REQUIRE(pImage != nullptr);

        IndexLookup indices(gltf);
        ResourceCollector resourceCollector;
        ExportOptions options;
        ImagesToJson converter(indices, resourceCollector, options, gltf);

        WHEN("converted")
        {
            nlohmann::json jsonImage;
            converter.convert(*pImage, jsonImage, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonImage.contains("name"));
            }
        }
    }

    GIVEN("all values explicitly set")
    {

        Gltf gltf;

        Image* pImage = gltf.create<Image>();
        REQUIRE(pImage != nullptr);

        core::string name = "good name for a image";
        REQUIRE(pImage->setName(name) == GltfResult::Success);

        core::string uri = "this/is/where/the/data/is";
        REQUIRE(pImage->setUri(uri) == GltfResult::Success);

        imageformats::ImageEncoding mimeType = imageformats::ImageEncoding::PNG;
        REQUIRE(pImage->setMimeType(mimeType) == GltfResult::Success);

        BufferView* pBufferView = gltf.create<BufferView>();
        REQUIRE(pBufferView != nullptr);
        REQUIRE(pImage->setBufferView(*pBufferView) == GltfResult::Success);

        ResourceCollector resourceCollector;
        IndexLookup indices(gltf);
        ExportOptions options;
        ImagesToJson converter(indices, resourceCollector, options, gltf);

        WHEN("converted")
        {
            nlohmann::json jsonImage;
            converter.convert(*pImage, jsonImage, {});

            THEN("has name")
            {
                REQUIRE(jsonImage.contains("name"));
                CHECK(jsonImage["name"].get<core::string>() == name);
            }
            THEN("has uri")
            {
                REQUIRE(jsonImage.contains("uri"));
                CHECK(jsonImage["uri"].get<core::string>() == uri);
            }
            THEN("has mimeType")
            {
                REQUIRE(jsonImage.contains("mimeType"));
                CHECK(jsonImage["mimeType"].get<core::string>() == imageformats::imageEncodingToString(mimeType));
            }
            THEN("has bufferView")
            {
                REQUIRE(jsonImage.contains("bufferView"));

                size_t testIndex = indices[pBufferView];
                size_t jsonIndex = jsonImage["bufferView"];

                CHECK(testIndex == jsonIndex);
            }
        }
    }
}
}
