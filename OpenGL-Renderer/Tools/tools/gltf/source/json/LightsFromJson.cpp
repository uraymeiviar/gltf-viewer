#include "gltf/json/LightsFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/ParsingUtils.h"
#include "gltf/json/PointerLookup.h"

ux3d::gltf::ConversionResult ux3d::gltf::LightsFromJson::convert(const nlohmann::json& _jsonLight)
{
    Light* pLight = createLight(_jsonLight, m_gltf);
    if (pLight == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pLight)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonLight.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pLight->setName(value.get<std::string>());
            } else {
                core::logWarning("light.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "color") {
            if (core::Succeeded(checkVector(value, 3, "light.color"))) {
                pLight->setColor(doublesToVector<3>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "intensity") {
            if (value.is_number()) {
                pLight->setIntensity(value.get<double>());
            } else {
                core::logWarning("light.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "range") {
            if (value.is_number()) {
                pLight->setRange(value.get<double>());
            } else {
                core::logWarning("light.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "spot") {
            if (value.is_object()) {
                convertSpot(value, *pLight);
            } else {
                core::logWarning("light.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonLight, pLight, "light")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonLight, pLight);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ux3d::gltf::Light* ux3d::gltf::LightsFromJson::createLight(const nlohmann::json& _jsonLight, Gltf& _Gltf) const
{
    if (_jsonLight["type"].is_string()) {
        const std::string& jsonType = _jsonLight["type"];
        Light::Type type = Light::Type::Directional;
        if (jsonType == "point") {
            type = Light::Type::Point;
        } else if (jsonType == "spot") {
            type = Light::Type::Spot;
        } else if (jsonType != "directional") {
            core::logError("Could not create light. Invalid type");
            return nullptr;
        }
        return _Gltf.create<Light>(type);
    } else {
        core::logError("Could not create light. Invalid type");
        return nullptr;
    }
}

ux3d::gltf::ConversionResult ux3d::gltf::LightsFromJson::convertSpot(const nlohmann::json& _jsonSpot, Light& _Light) const
{
    if (_Light.getSpot().has_value() == false) {
        _Light.createSpot();
    }
    Light::Spot& spot = _Light.getSpot().value();
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSpot.items()) {
        if (key == "innerConeAngle") {
            if (value.is_number()) {
                spot.setInnerConeAngle(value.get<double>());
            } else {
                core::logWarning("light.spot.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "outerConeAngle") {
            if (value.is_number()) {
                spot.setOuterConeAngle(value.get<double>());
            } else {
                core::logWarning("light.spot.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonSpot, &spot, "light.spot")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonSpot, &spot);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
