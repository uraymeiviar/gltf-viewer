// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfObjectToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class XMP;

class XMPToJson : GltfObjectToJson {
public:
    using GltfObjectToJson::GltfObjectToJson;
    void convert(const XMP& _xmp, nlohmann::json& _jsonXMP) const;

private:
    void convertContext(const XMP& _xmp, nlohmann::json& _jsonXMP) const;
    void convertPackets(const XMP& _xmp, nlohmann::json& _jsonXMP) const;
};
}
