// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "ImageOperations.h"
#include "HelperFunctions.h"

#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"
#include "reactor/GltfEnums.h"
#include "reactor/Reactor.h"
#include "reactor/Utils.h"

#include <gltf_schema/gltf_single_json.h>

#include <nlohmann/json.hpp>

#include <glm.hpp>
#include <gtc/constants.hpp>
#include <gtx/rotate_vector.hpp>

#include <fmt/format.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

/// Convert an 8-bit quantized pixel into a 32-bit floating-point pixel.
static double dequantize(uint8_t n)
{
    return n / 255.0;
}

/// Convert an 8-bit quantized pixel into a 32-bit floating-point pixel.
template <glm::length_t L>
static glm::vec<L, double> dequantize(glm::vec<L, uint8_t> v)
{
    return glm::vec<L, double> { v } / 255.0;
}

/// Quantize a 32-bit floating-point pixel using 8 bits.
static uint8_t quantize(double v)
{
    return std::clamp(255.0 * v, 0.0, 255.0);
}

/// Quantize a 32-bit floating-point pixel using 8 bits.
template <glm::length_t L>
static glm::vec<L, uint8_t> quantize(glm::vec<L, double> v)
{
    return { 255.0 * v };
}

namespace sampling {

static size_t wrap(
    int k,
    size_t supremum,
    ux3d::reactor::gltf::TextureWrappingMode wrap)
{
    if (wrap == ux3d::reactor::gltf::TextureWrappingMode::ClampToEdge) {
        return std::clamp(static_cast<size_t>(k), static_cast<size_t>(0), supremum - 1);
    }

    int repeats = k / supremum;
    k %= supremum;

    if (k < 0) {
        k += supremum;
    }

    if (wrap == ux3d::reactor::gltf::TextureWrappingMode::MirroredRepeat) {
        if (static_cast<size_t>(std::abs(repeats)) % 2 == 1) {
            k = supremum - k;
        }
    }

    return k;
}

static glm::vec<2, size_t> wrap(
    glm::vec<2, int> k,
    size_t width,
    size_t height,
    ux3d::reactor::gltf::TextureWrappingMode wrapS,
    ux3d::reactor::gltf::TextureWrappingMode wrapT)
{
    return {
        wrap(k.x, width, wrapS),
        wrap(k.y, height, wrapT),
    };
}

static glm::dvec4 fetch(
    glm::vec<2, size_t> p,
    const std::vector<uint8_t>& pixels,
    size_t width,
    size_t depth)
{
    size_t pixelOffset = depth * (width * p.y + p.x);
    glm::u8vec4 sample { 255 };
    for (size_t i = 0; i < depth; ++i) {
        sample[i] = pixels[pixelOffset + i];
    }
    return dequantize(sample);
}

static glm::dvec4 sample(
    const std::vector<uint8_t>& pixels,
    size_t width,
    size_t height,
    size_t depth,
    ux3d::reactor::gltf::TextureWrappingMode wrapS,
    ux3d::reactor::gltf::TextureWrappingMode wrapT,
    ux3d::reactor::gltf::TextureFilter filter,
    glm::dvec2 st)
{
    glm::dvec2 p { st.x * width, st.y * height };

    glm::vec<2, int> p00 { std::floor(p.x), std::floor(p.y) };
    auto p01 = p00;
    p01.x += 1;
    auto p10 = p00;
    p10.y += 1;
    auto p11 = p00;
    p11.x += 1;
    p11.y += 1;

    auto c00 = fetch(wrap(p00, width, height, wrapS, wrapT), pixels, width, depth);
    auto c01 = fetch(wrap(p01, width, height, wrapS, wrapT), pixels, width, depth);
    auto c10 = fetch(wrap(p10, width, height, wrapS, wrapT), pixels, width, depth);
    auto c11 = fetch(wrap(p11, width, height, wrapS, wrapT), pixels, width, depth);

    double dx = p.x - p00.x;
    double dy = p.y - p00.y;

    switch (filter) {
    case ux3d::reactor::gltf::TextureFilter::Nearest:
    case ux3d::reactor::gltf::TextureFilter::NearestMipmapNearest:
    case ux3d::reactor::gltf::TextureFilter::NearestMipmapLinear:
        if (dy < 0.5) {
            if (dx < 0.5) {
                return c00;
            } else {
                return c01;
            }
        } else {
            if (dx < 0.5) {
                return c10;
            } else {
                return c11;
            }
        }
    case ux3d::reactor::gltf::TextureFilter::Linear:
    case ux3d::reactor::gltf::TextureFilter::LinearMipmapNearest:
    case ux3d::reactor::gltf::TextureFilter::LinearMipmapLinear:
    default:
        return glm::mix(glm::mix(c00, c01, dx), glm::mix(c10, c11, dx), dy);
    }
}

}

