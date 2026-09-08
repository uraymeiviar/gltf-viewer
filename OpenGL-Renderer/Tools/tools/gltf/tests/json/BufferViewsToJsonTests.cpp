// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/BufferViewsToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("BufferViewsToJson", "[tools.gltf.BufferViewsToJson]")
{
    Gltf gltf;

    BufferView* pBufferView = gltf.create<BufferView>();
    REQUIRE(pBufferView != nullptr);

    Buffer* pBuffer = gltf.create<Buffer>();
    REQUIRE(pBuffer != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    BufferViewsToJson converter(indices, options);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonBufferView;
            converter.convert(*pBufferView, jsonBufferView, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonBufferView.contains("name"));
            }
            THEN("has no buffer index")
            {
                CHECK_FALSE(jsonBufferView.contains("buffer"));
            }
            THEN("has no byteOffset")
            {
                CHECK_FALSE(jsonBufferView.contains("byteOffset"));
            }
            THEN("has no byteLength")
            {
                CHECK_FALSE(jsonBufferView.contains("byteLength"));
            }
            THEN("has no byteStride")
            {
                CHECK_FALSE(jsonBufferView.contains("byteStride"));
            }
            THEN("has no target")
            {
                CHECK_FALSE(jsonBufferView.contains("target"));
            }
        }
    }

    GIVEN("all explicit values")
    {
        core::string name = "good name for a buffer view";
        uint32_t byteOffset = 5u;
        uint32_t byteLength = 6u;
        uint32_t byteStride = 7u;
        uint32_t target = BufferView::TargetArrayBuffer;
        pBufferView->setName(name);
        pBufferView->setBuffer(*pBuffer);
        pBufferView->setByteOffset(byteOffset);
        pBufferView->setByteLength(byteLength);
        pBufferView->setByteStride(byteStride);
        pBufferView->setTarget(target);

        WHEN("converted")
        {
            nlohmann::json jsonBufferView;
            converter.convert(*pBufferView, jsonBufferView, {});

            THEN("has correct name")
            {
                REQUIRE(jsonBufferView.contains("name"));
                CHECK(jsonBufferView["name"].get<core::string>() == name);
            }
            THEN("has correct buffer index")
            {
                REQUIRE(jsonBufferView.contains("buffer"));
                size_t index = indices[pBuffer];
                CHECK(jsonBufferView["buffer"].get<uint32_t>() == index);
            }
            THEN("has correct byteOffset")
            {
                REQUIRE(jsonBufferView.contains("byteOffset"));
                CHECK(jsonBufferView["byteOffset"].get<uint32_t>() == byteOffset);
            }
            THEN("has correct byteLength")
            {
                REQUIRE(jsonBufferView.contains("byteLength"));
                CHECK(jsonBufferView["byteLength"].get<uint32_t>() == byteLength);
            }
            THEN("has correct byteStride")
            {
                REQUIRE(jsonBufferView.contains("byteStride"));
                CHECK(jsonBufferView["byteStride"].get<uint32_t>() == byteStride);
            }
            THEN("has correct target")
            {
                REQUIRE(jsonBufferView.contains("target"));
                CHECK(jsonBufferView["target"].get<uint32_t>() == target);
            }
        }
    }
}
}
