#pragma once

namespace ux3d::core {
template <typename T>
class LinkedList;
template <typename T>
class Vector;
}

namespace ux3d::gltf {
class Gltf;
class ChildOfRoot;
}

namespace ux3d::editor {
class GltfCleanup {
public:
    /// all scenes are considered 'used' by default so whatever
    /// is completely disconnected from all scenes will be removed
    /// returns true if anything was deleted
    static bool deleteAllUnused(gltf::Gltf& _gltf);

private:
    template <typename T>
    static void collectUnused(
        const core::Vector<const gltf::ChildOfRoot*>& _usedObjects,
        const core::LinkedList<T>& _candidates,
        core::Vector<gltf::ChildOfRoot*>& _outUnusedObjects);
};
}
