#include "gltf/json/GltfToJson.h"

#include "gltf/ChildCollector.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/Gltf.h"
#include "gltf/MaterialVariant.h"
#include "gltf/ResourceCollector.h"
#include "gltf/json/GltfObjectToJson.h"
#include "gltf/json/JsonPath.h"
#include "nlohmann/json.hpp"

#include "gltf/json/AccessorsToJson.h"
#include "gltf/json/AnimationsToJson.h"
#include "gltf/json/AudioEmitterToJson.h"
#include "gltf/json/AudioSourceToJson.h"
#include "gltf/json/BufferViewsToJson.h"
#include "gltf/json/BuffersToJson.h"
#include "gltf/json/CamerasToJson.h"
#include "gltf/json/ImagesToJson.h"
#include "gltf/json/MaterialsToJson.h"
#include "gltf/json/MeshesToJson.h"
#include "gltf/json/NodesToJson.h"
#include "gltf/json/SamplersToJson.h"
#include "gltf/json/ScenesToJson.h"
#include "gltf/json/SkinsToJson.h"
#include "gltf/json/TexturesToJson.h"
#include "gltf/json/XMPToJson.h"

// extensions
#include "gltf/json/LightsToJson.h"

#include "core/Platform.h"

#include <ctime>
#include <vector>

using namespace ux3d;
using namespace ux3d::gltf;

const nlohmann::json& GltfToJson::getJson()
{
    return m_json;
}

ConversionResult GltfToJson::convert()
{
    if (checkValidity(m_gltf) == ConversionError::InvalidGltf) {
        core::logWarning("GltfToJson: Converting invalid glTF");
    }

    convertAssetMetaData();
    convertDefaultScene();

    convertScenes();
    convertNodes();
    convertCameras();
    convertMeshes();
    convertMaterials();
    convertTextures();
    convertImages();
    convertSamplers();
    convertAccessors();
    convertBufferViews();
    convertBuffers();
    convertSkins();
    convertAnimations();

    // extensions
    convertUsedExtensions();
    convertLights();
    convertMaterialVariants();
    convertXMP();
    convertAudio();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(m_gltf, m_json);
    gltfObjectToJson.convertUnknownExtensions(m_gltf, m_json);

    return ConversionResult::Success;
}

void GltfToJson::convertAssetMetaData()
{
    nlohmann::json jsonAsset;

    const Asset& asset = m_gltf.getAsset();

    if (m_exportOptions.generatorNameOverride.has_value()) {
        jsonAsset["generator"] = m_exportOptions.generatorNameOverride.value();
    } else if (asset.hasGenerator()) {
        jsonAsset["generator"] = asset.getGeneratorOrDefault();
    }

    if (m_exportOptions.versionOverride.has_value()) {
        jsonAsset["version"] = m_exportOptions.versionOverride.value();
    } else if (asset.hasVersion()) {
        jsonAsset["version"] = asset.getVersionOrDefault();
    }

    if (asset.hasCopyright()) {
        jsonAsset["copyright"] = asset.getCopyrightOrDefault();
    }

    if (asset.hasMinVersion()) {
        jsonAsset["minVersion"] = asset.getMinVersionOrDefault();
    }

    if (asset.hasXMPPacket()) {
        const PacketContainer& container = asset.getXMPPacket().value();
        Packet* pPacket = container.m_packet;
        if (m_gltf.getXMP().has_value()) {
            const XMP& xmp = m_gltf.getXMP().value();
            const core::LinkedList<Packet>& packets = xmp.getPackets();

            for (size_t i = 0; i < packets.size(); ++i) {
                if (&packets.at(i) == pPacket) {
                    jsonAsset["extensions"]["KHR_xmp_json_ld"]["packet"] = i;
                    break;
                }
            }
        }

        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(container, jsonAsset["extensions"]["KHR_xmp_json_ld"]);
        gltfObjectToJson.convertUnknownExtensions(container, jsonAsset["extensions"]["KHR_xmp_json_ld"]);
    }

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(asset, jsonAsset);
    gltfObjectToJson.convertUnknownExtensions(asset, jsonAsset);

    if (m_exportOptions.writeTimeStamp) {
        std::tm date {};
        std::time_t timer = std::time(0);
#ifdef UX3D_IS_POSIX
        localtime_r(&timer, &date);
#endif
#ifdef UX3D_IS_WINDOWS
        localtime_s(&date, &timer);
#endif
        char mbstr[100];
        if (std::strftime(mbstr, sizeof(mbstr), "%c", &date)) {
            jsonAsset["extras"]["created"] = mbstr;
        }
    }

    m_json["asset"] = jsonAsset;
}