namespace color {

// Implementation taken from https://github.com/tobspr/GLSL-Color-Spaces/blob/master/ColorSpaces.inc.glsl

static const double HCV_EPSILON = 1e-10;
static const double HSL_EPSILON = 1e-10;
static const double HCY_EPSILON = 1e-10;
static const double SRGB_GAMMA = 1.0 / 2.2;
static const double SRGB_INVERSE_GAMMA = 2.2;
static const glm::dvec3 HCY_WTS { 0.299, 0.587, 0.114 };
static const glm::dvec3 LUMA_COEFFICIENTS { 0.2126, 0.7152, 0.0722 };

static const glm::dmat3 RGB_TO_XYZ {
    { 0.4124564, 0.2126729, 0.0193339 },
    { 0.3575761, 0.7151522, 0.1191920 },
    { 0.1804375, 0.0721750, 0.9503041 },
};

static const glm::dmat3 XYZ_TO_RGB {
    { 3.2404542, -0.9692660, 0.0556434 },
    { -1.5371385, 1.8760108, -0.2040259 },
    { -0.4985314, 0.0415560, 1.0572252 },
};

double luminance(glm::dvec3 rgb)
{
    return glm::dot(LUMA_COEFFICIENTS, rgb);
}

glm::dvec3 rgbToSrgb(glm::dvec3 rgb)
{
    return glm::pow(rgb, glm::dvec3(SRGB_GAMMA));
}

glm::dvec3 srgbToRgb(glm::dvec3 srgb)
{
    return glm::pow(srgb, glm::dvec3(SRGB_INVERSE_GAMMA));
}

glm::dvec3 rgbToXyz(glm::dvec3 rgb)
{
    return RGB_TO_XYZ * rgb;
}

glm::dvec3 xyzToRgb(glm::dvec3 xyz)
{
    return XYZ_TO_RGB * xyz;
}

glm::dvec3 xyzToXyy(glm::dvec3 xyz)
{
    double Y = xyz.y;
    double x = xyz.x / (xyz.x + xyz.y + xyz.z);
    double y = xyz.y / (xyz.x + xyz.y + xyz.z);
    return { x, y, Y };
}

glm::dvec3 xyyToXyz(glm::dvec3 xyY)
{
    double Y = xyY.z;
    double x = Y * xyY.x / xyY.y;
    double z = Y * (1.0 - xyY.x - xyY.y) / xyY.y;
    return { x, Y, z };
}

glm::dvec3 rgbToHcv(glm::dvec3 rgb)
{
    glm::dvec4 P = (rgb.g < rgb.b) ? glm::dvec4 { rgb.b, rgb.g, -1.0, 2.0 / 3.0 } : glm::dvec4 { rgb.g, rgb.b, 0.0, -1.0 / 3.0 };
    glm::dvec4 Q = (rgb.r < P.x) ? glm::dvec4 { P.x, P.y, P.w, rgb.r } : glm::dvec4 { rgb.r, P.y, P.z, P.x };
    double C = Q.x - std::min(Q.w, Q.y);
    double H = abs((Q.w - Q.y) / (6.0 * C + HCV_EPSILON) + Q.z);
    return { H, C, Q.x };
}

glm::dvec3 hueToRgb(double hue)
{
    hue = std::fmod(hue, 1.0);
    if (hue < 0.0) {
        hue += 1.0;
    }
    double R = abs(hue * 6.0 - 3.0) - 1.0;
    double G = 2.0 - abs(hue * 6.0 - 2.0);
    double B = 2.0 - abs(hue * 6.0 - 4.0);
    return glm::clamp(glm::dvec3 { R, G, B }, glm::dvec3 { 0.0 }, glm::dvec3 { 1.0 });
}

glm::dvec3 hsvToRgb(glm::dvec3 hsv)
{
    glm::dvec3 rgb = hueToRgb(hsv.x);
    return ((rgb - 1.0) * hsv.y + 1.0) * hsv.z;
}

glm::dvec3 hslToRgb(glm::dvec3 hsl)
{
    glm::dvec3 rgb = hueToRgb(hsl.x);
    double C = (1.0 - abs(2.0 * hsl.z - 1.0)) * hsl.y;
    return (rgb - 0.5) * C + hsl.z;
}

glm::dvec3 hcyToRgb(glm::dvec3 hcy)
{
    glm::dvec3 RGB = hueToRgb(hcy.x);
    double Z = glm::dot(RGB, HCY_WTS);
    if (hcy.z < Z) {
        hcy.y *= hcy.z / Z;
    } else if (Z < 1.0) {
        hcy.y *= (1.0 - hcy.z) / (1.0 - Z);
    }
    return (RGB - Z) * hcy.y + hcy.z;
}

glm::dvec3 rgbToHsv(glm::dvec3 rgb)
{
    glm::dvec3 HCV = rgbToHcv(rgb);
    double S = HCV.y / (HCV.z + HCV_EPSILON);
    return glm::dvec3(HCV.x, S, HCV.z);
}

glm::dvec3 rgbToHsl(glm::dvec3 rgb)
{
    glm::dvec3 HCV = rgbToHcv(rgb);
    double L = HCV.z - HCV.y * 0.5;
    double S = HCV.y / (1.0 - abs(L * 2.0 - 1.0) + HSL_EPSILON);
    return glm::dvec3(HCV.x, S, L);
}

glm::dvec3 rgbToHcy(glm::dvec3 rgb)
{
    glm::dvec3 HCV = rgbToHcv(rgb);
    double Y = glm::dot(rgb, HCY_WTS);
    double Z = glm::dot(hueToRgb(HCV.x), HCY_WTS);
    if (Y < Z) {
        HCV.y *= Z / (HCY_EPSILON + Y);
    } else {
        HCV.y *= (1.0 - Z) / (HCY_EPSILON + 1.0 - Y);
    }
    return { HCV.x, HCV.y, Y };
}

glm::dvec3 rgbToYcbcr(glm::dvec3 rgb)
{
    double y = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;
    double cb = (rgb.b - y) * 0.565;
    double cr = (rgb.r - y) * 0.713;

    return { y, cb, cr };
}

glm::dvec3 ycbcrToRgb(glm::dvec3 yuv)
{
    return {
        yuv.x + 1.403 * yuv.z,
        yuv.x - 0.344 * yuv.y - 0.714 * yuv.z,
        yuv.x + 1.770 * yuv.y,
    };
}

}

