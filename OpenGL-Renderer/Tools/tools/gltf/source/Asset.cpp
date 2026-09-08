// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "gltf/Asset.h"

using namespace ux3d;
using namespace ux3d::gltf;

Asset::Asset()
    : GltfObject()
{
}

GltfResult Asset::setCopyright(const core::string& _copyright)
{
    m_copyright.emplace(_copyright);
    return GltfResult::Success;
}

GltfResult Asset::setGenerator(const core::string& _generator)
{
    m_generator.emplace(_generator);
    return GltfResult::Success;
}

GltfResult Asset::setMinVersion(const core::string& _minVersion)
{
    m_minVersion.emplace(_minVersion);
    return GltfResult::Success;
}

GltfResult Asset::setVersion(const core::string& _version)
{
    m_version.emplace(_version);
    return GltfResult::Success;
}

GltfResult Asset::setXMPPacket(Packet* _xmpPacket)
{
    if (m_xmpPacket.has_value() == false) {
        m_xmpPacket.emplace(PacketContainer());
    }
    m_xmpPacket.value().m_packet = _xmpPacket;
    return GltfResult::Success;
}

GltfResult Asset::unsetXMPPacket()
{
    m_xmpPacket.reset();
    return GltfResult::Success;
}

bool Asset::isValid() const
{
    if (m_version.has_value()) {
        return true;
    }
    return false;
}

GltfResult Asset::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    if (!m_version.has_value()) {
        if (_invalidProperties.push_back("version") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}
