#include "interactors/GltfInteractor.h"
#include "interfaces/GltfInteractorOutput.h"
#include "model/ProjectManager.h"
#include "useCases/GltfUseCases.h"

using namespace ux3d;
using namespace ux3d::editor;

GltfInteractor* GltfInteractor::getInstance()
{
    static GltfInteractor instance;
    return &instance;
}

gltf::Scene* GltfInteractor::getDefaultScene()
{
    gltf::Gltf& gltf = model::ProjectManager::getInstance()->getGltf();
    return gltf.getDefaultScene();
}

int32_t GltfInteractor::getDefaultSceneIndex()
{
    gltf::Scene* defaultScene = getDefaultScene();
    if (defaultScene == nullptr) {
        return -1;
    } else {
        return defaultScene->m_ID;
    }
}

InteractorResult GltfInteractor::execute(GltfSceneOpenUseCase& _useCase) const
{
    if (_useCase.pScene == nullptr) {
        _useCase.pScene = model::ProjectManager::getInstance()->getGltf().getDefaultScene();

        if (_useCase.pScene == nullptr) {
            auto& scenes = model::ProjectManager::getInstance()->getGltf().getScenes();
            _useCase.pScene = scenes.empty() ? nullptr : &scenes.front();
        }
    }

    model::ProjectManager::getInstance()->setCurrentScene(_useCase.pScene);

    for (GltfInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentSceneOpen(_useCase);
    }

    model::ProjectManager::stateChangedTemporarily();

    return InteractorResult::Success;
}
