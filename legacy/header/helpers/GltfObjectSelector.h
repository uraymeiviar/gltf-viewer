#pragma once

#include "enums/GltfObjectSelectionScheme.h"

namespace ux3d::core {
template <typename T>
class Vector;
}

namespace ux3d::gltf {
class Gltf;
class ChildOfRoot;
}

namespace ux3d::editor {
class GltfObjectSelector {
public:
    GltfObjectSelector(const gltf::Gltf& _gltf, GltfObjectSelectionScheme _selection);
    void gather(core::Vector<const gltf::ChildOfRoot*>& _outCandidates) const;

private:
    const gltf::Gltf& m_gltf;
    const GltfObjectSelectionScheme m_selection;
};
}
