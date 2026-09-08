#pragma once

#include "enums/InteractorResult.h"
#include "model/ProjectManager.h"

#include <list>

namespace ux3d::core {
template <typename T>
class LinkedList;
}

namespace ux3d::gltf {
class Scene;
}

namespace ux3d::editor {
struct GltfSceneOpenUseCase;
struct GltfGetStatistics;
struct GltfImportObjUseCase;
class GltfInteractorOutput;
struct SceneStatistics;

class GltfInteractor {
public:
    static GltfInteractor* getInstance();
    ~GltfInteractor() = default;
    GltfInteractor(GltfInteractor const&) = delete;
    void operator=(GltfInteractor const&) = delete;

    void registerOutput(GltfInteractorOutput* _NewOutput) { m_Outputs.push_back(_NewOutput); }
    void unregisterOutput(GltfInteractorOutput* _RemoveOutput) { m_Outputs.remove(_RemoveOutput); }

    gltf::Scene* getDefaultScene();
    int32_t getDefaultSceneIndex();
    // DO NOT USE THIS FOR MATERIAL VARIANTS
    InteractorResult execute(GltfSceneOpenUseCase& _useCase) const;

    template <typename TChildOfRoot>
    TChildOfRoot* getByID(uint32_t _id);

    template <typename TChildOfRoot>
    const TChildOfRoot* getByID(uint32_t _id) const;

private:
    GltfInteractor() = default;

    void gatherNodeStats(const gltf::Node* _node, SceneStatistics& _stats) const;

    std::list<GltfInteractorOutput*> m_Outputs;
};

template <typename TChildOfRoot>
TChildOfRoot* GltfInteractor::getByID(uint32_t _id)
{
    gltf::Gltf& gltf = model::ProjectManager::getInstance()->getGltf();
    return gltf.getByID<TChildOfRoot>(_id);
}

template <typename TChildOfRoot>
const TChildOfRoot* GltfInteractor::getByID(uint32_t _id) const
{
    gltf::Gltf& gltf = model::ProjectManager::getInstance()->getGltf();
    return gltf.getByID<TChildOfRoot>(_id);
}
}
