// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/LinkedList.h"
#include "core/Vector.h"

namespace ux3d::gltf {
class ChildOfRoot;
class GltfObject;
class Gltf;

class Accessor;

class ReferenceCollector {
public:
    ReferenceCollector(const Gltf& _gltf)
        : m_gltf(_gltf) {};

    void collectReferences(Accessor& _accessor);
    core::Vector<const GltfObject*> getCollectedReferences();
    void resetReferences();

private:
    void addToGltfObjectList(const GltfObject* _element);

    core::Vector<const GltfObject*> m_gltfObjectList;
    const Gltf& m_gltf;
};
}
