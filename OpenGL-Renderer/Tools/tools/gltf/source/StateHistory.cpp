// Copyright © 2020 UX3D GmbH. All rights reserved.

#include "gltf/StateHistory.h"
#include "core/Logger.h"
#include "gltf/Base64.h"
#include "gltf/Gltf.h"
#include "gltf/ResourceCollector.h"
#include "gltf/StorageTypeConverter.h"
#include "gltf/json/GltfFromJson.h"
#include "gltf/json/GltfToJson.h"
#include "imageformats/imageformats.h"
#include "mementos/Mementos.h"
#include "reactor/Utils.h"

#include <stdexcept>
#include <tbb/parallel_for_each.h>

using namespace ux3d;
using namespace ux3d::gltf;

StateHistory::StateHistory(datastorage::DataStorage& dataStorage, mementos::Mementos<nlohmann::json>& gltfMementos, std::mutex& gltfMementosMutex, const rxcpp::observe_on_one_worker& uiWorker)
    : m_gltfMementosMutex(gltfMementosMutex)
    , m_dataStorage(dataStorage)
    , m_gltfMementos(gltfMementos)
    , m_uiWorker(uiWorker)
{
}

void StateHistory::cleanseGltf()
{
    m_gltfMementosMutex.lock();
    if (m_gltfMementos.present.past.empty()) {
        m_gltfMementosMutex.unlock();
        return;
    }
    auto head = m_gltfMementos.present.past.back();
    m_gltfMementosMutex.unlock();
    cleansed.get_subscriber().on_next(head);
}

void StateHistory::storeMeshVisualizationSettings(Gltf& _gltf)
{
    nlohmann::json meshVisHelper;

    for (auto& mesh : _gltf.getMeshes()) {
        const uint32_t id = mesh.m_ID;
        nlohmann::json meshEntity;
        meshEntity["id"] = std::to_string(id);
        meshEntity["isFilled"] = mesh.getFilled();
        meshEntity["isVisible"] = mesh.getVisible();
        meshVisHelper.push_back(meshEntity);
    }
    m_visualizationSettings["meshVisualizationArray"] = meshVisHelper;
}

void StateHistory::storeNodeVisualizationSettings(Gltf& _gltf)
{
    nlohmann::json nodeVisHelper;

    for (auto& node : _gltf.getNodes()) {
        const uint32_t id = node.m_ID;
        nlohmann::json nodeEntity;
        nodeEntity["id"] = std::to_string(id);
        nodeEntity["isVisible"] = node.getVisible();
        nodeVisHelper.push_back(nodeEntity);
    }
    m_visualizationSettings["nodeVisualizationArray"] = nodeVisHelper;
}

void StateHistory::storeAnimationVisualizationSettings(Gltf& _gltf)
{
    nlohmann::json animationPlayHelper;

    for (auto& animation : _gltf.getAnimations()) {
        const uint32_t id = animation.m_ID;
        nlohmann::json animationEntity;
        animationEntity["id"] = std::to_string(id);
        animationEntity["isPlaying"] = animation.isPlaying();
        animationPlayHelper.push_back(animationEntity);
    }
    m_visualizationSettings["animationVisualizationArray"] = animationPlayHelper;
}

// Json -> Gltf
rxcpp::observable<Gltf*> StateHistory::applyToStateHistory()
{
    m_gltfMementosMutex.lock();
    if (m_gltfMementos.present.past.empty()) {
        m_gltfMementosMutex.unlock();
        return rxcpp::observable<Gltf*>();
    }
    auto head = m_gltfMementos.present.past.back();
    auto observable = applyToStateHistory(*head);
    m_gltfMementosMutex.unlock();
    return observable;
}

