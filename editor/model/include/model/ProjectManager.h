#pragma once

#include "datastorage/DataStorage.h"
#include "gltf/Gltf.h"
#include "gltf/StateHistory.h"
#include "mementos/Mementos.h"
#include "model/EditorModel.h"
#include "model/GltfModel.h"

#include <atomic>
#include <functional>
#include <list>
#include <string>

#include <rxcpp/rx.hpp>

namespace ux3d::model {
class ProjectManager {
public:
    static void init(const rxcpp::observe_on_one_worker& uiWorker, const rxcpp::observe_on_one_worker& miscWorker, std::function<void(const std::tuple<nlohmann::json, nlohmann::json>& tuple, gltf::Gltf* gltf)> convertToOldSelection);
    static ProjectManager* getInstance();
    ProjectManager(ProjectManager const&) = delete;
    void operator=(ProjectManager const&) = delete;

    gltf::Gltf& getGltf() { return m_gltf; }

    void resetPaths();

    std::string getGltfPath() const;
    std::string getGltfDirectory() const;
    std::string getGltfFileName() const;
    void setGltfPath(const std::string& _path);

    int getSelectedMaterialVariantIndex();
    void setSelectedMaterialVariant(std::optional<uint32_t> materialVariantID) { m_selectedMaterialVariant = materialVariantID; }
    std::optional<uint32_t> getSelectedMaterialVariant() { return m_selectedMaterialVariant; };

    void setRenderViewCameraNodeID(uint32_t nodeID) { m_renderViewCameraNodeID = nodeID; }
    void unsetRenderViewCameraNodeID() { m_renderViewCameraNodeID = std::nullopt; }
    std::optional<uint32_t> getRenderViewCameraNodeID() { return m_renderViewCameraNodeID; };

    void setCurrentScene(gltf::Scene* scene) { m_pCurrentScene = scene; }
    gltf::Scene* getCurrentScene() { return m_pCurrentScene; }
    int getCurrentSceneIndex() const;
    void setCurrentSceneByIndex(int _index);
    void resetCurrentScene() { m_pCurrentScene = nullptr; }

    static void stateChangedTemporarily();
    static void meshVisualizationChanged();
    static void nodeVisualizationChanged();
    static void animationVisualizationChanged();

    bool isCMDMode() { return m_cmdMode; }
    void setCMDMode(bool _cmdMode) { m_cmdMode = _cmdMode; };

    bool isRendererEnabled() { return m_rendererEnabled; }
    void setRendererEnabled(bool enabled) { m_rendererEnabled = enabled; }

    rxcpp::observe_on_one_worker uiWorker;
    rxcpp::observe_on_one_worker miscWorker;

    Gltf model;
    Editor editorModel;

    gltf::StateHistory stateHistory;
    rxcpp::subjects::subject<std::monostate> headChangeWithoutStateChanged;
    rxcpp::observable<bool> dirty;
    rxcpp::subjects::subject<std::monostate> dirtyStateChanged;

    std::string version = "0.0.1";

private:
    ProjectManager(const rxcpp::observe_on_one_worker& uiWorker, const rxcpp::observe_on_one_worker& miscWorker, std::function<void(const std::tuple<nlohmann::json, nlohmann::json>& tuple, gltf::Gltf* gltf)> convertToOldSelection);
    static ProjectManager* instance;

    std::string m_gltfDirectory = "";
    std::string m_gltfFileName = "";

    gltf::Gltf m_gltf;
    // surpress some behaviour when mementos head changes
    std::atomic<bool> m_surpressApplyToStateHistory;
    std::optional<uint32_t> m_selectedMaterialVariant = std::nullopt;
    gltf::Scene* m_pCurrentScene = nullptr;
    std::optional<uint32_t> m_renderViewCameraNodeID = std::nullopt;

    std::list<std::function<void()>> m_dirtyStateReceiver;
    std::list<std::function<void()>> m_dirtyStateTempReceiver;
    bool m_cmdMode = false;
    bool m_rendererEnabled = true;
};
}