void GltfToJson::unknownExtensionsToNames(
    const std::unordered_map<core::string, core::string>& _inUnknownExtensions,
    core::Vector<core::string>& _outNames)
{
    _outNames.clear();

    core::Vector<std::pair<core::string, core::string>> workList;

    for (const auto& pair : _inUnknownExtensions) {
        workList.emplace_back(pair);
    }

    for (size_t i = 0; i < workList.size(); ++i) {
        const auto& workElement = workList[i];

        _outNames.push_back(workElement.first);

        nlohmann::json jsonExtension = nlohmann::json::parse(workElement.second, nullptr, false);
        if (jsonExtension.is_object()) {
            const auto extensionsIt = jsonExtension.find("extensions");
            if (extensionsIt != jsonExtension.end()) {
                if (extensionsIt->is_object()) {
                    for (const auto& [extensionName, extension] : extensionsIt->items()) {
                        if (extension.is_object()) {
                            core::string extensionString = extension.dump();
                            workList.emplace_back(extensionName, extensionString);
                        }
                    }
                }
            }
        }
    }
}

void GltfToJson::convertUsedExtensions()
{
    Extensions usedExtensions = m_gltf.getUsedExtensions();

    for (Extension extension : getAllExtensions()) {
        if (core::hasFlag(usedExtensions, extension)) {
            m_json["extensionsUsed"].emplace_back(extensionToString(extension));

            if (isExtensionRequired(extension)) {
                m_json["extensionsRequired"].emplace_back(extensionToString(extension));
            }
        }
    }

    core::Vector<const GltfObject*> gltfObjectList;

    if (m_exportOptions.exportCandidates.empty()) {
        ChildCollector childCollector(m_gltf);
        childCollector.collectGltfObjects(gltfObjectList);
    } else {
        ChildCollector childCollector(m_exportOptions.exportCandidates);
        childCollector.collectGltfObjects(gltfObjectList);
    }

    std::unordered_map<core::string, core::string> unknownExtensions;
    for (const GltfObject* gltfObject : gltfObjectList) {
        const std::unordered_map<core::string, core::string>& additionalUnknownExtensions = gltfObject->getUnknownExtensions();

        for (const auto& element : additionalUnknownExtensions) {
            unknownExtensions.insert(element);
        }
    }

    core::Vector<core::string> unknownExtensionNames;
    unknownExtensionsToNames(unknownExtensions, unknownExtensionNames);

    std::sort(unknownExtensionNames.begin(), unknownExtensionNames.end());

    // remove duplicates
    // see https://en.cppreference.com/w/cpp/algorithm/unique
    auto last = std::unique(unknownExtensionNames.begin(), unknownExtensionNames.end());
    unknownExtensionNames.erase(last, unknownExtensionNames.end());

    for (const core::string& unknownExtensionName : unknownExtensionNames) {
        if (unknownExtensionName.empty() == false) {
            // avoid writing gestaltor specific extensions in glb/gltf files
            if (m_exportOptions.conversionUsage == ConversionUsage::ValidGltf && unknownExtensionName == "ux3d_gestaltor") {
                continue;
            }

            m_json["extensionsUsed"].emplace_back(unknownExtensionName);
        }
    }

    for (auto& requiredExtension : m_gltf.getRequiredUnknownExtensions()) {
        if (std::find(m_json["extensionsUsed"].begin(), m_json["extensionsUsed"].end(), requiredExtension) != m_json["extensionsUsed"].end()) {
            m_json["extensionsRequired"].emplace_back(requiredExtension);
        }
    }

    if (m_json.contains("extensionsUsed")) {
        std::sort(m_json["extensionsUsed"].begin(), m_json["extensionsUsed"].end());
    }
    if (m_json.contains("extensionsRequired")) {
        std::sort(m_json["extensionsRequired"].begin(), m_json["extensionsRequired"].end());
    }
}

void GltfToJson::convertDefaultScene()
{
    if (m_gltf.getDefaultScene() == nullptr) {
        return;
    }
    if (isExportElement(m_gltf.getDefaultScene())) {
        size_t index = m_indices[m_gltf.getDefaultScene()];
        m_json["scene"] = index;
    }
}

