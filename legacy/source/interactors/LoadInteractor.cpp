#include "interactors/LoadInteractor.h"
#include "gltf/Gltf.h"
#include "gltf/GltfParsing.h"
#include "gltf/Image.h"
#include "gltf/ImportOptions.h"
#include "helpers/QtFileSystem.h"
#include "interfaces/LoadInteractorOutput.h"
#include "model/GltfBinding.h"
#include "model/ProjectManager.h"
#include "useCases/LoadUseCases.h"
#include "widgets/QMLCard.h"

#include <filesystem>

using namespace ux3d;
using namespace ux3d::editor;

LoadInteractor* LoadInteractor::getInstance()
{
    static LoadInteractor instance;
    return &instance;
}

InteractorResult LoadInteractor::execute(const GltfOpenUseCase& _UseCase) const
{
    {
        nlohmann::json params = {};
        params["filepath"] = _UseCase.path;
        QMLCard::resolvePromiseToCard(
            model::ProjectManager::getInstance()->model.schedule(nlohmann::json({ { "load_file", params } })));
    }
    {
        nlohmann::json params = {};
        params["deleteEncodedData"] = false;
        params["cleanExtensionsUsed"] = false;
        model::ProjectManager::getInstance()->model.schedule(nlohmann::json({ { "resolve_draco_for_each", params } }));
    }

    std::filesystem::path gltfFile(_UseCase.path);
    auto& history = model::ProjectManager::getInstance()->stateHistory;
    history.setGltfDir(gltfFile.parent_path());
    // this line here blocks (same mutex as the shedule in the task above) and waits for the load operation to finish
    history.applyToStateHistory().subscribe([&](gltf::Gltf*) {
        for (LoadInteractorOutput* pOutput : m_Outputs) {
            auto& gltf = model::ProjectManager::getInstance()->getGltf();
            pOutput->presentGltfOpen(&gltf);
        }; }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });

    return InteractorResult::Success;
}

InteractorResult LoadInteractor::execute(GltfCreate& _UseCase) const
{
    model::ProjectManager::getInstance()->resetPaths();
    const std::string& gltfPath = model::ProjectManager::getInstance()->getGltfPath();
    std::filesystem::path gltfFile = (gltfPath);
    auto& history = model::ProjectManager::getInstance()->stateHistory;
    history.setGltfDir(gltfFile.parent_path());

    {
        nlohmann::json params = {};
        params["path"] = "/gltf";
        model::ProjectManager::getInstance()->model.schedule(nlohmann::json({ { "reset_gltf", params } }));
    }

    return InteractorResult::Success;
}

InteractorResult LoadInteractor::reloadGltf() const
{
    for (LoadInteractorOutput* pOutput : m_Outputs) {
        pOutput->presentGltfReloaded(&model::ProjectManager::getInstance()->getGltf());
    }
    return InteractorResult::Success;
}
