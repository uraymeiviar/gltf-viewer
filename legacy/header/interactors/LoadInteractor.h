#pragma once

#include "enums/InteractorResult.h"

#include <list>
#include <optional>
#include <string>
#include <unordered_set>

namespace ux3d::gltf {
class Image;
}

namespace ux3d::editor {
struct GltfOpenUseCase;
struct GltfCreate;
class LoadInteractorOutput;

class LoadInteractor {
public:
    static LoadInteractor* getInstance();

    ~LoadInteractor() = default;
    LoadInteractor(LoadInteractor const&) = delete;
    void operator=(LoadInteractor const&) = delete;

    void registerOutput(LoadInteractorOutput* _NewOutput) { m_Outputs.push_back(_NewOutput); }
    void unregisterOutput(LoadInteractorOutput* _RemoveOutput) { m_Outputs.remove(_RemoveOutput); }

    InteractorResult execute(const GltfOpenUseCase& _UseCase) const;
    InteractorResult reloadGltf() const;
    InteractorResult execute(GltfCreate& _UseCase) const;

private:
    LoadInteractor() = default;
    std::list<LoadInteractorOutput*> m_Outputs;
};
}
