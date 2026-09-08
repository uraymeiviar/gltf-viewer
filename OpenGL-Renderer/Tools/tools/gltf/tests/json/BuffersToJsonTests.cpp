// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/BuffersToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("BuffersToJson", "[tools.gltf.BuffersToJson]")
{
    Gltf gltf;

    Buffer* pBuffer = gltf.create<Buffer>();
    REQUIRE(pBuffer != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    BuffersToJson converter(indices, options);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonBuffer;
            converter.convert(*pBuffer, jsonBuffer, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonBuffer.contains("name"));
            }
            THEN("has empty uri")
            {
                CHECK_FALSE(jsonBuffer.contains("uri"));
            }
            THEN("has no byteLength")
            {
                CHECK_FALSE(jsonBuffer.contains("byteLength"));
            }
        }
    }

    GIVEN("all explicit values")
    {
        // byteLength is only implicit right now, so we store this data in the buffer
        core::Vector<uint8_t> data { 1u, 2u, 3u };
        core::string name = "good name for a buffer";
        core::string uri = "this/is/where/the/data/is";
        pBuffer->setName(name);
        pBuffer->setUri(uri);
        pBuffer->setData(data.getMemoryRange());

        WHEN("converted")
        {
            nlohmann::json jsonBuffer;
            converter.convert(*pBuffer, jsonBuffer, {});

            THEN("has correct name")
            {
                REQUIRE(jsonBuffer.contains("name"));
                CHECK(jsonBuffer["name"].get<core::string>() == name);
            }
            THEN("has correct uri")
            {
                REQUIRE(jsonBuffer.contains("uri"));
                CHECK(jsonBuffer["uri"].get<core::string>() == uri);
            }
            THEN("has correct byteLength")
            {
                REQUIRE(jsonBuffer.contains("byteLength"));
                CHECK(jsonBuffer["byteLength"].get<uint32_t>() == data.size());
            }
        }
    }
}
}
