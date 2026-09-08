#include "HierarchyModel.h"
#include "RxRunLoop.h"
#include "core/Logger.h"
#include "reactor/Gltf.h"

#include <QJsonDocument>

#include "model/ProjectManager.h"
#include "widgets/MainWindow.h"

#include <QJsonDocument>

#include "reactor/Json.h"
#include "reactor/Reactor.h"
#include "reactor/Utils.h"

using namespace ux3d;
using namespace ux3d::editor;

HierarchyModel::HierarchyModel(QObject* parent)
    : TreeModel(parent)
#ifdef HIERARCHY_MODEL_TESTING
    , tester(this, QAbstractItemModelTester::FailureReportingMode::Fatal, parent)
#endif
{
}

void HierarchyModel::init(const QString& observable)
{
    m_newGltfSubscription = model::ProjectManager::getInstance()->model.newGltfIndex.observe_on(model::ProjectManager::getInstance()->uiWorker).subscribe([this](auto) { setHierarchy(nlohmann::json::object({ { "children", nlohmann::json::array() } })); }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });

    if (observable == "hierarchy") {
        m_subscription = model::ProjectManager::getInstance()->model.hierarchy.observe_on(model::ProjectManager::getInstance()->uiWorker).subscribe([this](const nlohmann::json& hierarchy) { setHierarchy(hierarchy); }, [](std::exception_ptr ep) {
                                     try {
                                         std::rethrow_exception(ep);
                                     } catch (const std::exception& ex) {
                                         core::logError("%s", ex.what());
                                     } });
    } else {
        m_subscription = model::ProjectManager::getInstance()->model.resources.observe_on(model::ProjectManager::getInstance()->uiWorker).subscribe([this](const nlohmann::json& hierarchy) { setHierarchy(hierarchy); }, [](std::exception_ptr ep) {
                                     try {
                                         std::rethrow_exception(ep);
                                     } catch (const std::exception& ex) {
                                         core::logError("%s", ex.what());
                                     } });
    }

    connect(this, &HierarchyModel::itemPushed, this, [this](const Node& node) {
        auto pointer = nlohmann::json::json_pointer(node.data.json["id"].get<std::string>());
        auto id_info = node.data.json["id_info"];
        auto id_resolved = node.data.json["id_resolved"];
        uintptr_t id = reinterpret_cast<uintptr_t>(&node);

        const nlohmann::json& linearizedSchema = model::ProjectManager::getInstance()->model.m_linearizedSchema;

        auto filterInvalidPointerFunction = [this, pointer](const std::shared_ptr<const nlohmann::json>& headPtr) {
            auto& gltf = headPtr->at("gltf");
            return gltf.contains(pointer);
        };

        auto extractDataFunction = [pointer, this, &linearizedSchema, id_info, id_resolved](const std::shared_ptr<const nlohmann::json>& headPtr) {
            // Fill data
            auto& gltf = headPtr->at("gltf");
            nlohmann::json data = nlohmann::json::object();

            // If the pointer points to a glTF CoR reference, store the path to the referenced
            // object in resolvedPointer
            nlohmann::json::json_pointer resolvedPointer = pointer;
            if (!id_info["local_path"].get<std::string>().empty() && gltf.contains(pointer) && gltf.at(pointer).is_string()) {
                resolvedPointer = nlohmann::json::json_pointer(gltf.at(pointer));
            }

            // Get name of the resolved CoR property
            if (gltf.contains(resolvedPointer / "name")) {
                data["name"] = gltf[resolvedPointer / "name"];
            }

            // Get type from linearizedSchema
            std::string substitudedPath = reactor::gltf::substituteIndices(resolvedPointer.to_string());
            if (linearizedSchema.contains(substitudedPath) && linearizedSchema[substitudedPath].contains("title")) {
                data["type"] = linearizedSchema[substitudedPath]["title"];
            } else if (!resolvedPointer.parent_pointer().parent_pointer().empty()) {
                data["type"] = resolvedPointer.parent_pointer().back(); // Needed for xmp
            } else {
                data["type"] = resolvedPointer.back();
            }

            // Add image preview

            if (data.contains("type") && (data["type"] == "Image" || data["type"] == "Texture")) {
                nlohmann::json imageData = reactor::utils::extractImagePreviewFromData(resolvedPointer, headPtr, model::ProjectManager::getInstance()->model.m_dataStorage);
                if (!imageData.empty()) {
                    data["source"] = imageData["source"];
                }
            }
            // Check for default scene
            if (gltf.contains("scene") && !resolvedPointer.parent_pointer().empty() && resolvedPointer.parent_pointer().back() == "scenes") {
                data["isDefaultScene"] = resolvedPointer.to_string() == gltf["scene"];
            }

            // Get index
            if (gltf.contains(resolvedPointer.parent_pointer()) && gltf[resolvedPointer.parent_pointer()].is_array()) {
                data["index"] = std::stoi(resolvedPointer.back());
            }

            // id_info contains the UUID and localPath
            data["id_info"] = id_info;

            // id_resolved contains the gltf path and resolved UUID (which is different to UUID in case of a reference)
            data["id_resolved"] = id_resolved;
            return data;
        };

        auto subscription = model::ProjectManager::getInstance()->model.headShared
                                // filter pointers that became invalid during the setHierarchy update
                                .filter(filterInvalidPointerFunction)
                                // extract the data for the pointer from the head
                                .map(extractDataFunction)
                                // don't emit a dataChanged signal if nothing has changed
                                .distinct_until_changed()
                                .observe_on(model::ProjectManager::getInstance()->uiWorker)
                                .subscribe([&node, this, id](const auto& data) {
                                        m_data[reinterpret_cast<uintptr_t>(&node)] = std::move(data); // Cache data
                                        QModelIndex changedIndex = index(node, 0);
                                        emit dataChanged(changedIndex, changedIndex, { Qt::DisplayRole }); }, [](std::exception_ptr ep) {
                                            try {
                                                std::rethrow_exception(ep);
                                            } catch (const std::exception& ex) {
                                                core::logError("%s", ex.what());
                                            } });
        m_subscriptions.emplace(id, std::move(subscription)); // Store subscription
    });

    connect(this, &HierarchyModel::itemPopped, this, [this](const Node& node) {
        uintptr_t ID = reinterpret_cast<uintptr_t>(&node);
        if (m_subscriptions.contains(ID)) {
            m_subscriptions[ID].unsubscribe();
            m_subscriptions.erase(ID);
        }
        if (m_data.contains(ID)) {
            m_data.erase(ID);
        }
    });
}

