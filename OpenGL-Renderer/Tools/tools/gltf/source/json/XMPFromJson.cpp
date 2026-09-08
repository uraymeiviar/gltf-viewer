#include "gltf/json/XMPFromJson.h"
#include "gltf/Gltf.h"

using namespace ux3d;
using namespace ux3d::gltf;

XMPFromJson::XMPFromJson(Gltf& gltf)
    : m_gltf(gltf)
{
}

ConversionResult XMPFromJson::convert(const nlohmann::json& jsonXmp)
{
    XMP& xmp = m_gltf.createXMP();

    bool containsErrors = false;
    for (const auto& [key, value] : jsonXmp.items()) {
        if (key == "packets") {
            if (!value.is_array()) {
                core::logWarning("XMP packets must be an array");
                containsErrors = true;
                break;
            }
            for (const auto& jsonPacket : value) {
                containsErrors |= convertPacket(xmp, jsonPacket);
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(jsonXmp, &xmp, "XMP")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(jsonXmp, &xmp);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

bool XMPFromJson::convertPacket(XMP& xmp, const nlohmann::json& jsonPacket)
{
    bool containsErrors = false;
    Packet* pPacket = xmp.addEmptyPacket();
    bool hasContextEntry = false;

    for (const auto& [key, value] : jsonPacket.items()) {
        if (key == "@context") {
            hasContextEntry = true;
        }
        containsErrors |= convertEntry(pPacket->addEntry(key), value, false);
    }

    if (!hasContextEntry) {
        core::logWarning("XMP packet is missing \"@context\" property");
        containsErrors = true;
    }

    return containsErrors;
}

bool XMPFromJson::convertEntry(Entry* entry, const nlohmann::json& jsonEntry, bool isArray)
{
    bool containsErrors = false;

    if (jsonEntry.is_string()) {
        entry->addValue(jsonEntry.get<std::string>(), isArray);
    } else if (jsonEntry.is_object()) {
        for (const auto& [key, value] : jsonEntry.items()) {
            containsErrors |= convertEntry(entry->addEntry(key), value, false);
        }
    } else if (jsonEntry.is_array()) {
        for (const auto& item : jsonEntry) {
            containsErrors |= convertEntry(entry, item, true);
        }
    } else {
        return true;
    }

    return containsErrors;
}
