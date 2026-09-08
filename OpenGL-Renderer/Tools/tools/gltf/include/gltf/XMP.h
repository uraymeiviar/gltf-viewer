#pragma once

#include "GltfObject.h"
#include "core/LinkedList.h"
#include "core/VariantWrapper.h"
#include "nlohmann/json.hpp"
#include <variant>

// DEV NOTES:
//  XMP is globally stored in gltf
//  asset, scenes, nodes, meshes, materials, images, animations can reference XMP packets

// TODO update link after extension has been approved
// https://github.com/weegeekps/glTF/tree/khr_xmp_ld_extension/extensions/2.0/Khronos/KHR_xmp_json_ld

namespace ux3d::gltf {
class Entry {
public:
    Entry(core::string _name)
        : m_name(_name) {};

    const core::string& getName() const { return m_name; }
    void setName(core::string _name) { m_name = _name; }
    const core::VariantWrapper<std::monostate,
        core::LinkedList<core::string>,
        core::LinkedList<Entry>>&
    getEntries() const { return m_entries; }
    Entry* addEntry(core::string _name);
    void addValue(core::string _value, bool isArray);
    bool hasEntries() const { return m_entries.hasType<core::LinkedList<Entry>>(); };
    bool hasValues() const { return m_entries.hasType<core::LinkedList<core::string>>(); };

    bool m_isArray = false;

private:
    core::string m_name;
    core::VariantWrapper<std::monostate, core::LinkedList<core::string>, core::LinkedList<Entry>> m_entries;
};

class Packet : public GltfObject {
public:
    const core::LinkedList<Entry>& getEntries() const { return m_entries; }
    Entry* addEntry(core::string _name);

private:
    core::LinkedList<Entry> m_entries;
};

class PacketContainer : public GltfObject {
public:
    Packet* m_packet;
};

class XMP : public GltfObject {
public:
    XMP() = default;

    core::LinkedList<Packet>& getPackets() { return m_packets; }
    const core::LinkedList<Packet>& getPackets() const { return m_packets; }
    Packet* addEmptyPacket();
    Packet* addPacket(const Packet& _packet);
    bool deletePacket(const Packet& _packet);

private:
    core::LinkedList<Packet> m_packets;
};
}
