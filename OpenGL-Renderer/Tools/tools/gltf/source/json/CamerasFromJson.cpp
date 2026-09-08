#include "gltf/json/CamerasFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

ux3d::gltf::ConversionResult ux3d::gltf::CamerasFromJson::convert(const nlohmann::json& _jsonCamera)
{
    const core::string jsonType = _jsonCamera["type"].get<core::string>();

    Camera::ProjectionType cameraType;
    if (jsonType == "perspective") {
        cameraType = Camera::ProjectionType::Perspective;
    } else if (jsonType == "orthographic") {
        cameraType = Camera::ProjectionType::Orthographic;
    } else {
        core::logError("CamerasFromJson: Cannot parse Camera with unknown type '%s'", jsonType.c_str());
        return ConversionError::InvalidType;
    }

    Camera* pCamera = m_gltf.create<Camera>(cameraType);
    if (pCamera == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pCamera)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonCamera.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pCamera->setName(value.get<std::string>());
            } else {
                core::logWarning("camera.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "orthographic") {
            if (cameraType != Camera::ProjectionType::Orthographic) {
                core::logWarning("Camera has orthographic properties defined but is not orthographic. These properties are ignored.");
                continue;
            }
            if (value.is_object()) {
                for (const auto& [subkey, subvalue] : value.items()) {
                    if (subkey == "xmag") {
                        if (subvalue.is_number()) {
                            pCamera->getCameraOrthographic()->setXMag(subvalue);
                        } else {
                            core::logWarning("camera.%s.%s with value '%s' has invalid type", key.c_str(), subvalue.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (subkey == "ymag") {
                        if (subvalue.is_number()) {
                            pCamera->getCameraOrthographic()->setYMag(subvalue);
                        } else {
                            core::logWarning("camera.%s.%s with value '%s' has invalid type", key.c_str(), subvalue.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (subkey == "zfar") {
                        if (subvalue.is_number()) {
                            pCamera->getCameraOrthographic()->setZFar(subvalue);
                        } else {
                            core::logWarning("camera.%s.%s with value '%s' has invalid type", key.c_str(), subvalue.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (subkey == "znear") {
                        if (subvalue.is_number()) {
                            pCamera->getCameraOrthographic()->setZNear(subvalue);
                        } else {
                            core::logWarning("camera.%s.%s with value '%s' has invalid type", key.c_str(), subvalue.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
                if (pCamera->getCameraOrthographic().has_value()) {
                    if (!GltfObjectFromJson::convertUnknownExtensions(value, &pCamera->getCameraOrthographic().value(), "camera.orthographic")) {
                        containsErrors = true;
                    }
                    GltfObjectFromJson::convertExtras(value, &pCamera->getCameraOrthographic().value());
                }
            } else {
                core::logWarning("camera.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "perspective") {
            if (cameraType != Camera::ProjectionType::Perspective) {
                core::logWarning("Camera has perspective properties defined but is not perspective. These properties are ignored.");
                continue;
            }
            if (value.is_object()) {
                for (const auto& [subkey, subvalue] : value.items()) {
                    if (subkey == "aspectRatio") {
                        if (subvalue.is_number()) {
                            pCamera->getCameraPerspective()->setAspectRatio(subvalue);
                        } else {
                            core::logWarning("camera.%s.%s with value '%s' has invalid type", key.c_str(), subvalue.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (subkey == "yfov") {
                        if (subvalue.is_number()) {
                            pCamera->getCameraPerspective()->setYFov(subvalue);
                        } else {
                            core::logWarning("camera.%s.%s with value '%s' has invalid type", key.c_str(), subvalue.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (subkey == "zfar") {
                        if (subvalue.is_number()) {
                            pCamera->getCameraPerspective()->setZFar(subvalue);
                        } else {
                            core::logWarning("camera.%s.%s with value '%s' has invalid type", key.c_str(), subvalue.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (subkey == "znear") {
                        if (subvalue.is_number()) {
                            pCamera->getCameraPerspective()->setZNear(subvalue);
                        } else {
                            core::logWarning("camera.%s.%s with value '%s' has invalid type", key.c_str(), subvalue.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
                if (pCamera->getCameraPerspective().has_value()) {
                    if (!GltfObjectFromJson::convertUnknownExtensions(value, &pCamera->getCameraPerspective().value(), "camera.perspective")) {
                        containsErrors = true;
                    }
                    GltfObjectFromJson::convertExtras(value, &pCamera->getCameraPerspective().value());
                }
            } else {
                core::logWarning("camera.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonCamera, pCamera, "camera")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonCamera, pCamera);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
