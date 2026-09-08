#include "SelectionManager.h"

//-----------------------
// imports from legacy content logic
#include "Actions.h"
#include "ExternalEditingManager.h"
#include "core/Platform.h"
#include "interactors/GltfInteractor.h"
#include "model/ProjectManager.h"
#include "useCases/GltfUseCases.h"
#include "widgets/QMLCard.h"

#include <QAction>
#include <QFileDialog>
//------------------------

using ux3d::editor::SelectionManager;

SelectionManager::SelectionManager()
{
    connect(this, &SelectionManager::selectObjects, [this](const std::vector<SelectionInfo>& selection) {
        m_selectedObjects = selection;
    });

    // Create actions
    connect(Actions::instance().setDefaultScene, &QAction::triggered, this, &SelectionManager::setDefaultScene);
    connect(Actions::instance().unsetDefaultScene, &QAction::triggered, this, &SelectionManager::unsetDefaultScene);
    connect(Actions::instance().openScene, &QAction::triggered, this, &SelectionManager::openSelectedScene);
    connect(Actions::instance().editImageExternally, &QAction::triggered, this, &SelectionManager::editImageExternally);
    // [this] {
    // auto& selection = SelectionManager::getInstance()->getCurrentSelection();
    // if (selection.type == GltfPropertyType::Images) {
    //     gltf::Image* image = selection.pointer.unsafeGet<gltf::Image*>();
    //     ExternalEditingManager::getInstance()->edit(image);
    // }
    //});
    const auto onError = [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        }
    };
    model::ProjectManager::getInstance()->editorModel.selection.subscribe([this](const nlohmann::json& json) {
        m_selection = json;
    },
        onError);
    model::ProjectManager::getInstance()->model.uuidPaths.subscribe([this](const nlohmann::json& json) {
        m_uuidPaths = json;
    },
        onError);
    model::ProjectManager::getInstance()->model.headRaw.subscribe([this](const nlohmann::json headRaw) {
        m_headRaw = headRaw;
    },
        onError);
}

SelectionManager* SelectionManager::getInstance()
{
    static SelectionManager instance;
    return &instance;
}

//-------------------------------------------
// Legacy logic from content
void SelectionManager::openSelectedScene()
{
    const auto& selectedObjects = SelectionManager::getInstance()->getCurrentSelection();
    if (selectedObjects.empty()) {
        return;
    }

    auto& selection = selectedObjects[0];
    if (selection.type == GltfPropertyType::None) {
        return;
    }

    if (selection.type == GltfPropertyType::Scenes && selection.pointer.hasType<gltf::Scene*>() && selection.pointer.unsafeGet<gltf::Scene*>() != nullptr) {
        GltfSceneOpenUseCase useCase;
        useCase.pScene = selection.pointer.unsafeGet<gltf::Scene*>();
        GltfInteractor::getInstance()->execute(useCase);
    }
}

void SelectionManager::editImageExternally()
{
    const auto& selection = SelectionManager::getInstance()->m_selection;

    if (m_selection.size() == 0) {
        return;
    }

    std::string firstSelItem = m_selection[0]["elementAbsolutPath"];

    if (firstSelItem.rfind("/images/", 0) != 0) {
        return;
    }
    ExternalEditingManager::getInstance()->edit("/gltf" + firstSelItem, m_headRaw);
}

void SelectionManager::setDefaultScene()
{
    const auto& selection = SelectionManager::getInstance()->m_selection;

    if (m_selection.size() == 0) {
        return;
    }

    std::string firstSelItem = m_selection[0]["elementAbsolutPath"];

    if (firstSelItem.rfind("/scenes/", 0) != 0) {
        return;
    }

    auto taskDescription = nlohmann::json({ { "set_gltf_pointer", nlohmann::json::object() } });
    taskDescription["set_gltf_pointer"]["path"] = "/gltf/scene";
    taskDescription["set_gltf_pointer"]["pointer"] = "/gltf" + firstSelItem;

    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    model::ProjectManager::getInstance()->model.schedule(taskDescription).error.observe_on(uiWorker).subscribe([](const std::string& e) {
        QMLCard::error(QString::fromStdString(e.c_str()));
    });
}

void SelectionManager::unsetDefaultScene()
{
    auto taskDescription = nlohmann::json({ { "reset", nlohmann::json::object() } });
    taskDescription["reset"]["path"] = "/gltf/scene";

    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    model::ProjectManager::getInstance()->model.schedule(taskDescription).error.observe_on(uiWorker).subscribe([](const std::string& e) {
        QMLCard::error(QString::fromStdString(e.c_str()));
    });
}
//------------------------------
