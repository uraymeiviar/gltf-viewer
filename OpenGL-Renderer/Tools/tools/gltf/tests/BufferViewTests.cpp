// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Gltf.h"

namespace ux3d {
namespace tests {
    constexpr auto TestNamespace = "[tools.gltf.BufferView]";

    TEST_CASE("Raw BufferView data", TestNamespace)
    {
        gltf::Buffer buffer(0);
        gltf::BufferView bufferView(0);

        SECTION("get init values")
        {
            CHECK(bufferView.getBuffer() == nullptr);
            CHECK(bufferView.getTarget().has_value() == false);
            CHECK(bufferView.getByteOffset() == 0);
            CHECK(bufferView.getByteLength() == 0);
            CHECK(bufferView.getByteStride().has_value() == false);
        }

        bufferView.setBuffer(buffer);

        SECTION("get raw data")
        {
            core::Vector<uint8_t> expectedData;
            REQUIRE(expectedData.assign({ 0, 1, 2, 3 }));
            buffer.setData(expectedData.getMemoryRange());
            bufferView.setByteLength(4);
            bufferView.setByteOffset(0);

            core::MemoryRange<const uint8_t> actualData;
            bufferView.getData(actualData);

            REQUIRE(actualData.size() == expectedData.size());

            auto actualIt = actualData.begin();
            for (size_t i = 0; i < expectedData.size(); ++i, ++actualIt) {
                CHECK(expectedData.at(i) == *actualIt);
            }
        }

        SECTION("get raw data with byte offset")
        {
            core::Vector<uint8_t> expectedData;
            REQUIRE(expectedData.assign({ 0, 1, 2, 3 }));
            buffer.setData(expectedData.getMemoryRange());
            bufferView.setByteLength(uint32_t(expectedData.size() - 1));
            bufferView.setByteOffset(1);

            core::MemoryRange<const uint8_t> actualData;
            bufferView.getData(actualData);

            REQUIRE(actualData.size() == expectedData.size() - 1);

            auto actualIt = actualData.begin();
            for (size_t i = 1; i < expectedData.size(); ++i, ++actualIt) {
                CHECK(expectedData.at(i) == *actualIt);
            }
        }
    }
}
}
