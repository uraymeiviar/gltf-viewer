#pragma once

namespace ux3d::gltf {
class Scene;
}

namespace ux3d::editor {

struct GltfSceneOpenUseCase {
    gltf::Scene* pScene = nullptr;
};

}
