#include "model/ProjectManager.h"
#include "reactor/Reactor.h"
#include <filesystem>

#include <gltf_schema/gltf_single_json.h>

using namespace ux3d;
using namespace ux3d::model;

ProjectManager* ProjectManager::instance;

ProjectManager::ProjectManager(const rxcpp::observe_on_one_worker& uiWorker, const rxcpp::observe_on_one_worker& miscWorker, std::function<void(const std::tuple<nlohmann::json, nlohmann::json>& tuple, gltf::Gltf* gltf)> convertToOldSelection)
    : uiWorker(uiWorker)
    , miscWorker(miscWorker)
    , model(Gltf::observeOnNewThread)
    , stateHistory(model.m_dataStorage, model.m_mementos, model.m_mementosMutex, uiWorker)
    , editorModel(model)
{

    model.m_mementos.head.distinct_until_changed().observe_on(uiWorker).subscribe([this, convertToOldSelection](const nlohmann::json& head) {
        if (!m_surpressApplyToStateHistory) {
            stateHistory.applyToStateHistory(head).with_latest_from(model.uuidPaths).subscribe([&, convertToOldSelection](const std::tuple<gltf::Gltf*, nlohmann::json> tuple) {
                const auto& [gltf, uuidPaths] = tuple;
                this->m_gltf = std::move(*gltf);
                convertToOldSelection(std::make_tuple(nlohmann::json::object(), uuidPaths), &m_gltf);
                this->headChangeWithoutStateChanged.get_subscriber().on_next(std::monostate());
            }, [](std::exception_ptr ep) {
                try {
                    std::rethrow_exception(ep);
                } catch (const std::exception& ex) {
                    core::logError("%s", ex.what());
                }
            });
        } }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });

    dirty = model.m_mementos.headPointer
                .observe_on(uiWorker)
                .combine_latest(stateHistory.cleansed.get_observable())
                .map([](const std::tuple<std::shared_ptr<const nlohmann::json>, std::shared_ptr<const nlohmann::json>>& tuple) {
                    return std::get<0>(tuple) != std::get<1>(tuple);
                })
                .start_with(false)
                .distinct_until_changed();
}

void ProjectManager::init(const rxcpp::observe_on_one_worker& uiWorker, const rxcpp::observe_on_one_worker& miscWorker, std::function<void(const std::tuple<nlohmann::json, nlohmann::json>& tuple, gltf::Gltf* gltf)> convertToOldSelection)
{
    instance = new ProjectManager(uiWorker, miscWorker, convertToOldSelection);
}

ProjectManager* ProjectManager::getInstance()
{
    return instance;
}

std::string ProjectManager::getGltfPath() const
{
    if (m_gltfDirectory.empty() || m_gltfFileName.empty()) {
        return "";
    }

    std::filesystem::path filePath(m_gltfDirectory + "/" + m_gltfFileName);
    return filePath.make_preferred().string();
}

std::string ProjectManager::getGltfDirectory() const
{
    return m_gltfDirectory;
}

std::string ProjectManager::getGltfFileName() const
{
    return m_gltfFileName;
}

void ProjectManager::resetPaths()
{
    m_gltfDirectory = "";
    m_gltfFileName = "";
}

void ProjectManager::setGltfPath(const std::string& _path)
{
    std::filesystem::path filePath(_path);
    filePath = filePath.make_preferred();
    m_gltfDirectory = filePath.parent_path().string();
    m_gltfFileName = filePath.filename().string();
}

void ProjectManager::stateChangedTemporarily()
{
    getInstance()->dirtyStateChanged.get_subscriber().on_next(std::monostate());
}

void ProjectManager::meshVisualizationChanged()
{
    getInstance()->stateHistory.storeMeshVisualizationSettings(getInstance()->getGltf());
}

void ProjectManager::nodeVisualizationChanged()
{
    getInstance()->stateHistory.storeNodeVisualizationSettings(getInstance()->getGltf());
}

void ProjectManager::animationVisualizationChanged()
{
    getInstance()->stateHistory.storeAnimationVisualizationSettings(getInstance()->getGltf());
}

int ProjectManager::getSelectedMaterialVariantIndex()
{
    if (!m_selectedMaterialVariant.has_value()) {
        return -1;
    }

    core::LinkedList<gltf::MaterialVariant>& list = m_gltf.getMaterialVariants();
    for (int i = 0; i < list.size(); ++i) {
        if (list.at(i).m_ID == m_selectedMaterialVariant.value()) {
            return i;
        }
    }

    m_selectedMaterialVariant = std::nullopt;
    return -1;
}

int ProjectManager::getCurrentSceneIndex() const
{
    core::LinkedList<gltf::Scene>& list = model::ProjectManager::getInstance()->getGltf().getScenes();
    int32_t i = 0;
    for (auto& scene : list) {
        if (&scene == m_pCurrentScene) {
            return i;
        }
        i++;
    }
    return -1;
}

void ProjectManager::setCurrentSceneByIndex(int _index)
{
    core::LinkedList<gltf::Scene>& list = model::ProjectManager::getInstance()->getGltf().getScenes();
    if (_index < 0 || list.empty()) {
        m_pCurrentScene = nullptr;
    } else {
        size_t pos = (_index < list.size()) ? _index : list.size() - 1;
        m_pCurrentScene = &list.at(_index);
    }
}
