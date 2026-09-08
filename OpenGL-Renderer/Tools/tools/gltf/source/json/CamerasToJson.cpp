#include "gltf/json/CamerasToJson.h"
#include "gltf/Camera.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d;
using namespace ux3d::gltf;

void CamerasToJson::convert(const Camera& _camera, nlohmann::json& _jsonCamera, const JsonPath& _jsonPath) const
{
    convertBase(_camera, _jsonCamera);
    convertType(_camera, _jsonCamera);
    convertOrthographic(_camera, _jsonCamera);
    convertPerspective(_camera, _jsonCamera);
}

void CamerasToJson::convertType(const Camera& _camera, nlohmann::json& _jsonCamera) const
{
    if (_camera.getType() == Camera::ProjectionType::Perspective) {
        _jsonCamera["type"] = "perspective";
    } else if (_camera.getType() == Camera::ProjectionType::Orthographic) {
        _jsonCamera["type"] = "orthographic";
    } else {
        core::logWarning("CamerasToJson: Unknown camera type");
    }
}

void CamerasToJson::convertOrthographic(const Camera& _camera, nlohmann::json& _jsonCamera) const
{
    const auto& orthographic = _camera.getCameraOrthographic();
    if (!orthographic.has_value()) {
        return;
    }

    nlohmann::json jsonOrthographic = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(orthographic.value(), jsonOrthographic);
    gltfObjectToJson.convertUnknownExtensions(orthographic.value(), jsonOrthographic);

    jsonOrthographic["xmag"] = orthographic->getXMag();
    jsonOrthographic["ymag"] = orthographic->getYMag();
    jsonOrthographic["zfar"] = orthographic->getZFar();
    jsonOrthographic["znear"] = orthographic->getZNear();

    _jsonCamera["orthographic"] = jsonOrthographic;
}

void CamerasToJson::convertPerspective(const Camera& _camera, nlohmann::json& _jsonCamera) const
{
    const auto& perspective = _camera.getCameraPerspective();
    if (!perspective.has_value()) {
        return;
    }

    nlohmann::json jsonPerspective = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(perspective.value(), jsonPerspective);
    gltfObjectToJson.convertUnknownExtensions(perspective.value(), jsonPerspective);

    jsonPerspective["yfov"] = perspective->getYFov();
    jsonPerspective["znear"] = perspective->getZNear();

    if (perspective->getAspectRatio().has_value()) {
        jsonPerspective["aspectRatio"] = perspective->getAspectRatio().value();
    }
    if (perspective->getZFar().has_value()) {
        jsonPerspective["zfar"] = perspective->getZFar().value();
    }

    _jsonCamera["perspective"] = jsonPerspective;
}
