#include "gltf/json/GltfObjectFromJson.h"
#include "core/Algorithms.h"
#include "gltf/Extensions.h"
#include "gltf/GltfObject.h"
#include "gltf/MeshoptCompression.h"
#include "gltf/Node.h"

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
                // EXT/KHR_meshopt_compression is read by BufferViewsFromJson / BuffersFromJson and decoded at load; the
                // decoded model has plain bufferViews, so the extension is not kept for writing back.
                if (meshopt::isMeshoptExtension(extensionName)) {
                    continue;
                }
                // KHR_node_visibility / _selectability / _hoverability are read by NodesFromJson into the Node; the
                // Inspector shows those values (read-only), so they are not kept as editable unknown extensions.
                if (extensionName == node_visibility::ExtensionName || extensionName == node_interaction::SelectabilityName
                    || extensionName == node_interaction::HoverabilityName) {
                    continue;
                }
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
