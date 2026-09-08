// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once
#include "GltfObject.h"
#include "GltfResult.h"
#include "XMP.h"
#include "core/String.h"
#include <optional>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#asset

namespace ux3d::gltf {

class Asset : public GltfObject {
private:
    std::optional<core::string> m_copyright;
    std::optional<core::string> m_generator;
    std::optional<core::string> m_minVersion;
    std::optional<core::string> m_version;

    // extensions
    std::optional<PacketContainer> m_xmpPacket;

public:
    Asset();

    bool hasCopyright() const { return m_copyright.has_value(); }
    const std::optional<core::string>& getCopyright() const { return m_copyright; }
    core::string getCopyrightOrDefault() const { return m_copyright.value_or(""); }
    GltfResult setCopyright(const core::string& _copyright);
    void unsetCopyright() { m_copyright.reset(); }

    bool hasGenerator() const { return m_generator.has_value(); }
    const std::optional<core::string>& getGenerator() const { return m_generator; }
    core::string getGeneratorOrDefault() const { return m_generator.value_or(""); }
    GltfResult setGenerator(const core::string& _generator);

    bool hasMinVersion() const { return m_minVersion.has_value(); }
    const std::optional<core::string>& getMinVersion() const { return m_minVersion; }
    core::string getMinVersionOrDefault() const { return m_minVersion.value_or("2.0"); }
    GltfResult setMinVersion(const core::string& _minVersion);

    bool hasVersion() const { return m_version.has_value(); }
    const std::optional<core::string>& getVersion() const { return m_version; }
    core::string getVersionOrDefault() const { return m_version.value_or("2.0"); }
    GltfResult setVersion(const core::string& _version);

    bool hasXMPPacket() const { return m_xmpPacket.has_value(); }
    std::optional<PacketContainer>& getXMPPacket() { return m_xmpPacket; }
    const std::optional<PacketContainer>& getXMPPacket() const { return m_xmpPacket; }
    GltfResult setXMPPacket(Packet* _xmpPacket);
    GltfResult unsetXMPPacket();

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const;
};
}
