// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "../TestAssets.h"
#include "gltf/Gltf.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
namespace {
    constexpr auto TestNamespace = "[tools.gltf.GltfToGlb]";

    const gltf::Gltf& resolveTestAsset(TestAsset& _Asset) { return _Asset.getRoundtrip(); }
}

SCENARIO("GltfToGlb", TestNamespace)
{
    const Gltf& gltf = resolveTestAsset(DuckBinary);

    SECTION("image")
    {
        const uint32_t channelCount = 3;
        const uint32_t bitsPerChannel = 8;
        const uint32_t height = 512;
        const uint32_t width = 512;

        const auto& images = gltf.getImages();
        REQUIRE_FALSE(images.empty());

        const Image& image = images.front();

        const image::Image& runtimeImage = image.getData();

        CHECK(runtimeImage.getChannelCount() == channelCount);
        CHECK(runtimeImage.getChannelSize(image::Channel::RED) == bitsPerChannel);
        CHECK(runtimeImage.getHeight() == height);
        CHECK(runtimeImage.getWidth() == width);
    }

    SECTION("buffer")
    {
        const size_t bufferSize = 102040;
        const size_t imageSize = 50864;
        const size_t combinedBufferSize = bufferSize + imageSize;

        const auto& buffers = gltf.getBuffers();
        REQUIRE_FALSE(buffers.empty());

        const Buffer& buffer = buffers.front();
        CHECK(buffer.getByteLength() == combinedBufferSize);
    }
}
}
