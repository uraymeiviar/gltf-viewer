#pragma once

#include "GltfObjectFromJson.h"

namespace ux3d::gltf {
class Gltf;
class XMP;
class Entry;

class XMPFromJson : public GltfObjectFromJson {
protected:
    Gltf& m_gltf;

public:
    XMPFromJson(Gltf& gltf);

    ConversionResult convert(const nlohmann::json& jsonXmp);
    bool convertPacket(XMP& xmp, const nlohmann::json& jsonPacket);
    bool convertEntry(Entry* entry, const nlohmann::json& jsonEntry, bool isArray);
};
}
