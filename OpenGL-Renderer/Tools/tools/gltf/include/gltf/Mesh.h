// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "HelperFilled.h"
#include "HelperVisible.h"
#include "XMP.h"
#include "core/LinkedList.h"
#include "math/Math.h"
#include "mesh/Primitive.h"

#include <vec3.hpp>

#include <optional>
#include <vector>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#mesh

namespace ux3d::gltf {

struct PositionBounds {
    glm::vec3 min;
    glm::vec3 max;
};

class Mesh : public ChildOfRoot, public HelperVisible, public HelperFilled {
private:
    core::LinkedList<Primitive> m_Primitives;

    // extensions
    std::optional<PacketContainer> m_xmpPacket = std::nullopt;

public:
    Mesh(uint32_t _id); // regular constructor
    Mesh(uint32_t _id, const Mesh& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Mesh; }

    core::LinkedList<Primitive>& getPrimitives() { return m_Primitives; }
    const core::LinkedList<Primitive>& getPrimitives() const { return m_Primitives; }

    std::optional<std::vector<double>> weights;

    bool hasXMPPacket() const { return m_xmpPacket.has_value(); }
    std::optional<PacketContainer>& getXMPPacket() { return m_xmpPacket; }
    const std::optional<PacketContainer>& getXMPPacket() const { return m_xmpPacket; }
    GltfResult setXMPPacket(Packet* _xmpPacket);
    GltfResult unsetXMPPacket();

    Primitive* createPrimitive() { return m_Primitives.emplace_back(*this); }

    GltfResult deletePrimitive(Primitive& _Primitive);

    Extensions getUsedExtensions() const override;

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;

    /// Computes the position bounds over all primitives combined.
    std::optional<PositionBounds> getPositionBounds() const;

    /**
     * @brief Returns the index of the provided primitive in the primitives array. Returns -1 if not found
     */
    int getPrimitiveIndex(const Primitive& _Primitive) const;
};
}
