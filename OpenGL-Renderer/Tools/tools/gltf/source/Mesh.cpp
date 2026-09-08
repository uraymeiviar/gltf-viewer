// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Mesh.h"
#include "gltf/Accessor.h"
#include "gltf/XMP.h"

#include <glm.hpp>

using namespace ux3d;
using namespace ux3d::gltf;

Mesh::Mesh(uint32_t _id)
    : ChildOfRoot(_id)
{
}

Mesh::Mesh(uint32_t _id, const Mesh& _source)
    : ChildOfRoot(_id, _source)
    , m_Primitives(_source.m_Primitives)
    , m_xmpPacket(_source.m_xmpPacket)
    , weights(_source.weights)
{
}

GltfResult Mesh::deletePrimitive(Primitive& _Primitive)
{
    for (auto it = m_Primitives.begin(); it != m_Primitives.end(); ++it) {
        if (&*it == &_Primitive) {
            m_Primitives.erase(it);
            return GltfResult::Success;
        }
    }
    return GltfError::ObjectNotFound;
}

GltfResult Mesh::setXMPPacket(Packet* _xmpPacket)
{
    if (m_xmpPacket.has_value() == false) {
        m_xmpPacket.emplace(PacketContainer());
    }
    m_xmpPacket.value().m_packet = _xmpPacket;
    return GltfResult::Success;
}

GltfResult Mesh::unsetXMPPacket()
{
    m_xmpPacket.reset();
    return GltfResult::Success;
}

Extensions Mesh::getUsedExtensions() const
{
    Extensions usedExtensions = Extension::None;
    for (const Primitive& primitive : getPrimitives()) {
        if (primitive.getDracoMeshCompression().has_value()) {
            usedExtensions |= Extension::DracoMeshCompression;
        }
        if (primitive.getMaterialVariantsMapping().has_value()) {
            usedExtensions |= Extension::MaterialsVariants;
        }
    }
    return usedExtensions;
}

bool ux3d::gltf::Mesh::isValid() const
{
    return !m_Primitives.empty();
}

GltfResult Mesh::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    if (m_Primitives.empty()) {
        if (_InvalidProperties.push_back("primitives") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}

std::optional<PositionBounds> Mesh::getPositionBounds() const
{
    std::optional<PositionBounds> bounds;
    for (const auto& primitive : m_Primitives) {
        for (const auto& attribute : primitive.getAttributes()) {
            if (attribute.getName() != "POSITION") {
                continue;
            }
            const gltf::Accessor* accessor = attribute.getAccessor();
            if (accessor == nullptr || accessor->getMin().size() < 3 || accessor->getMax().size() < 3) {
                continue;
            }
            auto primitiveMin = glm::vec3(accessor->getMin()[0], accessor->getMin()[1], accessor->getMin()[2]);
            auto primitiveMax = glm::vec3(accessor->getMax()[0], accessor->getMax()[1], accessor->getMax()[2]);
            if (bounds) {
                bounds->min = glm::min(bounds->min, primitiveMin);
                bounds->max = glm::max(bounds->max, primitiveMax);
            } else {
                bounds.emplace();
                bounds->min = primitiveMin;
                bounds->max = primitiveMax;
            }
        }
    }
    return bounds;
}

int Mesh::getPrimitiveIndex(const Primitive& _Primitive) const
{
    uint32_t primIndexCounter = 0;
    for (auto it = m_Primitives.begin(); it != m_Primitives.end(); ++it) {
        if (&*it == &_Primitive) {
            break;
        }
        primIndexCounter++;
    }
    // return -1 if not found
    return primIndexCounter != m_Primitives.size() ? primIndexCounter : -1;
}
