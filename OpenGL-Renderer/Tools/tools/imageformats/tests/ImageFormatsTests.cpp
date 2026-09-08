// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "imageformats/imageformats.h"

#include "core/HeapSlice.h"

#include "filesystem/File.h"
#include "testHelpers/TestFileSystem.h"

namespace ux3d {
namespace tests {
    namespace {
        const auto TestNamespace = "[tools.imageformats]";
        const auto& fs = filesystem::TestFileSystem::instance();
    }

    TEST_CASE("Decode PNG image", TestNamespace)
    {
        core::Vector<uint8_t> buffer;
        fs.toFile(UX3D_TEST_RESOURCES "/BoomBoxWithAxes_baseColor.png").load(buffer);
        REQUIRE(buffer.empty() == false);

        image::Image image;
        REQUIRE(imageformats::decodeImage(buffer, image));

        CHECK(image.getChannelSize(image::Channel::RED) == 8);
        CHECK(image.getChannelSize(image::Channel::GREEN) == 8);
        CHECK(image.getChannelSize(image::Channel::BLUE) == 8);
        CHECK(image.getChannelCount() == 3);
        CHECK(image.getHeight() == 2048);
        CHECK(image.getWidth() == 2048);
        CHECK(image.getPixels().size() == image.getHeight() * image.getWidth() * image.getChannelCount());
    }

    TEST_CASE("Decode PNG from uri", TestNamespace)
    {
        image::Image image;
        filesystem::File file = fs.toFile(UX3D_TEST_RESOURCES "/BoomBoxWithAxes_baseColor.png");

        REQUIRE(imageformats::decodeImageFromFile(file, image));

        CHECK(image.getChannelSize(image::Channel::RED) == 8);
        CHECK(image.getChannelSize(image::Channel::GREEN) == 8);
        CHECK(image.getChannelSize(image::Channel::BLUE) == 8);
        CHECK(image.getChannelCount() == 3);
        CHECK(image.getHeight() == 2048);
        CHECK(image.getWidth() == 2048);
        CHECK(image.getPixels().size() == image.getHeight() * image.getWidth() * image.getChannelCount());
    }
}
}
