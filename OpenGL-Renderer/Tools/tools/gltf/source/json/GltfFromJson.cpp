#include "gltf/json/GltfFromJson.h"
#include "core/LoadProgress.h"
#include "core/String.h"
#include "gltf/Gltf.h"
#include "gltf/MaterialVariant.h"
#include "gltf/json/AccessorsFromJson.h"
#include "gltf/json/AnimationsFromJson.h"
#include "gltf/json/AudioEmitterFromJson.h"
#include "gltf/json/AudioSourceFromJson.h"
#include "gltf/json/BufferViewsFromJson.h"
#include "gltf/json/BuffersFromJson.h"
#include "gltf/json/CamerasFromJson.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/ImagesFromJson.h"
#include "gltf/json/LightsFromJson.h"
#include "gltf/json/MaterialsFromJson.h"
#include "gltf/json/MeshesFromJson.h"
#include "gltf/json/NodesFromJson.h"
#include "gltf/json/SamplersFromJson.h"
#include "gltf/json/ScenesFromJson.h"
#include "gltf/json/SkinsFromJson.h"
#include "gltf/json/TexturesFromJson.h"
#include "gltf/json/XMPFromJson.h"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult GltfFromJson::convert(const core::string& _gltfString, std::optional<ImportOptions> importOptions)
{
    core::loading::begin(core::loading::Json);
    core::loading::Timer timer(core::loading::Json);
    if (_gltfString.empty()) {
        return ConversionError::InvalidJson;
    }

    m_gltf.reset();

    m_json = nlohmann::json::parse(_gltfString, nullptr, false);

    if (m_json.is_discarded()) // is set if parse is called with allow_exeptions = false
    {
        core::logError("GltfFromJson: Invalid Json File");
        return ConversionError::InvalidJson;
    }

    GltfObjectFromJson::g_importOptions = importOptions;

    convertAsset();
    convertRootExtensions();
    convertRequiredExtensions();

    ConversionResult result;

    result = core::Max(createObjects(), result);
    result = core::Max(resolveDependencies(), result);

    return result;
}