const ux3d::operations::Operation ux3d::operations::image::applyTextureTransform = {
    .name = "apply_texture_transform",

    .schema = schema::operation(
        "Apply Texture Transform",
        "Apply texture transform to referenced image",
        {
            { "type", "object" },
            { "additionalProperties", false },
            { "required", { "path" } },
            {
                "properties",
                {
                    schema::parameter::path("/gltf/.*/extensions/KHR_texture_transform"),
                },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto textureTransformPath = nlohmann::json::json_pointer { params["path"] };

        auto textureInfoPath = textureTransformPath.parent_pointer().parent_pointer();
        if (!state.contains(textureTransformPath) || state[textureTransformPath].size() == 0) {
            return state;
        }

        auto offset = state[textureTransformPath].value("offset", glm::dvec2 { 0.0, 0.0 });
        auto rotation = state[textureTransformPath].value("rotation", 0.0);
        auto scale = state[textureTransformPath].value("scale", glm::dvec2 { 1.0, 1.0 });

        auto texturePath = nlohmann::json::json_pointer { "/gltf/textures" } / state[textureInfoPath]["index"].get<size_t>();
        auto imagePath = nlohmann::json::json_pointer { "/gltf/images" } / state[texturePath]["source"].get<size_t>();

        auto imageData = reactor::utils::decodeImageData(imagePath, state, dataStorage);
        size_t width = imageData.info["width"];
        size_t height = imageData.info["height"];
        size_t depth = imageData.info["channels"];

        auto wrapS = reactor::gltf::TextureWrappingMode::Repeat;
        auto wrapT = reactor::gltf::TextureWrappingMode::Repeat;
        auto filter = reactor::gltf::TextureFilter::Linear;

        if (state[texturePath].contains("sampler")) {
            size_t sampler_index = state[texturePath]["sampler"];
            wrapS = state["gltf"]["samplers"][sampler_index].value("wrapS", reactor::gltf::TextureWrappingMode::Repeat);
            wrapT = state["gltf"]["samplers"][sampler_index].value("wrapT", reactor::gltf::TextureWrappingMode::Repeat);
            filter = state["gltf"]["samplers"][sampler_index].value("filter", reactor::gltf::TextureFilter::Linear);
        }

        std::vector<uint8_t> output;
        output.resize(width * height * depth);
        for (size_t y = 1; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                size_t i = y * height + x;
                i *= depth;

                glm::dvec2 st {
                    static_cast<double>(x) / static_cast<double>(width),
                    static_cast<double>(y) / static_cast<double>(height),
                };

                st = glm::rotate(st, rotation);
                st *= scale;
                st += offset;

                auto s = sampling::sample(imageData.data, width, height, depth, wrapS, wrapT, filter, st);
                auto q = quantize(s);

                for (size_t d = 0; d < depth; ++d) {
                    output[i + d] = q[d];
                }
            }
        }

        imageData.data = std::move(output);

        auto transformedImage = nlohmann::json::object();
        transformedImage["uri"] = reactor::utils::createURIStem(state) + ".png";
        state["gltf"]["images"].emplace_back(transformedImage);

        auto transformedImagePath = reactor::utils::getDataIDPath(transformedImage, state);
        state[transformedImagePath] = dataStorage.insert(reactor::image::encode(imageData, { { "mimeType", "image/png" } }));

        auto transformedTexture = state[texturePath];
        transformedTexture["source"] = state["gltf"]["images"].size() - 1;
        state["gltf"]["textures"].emplace_back(std::move(transformedTexture));

        state[textureInfoPath]["index"] = state["gltf"]["textures"].size() - 1;

        state[textureInfoPath]["extensions"].erase("KHR_texture_transform");

        return state;
    }
};

const ux3d::operations::Operation ux3d::operations::image::transform = {
    .name = "transform_image",

    .schema = schema::operation(
        "Transform Image",
        "Translate, scale, and rotate the content of an image",
        {
            { "type", "object" },
            { "additionalProperties", false },
            { "required", { "path" } },
            {
                "properties",
                {
                    schema::parameter::path("/gltf/images/\\d+"),
                    {
                        "offset",
                        {
                            { "type", "array" },
                            { "minItems", 2 },
                            { "maxItems", 2 },
                            { "default", { 0, 0 } },
                        },
                    },
                    {
                        "scale",
                        {
                            { "type", "array" },
                            { "minItems", 2 },
                            { "maxItems", 2 },
                            { "default", { 1, 1 } },
                        },
                    },
                    { "rotation", schema::number(0) },
                    {
                        "wrapS",
                        schema::enumeration(
                            {
                                reactor::gltf::TextureWrappingMode::ClampToEdge,
                                reactor::gltf::TextureWrappingMode::MirroredRepeat,
                                reactor::gltf::TextureWrappingMode::Repeat,
                            },
                            reactor::gltf::TextureWrappingMode::Repeat),
                    },
                    {
                        "wrapT",
                        schema::enumeration(
                            {
                                reactor::gltf::TextureWrappingMode::ClampToEdge,
                                reactor::gltf::TextureWrappingMode::MirroredRepeat,
                                reactor::gltf::TextureWrappingMode::Repeat,
                            },
                            reactor::gltf::TextureWrappingMode::Repeat),
                    },
                    {
                        "filter",
                        schema::enumeration(
                            {
                                reactor::gltf::TextureFilter::Nearest,
                                reactor::gltf::TextureFilter::Linear,
                                reactor::gltf::TextureFilter::NearestMipmapNearest,
                                reactor::gltf::TextureFilter::LinearMipmapNearest,
                                reactor::gltf::TextureFilter::NearestMipmapLinear,
                                reactor::gltf::TextureFilter::LinearMipmapLinear,
                            },
                            reactor::gltf::TextureFilter::Linear),
                    },
                },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto imagePath = nlohmann::json::json_pointer { params["path"] };
        glm::dvec2 offset = params["offset"];
        glm::dvec2 scale = params["scale"];
        double rotation = params["rotation"];
        reactor::gltf::TextureWrappingMode wrapS = params["wrapS"];
        reactor::gltf::TextureWrappingMode wrapT = params["wrapT"];
        reactor::gltf::TextureFilter filter = params["filter"];

        auto imageData = reactor::utils::decodeImageData(imagePath, state, dataStorage);
        size_t width = imageData.info["width"];
        size_t height = imageData.info["height"];
        size_t depth = imageData.info["channels"];

        std::vector<uint8_t> output;
        output.resize(width * height * depth);
        for (size_t y = 1; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                size_t i = y * height + x;
                i *= depth;

                glm::dvec2 st {
                    static_cast<double>(x) / static_cast<double>(width),
                    static_cast<double>(y) / static_cast<double>(height),
                };

                st = glm::rotate(st, rotation);
                st *= scale;
                st += offset;

                auto s = sampling::sample(imageData.data, width, height, depth, wrapS, wrapT, filter, st);
                auto q = quantize(s);

                for (size_t d = 0; d < depth; ++d) {
                    output[i + d] = q[d];
                }
            }
        }

        imageData.data = std::move(output);

        reactor::utils::setImageData(imagePath, imageData, state, dataStorage);

        return state;
    }
};

static void modulate(ux3d::reactor::image::ImageData& imageData, std::function<glm::dvec3(glm::dvec3)> f)
{
    size_t width = imageData.info["width"];
    size_t height = imageData.info["height"];
    size_t depth = imageData.info["channels"];

    if (depth < 3) {
        throw std::runtime_error { fmt::format("Insufficent image depth of {}", depth) };
    }

    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            size_t i = depth * (y * width + x);

            auto color = dequantize<3>({
                imageData.data[i + 0],
                imageData.data[i + 1],
                imageData.data[i + 2],
            });

            color = f(color);

            auto pixel = quantize(color);
            for (size_t d = 0; d < depth; ++d) {
                imageData.data[i + d] = pixel[d];
            }
        }
    }
}

