// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/SkinMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

bool ux3d::gltf::SkinMerger::resolveDependencies(const Skin& _SourceSkin, Skin& _Skin)
{
    resolveInverseBindMatricesAccessor(_SourceSkin, _Skin);
    resolveSkeletonRoot(_SourceSkin, _Skin);
    return resolveJoints(_SourceSkin, _Skin);
}

void ux3d::gltf::SkinMerger::resolveInverseBindMatricesAccessor(const Skin& _SourceSkin, Skin& _Skin)
{
    const Accessor* pSourceAccessor = _SourceSkin.getInverseBindMatricesAccessor();
    if (pSourceAccessor != nullptr) {
        const size_t accessorIndex = m_Indices[pSourceAccessor];
        Accessor& accessor = m_Destination.getAccessors().at(accessorIndex);
        _Skin.setInverseBindMatricesAccessor(accessor);
    }
}

void ux3d::gltf::SkinMerger::resolveSkeletonRoot(const Skin& _SourceSkin, Skin& _Skin)
{
    const Node* pSourceNode = _SourceSkin.getSkeletonRoot();
    if (pSourceNode != nullptr) {
        const size_t nodeIndex = m_Indices[pSourceNode];
        Node& node = m_Destination.getNodes().at(nodeIndex);
        _Skin.setSkeletonRoot(node);
    }
}

bool ux3d::gltf::SkinMerger::resolveJoints(const Skin& _SourceSkin, Skin& _Skin)
{
    _Skin.getJoints().clear();
    for (const Node* pSourceNode : _SourceSkin.getJoints()) {
        const size_t nodeIndex = m_Indices[pSourceNode];
        Node& node = m_Destination.getNodes().at(nodeIndex);
        if (!_Skin.addJoint(node)) {
            return false;
        }
    }
    return true;
}