QJsonObject HierarchyModel::getHierarchy() const
{
    auto document = QJsonDocument::fromJson(QString(this->treeData.dump(4).c_str()).toUtf8());
    return document.object();
}

HierarchyModel::~HierarchyModel()
{
    m_subscription.unsubscribe();
    for (auto& [id, subscription] : m_subscriptions) {
        subscription.unsubscribe();
    }

    m_newGltfSubscription.unsubscribe();
}

QJsonObject HierarchyModel::filter(const QJsonObject& gltf, const QJsonObject& input, const QString& filter)
{
    if (filter == "") {
        return input;
    }
    auto dumped = QJsonDocument(input).toJson(QJsonDocument::Compact).toStdString();
    auto root = nlohmann::json::parse(dumped);
    dumped = QJsonDocument(gltf).toJson(QJsonDocument::Compact).toStdString();
    auto parsedGltf = nlohmann::json::parse(dumped);
    if (!filterHelper(parsedGltf, root, filter.toStdString(), "")) {
        return QJsonDocument().object();
    }
    auto document = QJsonDocument::fromJson(QString(root.dump().c_str()).toUtf8());
    return document.object();
}

bool HierarchyModel::filterHelper(const nlohmann::json& gltf, nlohmann::json& target, const std::string& filter, const std::string& currentPos)
{
    nlohmann::json::json_pointer hierarchyPointer(currentPos);
    if (!target.contains(hierarchyPointer)) {
        return false;
    }
    nlohmann::json& hierarchyObject = target.at(hierarchyPointer);
    std::string path = hierarchyObject["path"];
    if (path == "" || path.front() == '/') {
        nlohmann::json::json_pointer gltfPointer(path);
        if (!gltf.contains(gltfPointer)) {
            return false;
        }
        if (gltf.at(gltfPointer).contains("name")) {
            std::string name = gltf.at(gltfPointer)["name"];
            // Tranform all strings to lower case
            std::transform(name.begin(), name.end(), name.begin(),
                [](unsigned char c) { return std::tolower(c); });
            std::string filterCopy = filter;
            std::transform(filterCopy.begin(), filterCopy.end(), filterCopy.begin(),
                [](unsigned char c) { return std::tolower(c); });
            if (name.find(filterCopy) != std::string::npos) {
                return true;
            }
        }
    }
    if (hierarchyObject.contains("children") && hierarchyObject["children"].size() > 0) {
        bool match = false;
        for (int i = static_cast<int>(hierarchyObject["children"].size()) - 1; i >= 0; --i) {
            match |= filterHelper(gltf, target, filter, currentPos + "/children/" + std::to_string(i));
        }
        if (match) {
            return match;
        }
    }
    if (!hierarchyPointer.empty()) {
        target.at(hierarchyPointer.parent_pointer()).erase(std::stoi(hierarchyPointer.back()));
    }
    return false;
}

int HierarchyModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

QVariant HierarchyModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }

    auto node = reinterpret_cast<Node*>(index.internalPointer());
    if (!node->data.json.contains("id")) {
        return QString();
    }
    const auto& id_resolved = node->data.json["id_resolved"];
    const auto& id_info = node->data.json["id_info"];
    uintptr_t nodeId = reinterpret_cast<uintptr_t>(node);

    // Fill data
    nlohmann::json data;
    if (m_data.contains(nodeId)) {
        data = m_data.at(nodeId);
    } else {
        data = nlohmann::json::object();
        try {
            // for some reason sometimes the id is not set
            data["type"] = nlohmann::json::json_pointer(node->data.json["id"]).back();
        } catch (const std::exception& ex) {
            data["type"] = "unknown";
        }
        data["id_resolved"] = id_resolved;
        data["id_info"] = id_info;
        data["name"] = "ERROR: " + id_info["uuid"].get<std::string>();
        if (node->data.json.contains("source")) {
            data["source"] = node->data.json["source"];
        }
    }
    data["childCount"] = node->children.size();
    data["has_children"] = !node->children.empty();

    // Convert to Qt JSON
    return QString(data.dump().c_str()).toUtf8();
}

void HierarchyModel::setHierarchy(const QJsonObject& hierarchy)
{
    auto dumped = QJsonDocument(hierarchy).toJson(QJsonDocument::Compact).toStdString();
    auto root = nlohmann::json::parse(dumped);
    setHierarchy(root);
}

/**
 * @brief Updates the hierarchy by applying a diff between root and m_root
 * @param root updated hierarchy
 */
void HierarchyModel::setHierarchy(const nlohmann::json& root)
{
    this->updateTree(root);
}
