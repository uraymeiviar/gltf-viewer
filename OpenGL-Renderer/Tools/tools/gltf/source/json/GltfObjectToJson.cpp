#include "gltf/json/GltfObjectToJson.h"
#include "core/String.h"
#include "gltf/ExportOptions.h"
#include "gltf/GltfObject.h"

using namespace ux3d;
using namespace ux3d::gltf;

void GltfObjectToJson::convertUnknownExtensions(const GltfObject& _gltfObject, nlohmann::json& _jsonObject) const
{
    const auto& extensions = _gltfObject.getUnknownExtensions();

    if (extensions.empty()) {
        return;
    }

    if (_jsonObject.is_null()) {
        _jsonObject["extensions"] = nlohmann::json::object();
    }

    for (const auto& extension : extensions) {
        // avoid writing gestaltor specific extensions in glb/gltf files
        if (m_exportOptions.conversionUsage == ConversionUsage::ValidGltf && extension.first == "ux3d_gestaltor") {
            continue;
        }

        nlohmann::json extensionJson = nlohmann::json::parse(extension.second, nullptr, false);
        _jsonObject["extensions"][extension.first] = extensionJson;
    }
}

void GltfObjectToJson::convertExtras(const GltfObject& _gltfObject, nlohmann::json& _jsonObject) const
{
    const core::string& extrasString = _gltfObject.getExtras();

    convertExtras(extrasString, _jsonObject);
}

void GltfObjectToJson::convertExtras(const core::string& _extras, nlohmann::json& _jsonObject) const
{
    if (_extras.empty()) {
        return;
    }

    nlohmann::json extrasJson = nlohmann::json::parse(_extras, nullptr, false);

    if (extrasJson.is_discarded()) // is set if parse is called with allow_exceptions = false
    {
        core::logWarning("Not able to convert extras");
        return;
    }

    _jsonObject["extras"] = extrasJson;
}
