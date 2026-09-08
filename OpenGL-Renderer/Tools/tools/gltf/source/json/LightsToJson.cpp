// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/LightsToJson.h"
#include "gltf/Light.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d;
using namespace ux3d::gltf;

void LightsToJson::convert(const Light& _light, nlohmann::json& _jsonLight, const JsonPath& _jsonPath) const
{
    convertBase(_light, _jsonLight);
    convertColor(_light, _jsonLight);
    convertIntensity(_light, _jsonLight);
    convertType(_light, _jsonLight);
    convertRange(_light, _jsonLight);
    convertSpot(_light, _jsonLight);
}

void LightsToJson::convertColor(const Light& _light, nlohmann::json& _jsonLight) const
{
    if (_light.hasColor() == false) {
        return;
    }

    math::dvec3 color = _light.getColorOrDefault();
    nlohmann::json jsonColor = nlohmann::json::array();

    jsonColor.emplace_back(color.r);
    jsonColor.emplace_back(color.g);
    jsonColor.emplace_back(color.b);

    _jsonLight["color"] = jsonColor;
}

void LightsToJson::convertIntensity(const Light& _light, nlohmann::json& _jsonLight) const
{
    if (_light.hasIntensity()) {
        _jsonLight["intensity"] = _light.getIntensityOrDefault();
    }
}

void LightsToJson::convertType(const Light& _light, nlohmann::json& _jsonLight) const
{
    if (_light.getType() == Light::Type::Directional) {
        _jsonLight["type"] = "directional";
    } else if (_light.getType() == Light::Type::Point) {
        _jsonLight["type"] = "point";
    } else if (_light.getType() == Light::Type::Spot) {
        _jsonLight["type"] = "spot";
    } else {
        core::logWarning("LightsToJson: Unknown light type");
    }
}

void LightsToJson::convertRange(const Light& _light, nlohmann::json& _jsonLight) const
{
    if (_light.hasRange()) {
        _jsonLight["range"] = _light.getRangeOrDefault();
    }
}

void LightsToJson::convertSpot(const Light& _light, nlohmann::json& _jsonLight) const
{
    const auto& spot = _light.getSpot();
    if (!spot.has_value()) {
        return;
    }

    nlohmann::json jsonSpot = nlohmann::json::object();
    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(spot.value(), jsonSpot);
    gltfObjectToJson.convertUnknownExtensions(spot.value(), jsonSpot);

    if (spot->hasInnerConeAngle()) {
        jsonSpot["innerConeAngle"] = spot->getInnerConeAngleOrDefault();
    }
    if (spot->hasOuterConeAngle()) {
        jsonSpot["outerConeAngle"] = spot->getOuterConeAngleOrDefault();
    }
    if (!jsonSpot.empty() || _light.getType() == Light::Type::Spot) {
        // Spot property (even if it is empty) is required for spot light type
        _jsonLight["spot"] = jsonSpot;
    }
}
