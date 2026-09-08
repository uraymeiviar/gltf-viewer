// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "core/Result.h"
#include "filesystem/File.h"
#include "gltf/Gltf.h"
#include "gltf/Image.h"
#include "image/Image.h"
#include "testHelpers/TestFileSystem.h"

namespace ux3d::tests {
namespace {
    constexpr auto TestNamespace = "[tools.gltf.Image]";
    const auto& fs = filesystem::TestFileSystem::instance();
}

TEST_CASE("Image constructor", TestNamespace)
{
    gltf::Gltf root;
    gltf::Image gltfImage(0);

    CHECK(gltfImage.getUri().empty());
    CHECK(gltfImage.getBufferView() == nullptr);
    CHECK(gltfImage.getMimeType() == imageformats::ImageEncoding::Undefined);

    CHECK_FALSE(gltfImage.loadDataToMemory(filesystem::Dir(fs)));
    CHECK(gltfImage.getData().getPixels().empty());
}

TEST_CASE("Image set uri and get correct data", TestNamespace)
{
    gltf::Gltf root;
    gltf::Image gltfImage(0);

    core::string uri = UX3D_TEST_RESOURCES "/BoomBoxWithAxes_baseColor.png";
    gltfImage.setUri(uri);

    REQUIRE(gltfImage.getUri() == uri);

    REQUIRE(gltfImage.loadDataToMemory(filesystem::Dir(fs)).derived == gltf::GltfResult::Success);

    const image::Image& image = gltfImage.getData();
    CHECK(image.getChannelSize(image::Channel::RED) == 8);
    CHECK(image.getChannelSize(image::Channel::GREEN) == 8);
    CHECK(image.getChannelSize(image::Channel::BLUE) == 8);
    CHECK(image.getChannelCount() == 3);
    CHECK(image.getHeight() == 2048);
    CHECK(image.getWidth() == 2048);
    CHECK(image.getPixels().size() == image.getHeight() * image.getWidth() * image.getChannelCount());
}

TEST_CASE("Image set buffer and get correct data", TestNamespace)
{
    gltf::Gltf root;
    gltf::Buffer gltfBuffer(0);
    gltf::BufferView bufferView(0);
    gltf::Image gltfImage(0);

    core::Vector<uint8_t> buffer;
    filesystem::File(fs, UX3D_TEST_RESOURCES "/BoomBoxWithAxes_baseColor.png").load(buffer);
    gltfBuffer.setData(buffer.getMemoryRange());

    bufferView.setBuffer(gltfBuffer);
    bufferView.setByteLength(uint32_t(buffer.size()));

    gltfImage.setBufferView(bufferView);

    REQUIRE(gltfImage.loadDataToMemory(filesystem::Dir(fs)).derived == gltf::GltfResult::Success);

    const image::Image& image = gltfImage.getData();

    CHECK(image.getChannelSize(image::Channel::RED) == 8);
    CHECK(image.getChannelSize(image::Channel::GREEN) == 8);
    CHECK(image.getChannelSize(image::Channel::BLUE) == 8);
    CHECK(image.getChannelCount() == 3);
    CHECK(image.getHeight() == 2048);
    CHECK(image.getWidth() == 2048);
    CHECK(image.getPixels().size() == image.getHeight() * image.getWidth() * image.getChannelCount());
}

TEST_CASE("Image with URI and BufferView (URI should be ignored)", TestNamespace)
{
    gltf::Gltf root;
    gltf::Buffer buffer(0);
    gltf::BufferView bufferView(0);
    gltf::Image image(0);

    core::Vector<uint8_t> data;
    filesystem::File(fs, UX3D_TEST_RESOURCES "/BoomBoxWithAxes_baseColor.png").load(data);
    buffer.setData(data.getMemoryRange());
    bufferView.setBuffer(buffer);
    bufferView.setByteLength(uint32_t(data.size()));

    image.setBufferView(bufferView);
    image.setUri("very well, you are my friend.png");

    REQUIRE(core::Succeeded(image.loadDataToMemory(filesystem::Dir(fs))));
    CHECK(image.getData().getHeight() == 2048);
}
}
