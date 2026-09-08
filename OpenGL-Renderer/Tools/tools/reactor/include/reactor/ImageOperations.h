// Copyright © 2022 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vector>

#include "datastorage/DataStorage.h"

namespace ux3d::reactor::image {
static const double SRGB_GAMMA = 2.2;
static const glm::dvec3 DIELECTRIC_SPECULAR { 0.04, 0.04, 0.04 };
static const double EPSILON = 1e-6;
static const glm::dvec3 PERCEIVED_BRIGHTNESS_COEFFICENTS { 0.299, 0.587, 0.114 };

struct ImageData {
    std::vector<uint8_t> data;
    nlohmann::json info;
};

double perceivedBrightness(const glm::dvec3& v);
glm::dvec3 toLinear(const glm::dvec3& v);
glm::dvec4 toLinear(const glm::dvec4& v);
glm::dvec3 toSrgb(const glm::dvec3& v);
glm::dvec4 toSrgb(const glm::dvec4& v);
double maxComponent(const glm::dvec3& v);
double solveMetallic(double diffuseBrightness, double specularBrightness, double oneMinusSpecularStrength, double dielectricSpecular);
struct MetallicRoughnessValue {
    glm::dvec4 baseColor;
    double metallic;
    double roughness;
};
MetallicRoughnessValue convertSpecularGlossinessToMetallicRoughness(glm::dvec4 diffuse, glm::dvec3 specularColor, double glossiness);

ImageData decode(const std::vector<uint8_t>& rawData);
/// @brief Extractes image meta data from raw image data.
/// @param rawData
/// @throw std::runtime_error if the image data is invalid. (or other exceptions)
/// @return returns a json with these properties: width, height, channels, mimeType
// { "width": number, "height": number, "channels": number (3 or 4 most likely), "mimeType": string (something like "image/webp"/ "image/png") }
nlohmann::json decodeInfo(const std::vector<uint8_t>& rawData);

std::vector<uint8_t> encode(const ImageData& input, const nlohmann::json& encodeInformation);
image::ImageData scaleImage(const image::ImageData& input, int width, int height);
image::ImageData blendImages(const image::ImageData& inputA, const image::ImageData& inputB, float weight);
std::string getMimeType(const std::vector<uint8_t>& encodedBytes);
std::string getFileExtension(const std::string& mimeType);

}