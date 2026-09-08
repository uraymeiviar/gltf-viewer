#include "gltf/json/GltfObjectFromJson.h"
#include "core/Algorithms.h"
#include "gltf/Extensions.h"
#include "gltf/GltfObject.h"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult GltfObjectFromJson::convertUnknownExtensions(const nlohmann::json& _json, GltfObject* _gltfObject, const core::string& _logMessage)
{
    if (_gltfObject == nullptr) {
        return ConversionError::SuccessWithErrors;
    }
    bool containsErrors = false;
    auto pValue = _json.find("extensions");
    if (pValue != _json.end()) {
        if (pValue->is_object()) {
            auto extensionNames = getAllExtensionNames();
            for (const auto& [extensionName, extension] : pValue->items()) {
                if (core::find(extensionNames.begin(), extensionNames.end(), extensionName) == extensionNames.end()) {
                    if (extension.is_object()) {
                        bool skipExtension = false;
                        if (g_importOptions.has_value() && g_importOptions->skipExtensions.has_value()) {
                            if (g_importOptions->skipExtensions.value().count(extensionName) != 0) {
                                skipExtension = true;
                            }
                        }
                        if (!skipExtension) {
                            _gltfObject->addUnknownExtension(extensionName, extension.dump());
                        }

                    } else {
                        core::logWarning("%s.extensions.%s with value '%s' has invalid type", _logMessage.c_str(), extensionName.c_str(), extension.dump().c_str());
                        containsErrors = true;
                    }
                }
            }
        } else {
            core::logWarning("%s.extensions with value '%s' has invalid type", _logMessage.c_str(), pValue->dump().c_str());
            containsErrors = true;
        }
    }
    return containsErrors ? ConversionError::SuccessWithErrors : ConversionResult::Success;
}

void GltfObjectFromJson::convertExtras(const nlohmann::json& _json, GltfObject* _gltfObject)
{
    if (_gltfObject != nullptr) {
        auto pValue = _json.find("extras");
        if (pValue != _json.end()) {
            _gltfObject->setExtras(pValue->dump());
        }
    }
}
