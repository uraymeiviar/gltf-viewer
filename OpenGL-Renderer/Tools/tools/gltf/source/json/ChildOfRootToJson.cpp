#include "gltf/json/ChildOfRootToJson.h"
#include "core/Logger.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/ExportOptions.h"

void ux3d::gltf::ChildOfRootToJson::convertBase(const ChildOfRoot& _gltfObject, nlohmann::json& _jsonObject) const
{
    checkValidity(_gltfObject);
    convertName(_gltfObject, _jsonObject);

    convertUnknownExtensions(_gltfObject, _jsonObject);
    convertExtras(_gltfObject, _jsonObject);

    if (m_exportOptions.conversionUsage == ConversionUsage::StateHistory) {
        convertId(_gltfObject, _jsonObject);
    }
}

void ux3d::gltf::ChildOfRootToJson::checkValidity(const ChildOfRoot& _gltfObject) const
{
    if (_gltfObject.isValid()) {
        return;
    }

    core::string message = "Skipping invalid properties while serializing " + core::string(gltfTypeToString(_gltfObject.getGltfType()));
    if (!_gltfObject.getName().empty()) {
        message += " with name '" + _gltfObject.getName() + "'";
    }

    core::Vector<core::string> invalidProperties;
    if (_gltfObject.getInvalidProperties(invalidProperties)) {
        nlohmann::json jsonProperties = nlohmann::json::array();

        for (const core::string& property : invalidProperties) {
            jsonProperties.emplace_back(property);
        }

        message += ". Invalid properties: " + jsonProperties.dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
    }

    core::logWarning(message.c_str());
}

void ux3d::gltf::ChildOfRootToJson::convertName(const ChildOfRoot& _gltfObject, nlohmann::json& _jsonObject) const
{
    if (_gltfObject.hasName()) {
        _jsonObject["name"] = _gltfObject.getName();
    }
}

void ux3d::gltf::ChildOfRootToJson::convertId(const ChildOfRoot& _gltfObject, nlohmann::json& _jsonObject) const
{
    _jsonObject["extensions"]["ux3d_gestaltor"]["id"] = _gltfObject.m_ID;
}
