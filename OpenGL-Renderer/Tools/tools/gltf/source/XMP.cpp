// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/XMP.h"

using namespace ux3d;
using namespace ux3d::gltf;

Entry* Entry::addEntry(core::string _name)
{
    if (m_entries.hasType<core::LinkedList<core::string>>() == false) {
        if (m_entries.hasType<std::monostate>()) {
            m_entries = core::LinkedList<Entry>();
        }

        return m_entries.unsafeGet<core::LinkedList<Entry>>().emplace_back(_name);
    } else {
        core::logWarning("XMP: tried to add an entry to a value typed list.");
        return nullptr;
    }
}

void Entry::addValue(core::string _value, bool isArray)
{
    if (m_entries.hasType<core::LinkedList<Entry>>() == false) {
        if (m_entries.hasType<std::monostate>()) {
            m_entries = core::LinkedList<core::string>();
        }

        m_entries.unsafeGet<core::LinkedList<core::string>>().emplace_back(_value);
        if (m_entries.unsafeGet<core::LinkedList<core::string>>().size() > 1) {
            m_isArray = true;
        } else {
            m_isArray = isArray;
        }
    } else {
        core::logWarning("XMP: tried to add a value to an entry typed list.");
    }
}

Entry* Packet::addEntry(core::string _name)
{
    return m_entries.emplace_back(_name);
}

Packet* XMP::addEmptyPacket()
{
    return m_packets.emplace_back();
}

Packet* XMP::addPacket(const Packet& _packet)
{
    return m_packets.emplace_back(_packet);
}
bool XMP::deletePacket(const Packet& _packet)
{
    return core::remove(m_packets, _packet);
}
