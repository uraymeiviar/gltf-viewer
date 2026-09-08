#include "reactor/ImageOperations.h"
#include "Ktx2.h"
#include "WebP.h"
#include <stdexcept>

#ifndef __APPLE__
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#endif
#include <stb_image.h>
#include <stb_image_resize.h>
#include <stb_image_write.h>

using namespace ux3d;
using namespace ux3d::reactor;

double image::perceivedBrightness(const glm::dvec3& v)
{
    return glm::length(PERCEIVED_BRIGHTNESS_COEFFICENTS * v * v);
}

glm::dvec3 image::toLinear(const glm::dvec3& v)
{
    return glm::pow(v, glm::dvec3(SRGB_GAMMA));
}

glm::dvec4 image::toLinear(const glm::dvec4& v)
{
    return glm::dvec4(toLinear(glm::dvec3(v)), v.a);
}

glm::dvec3 image::toSrgb(const glm::dvec3& v)
{
    return glm::pow(v, glm::dvec3(1.0 / SRGB_GAMMA));
}

glm::dvec4 image::toSrgb(const glm::dvec4& v)
{
    return glm::dvec4(image::toSrgb(glm::dvec3(v)), v.a);
}

double image::maxComponent(const glm::dvec3& v)
{
    return glm::max(glm::max(v.r, v.g), v.b);
}

double image::solveMetallic(double diffuseBrightness, double specularBrightness, double oneMinusSpecularStrength, double dielectricSpecular)
{
    if (specularBrightness < dielectricSpecular) {
        return 0.0;
    }

    double a = dielectricSpecular;
    double b = diffuseBrightness * oneMinusSpecularStrength / (1 - dielectricSpecular) + specularBrightness - 2 * dielectricSpecular;
    double c = dielectricSpecular - specularBrightness;
    double d = glm::max(b * b - 4 * a * c, 0.0);
    return glm::clamp((-b + glm::sqrt(d)) / (2 * a), 0.0, 1.0);
}

image::MetallicRoughnessValue image::convertSpecularGlossinessToMetallicRoughness(glm::dvec4 diffuse, glm::dvec3 specularColor, double glossiness)
{
    glm::dvec3 diffuseColor { diffuse.r, diffuse.g, diffuse.b };
    double oneMinusSpecularStrength = 1 - image::maxComponent(specularColor);
    double metallic = image::solveMetallic(
        image::perceivedBrightness(diffuseColor),
        image::perceivedBrightness(specularColor),
        oneMinusSpecularStrength,
        image::DIELECTRIC_SPECULAR.r);

    auto baseColorFromDiffuse = diffuseColor * (oneMinusSpecularStrength / (1 - DIELECTRIC_SPECULAR.r) / glm::max(1 - metallic, image::EPSILON));
    auto baseColorFromSpecular = (specularColor - (DIELECTRIC_SPECULAR * (1 - metallic))) * (1 / glm::max(metallic, image::EPSILON));

    auto baseColor = glm::mix(baseColorFromDiffuse, baseColorFromSpecular, metallic * metallic);

    return image::MetallicRoughnessValue {
        .baseColor = glm::dvec4(glm::clamp(baseColor, 0.0, 1.0), diffuse.a),
        .metallic = metallic,
        .roughness = 1 - glossiness,
    };
}

image::ImageData image::decode(const std::vector<uint8_t>& rawData)
{
    std::string mimeType = image::getMimeType(rawData);
    image::ImageData output;
    if (mimeType.compare("image/ktx2") == 0) {

        output = image::decodeKTX(rawData);
    } else if (mimeType.compare("image/webp") == 0) {
        output = image::decodeWebP(rawData);
    } else if (mimeType.compare("image/png") == 0 || mimeType.compare("image/jpeg") == 0) {
        int width;
        int height;
        int channels;
        uint8_t* imageDataStb = static_cast<uint8_t*>(
            stbi_load_from_memory(
                static_cast<stbi_uc const*>(&rawData[0]),
                static_cast<int>(rawData.size()), &width, &height, &channels, 0));

        if (imageDataStb == nullptr) {
            throw std::runtime_error("Decoding error");
        }
        size_t imageSize = width * height * channels;
        output.data.assign(imageDataStb, imageDataStb + imageSize);
        stbi_image_free(imageDataStb);
        output.info["width"] = width;
        output.info["height"] = height;
        output.info["channels"] = channels;
        output.info["pixelByteSize"] = channels;

    } else {
        throw std::runtime_error("Mime type currently not supported");
    }
    return output;
}

