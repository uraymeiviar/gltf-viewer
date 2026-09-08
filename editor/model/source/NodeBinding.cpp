// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: David Labode

#include "model/NodeBinding.h"
#include "model/ProjectManager.h"

#include "gltf/Gltf.h"

using namespace ux3d;
using namespace ux3d::model;

NodeBinding::NodeBinding(gltf::Node* node, nlohmann::json::json_pointer& pointer)
    : m_gltfId(node->m_ID)
    , nodePointer(pointer)
{
}

gltf::Node* NodeBinding::getGltfObject() const
{
    return ProjectManager::getInstance()->getGltf().getByID<gltf::Node>(m_gltfId);
}

void NodeBinding::setTranslation(const math::dvec3& translation) const
{
    gltf::Node* node = getGltfObject();
    if (node == nullptr) {
        return;
    }

    node->setTranslation(translation);
    ProjectManager::stateChangedTemporarily();
}

void NodeBinding::setRotation(const math::dquat& rotation) const
{
    gltf::Node* node = getGltfObject();
    if (node == nullptr) {
        return;
    }

    auto normalizedRotation = math::normalize(rotation);
    if (core::Failed(node->setRotation(normalizedRotation))) {
        return; // TODO: present error
    }

    ProjectManager::stateChangedTemporarily();
}

void NodeBinding::setScale(const math::dvec3& scale) const
{
    gltf::Node* node = getGltfObject();
    if (node == nullptr) {
        return;
    }

    node->setScale(scale);
    ProjectManager::stateChangedTemporarily();
}

// saves translation to mementos
void NodeBinding::saveTranslation()
{
    gltf::Node* node = getGltfObject();
    if (node == nullptr || nodePointer.empty()) {
        return;
    }

    const auto& translation = node->getTranslationOrDefault();
    nlohmann::json valueArray = nlohmann::json::array();
    valueArray.push_back(translation.x);
    valueArray.push_back(translation.y);
    valueArray.push_back(translation.z);

    auto taskDescription = nlohmann::json({ { "set_gltf_value", nlohmann::json::object() } });
    taskDescription["set_gltf_value"]["path"] = "/gltf" + nodePointer.to_string() + "/translation";
    taskDescription["set_gltf_value"]["value"] = valueArray;

    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    model::ProjectManager::getInstance()->model.schedule(taskDescription).error.observe_on(uiWorker).subscribe([](const std::string& e) {
        core::logError(e.c_str());
    });
}
// saves rotation to mementos
void NodeBinding::saveRotation()
{
    gltf::Node* node = getGltfObject();
    if (node == nullptr || nodePointer.empty()) {
        return;
    }

    const auto& rotation = node->getRotationOrDefault();
    nlohmann::json valueArray = nlohmann::json::array();
    valueArray.push_back(rotation.x);
    valueArray.push_back(rotation.y);
    valueArray.push_back(rotation.z);
    valueArray.push_back(rotation.w);

    auto taskDescription = nlohmann::json({ { "set_gltf_value", nlohmann::json::object() } });
    taskDescription["set_gltf_value"]["path"] = "/gltf" + nodePointer.to_string() + "/rotation";
    taskDescription["set_gltf_value"]["value"] = valueArray;

    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    model::ProjectManager::getInstance()->model.schedule(taskDescription).error.observe_on(uiWorker).subscribe([](const std::string& e) {
        core::logError(e.c_str());
    });
}
// saves scale to mementos
void NodeBinding::saveScale()
{
    gltf::Node* node = getGltfObject();
    if (node == nullptr || nodePointer.empty()) {
        return;
    }

    const auto& scale = node->getScaleOrDefault();
    nlohmann::json valueArray = nlohmann::json::array();
    valueArray.push_back(scale.x);
    valueArray.push_back(scale.y);
    valueArray.push_back(scale.z);

    auto taskDescription = nlohmann::json({ { "set_gltf_value", nlohmann::json::object() } });
    taskDescription["set_gltf_value"]["path"] = "/gltf" + nodePointer.to_string() + "/scale";
    taskDescription["set_gltf_value"]["value"] = valueArray;

    const auto& uiWorker = model::ProjectManager::getInstance()->uiWorker;
    model::ProjectManager::getInstance()->model.schedule(taskDescription).error.observe_on(uiWorker).subscribe([](const std::string& e) {
        core::logError(e.c_str());
    });
}