ConversionResult GltfFromJson::createObjects()
{
    ConversionResult result;

    // convert extensions first to avoid potential dependency problems
    // (i.e. animations are stored (and therefore loaded) before extensions in gltf.
    // leading to a dependency issue with the xmp extension which is would not yet be loaded
    // if extensions where handled in the below or loop.)
    if (m_json.contains("extensions")) {
        nlohmann::json extensions = m_json["extensions"];
        if (extensions.is_object()) {
            for (const auto& [extensionName, extension] : extensions.items()) {
                if (extensionName == "KHR_lights_punctual") {
                    if (extension.is_object()) {
                        GltfObject& lightsPunctual = m_gltf.createLightsPunctual();
                        if (!GltfObjectFromJson::convertUnknownExtensions(extension, &lightsPunctual, "gltf.extensions.KHR_lights_punctual")) {
                            core::logWarning("extensions.%s with value '%s' has unknown extension", extensionName.c_str(), extension.dump().c_str());
                            result = ConversionError::SuccessWithErrors;
                        }
                        GltfObjectFromJson::convertExtras(extension, &lightsPunctual);

                        LightsFromJson converter(m_gltf, m_pointerLookup);
                        if (extension.contains("lights") && extension["lights"].is_array()) {
                            result = core::Max(create(extension["lights"], converter), result);
                        } else {
                            core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                            result = ConversionError::SuccessWithErrors;
                        }
                    } else {
                        core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                        result = ConversionError::SuccessWithErrors;
                    }
                }
                if (extensionName == "KHR_materials_variants") {
                    if (extension.is_object()) {
                        GltfObject& materialVariants = m_gltf.createMaterialVariants();
                        result = resolveMaterialVariants(extension);
                        if (result != ConversionResult::Success) {
                            return result;
                        }

                        if (!GltfObjectFromJson::convertUnknownExtensions(extension, &materialVariants, "gltf.extensions.KHR_materials_variants")) {
                            core::logWarning("extensions.%s with value '%s' has unknown extension", extensionName.c_str(), extension.dump().c_str());
                            result = ConversionError::SuccessWithErrors;
                        }
                        GltfObjectFromJson::convertExtras(extension, &materialVariants);
                    } else {
                        core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                        result = ConversionError::SuccessWithErrors;
                    }
                }
                if (extensionName == "KHR_xmp_json_ld") {
                    if (extension.is_object()) {
                        XMPFromJson converter(m_gltf);
                        result = converter.convert(extension);
                    } else {
                        core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                        result = ConversionError::SuccessWithErrors;
                    }
                }
                if (extensionName == "KHR_audio") {
                    if (extension.is_object()) {
                        GltfObject& audio = m_gltf.createAudio();
                        if (extension.contains("sources")) {
                            if (extension["sources"].is_array()) {
                                AudioSourceFromJson converter(m_gltf, m_pointerLookup);
                                result = core::Max(create(extension["sources"], converter), result);
                            } else {
                                core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                                result = ConversionError::SuccessWithErrors;
                            }
                        }
                        if (extension.contains("emitters")) {
                            if (extension["emitters"].is_array()) {
                                AudioEmitterFromJson converter(m_gltf, m_pointerLookup);
                                result = core::Max(create(extension["emitters"], converter), result);
                            } else {
                                core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                                result = ConversionError::SuccessWithErrors;
                            }
                        }

                        if (!GltfObjectFromJson::convertUnknownExtensions(extension, &audio, "gltf.extensions.KHR_materials_variants")) {
                            core::logWarning("extensions.%s with value '%s' has unknown extension", extensionName.c_str(), extension.dump().c_str());
                            result = ConversionError::SuccessWithErrors;
                        }
                        GltfObjectFromJson::convertExtras(extension, &audio);
                    } else {
                        core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                        result = ConversionError::SuccessWithErrors;
                    }
                }
            }
        }
    }

    for (const auto& [key, value] : m_json.items()) {
        if (value.is_array()) {
            if (key == "scenes") {
                ScenesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "nodes") {
                NodesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "meshes") {
                MeshesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "cameras") {
                CamerasFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "skins") {
                SkinsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "accessors") {
                AccessorsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "materials") {
                MaterialsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "animations") {
                AnimationsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "bufferViews") {
                BufferViewsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "textures") {
                TexturesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "buffers") {
                BuffersFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "images") {
                ImagesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            } else if (key == "samplers") {
                SamplersFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(create(value, converter), result);
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(m_json, &m_gltf, "gltf")) {
        result = ConversionError::SuccessWithErrors;
    }
    GltfObjectFromJson::convertExtras(m_json, &m_gltf);

    return result;
}

ConversionResult GltfFromJson::resolveDependencies()
{
    ConversionResult result;

    for (const auto& [key, value] : m_json.items()) {
        if (value.is_array()) {
            if (key == "nodes") {
                NodesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getNodes(), converter), result);
            } else if (key == "meshes") {
                MeshesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getMeshes(), converter), result);
            } else if (key == "materials") {
                MaterialsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getMaterials(), converter), result);
            } else if (key == "textures") {
                TexturesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getTextures(), converter), result);
            } else if (key == "images") {
                ImagesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getImages(), converter), result);
            } else if (key == "accessors") {
                AccessorsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getAccessors(), converter), result);
            } else if (key == "bufferViews") {
                BufferViewsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getBufferViews(), converter), result);
            } else if (key == "animations") {
                AnimationsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getAnimations(), converter), result);
            } else if (key == "skins") {
                SkinsFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getSkins(), converter), result);
            } else if (key == "scenes") {
                ScenesFromJson converter(m_gltf, m_pointerLookup);
                result = core::Max(resolve(value, m_gltf.getScenes(), converter), result);
            }
        } else if (key == "scene") {
            if (value.is_number_unsigned()) {
                result = core::Max(resolveDefaultScene(value), result);
            } else {
                core::logWarning("scene with value '%s' has invalid type", value.dump().c_str());

                result = core::Max(ConversionError::InvalidType, result);
            }
        } else if (value.is_object()) {
            if (key == "asset") {
                result = core::Max(resolveAsset(value), result);
            }
        }
    }

    if (m_json.contains("extensions")) {
        nlohmann::json extensions = m_json["extensions"];
        if (extensions.is_object()) {
            for (const auto& [extensionName, extension] : extensions.items()) {
                if (extensionName == "KHR_audio") {
                    if (extension.is_object()) {
                        if (extension.contains("sources")) {
                            AudioSourceFromJson converter(m_gltf, m_pointerLookup);
                            result = core::Max(resolve(extension["sources"], m_gltf.getAudioSources(), converter), result);
                        }
                        if (extension.contains("emitters")) {
                            AudioEmitterFromJson converter(m_gltf, m_pointerLookup);
                            result = core::Max(resolve(extension["emitters"], m_gltf.getAudioEmitters(), converter), result);
                        }
                    }
                }
            }
        }
    }

    return result;
}