nlohmann::json image::decodeInfo(const std::vector<uint8_t>& rawData)
{
    std::string mimeType = image::getMimeType(rawData);
    nlohmann::json info;
    if (mimeType.compare("image/ktx2") == 0) {
        return image::decodeKTXInfo(rawData);

    } else if (mimeType.compare("image/webp") == 0) {
        return image::decodeWebPInfo(rawData);

    } else if (mimeType.compare("image/png") == 0 || mimeType.compare("image/jpeg") == 0) {
        int width;
        int height;
        int channels;
        uint8_t* imageDataStb = static_cast<uint8_t*>(
            stbi_load_from_memory(
                static_cast<stbi_uc const*>(&rawData[0]),
                static_cast<int>(rawData.size()), &width, &height, &channels, 0));

        if (imageDataStb == nullptr) {
            throw std::runtime_error("Decoding error");
        }
        stbi_image_free(imageDataStb);

        info["mimeType"] = mimeType;
        info["width"] = width;
        info["height"] = height;
        info["channels"] = channels;
        info["pixelByteSize"] = channels;

    } else {
        throw std::runtime_error("Mime type currently not supported");
    }
    return info;
}
namespace ux3d::reactor::image {
static void writeFunction(void* context, void* data, int size)
{
    if (!context || !data || !size) {
        return;
    }

    auto* outBuffer = static_cast<std::vector<uint8_t>*>(context);
    size_t currentSize = outBuffer->size();

    outBuffer->resize(currentSize + static_cast<size_t>(size));
    memcpy(&(*outBuffer)[currentSize], data, static_cast<size_t>(size));
}
}
std::vector<uint8_t> image::encode(const image::ImageData& input, const nlohmann::json& encodeInformation)
{
    std::vector<uint8_t> output;
    int width = input.info["width"];
    int height = input.info["height"];
    int channels = input.info["channels"];
    int pixelByteSize = input.info["pixelByteSize"];

    std::string mimeType = encodeInformation["mimeType"];
    if (mimeType == "image/png") {
        int strideInBytes = width * pixelByteSize;
        bool success = stbi_write_png_to_func(writeFunction, &output, width, height, channels, input.data.data(), strideInBytes);
        if (!success) {
            throw std::runtime_error("Encoding error");
        }
    } else if (mimeType == "image/jpeg") {
        int quality = encodeInformation.value("quality", 90);
        bool success = stbi_write_jpg_to_func(writeFunction, &output, width, height, channels, input.data.data(), quality);
        if (!success) {
            throw std::runtime_error("Encoding error");
        }
    } else if (mimeType == "image/ktx2") {
        output = image::encodeKTX(input, encodeInformation);
    } else if (mimeType == "image/webp") {
        output = image::encodeWebP(input, encodeInformation);
    } else {
        throw std::runtime_error { "Cannot encode image, unsupported mime type" };
    }

    return output;
}

image::ImageData image::scaleImage(const image::ImageData& input, int width, int height)
{
    if (input.info["width"].get<int>() == width && input.info["height"].get<int>() == height) {
        return input;
    }

    image::ImageData output;
    output.info = input.info;
    output.info["width"] = width;
    output.info["height"] = height;

    int pixelByteSize = input.info.at("pixelByteSize").get<int>();
    int inStrideInBytes = input.info.at("width").get<int>() * pixelByteSize;

    int outStrideInBytes = width * pixelByteSize;
    output.data.resize(outStrideInBytes * height);

    stbir_resize_uint8(
        input.data.data(),
        input.info.at("width").get<int>(),
        input.info.at("height").get<int>(),
        inStrideInBytes,
        output.data.data(),
        width,
        height,
        outStrideInBytes,
        input.info.at("channels").get<int>());

    return output;
}

