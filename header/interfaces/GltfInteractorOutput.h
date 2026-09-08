#pragma once

namespace ux3d::editor {
struct GltfSceneOpenUseCase;
struct GltfImportObjUseCase;

class GltfInteractorOutput {
public:
    virtual ~GltfInteractorOutput() { }
    virtual void presentSceneOpen(const GltfSceneOpenUseCase&) { }
};
}
