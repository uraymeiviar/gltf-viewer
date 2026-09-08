// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#skin

namespace ux3d::gltf {
class Node;
class Accessor;

class Skin : public ChildOfRoot {
private:
    Accessor* m_pInverseBindMatricesAccessor = nullptr;
    Node* m_pSkeletonRoot = nullptr;
    core::Vector<Node*> m_Joints;

public:
    Skin(uint32_t _id); // regular constructor
    Skin(uint32_t _id, const Skin& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Skin; }

    Accessor* getInverseBindMatricesAccessor() { return m_pInverseBindMatricesAccessor; }
    const Accessor* getInverseBindMatricesAccessor() const { return m_pInverseBindMatricesAccessor; }

    Node* getSkeletonRoot() { return m_pSkeletonRoot; }
    const Node* getSkeletonRoot() const { return m_pSkeletonRoot; }

    core::Vector<Node*>& getJoints() { return m_Joints; }
    const core::Vector<Node*>& getJoints() const { return m_Joints; }

    GltfResult setInverseBindMatricesAccessor(Accessor& _Accessor);
    GltfResult setSkeletonRoot(Node& _Node);
    GltfResult addJoint(Node& _Joint);
    GltfResult removeJoint(Node& _Joint);

    void unsetInverseBindMatricesAccessor() { m_pInverseBindMatricesAccessor = nullptr; }
    void unsetSkeletonRoot() { m_pSkeletonRoot = nullptr; }

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;
};
}