void GltfFromJson::convertRootExtensions()
{
    if (!m_json.contains("extensionsUsed")) {
        return;
    }

    if (m_json["extensionsUsed"].is_array()) {
        for (const nlohmann::json& jsonObject : m_json["extensionsUsed"]) {
            if (!jsonObject.is_string()) {
                continue;
            }
            core::string extensionName = jsonObject.get<core::string>();

            if (GltfObjectFromJson::g_importOptions.has_value() && GltfObjectFromJson::g_importOptions->skipExtensions.has_value()) {
                if (GltfObjectFromJson::g_importOptions->skipExtensions.value().count(extensionName) != 0) {
                    continue;
                }
            }
            m_gltf.addDeclaredExtension(extensionName);
            Extension extension = stringToExtension(extensionName);

            if (extension == Extension::MeshQuantization) {
                m_gltf.addRootExtension(Extension::MeshQuantization);
            } else if (extension == Extension::MaterialsVariants) {
                m_gltf.addRootExtension(Extension::MaterialsVariants);
            }
        }
    }
}

void GltfFromJson::convertRequiredExtensions()
{
    if (!m_json.contains("extensionsRequired")) {
        return;
    }

    if (m_json["extensionsRequired"].is_array()) {
        for (const nlohmann::json& jsonObject : m_json["extensionsRequired"]) {
            if (!jsonObject.is_string()) {
                continue;
            }

            core::string extName = jsonObject.get<core::string>();

            if (extensionStringKnown(extName)) {
                m_gltf.addRequiredKnownExtension(stringToExtension(extName));
            } else {
                m_gltf.addRequiredUnknownExtension(extName);
            }
        }
    }
}

void GltfFromJson::convertAsset()
{
    if (!m_json.contains("asset")) {
        core::logWarning("GltfFromJson: Json file has no asset");
        return;
    }

    nlohmann::json jsonAsset = m_json["asset"];

    Asset& asset = m_gltf.getAsset();

    for (const auto& [key, value] : jsonAsset.items()) {
        if (key == "version") {
            if (!value.is_string()) {
                core::logWarning("GltfFromJson: Version is not of type string");
                continue;
            }
            auto version = value.get<std::string>();
            if (version != "2.0") {
                core::logWarning("Unsupported glTF version %s", version.c_str());
            }
            asset.setVersion(version);
        }
        if (key == "minVersion") {
            if (!value.is_string()) {
                core::logWarning("GltfFromJson: minVersion is not of type string");
                continue;
            }
            asset.setMinVersion(value.get<std::string>());
        }
        if (key == "generator") {
            if (!value.is_string()) {
                core::logWarning("GltfFromJson: Generator is not of type string");
                continue;
            }
            asset.setGenerator(value.get<std::string>());
        }
        if (key == "copyright") {
            if (!value.is_string()) {
                core::logWarning("GltfFromJson: Copyright is not of type string");
                continue;
            }
            asset.setCopyright(value.get<std::string>());
        }
    }
    GltfObjectFromJson::convertUnknownExtensions(jsonAsset, &asset, "asset");
    GltfObjectFromJson::convertExtras(jsonAsset, &asset);
}

ConversionResult GltfFromJson::create(const nlohmann::json::array_t& _jsonObjects, ChildOfRootFromJson& _converter)
{
    ConversionResult result = ConversionResult::Success;

    for (const auto& jsonObject : _jsonObjects) {
        result = core::Max(_converter.convert(jsonObject), result);
    }

    return result;
}

