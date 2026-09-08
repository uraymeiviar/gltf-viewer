// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "gltf/Gltf.h"
#include "gltf/GltfParsing.h"
#include "gltf/ImportOptions.h"
#include "testHelpers/TestFileSystem.h"

namespace ux3d::gltf::tests {
namespace {
    constexpr auto TestNamespace = "[tools.gltf.StorageTypeConversion]";
    const auto& fs = filesystem::TestFileSystem::instance();
}

TEST_CASE("StorageTypeConversionTests: file to embedded", TestNamespace)
{
    gltf::Gltf gltf;
    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    GltfParsing::fromFile(gltf, fs.toFile(UX3D_TEST_RESOURCES "/Duck.gltf"), importOptions);

    auto& images = gltf.getImages();
    REQUIRE_FALSE(images.empty());
    Image& image = images.front();
    REQUIRE(image.getMimeType() == imageformats::ImageEncoding::PNG);

    image.setStorageFormat(StorageFormat::Embedded);

    ExportOptions options { FileContainer::Gltf };
    GltfParsing::toFile(gltf, fs.toFile("Duck_Embedded_Export_A.gltf"), options);

    gltf::Gltf gltfImport;
    GltfParsing::fromFile(gltfImport, fs.toFile("Duck_Embedded_Export_A.gltf"), importOptions);

    SECTION("image")
    {
        const uint32_t channelCount = 3;
        const uint32_t bitsPerChannel = 8;
        const uint32_t height = 512;
        const uint32_t width = 512;

        const auto& imagesImport = gltfImport.getImages();
        REQUIRE_FALSE(imagesImport.empty());

        const Image& imageImport = imagesImport.front();

        const image::Image& runtimeImage = imageImport.getData();

        CHECK(runtimeImage.getChannelCount() == channelCount);
        CHECK(runtimeImage.getChannelSize(image::Channel::RED) == bitsPerChannel);
        CHECK(runtimeImage.getHeight() == height);
        CHECK(runtimeImage.getWidth() == width);
        CHECK(imageImport.getMimeType() == imageformats::ImageEncoding::PNG);
    }
}

TEST_CASE("StorageTypeConversionTests: embedded to file", TestNamespace)
{
    gltf::Gltf gltf;
    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    GltfParsing::fromFile(gltf, fs.toFile(UX3D_TEST_RESOURCES "/Duck_Embedded.gltf"), importOptions);

    auto& images = gltf.getImages();
    REQUIRE_FALSE(images.empty());
    Image& image = images.front();
    REQUIRE(image.getMimeType() == imageformats::ImageEncoding::PNG);

    image.setStorageFormat(StorageFormat::File);

    ExportOptions options { FileContainer::Gltf };
    GltfParsing::toFile(gltf, fs.toFile("Duck_File_Export_A.gltf"), options);

    gltf::Gltf gltfImport;
    GltfParsing::fromFile(gltfImport, fs.toFile("Duck_File_Export_A.gltf"), importOptions);

    SECTION("image")
    {
        const uint32_t channelCount = 3;
        const uint32_t bitsPerChannel = 8;
        const uint32_t height = 512;
        const uint32_t width = 512;

        const auto& imagesImport = gltfImport.getImages();
        REQUIRE_FALSE(imagesImport.empty());

        const Image& imageImport = imagesImport.front();

        const image::Image& runtimeImage = imageImport.getData();

        CHECK(runtimeImage.getChannelCount() == channelCount);
        CHECK(runtimeImage.getChannelSize(image::Channel::RED) == bitsPerChannel);
        CHECK(runtimeImage.getHeight() == height);
        CHECK(runtimeImage.getWidth() == width);
        CHECK(imageImport.getMimeType() == imageformats::ImageEncoding::PNG);
    }
}

TEST_CASE("StorageTypeConversionTests: buffer to file", TestNamespace)
{
    gltf::Gltf gltf;
    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    GltfParsing::fromFile(gltf, fs.toFile(UX3D_TEST_RESOURCES "/Duck.glb"), importOptions);

    auto& images = gltf.getImages();
    REQUIRE_FALSE(images.empty());
    Image& image = images.front();
    REQUIRE(image.getMimeType() == imageformats::ImageEncoding::PNG);

    image.setStorageFormat(StorageFormat::File);

    ExportOptions options { FileContainer::Gltf };
    GltfParsing::toFile(gltf, fs.toFile("Duck_File_Export_B.gltf"), options);

    gltf::Gltf gltfImport;
    GltfParsing::fromFile(gltfImport, fs.toFile("Duck_File_Export_B.gltf"), importOptions);

    SECTION("image")
    {
        const uint32_t channelCount = 3;
        const uint32_t bitsPerChannel = 8;
        const uint32_t height = 512;
        const uint32_t width = 512;

        const auto& imagesImport = gltfImport.getImages();
        REQUIRE_FALSE(imagesImport.empty());

        const Image& imageImport = imagesImport.front();

        const image::Image& runtimeImage = imageImport.getData();

        CHECK(runtimeImage.getChannelCount() == channelCount);
        CHECK(runtimeImage.getChannelSize(image::Channel::RED) == bitsPerChannel);
        CHECK(runtimeImage.getHeight() == height);
        CHECK(runtimeImage.getWidth() == width);
        CHECK(imageImport.getMimeType() == imageformats::ImageEncoding::PNG);
    }
}

TEST_CASE("StorageTypeConversionTests: image files to bufferview", TestNamespace)
{
    gltf::Gltf gltf;
    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    GltfParsing::fromFile(gltf, fs.toFile(UX3D_TEST_RESOURCES "/BoomBoxWithAxes.gltf"), importOptions);

    auto& images = gltf.getImages();
    REQUIRE_FALSE(images.empty());
    Image& imageA = images.at(0);
    Image& imageB = images.at(1);
    REQUIRE(imageA.getMimeType() == imageformats::ImageEncoding::PNG);

    imageA.setStorageFormat(StorageFormat::BufferView);
    imageB.setStorageFormat(StorageFormat::BufferView);

    ExportOptions options { FileContainer::Gltf };
    GltfParsing::toFile(gltf, fs.toFile("BoomBoxWithAxes_BufferView_Export.gltf"), options);

    gltf::Gltf gltfImport;
    GltfParsing::fromFile(gltfImport, fs.toFile("BoomBoxWithAxes_BufferView_Export.gltf"), importOptions);

    SECTION("image")
    {

        const uint32_t height = 2048;
        const uint32_t width = 2048;

        const auto& imagesImport = gltfImport.getImages();
        REQUIRE_FALSE(imagesImport.empty());

        const Image& imageA = imagesImport.at(0);
        const Image& imageB = imagesImport.at(1);

        const image::Image& runtimeImageA = imageA.getData();
        const image::Image& runtimeImageB = imageB.getData();

        CHECK(runtimeImageA.getHeight() == height);
        CHECK(runtimeImageA.getWidth() == width);

        CHECK(runtimeImageB.getHeight() == height);
        CHECK(runtimeImageB.getWidth() == width);
    }
}

TEST_CASE("StorageTypeConversionTests: file to embedded to file", TestNamespace)
{
    gltf::Gltf gltf;
    ImportOptions importOptions;
    importOptions.conversionUsage = ConversionUsage::ValidGltf;
    GltfParsing::fromFile(gltf, fs.toFile(UX3D_TEST_RESOURCES "/Duck.glb"), importOptions);

    auto& images = gltf.getImages();
    REQUIRE_FALSE(images.empty());
    Image& image = images.front();
    REQUIRE(image.getMimeType() == imageformats::ImageEncoding::PNG);

    image.setStorageFormat(StorageFormat::Embedded);

    ExportOptions options { FileContainer::Gltf };
    GltfParsing::toFile(gltf, fs.toFile("Duck_Embedded_Export_B.gltf"), options);

    gltf::Gltf gltfImport;
    GltfParsing::fromFile(gltfImport, fs.toFile("Duck_Embedded_Export_B.gltf"), importOptions);

    SECTION("image")
    {
        const uint32_t channelCount = 3;
        const uint32_t bitsPerChannel = 8;
        const uint32_t height = 512;
        const uint32_t width = 512;

        const auto& imagesImport = gltfImport.getImages();
        REQUIRE_FALSE(imagesImport.empty());

        const Image& imageImport = imagesImport.front();

        const image::Image& runtimeImage = imageImport.getData();

        CHECK(runtimeImage.getChannelCount() == channelCount);
        CHECK(runtimeImage.getChannelSize(image::Channel::RED) == bitsPerChannel);
        CHECK(runtimeImage.getHeight() == height);
        CHECK(runtimeImage.getWidth() == width);
        CHECK(imageImport.getMimeType() == imageformats::ImageEncoding::PNG);
    }
}
}
