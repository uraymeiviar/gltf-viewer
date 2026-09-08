#include "TestAssets.h"
#include "gltf/Base64.h"
#include "gltf/EmbeddedData.h"
#include "gltf/Gltf.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
namespace {
    constexpr auto TestNamespace = "[tools.gltf.EmbeddedData]";

    const gltf::Gltf& resolveTestAsset(TestAsset& _Asset) { return _Asset.getRoundtrip(); }
}

SCENARIO("Base64", TestNamespace)
{
    core::string input = "AvWagBija+0cDn3Br25+";
    core::Vector<uint8_t> buffer;
    Base64::decode(input, buffer);
    core::string output = Base64::encode(buffer);

    CHECK(output == input);
}

SCENARIO("EmbeddedData Import", TestNamespace)
{
    // DuckEmbedded contains a buffer comprising a data uri
    const Gltf& gltf = resolveTestAsset(DuckEmbedded);

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
        const size_t embeddedBufferSize = bufferSize + imageSize;

        const auto& buffers = gltf.getBuffers();
        REQUIRE_FALSE(buffers.empty());

        const Buffer& buffer = buffers.front();
        CHECK(buffer.getByteLength() == embeddedBufferSize);
    }
}

SCENARIO("EmbeddedData Roundtrip", TestNamespace)
{
    const Gltf& gltf = resolveTestAsset(DuckEmbedded);

    SECTION("image")
    {

        const auto& images = gltf.getImages();
        REQUIRE_FALSE(images.empty());

        const Image& image = images.front();

        const image::Image& runtimeImage = image.getData();

        const core::Vector<uint8_t>& initialBuffer = runtimeImage.getPixels();
        core::MemoryRange<const uint8_t> memoryRange = initialBuffer.getMemoryRange();

        core::string dataString;
        REQUIRE(embedded_data::createBase64Data(memoryRange, imageformats::imageEncodingToString(imageformats::ImageEncoding::PNG), dataString) == ConversionResult::Success);

        core::Vector<uint8_t> buffer;
        core::string mimeType = "";
        REQUIRE(embedded_data::extractBase64Data(dataString, mimeType, buffer) == ConversionResult::Success);

        CHECK(mimeType == imageformats::imageEncodingToString(imageformats::ImageEncoding::PNG));

        REQUIRE(initialBuffer.size() == buffer.size());

        for (size_t i = 0; i < initialBuffer.size() / 100; i++) // do not test full buffer
        {
            CHECK(initialBuffer[i] == buffer[i]);
        }
    }
}

}
