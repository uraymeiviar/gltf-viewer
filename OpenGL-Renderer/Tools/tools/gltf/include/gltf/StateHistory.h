// Copyright © 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "core/DispatchQueue.h"
#include "core/LinkedList.h"
#include "datastorage/DataStorage.h"
#include <filesystem>

#include "nlohmann/json.hpp"
#include <memory>
#include <mutex>
#include <optional>
#include <variant>
#include <vector>

#include <rxcpp/rx.hpp>

namespace ux3d::mementos {
template <class T>
class Mementos;
}

namespace ux3d::gltf {
class Gltf;
class Node;
class Buffer;
class Image;

class StateHistory {
public:
    explicit StateHistory(datastorage::DataStorage& dataStorage, mementos::Mementos<nlohmann::json>& gltfMementos, std::mutex& gltfMementosMutex, const rxcpp::observe_on_one_worker& uiWorker);

    // Mementos to state history conversion.
    rxcpp::observable<Gltf*> applyToStateHistory(const nlohmann::json& head);
    rxcpp::observable<Gltf*> applyToStateHistory();

    // Un-dirty the project, for example when saving or opening a new file.
    void cleanseGltf();

    void setGltfDir(const std::filesystem::path& _gltfDir) { m_gltfDir = _gltfDir; }
    void resetGltfDir() { m_gltfDir.reset(); }
    void storeMeshVisualizationSettings(Gltf& _gltf);
    void storeNodeVisualizationSettings(Gltf& _gltf);
    void storeAnimationVisualizationSettings(Gltf& _gltf);

    // Head pointers considered to be not dirty.
    rxcpp::subjects::subject<std::shared_ptr<const nlohmann::json>> cleansed;

    rxcpp::observable<Gltf> gltf;

private:
    std::mutex& m_gltfMementosMutex;
    std::optional<std::filesystem::path> m_gltfDir;
    nlohmann::json m_visualizationSettings;
    datastorage::DataStorage& m_dataStorage;
    mementos::Mementos<nlohmann::json>& m_gltfMementos;
    model::DispatchQueue m_dispatchQueue;
    rxcpp::observe_on_one_worker m_uiWorker;

    void copyBuffers(Gltf& _toGltf);
    void copyImages(Gltf& _toGltf);
    void copyAudios(Gltf& _toGltf);
    void verifyURIs();

    void applyCurrentMeshVisualizationSettings(Gltf& _toGltf);
    void applyCurrentNodeVisualizationSettings(Gltf& _toGltf);
    void applyCurrentAnimationVisualizationSettings(Gltf& _toGltf);
};
}