image::ImageData image::blendImages(const image::ImageData& inputA, const image::ImageData& inputB, float weight)
{
    auto isEqual = [](auto a, auto b) -> bool {
        return (a.at("width").template get<int>() == b.at("width").template get<int>()
            && a.at("height").template get<int>() == b.at("height").template get<int>()
            && a.at("channels").template get<int>() == b.at("channels").template get<int>()
            && a.at("pixelByteSize").template get<int>() == b.at("pixelByteSize").template get<int>());
    };

    if (!isEqual(inputA.info, inputB.info)) {
        throw std::invalid_argument("Images have different properties");
    }

    if (inputA.info.at("pixelByteSize").get<int>() / inputA.info.at("channels").get<int>() != 1) {
        throw std::invalid_argument("Unsupported channel byte size");
    }

    image::ImageData output;
    output.info = inputA.info;

    int width = inputA.info.at("width").get<int>();
    int height = inputA.info.at("height").get<int>();
    int channels = inputA.info.at("channels").get<int>();
    int pixelByteSize = inputA.info.at("pixelByteSize").get<int>();

    output.data.resize(width * height * pixelByteSize);

    for (uint32_t i = 0; i < output.data.size(); i++) {
        uint8_t value = inputA.data[i] * weight + inputB.data[i] * (1.0f - weight);
        output.data[i] = value;
    }

    return output;
}

std::string image::getFileExtension(const std::string& mimeType)
{
    if (mimeType == "image/png") {
        return ".png";
    }
    if (mimeType == "image/jpeg") {
        return ".jpg";
    }
    if (mimeType == "image/ktx2") {
        return ".ktx2";
    }
    if (mimeType == "image/vnd.radiance") {
        return ".hdr";
    }
    if (mimeType == "image/webp") {
        return ".webp";
    }
    return "";
}

std::string image::getMimeType(const std::vector<uint8_t>& encodedBytes)
{
    // detect png
    if (encodedBytes.size() >= 8) {
        // see http://www.libpng.org/pub/png/spec/1.2/PNG-Rationale.html#R.PNG-file-signature
        if (encodedBytes[0] == 137 && encodedBytes[1] == 80 && encodedBytes[2] == 78 && encodedBytes[3] == 71 && encodedBytes[4] == 13 && encodedBytes[5] == 10 && encodedBytes[6] == 26 && encodedBytes[7] == 10) {
            return "image/png";
        }
    }
    // detect jpg
    if (encodedBytes.size() >= 2) {
        // see https://www.garykessler.net/library/file_sigs.html
        if (encodedBytes[0] == 0xFF && encodedBytes[1] == 0xD8) {
            size_t scanIndex = encodedBytes.size() - 1;
            while (scanIndex >= 1) {
                if (encodedBytes[scanIndex - 1] == 0xFF && encodedBytes[scanIndex] == 0xD9) {
                    return "image/jpeg";
                }
                scanIndex--;
            }
        }
    }
    // detect HDR
    if (encodedBytes.size() >= 10) {
        // see https://www.garykessler.net/library/file_sigs.html
        if (encodedBytes[0] == '#' && encodedBytes[1] == '?' && encodedBytes[2] == 'R' && encodedBytes[3] == 'A' && encodedBytes[4] == 'D' && encodedBytes[5] == 'I' && encodedBytes[6] == 'A' && encodedBytes[7] == 'N' && encodedBytes[8] == 'C' && encodedBytes[9] == 'E') {
            return "image/vnd.radiance";
        }
    }
    // detect ktx2
    if (encodedBytes.size() >= 12) {
        // see http://github.khronos.org/KTX-Specification/
        if (encodedBytes[0] == 0xAB && encodedBytes[1] == 0x4B && encodedBytes[2] == 0x54 && encodedBytes[3] == 0x58 && encodedBytes[4] == 0x20 && encodedBytes[5] == 0x32 && encodedBytes[6] == 0x30 && encodedBytes[7] == 0xBB && encodedBytes[8] == 0x0D && encodedBytes[9] == 0x0A && encodedBytes[10] == 0x1A && encodedBytes[11] == 0x0A) {
            return "image/ktx2";
        }
    }
    // detect webp
    if (encodedBytes.size() >= 12) {
        // see https://developers.google.com/speed/webp/docs/riff_container
        // see https://www.garykessler.net/library/file_sigs.html
        if (encodedBytes[0] == 0x52 && encodedBytes[1] == 0x49 && encodedBytes[2] == 0x46 && encodedBytes[3] == 0x46 && encodedBytes[8] == 0x57 && encodedBytes[9] == 0x45 && encodedBytes[10] == 0x42 && encodedBytes[11] == 0x50) {
            return "image/webp";
        }
    }

    // default
    return std::string();
}
