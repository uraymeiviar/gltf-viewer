#pragma once

#include <string>

namespace ux3d::gltf {
class Gltf;
}

namespace ux3d::editor {
class LoadInteractorOutput {
public:
    virtual ~LoadInteractorOutput() = default;
    virtual void presentGltfOpen(ux3d::gltf::Gltf*) { }
    virtual void presentGltfImport(ux3d::gltf::Gltf*) { }

    virtual void presentGltfReloaded(ux3d::gltf::Gltf*) { }
};
}