template <typename GltfType, typename Converter>
void GltfToJson::convertElements(
    const core::LinkedList<GltfType>& _elements,
    Converter& _converter,
    const core::string& _listName,
    const core::string& _extensionName)
{
    if (_elements.empty()) {
        return;
    }

    nlohmann::json jsonElements = nlohmann::json::array();
    for (const GltfType& element : _elements) {
        const ChildOfRoot* candidateElement = &element;
        if (!isExportElement(candidateElement)) {
            continue;
        }

        JsonPath jsonPath {}; // specifies where jsonElement is written in global json structure
        if (!_extensionName.empty()) {
            jsonPath.emplace_back("extension");
            jsonPath.emplace_back(_extensionName);
        }

        jsonPath.emplace_back(_listName);
        jsonPath.emplace_back(static_cast<uint32_t>(jsonElements.size()));

        nlohmann::json jsonElement = nlohmann::json::object();
        _converter.convert(element, jsonElement, jsonPath);

        jsonElements.emplace_back(jsonElement);
    }

    if (_extensionName.empty()) {
        m_json[_listName] = jsonElements;
    } else {
        m_json["extensions"][_extensionName][_listName] = jsonElements;
    }
}

void GltfToJson::convertScenes()
{
    ScenesToJson converter(m_indices, m_exportOptions, m_gltf);
    convertElements(m_gltf.getScenes(), converter, "scenes");
}

void GltfToJson::convertNodes()
{
    NodesToJson converter(m_indices, m_exportOptions, m_gltf);
    convertElements(m_gltf.getNodes(), converter, "nodes");
}

void GltfToJson::convertCameras()
{
    CamerasToJson converter(m_indices, m_exportOptions);
    convertElements(m_gltf.getCameras(), converter, "cameras");
}

void GltfToJson::convertMeshes()
{
    MeshesToJson converter(m_indices, m_resourceCollector, m_exportOptions, m_gltf);
    convertElements(m_gltf.getMeshes(), converter, "meshes");
}

void GltfToJson::convertMaterials()
{
    MaterialsToJson converter(m_indices, m_exportOptions, m_gltf);
    convertElements(m_gltf.getMaterials(), converter, "materials");
}

void GltfToJson::convertTextures()
{
    TexturesToJson converter(m_indices, m_exportOptions);
    convertElements(m_gltf.getTextures(), converter, "textures");
}

void GltfToJson::convertImages()
{
    ImagesToJson converter(m_indices, m_resourceCollector, m_exportOptions, m_gltf);
    convertElements(m_gltf.getImages(), converter, "images");
}

void GltfToJson::convertSamplers()
{
    SamplersToJson converter(m_indices, m_exportOptions);
    convertElements(m_gltf.getSamplers(), converter, "samplers");
}

void GltfToJson::convertAccessors()
{
    AccessorsToJson converter(m_indices, m_resourceCollector, m_exportOptions);
    convertElements(m_gltf.getAccessors(), converter, "accessors");
}

void GltfToJson::convertBufferViews()
{
    BufferViewsToJson converter(m_indices, m_exportOptions);
    convertElements(m_gltf.getBufferViews(), converter, "bufferViews");
}

void GltfToJson::convertBuffers()
{
    BuffersToJson converter(m_indices, m_exportOptions);
    convertElements(m_gltf.getBuffers(), converter, "buffers");
}

void GltfToJson::convertSkins()
{
    SkinsToJson converter(m_indices, m_exportOptions);
    convertElements(m_gltf.getSkins(), converter, "skins");
}

void GltfToJson::convertAnimations()
{
    AnimationsToJson converter(m_indices, m_exportOptions, m_gltf);
    convertElements(m_gltf.getAnimations(), converter, "animations");
}

// extensions

void GltfToJson::convertLights()
{
    if (m_gltf.getLightsPunctual().has_value()) {
        nlohmann::json jsonLightsPunctual = nlohmann::json::object();
        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(m_gltf.getLightsPunctual().value(), jsonLightsPunctual);
        gltfObjectToJson.convertUnknownExtensions(m_gltf.getLightsPunctual().value(), jsonLightsPunctual);
        m_json["extensions"]["KHR_lights_punctual"] = jsonLightsPunctual;
    }

    LightsToJson converter(m_indices, m_exportOptions);
    convertElements(m_gltf.getLights(), converter, "lights", "KHR_lights_punctual");
}