template <typename GltfObject, typename Converter>
ConversionResult GltfFromJson::resolve(const nlohmann::json& _jsonObjects, core::LinkedList<GltfObject>& _gltfObjects, Converter& _converter)
{
    ConversionResult result;

    auto jsonIt = _jsonObjects.begin();
    auto jsonEnd = _jsonObjects.end();
    auto gltfIt = _gltfObjects.begin();
    auto gltfEnd = _gltfObjects.end();

    for (; jsonIt != jsonEnd && gltfIt != gltfEnd; jsonIt++, gltfIt++) {
        result = core::Max(_converter.resolve(*jsonIt, *gltfIt), result);
    }

    return result;
}

ConversionResult GltfFromJson::resolveDefaultScene(const nlohmann::json& _jsonScene)
{
    int32_t index = _jsonScene.get<int32_t>();
    Scene* pScene = core::safeAt(m_pointerLookup.Scenes, index);

    if (pScene == nullptr) {
        core::logWarning("(default) scene index %d is invalid (scenes has %u elements)",
            index, m_pointerLookup.Scenes.size());
        return ConversionError::SuccessWithErrors;
    }

    m_gltf.setDefaultScene(*pScene);
    return ConversionResult::Success;
}

ConversionResult GltfFromJson::resolveMaterialVariants(const nlohmann::json& _jsonMaterialVariants)
{
    bool containsErrors = false;
    core::Vector<MaterialVariant> matVarsVec;
    for (const auto& [jsonKey, jsonValue] : _jsonMaterialVariants.items()) {
        if (jsonKey != "variants") {
            continue;
        }
        if (jsonValue.is_array() == false) {
            core::logWarning("KHR_materials_variants.%s with value '%s' has invalid type", jsonKey.c_str(), jsonValue.dump().c_str());
            containsErrors = true;
            continue;
        }

        for (auto jsonProperty : jsonValue) {
            if (jsonProperty.is_object() == false) {
                core::logWarning("KHR_materials_variants.variant with value '%s' has invalid type", jsonProperty.dump().c_str());
                containsErrors = true;
                continue;
            }

            for (const auto& [variantKey, variantValue] : jsonProperty.items()) {
                if (variantKey != "name") {
                    continue;
                }
                if (variantValue.is_string() == false) {
                    core::logWarning("KHR_materials_variants.variant.%s with value '%s' has invalid type", variantKey.c_str(), variantValue.dump().c_str());
                    containsErrors = true;
                    continue;
                }

                MaterialVariant* matVar = m_gltf.create<MaterialVariant>();
                if (m_pointerLookup.add(matVar) == false) {
                    return ConversionError::AllocationFailure;
                }
                matVar->setName(variantValue.get<std::string>());
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult GltfFromJson::resolveAsset(const nlohmann::json& _jsonAsset)
{
    bool containsErrors = false;
    Asset& asset = m_gltf.getAsset();

    for (const auto& [key, value] : _jsonAsset.items()) {
        if (key == "extensions") {
            if (!value.is_object()) {
                core::logWarning("GltfFromJson: Asset.extensions is not of type object");
                containsErrors = true;
                continue;
            }

            for (const auto& [extKey, extValue] : value.items()) {
                if (extKey == "KHR_xmp_json_ld" && extValue.is_object()) {
                    for (const auto& [xmpKey, xmpValue] : extValue.items()) {
                        if (xmpKey == "packet") {
                            if (m_gltf.getXMP().has_value() == false) {
                                core::logWarning("GltfFromJson: Asset.XMP is defined, but the root extension KHR_xmp_json_ld is missing");
                                containsErrors = true;
                                continue;
                            }

                            XMP& xmp = m_gltf.getXMP().value();
                            core::LinkedList<Packet>& packets = xmp.getPackets();
                            const size_t index = xmpValue.get<size_t>();

                            Packet* pPacket = &packets.at(index);
                            asset.setXMPPacket(pPacket);
                        }
                    }
                    if (asset.hasXMPPacket()) {
                        if (!GltfObjectFromJson::convertUnknownExtensions(extValue, &asset.getXMPPacket().value(), "scenes.extensions.KHR_xmp_json_ld")) {
                            containsErrors = true;
                        }
                        GltfObjectFromJson::convertExtras(extValue, &asset.getXMPPacket().value());
                    }
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