// Json -> Gltf
rxcpp::observable<Gltf*> StateHistory::applyToStateHistory(const nlohmann::json& head)
{
    Gltf* updatedGltf = new Gltf();

    GltfFromJson converter(*updatedGltf);
    converter.convert(head["gltf"].dump());

    for (Buffer& buffer : updatedGltf->getBuffers()) {
        if (buffer.getStorageFormat() == StorageFormat::NotStored) {
            // this is probably a draco decoder buffer
        }
        std::string dataCategory = "";
        std::string uri = buffer.getUri();
        if (uri.empty()) {
            // GLB-stored buffer --> construct URI for files section
            dataCategory = "glb";
            uri = "glb_buffer";
        } else if (uri.find("base64_embedded_data_") != std::string::npos) {
            dataCategory = "embedded";
        } else {
            dataCategory = "files";
        }
        const nlohmann::json& dataLookup = head[dataCategory];

        if (false == dataLookup.contains(uri)) {
            throw std::runtime_error("buffer was not stored in datastorage");
        }

        auto dataId = dataLookup[uri].get<datastorage::DataStorage::ID>();
        const auto& data = m_dataStorage.at(dataId);

        core::MemoryRange<const uint8_t> memRange(data.data(), data.size());
        buffer.setData(memRange);
    }

    std::vector<gltf::Image*> images;
    std::transform(updatedGltf->getImages().begin(), updatedGltf->getImages().end(), std::back_inserter(images), [](gltf::Image& image) { return &image; });
    tbb::parallel_for_each(images.begin(), images.end(), [&head, this](gltf::Image* newImage) {
        auto uri = newImage->getUri();

        core::Vector<uint8_t> encodedData;
        if (uri.empty()) {
            // buffer stored image --> construct URI for files section
            nlohmann::json::json_pointer path = nlohmann::json::json_pointer("/gltf/bufferViews") / newImage->m_bufferViewIndex;
            try {
                std::vector<uint8_t> data = reactor::utils::getBufferViewData(path, head, m_dataStorage);
                encodedData.reserve(data.size());
                encodedData.assign(data.begin(), data.end());
                // newImage.setStorageFormat(StorageFormat::BufferView);
            } catch (std::exception e) {
                newImage->loadAsErrorImage();
                return;
            }
        } else {
            std::string dataCategory = "";
            if (uri.find("base64_embedded_data_") != std::string::npos) {
                dataCategory = "embedded";
                // newImage.setStorageFormat(StorageFormat::Embedded);
            } else {
                dataCategory = "files";
                // newImage.setStorageFormat(StorageFormat::File);
            }
            const nlohmann::json& dataLookup = head[dataCategory];

            if (!dataLookup.contains(uri)) {
                newImage->loadAsErrorImage();
                return;
            }

            auto dataId = dataLookup[uri].get<datastorage::DataStorage::ID>();
            auto& data = m_dataStorage.at(dataId);

            encodedData.reserve(data.size());
            encodedData.assign(data.begin(), data.end());
        }

        newImage->setEncodedBytes(encodedData);

        image::Image image;
        imageformats::decodeImage(*newImage->getEncodedBytes(), image);
        newImage->setData(std::move(image));
    });

    applyCurrentMeshVisualizationSettings(*updatedGltf);
    applyCurrentNodeVisualizationSettings(*updatedGltf);
    applyCurrentAnimationVisualizationSettings(*updatedGltf);
    return rxcpp::observable<>::just(updatedGltf);
}

void StateHistory::applyCurrentMeshVisualizationSettings(Gltf& _toGltf)
{
    const auto& arr = m_visualizationSettings["meshVisualizationArray"];
    for (auto& mesh : _toGltf.getMeshes()) {
        const uint32_t id = mesh.m_ID;
        if (!arr.is_array() || id >= arr.size())
            continue;

        const auto& entry = arr[id];
        const std::string storedId = entry.value("id", "");
        if (std::to_string(id) == storedId) {
            mesh.setFilled(entry.value("isFilled", mesh.getFilled()));
            mesh.setVisible(entry.value("isVisible", mesh.getVisible()));
        }
    }
}

void StateHistory::applyCurrentNodeVisualizationSettings(Gltf& _toGltf)
{
    const auto& arr = m_visualizationSettings["nodeVisualizationArray"];
    for (auto& node : _toGltf.getNodes()) {
        const uint32_t id = node.m_ID;
        if (!arr.is_array() || id >= arr.size())
            continue;

        const auto& entry = arr[id];
        const std::string storedId = entry.value("id", "");
        if (std::to_string(id) == storedId) {
            node.setVisible(entry.value("isVisible", node.getVisible()));
        }
    }
}

void StateHistory::applyCurrentAnimationVisualizationSettings(Gltf& _toGltf)
{
    const auto& arr = m_visualizationSettings["animationVisualizationArray"];
    for (auto& animation : _toGltf.getAnimations()) {
        const uint32_t id = animation.m_ID;
        if (!arr.is_array() || id >= arr.size())
            continue;

        const auto& entry = arr[id];
        const std::string storedId = entry.value("id", "");
        if (std::to_string(id) == storedId) {
            animation.setPlaying(entry.value("isPlaying", animation.isPlaying()));
        }
    }
}
