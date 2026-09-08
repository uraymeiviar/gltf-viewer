// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "gltf/Accessor.h"
#include "gltf/Buffer.h"
#include "gltf/Gltf.h"

namespace ux3d {
namespace tests {
    constexpr auto TestNamespace = "[tools.gltf.Accessor]";

    TEST_CASE("Raw Accessor data", TestNamespace)
    {
        gltf::Buffer buffer(0);
        gltf::BufferView bufferView(0);
        gltf::Accessor accessor(0);

        SECTION("get init values")
        {
            CHECK(accessor.getBufferView() == nullptr);
            CHECK(accessor.getByteOffset() == 0);
            CHECK(accessor.getCount() == 0);
            CHECK(accessor.hasName() == false);
            CHECK(accessor.isNormalized() == false);
            CHECK(accessor.getMin().empty());
            CHECK(accessor.getMax().empty());
        }

        bufferView.setBuffer(buffer);
        accessor.setBufferView(bufferView);

        SECTION("get raw data")
        {
            core::Vector<uint8_t> expectedData;
            REQUIRE(expectedData.assign({ 0, 1, 2, 3 }));
            buffer.setData(expectedData.getMemoryRange());
            bufferView.setByteLength(4);
            bufferView.setByteOffset(0);
            accessor.setByteOffset(0);

            core::MemoryRange<const uint8_t> actualData;
            accessor.getData(actualData);

            auto actualIt = actualData.begin();
            for (size_t i = 0; i < expectedData.size(); ++i, ++actualIt) {
                CHECK(expectedData.at(i) == *actualIt);
            }
        }

        SECTION("get raw data with byte offset")
        {
            core::Vector<uint8_t> bufferData;
            REQUIRE(bufferData.assign({ 0, 1, 2, 3 }));
            core::Vector<uint8_t> expectedData;
            REQUIRE(expectedData.assign({ 2, 3 }));
            buffer.setData(bufferData.getMemoryRange());
            bufferView.setByteLength(3);
            bufferView.setByteOffset(1);
            accessor.setByteOffset(1);

            core::MemoryRange<const uint8_t> actualData;
            accessor.getData(actualData);

            auto actualIt = actualData.begin();
            for (size_t i = 0; i < expectedData.size(); ++i, ++actualIt) {
                CHECK(expectedData.at(i) == *actualIt);
            }
        }

        SECTION("get data as")
        {
            core::Vector<uint8_t> bufferData;
            REQUIRE(bufferData.assign({ 0, 1, 2, 3 }));
            buffer.setData(bufferData.getMemoryRange());
            bufferView.setByteLength(4);
            bufferView.setByteOffset(0);
            accessor.setByteOffset(0);
            accessor.setComponentType(ux3d::gltf::ComponentType::UnsignedByte);
            accessor.setCount(4);
            accessor.setType(ux3d::gltf::AttributeType::Scalar);

            SECTION("original format")
            {
                core::Slice<uint8_t> actualData;
                accessor.getDataAs<uint8_t>(actualData);

                REQUIRE(actualData.size() == bufferData.size());

                auto actualIt = actualData.begin();
                for (size_t i = 0; i < bufferData.size(); ++i, ++actualIt) {
                    CHECK(bufferData.at(i) == *actualIt);
                }
            }
            SECTION("bigger int")
            {
                core::Slice<uint32_t> actualData;
                accessor.getDataAs<uint32_t>(actualData);

                REQUIRE(actualData.size() == bufferData.size() / 4u);

                auto actualIt = actualData.begin();
                for (size_t i = 0; i + 3 < bufferData.size(); i += 4, ++actualIt) {
                    uint32_t expected = 0u;
                    expected += bufferData.at(i);
                    expected += bufferData.at(i + 1) << 8u;
                    expected += bufferData.at(i + 2) << 16u;
                    expected += bufferData.at(i + 3) << 24u;

                    CHECK(expected == *actualIt);
                }
            }
        }
    }
}
}