void GltfToJson::convertMaterialVariants()
{
    if (m_gltf.hasMaterialVariants() && !m_gltf.getMaterialVariants().empty()) {
        nlohmann::json jsonMaterialVariants = nlohmann::json::object();
        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(m_gltf.getMaterialVariantsOptional().value(), jsonMaterialVariants);
        gltfObjectToJson.convertUnknownExtensions(m_gltf.getMaterialVariantsOptional().value(), jsonMaterialVariants);
        nlohmann::json jsonVariants = nlohmann::json::array();
        for (const gltf::MaterialVariant& variant : m_gltf.getMaterialVariants()) {
            nlohmann::json jsonVariant = nlohmann::json::object();
            jsonVariant["name"] = variant.getName();

            jsonVariants.push_back(jsonVariant);
        }

        jsonMaterialVariants["variants"] = jsonVariants;
        m_json["extensions"]["KHR_materials_variants"] = jsonMaterialVariants;
    }
}

void GltfToJson::convertXMP()
{
    if (m_gltf.hasXMP()) {
        XMPToJson converter(m_exportOptions);
        nlohmann::json jsonXMP = nlohmann::json::object();
        converter.convert(m_gltf.getXMP().value(), jsonXMP);
        m_json["extensions"]["KHR_xmp_json_ld"] = jsonXMP;
    }
}

void GltfToJson::convertAudio()
{
    if (m_gltf.hasAudio()) {
        nlohmann::json jsonAudio = nlohmann::json::object();

        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(m_gltf.getAudio().value(), jsonAudio);
        gltfObjectToJson.convertUnknownExtensions(m_gltf.getAudio().value(), jsonAudio);

        nlohmann::json jsonAudioSources = nlohmann::json::array();
        for (const gltf::AudioSource& source : m_gltf.getAudioSources()) {
            nlohmann::json jsonSource = nlohmann::json::object();
            AudioSourceToJson converter(m_indices, m_exportOptions, m_resourceCollector);
            converter.convert(source, jsonSource);
            jsonAudioSources.push_back(jsonSource);
        }
        m_json["extensions"]["KHR_audio"]["sources"] = jsonAudioSources;

        nlohmann::json jsonAudioEmitters = nlohmann::json::array();
        for (const gltf::AudioEmitter& emitter : m_gltf.getAudioEmitters()) {
            nlohmann::json jsonEmitter = nlohmann::json::object();
            AudioEmitterToJson converter(m_indices, m_exportOptions);
            converter.convert(emitter, jsonEmitter);
            jsonAudioEmitters.push_back(jsonEmitter);
        }
        m_json["extensions"]["KHR_audio"]["emitters"] = jsonAudioEmitters;
    }
}

template <typename GltfType>
ConversionResult GltfToJson::checkValidity(const core::LinkedList<GltfType>& _elements) const
{
    if (_elements.empty()) {
        return ConversionResult::Success;
    }

    for (const GltfType& element : _elements) {
        if (!element.isValid()) {
            return ConversionError::InvalidGltf;
        }
    }

    return ConversionResult::Success;
}

bool GltfToJson::isExportElement(const ChildOfRoot* _element)
{
    if (m_exportOptions.exportCandidates.empty()) {
        return true;
    }
    return core::contains(m_exportOptions.exportCandidates, _element);
}

ConversionResult GltfToJson::checkValidity(const Gltf& _gltf) const
{
    ConversionResult result;

    result = core::Max(result, checkValidity(_gltf.getScenes()));
    result = core::Max(result, checkValidity(_gltf.getNodes()));
    result = core::Max(result, checkValidity(_gltf.getCameras()));
    result = core::Max(result, checkValidity(_gltf.getMeshes()));
    result = core::Max(result, checkValidity(_gltf.getMaterials()));
    result = core::Max(result, checkValidity(_gltf.getTextures()));
    result = core::Max(result, checkValidity(_gltf.getAccessors()));
    result = core::Max(result, checkValidity(_gltf.getBufferViews()));
    result = core::Max(result, checkValidity(_gltf.getBuffers()));
    result = core::Max(result, checkValidity(_gltf.getSkins()));
    result = core::Max(result, checkValidity(_gltf.getAnimations()));
    result = core::Max(result, checkValidity(_gltf.getLights()));

    return result;
}
