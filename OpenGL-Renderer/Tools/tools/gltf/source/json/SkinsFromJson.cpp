#include "gltf/json/SkinsFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

ux3d::gltf::ConversionResult ux3d::gltf::SkinsFromJson::convert(const nlohmann::json& _jsonSkin)
{
    Skin* pSkin = m_gltf.create<Skin>(getID(_jsonSkin));
    if (pSkin == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pSkin)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSkin.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pSkin->setName(value.get<std::string>());
            } else {
                core::logWarning("skin.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonSkin, pSkin, "skin")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonSkin, pSkin);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ux3d::gltf::ConversionResult ux3d::gltf::SkinsFromJson::resolve(const nlohmann::json& _jsonSkin, Skin& _skin)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSkin.items()) {
        if (key == "skeleton") {
            if (checkIndex(value, m_pointerLookup.Nodes.size(), "skin.skeleton")) {
                Node* pNode = m_pointerLookup.Nodes[value];
                _skin.setSkeletonRoot(*pNode);
            } else {
                containsErrors = true;
            }
        } else if (key == "inverseBindMatrices") {
            if (checkIndex(value, m_pointerLookup.Accessors.size(), "skin.inverseBindMatrices")) {
                Accessor* pAccessor = m_pointerLookup.Accessors[value];
                _skin.setInverseBindMatricesAccessor(*pAccessor);
            } else {
                containsErrors = true;
            }
        } else if (key == "joints") {
            if (value.is_array()) {
                for (const auto& jsonJoint : value) {
                    if (checkIndex(jsonJoint, m_pointerLookup.Nodes.size(), "skin.joints[?]")) {
                        Node* pJoint = m_pointerLookup.Nodes[jsonJoint.get<uint32_t>()];
                        if (pJoint->getStoreAsMatrix()) {
                            core::logWarning("Using matrix transform for a joint might cause issues with some renderers and causes an invalid glTF if used in an animtion. To resolve all joints to TRS use the menu action in the Automation menu.");
                        }
                        _skin.addJoint(*pJoint);
                    } else {
                        containsErrors = true;
                    }
                }
            } else {
                core::logWarning("skin.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
