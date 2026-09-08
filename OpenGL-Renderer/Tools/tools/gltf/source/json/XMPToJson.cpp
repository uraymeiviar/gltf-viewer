// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/XMPToJson.h"
#include "gltf/XMP.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d;
using namespace ux3d::gltf;

void XMPToJson::convert(const XMP& xmp, nlohmann::json& json) const
{
    convertUnknownExtensions(xmp, json);
    convertExtras(xmp, json);

    std::function<void(nlohmann::json&, const gltf::Entry&)> convertEntry;
    convertEntry = [&convertEntry](nlohmann::json& json, const gltf::Entry& entry) {
        nlohmann::json value;
        if (entry.hasEntries()) {
            value = nlohmann::json::object();
            for (const auto& entry : entry.getEntries().unsafeGet<core::LinkedList<Entry>>()) {
                convertEntry(value, entry);
            }
        } else if (entry.hasValues()) {
            const auto& values = entry.getEntries().unsafeGet<core::LinkedList<std::string>>();
            if (values.size() == 1 && !entry.m_isArray) {
                value = values.front();
            } else {
                value = nlohmann::json::array();
                for (const auto& string : values) {
                    value.push_back(string);
                }
            }
        }
        json[entry.getName()] = value;
    };

    auto jsonPackets = nlohmann::json::array();
    for (const auto& packet : xmp.getPackets()) {
        auto jsonPacket = nlohmann::json::object();
        for (const auto& entry : packet.getEntries()) {
            convertEntry(jsonPacket, entry);
        }
        jsonPackets.push_back(jsonPacket);
    }
    json["packets"] = jsonPackets;
}