const ux3d::operations::Operation ux3d::operations::image::hsv = {
    .name = "image_hsv",

    .schema = schema::operation(
        "Hue Saturation Brightness",
        "Apply a color transformation in the HSB/HSV color model.",
        {
            { "type", "object" },
            { "additionalProperties", false },
            { "required", { "path" } },
            {
                "properties",
                {
                    schema::parameter::path("/gltf/images/\\d+"),
                    {
                        "hue",
                        {
                            { "type", "number" },
                            { "default", 0 },
                            { "description", "Additive hue shift in degrees. A value of of 30 rotates reds into yellows." },
                        },
                    },
                    { "saturation", schema::number(1) },
                    { "brightness", schema::number(1) },
                },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto imagePath = nlohmann::json::json_pointer { params["path"] };
        double hue = params["hue"];
        double saturation = params["saturation"];
        double brightness = params["brightness"];

        auto imageData = reactor::utils::decodeImageData(imagePath, state, dataStorage);

        modulate(imageData, [&](glm::dvec3 color) {
            color = color::rgbToHsv(color);

            color.x += hue / 360.0;
            color.y *= saturation;
            color.z *= brightness;

            color = color::hsvToRgb(color);
            color = glm::clamp(color, glm::dvec3 { 0.0 }, glm::dvec3 { 1.0 });
            return color;
        });

        reactor::utils::setImageData(imagePath, imageData, state, dataStorage);

        return state;
    }
};

const ux3d::operations::Operation ux3d::operations::image::colorSpace = {
    .name = "convert_color_space",

    .schema = schema::operation(
        "Convert Color Space",
        "Convert between color spaces",
        {
            { "type", "object" },
            { "required", { "path", "source", "target" } },
            { "additionalProperties", false },
            {
                "properties",
                {
                    schema::parameter::path("/gltf/images/\\d+"),
                    { "source", schema::enumeration({ "rgb", "srgb", "hsv", "hsl", "hcy", "xyz", "xyy", "ycbcr" }) },
                    { "target", schema::enumeration({ "rgb", "srgb", "hsv", "hsl", "hcy", "xyz", "xyy", "ycbcr" }) },
                },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto imagePath = nlohmann::json::json_pointer { params["path"] };
        std::string source = params["source"];
        std::string target = params["target"];

        using TransferFn = std::function<glm::dvec3(glm::dvec3)>;

        TransferFn identity = [](glm::dvec3 c) { return c; };
        auto compose = [](TransferFn g, TransferFn h) {
            return [=](glm::dvec3 c) { return g(h(c)); };
        };

        TransferFn sourceToRgb = identity;
        TransferFn rgbToTarget = identity;

        if (source == "srgb") {
            sourceToRgb = color::srgbToRgb;
        } else if (source == "hsv") {
            sourceToRgb = color::hsvToRgb;
        } else if (source == "hsl") {
            sourceToRgb = color::hslToRgb;
        } else if (source == "hcy") {
            sourceToRgb = color::hcyToRgb;
        } else if (source == "xyz") {
            sourceToRgb = color::xyzToRgb;
        } else if (source == "xyy") {
            sourceToRgb = compose(color::xyzToRgb, color::xyyToXyz);
        } else if (source == "ycbcr") {
            sourceToRgb = color::ycbcrToRgb;
        }

        if (target == "srgb") {
            sourceToRgb = color::rgbToSrgb;
        } else if (target == "hsv") {
            sourceToRgb = color::rgbToHsv;
        } else if (target == "hsl") {
            sourceToRgb = color::rgbToHsl;
        } else if (target == "hcy") {
            sourceToRgb = color::rgbToHcy;
        } else if (target == "xyz") {
            sourceToRgb = color::rgbToXyz;
        } else if (target == "xyy") {
            sourceToRgb = compose(color::xyzToXyy, color::rgbToXyz);
        } else if (target == "ycbcr") {
            sourceToRgb = color::rgbToYcbcr;
        }

        TransferFn transfer = compose(rgbToTarget, sourceToRgb);

        auto imageData = reactor::utils::decodeImageData(imagePath, state, dataStorage);
        modulate(imageData, transfer);
        reactor::utils::setImageData(imagePath, imageData, state, dataStorage);

        return state;
    }
};

static void convolve(
    ux3d::reactor::image::ImageData& imageData,
    int horizontalRadius,
    int verticalRadius,
    std::vector<double> kernel)
{
    int width = imageData.info["width"];
    int height = imageData.info["height"];
    int depth = imageData.info["channels"];

    int kernelWidth = 2 * horizontalRadius + 1;
    int kernelHeight = 2 * verticalRadius + 1;

    assert(kernelWidth * kernelHeight == kernel.size());

    std::vector<uint8_t> output;
    output.resize(imageData.data.size(), 255);

    for (int yi = 0; yi < height; ++yi) {
        for (int xi = 0; xi < width; ++xi) {
            for (size_t d = 0; d < depth; ++d) {
                double acc = 0.0;
                for (int yk = 0; yk < kernelHeight; ++yk) {
                    for (int xk = 0; xk < kernelWidth; ++xk) {
                        int x = std::clamp(xi + xk - kernelWidth / 2, static_cast<int>(0), width - 1);
                        int y = std::clamp(yi + yk - kernelHeight / 2, static_cast<int>(0), height - 1);
                        double value = dequantize(imageData.data[depth * (y * width + x) + d]);
                        double coefficient = kernel[yk * kernelWidth + xk];
                        acc += coefficient * value;
                    }
                }
                int i = depth * (yi * width + xi) + d;
                output[depth * (yi * width + xi) + d] = quantize(acc);
            }
        }
    }

    imageData.data = std::move(output);
}

static std::vector<double> functionalKernel(
    int horizontalRadius,
    int verticalRadius,
    std::function<double(glm::dvec2)> f)
{
    size_t width = 2 * horizontalRadius + 1;
    size_t height = 2 * verticalRadius + 1;
    std::vector<double> kernel;
    kernel.resize(width * height);
    for (int iy = 0; iy < height; ++iy) {
        for (int ix = 0; ix < width; ++ix) {
            double w = f({ ix - horizontalRadius, iy - verticalRadius });
            kernel[iy * width + ix] = w;
        }
    }
    return kernel;
}

const ux3d::operations::Operation ux3d::operations::image::blur = {
    .name = "image_blur",

    .schema = schema::operation(
        "Blur",
        "Apply Gaussian filtering to the image",
        {
            { "type", "object" },
            { "additionalProperties", false },
            { "required", { "path" } },
            {
                "properties",
                {
                    schema::parameter::path("/gltf/images/\\d+"),
                    {
                        "radius",
                        {
                            { "type", "number" },
                            { "default", 5 },
                            { "description", "Defines the pixel radius of the Gaussian filter kernel." },
                        },
                    },
                },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto imagePath = nlohmann::json::json_pointer { params["path"] };
        int radius = params["radius"];
        double sigma = radius / 3.0;

        if (radius <= 0) {
            return state;
        }

        auto imageData = reactor::utils::decodeImageData(imagePath, state, dataStorage);

        auto gaussian = [sigma](double x) {
            x *= -x;
            x /= 2 * sigma * sigma;
            x = std::exp(x);
            x /= std::sqrt(2 * glm::pi<double>() * sigma * sigma);
            return x;
        };

        auto kernelX = functionalKernel(radius, 0, [gaussian](glm::dvec2 p) {
            return gaussian(p.x);
        });

        auto kernelY = functionalKernel(0, radius, [gaussian](glm::dvec2 p) {
            return gaussian(p.y);
        });

        convolve(imageData, radius, 0, kernelX);

        convolve(imageData, 0, radius, kernelY);

        reactor::utils::setImageData(imagePath, imageData, state, dataStorage);

        return state;
    }
};

const ux3d::operations::Operation ux3d::operations::image::laplace = {
    .name = "image_laplace",

    .schema = schema::operation(
        "Laplace",
        "Laplacian edge detection convolution",
        {
            { "type", "object" },
            { "additionalProperties", false },
            { "required", { "path" } },
            {
                "properties",
                { schema::parameter::path("/gltf/images/\\d+") },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto imagePath = nlohmann::json::json_pointer { params["path"] };

        auto imageData = reactor::utils::decodeImageData(imagePath, state, dataStorage);

        std::vector<double> kernel { -1, -1, -1, -1, 8, -1, -1, -1, -1 };

        convolve(imageData, 1, 1, kernel);

        reactor::utils::setImageData(imagePath, imageData, state, dataStorage);

        return state;
    }
};

const ux3d::operations::Operation ux3d::operations::image::sharpen = {
    .name = "image_sharpen",

    .schema = schema::operation(
        "Sharpen",
        "Sharpening image convolution",
        {
            { "type", "object" },
            { "additionalProperties", false },
            { "required", { "path" } },
            {
                "properties",
                {
                    schema::parameter::path("/gltf/images/\\d+"),
                    { "intensity", schema::number(1.0) },
                },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto imagePath = nlohmann::json::json_pointer { params["path"] };
        double amount = params["intensity"];

        auto imageData = reactor::utils::decodeImageData(imagePath, state, dataStorage);

        // https://en.wikipedia.org/wiki/Unsharp_masking
        std::vector<double> kernel;
        kernel.resize(9, 0.0);
        kernel[4] = 1.0 + amount * (1.0 - 1.0 / 5.0);
        kernel[1] = kernel[3] = kernel[5] = kernel[7] = -amount / 5.0;

        convolve(imageData, 1, 1, kernel);

        reactor::utils::setImageData(imagePath, imageData, state, dataStorage);

        return state;
    }
};

const ux3d::operations::Operation ux3d::operations::image::invert = {
    .name = "image_invert",

    .schema = schema::operation(
        "Invert",
        "Produce the negative of an image",
        {
            { "type", "object" },
            { "additionalProperties", false },
            { "required", { "path" } },
            {
                "properties",
                { schema::parameter::path("/gltf/images/\\d+") },
            },
        }),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto imagePath = nlohmann::json::json_pointer { params["path"] };

        auto imageData = reactor::utils::decodeImageData(imagePath, state, dataStorage);

        int width = imageData.info["width"];
        int height = imageData.info["height"];
        int depth = imageData.info["channels"];

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                for (size_t d = 0; d < depth; ++d) {
                    size_t i = depth * (y * width + x) + d;
                    auto value = dequantize(imageData.data[i]);
                    value = 1.0 - value;
                    imageData.data[i] = quantize(value);
                }
            }
        }

        reactor::utils::setImageData(imagePath, imageData, state, dataStorage);

        return state;
    }
};
