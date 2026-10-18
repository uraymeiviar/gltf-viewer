#include "InspectorModel.h"
#include "RxRunLoop.h"
#include "core/Logger.h"
#include "reactor/Gltf.h"
#include "reactor/Utils.h"

#include "model/ProjectManager.h"
#include "widgets/MainWindow.h"

#include "reactor/Json.h"
#include "reactor/Operations.h"
#include "reactor/Reactor.h"

#include <QJsonDocument>
#include <optional>

using namespace ux3d;
using namespace ux3d::editor;

InspectorModel::InspectorModel(QObject* qparent)
    : QAbstractItemModel(qparent)
{
    this->operations = reactor::operations::byPathPattern(reactor::operations::byName(model::ProjectManager::getInstance()->model.operationsSchema));
    JsonTreeElement* currentView = nullptr;
    std::string previousParent = "";
    std::map<std::string, JsonTreeElement*> treeElements;
    for (const auto& [key, value] : model::ProjectManager::getInstance()->model.m_linearizedSchema.items()) {
        nlohmann::json::json_pointer pointer(key);
        std::string parent = "";
        if (!pointer.parent_pointer().empty()) {
            parent = pointer.parent_pointer().to_string();
        }
        // root or objects inside arrays
        if (value.contains("type") && value.at("type") == "object" && (pointer.empty() || pointer.back() == "\\d+")) {
            nlohmann::json jsonKey;
            nlohmann::json jsonValue;
            collectProperties(key, value, jsonKey, jsonValue);
            auto pair = inspectorTemplates.try_emplace(key, key, jsonKey, jsonValue);
            currentView = &(pair.first->second);
            treeElements.emplace(key, currentView);
        } else if (pointer.back() == "\\d+" || pointer.back() == "extras") {
            // Do nothing
            continue;
        } else if (currentView != nullptr) {
            if (parent != previousParent) {
                if (treeElements.contains(parent)) {
                    currentView = treeElements.at(parent);
                } else {
                    core::debugLog(core::LogLevel::Error, "Error");
                }
            }
            nlohmann::json jsonKey;
            nlohmann::json jsonValue;
            jsonKey["title"] = pointer.back();

            std::string type;
            collectProperties(key, value, jsonKey, jsonValue);

            // check required field in parent
            const nlohmann::json& parentSchema = model::ProjectManager::getInstance()->model.m_linearizedSchema.at(parent);
            if (parentSchema.contains("required")) {
                const nlohmann::json& requiredArray = parentSchema.at("required");
                if (std::find(requiredArray.begin(), requiredArray.end(), pointer.back()) != requiredArray.end()) {
                    jsonValue["required"] = true;
                }
            }

            if (jsonValue.contains("type")) {
                type = jsonValue["type"];
            } else {
                core::debugLog(core::LogLevel::Error, "Error");
            }

            if (type == "array" || type == "color" || type == "vector" || type == "matrix") {
                nlohmann::json item = value["items"];
                std::string itemType;
                nlohmann::json unused;

                collectProperties(key + "/\\d+", item, unused, jsonValue["items"]);
                if (!jsonValue.contains("/items/type"_json_pointer)) {
                    core::debugLog(core::LogLevel::Error, "Error");
                }
                if (jsonValue["items"]["type"] == "object") {
                    jsonValue["items"]["type"] = "pointer";
                    jsonValue["items"]["icon"] = key;
                    // Only add create_gltf_component for materials variants for now
                    if (jsonValue.contains("/operations/create_gltf_component"_json_pointer) && pointer.back() != "mappings") {
                        jsonValue.at("operations").erase("create_gltf_component");
                    }
                } else {
                }
            } else if (type == "object") {
                if (pointer.back() == "extensions" && (!jsonValue.contains("childCount") || jsonValue["childCount"] == 0)) {
                    previousParent = parent;
                    continue;
                }
            }

            if (nlohmann::json::json_pointer parentPointer(parent); !parentPointer.empty() && parentPointer.back() == "extensions") {
                std::string propertyName = pointer.back();
                size_t first_ = propertyName.find_first_of('_');
                jsonKey["extensionPrefix"] = propertyName.substr(0, first_);
            }

            // get sorting weight from schema
            nlohmann::json propertyOrdering = nlohmann::json();
            if (parentSchema.contains("ux3d_propertyOrdering")) {
                propertyOrdering.update(parentSchema.at("ux3d_propertyOrdering"));
            }
            if (parentSchema.contains("allOf")) {
                for (auto& item : parentSchema.at("allOf")) {
                    if (item.contains("ux3d_propertyOrdering")) {
                        propertyOrdering.update(item.at("ux3d_propertyOrdering"));
                    }
                }
            }

            float sort = 0.0f;
            if (propertyOrdering.contains(pointer.back())) {
                sort = propertyOrdering.at(pointer.back()).get<float>();
            }

            auto& element = currentView->addProperty(pointer.to_string(), jsonKey, jsonValue, sort);
            treeElements.emplace(key, &element);
        } else {
            core::debugLog(core::LogLevel::Error, "Error");
        }
        previousParent = parent;
    }

    // Add preview to image
    nlohmann::json previewKey;
    nlohmann::json previewValue;
    previewKey["type"] = "imagePreview";
    previewValue["type"] = "imagePreview";
    previewKey["metaData"] = true;
    previewKey["title"] = "Preview";

    inspectorTemplates.at("/images/\\d+").addProperty("/images/\\d+/preview", previewKey, previewValue, -3000);
    inspectorTemplates.at("/textures/\\d+").addProperty("/textures/\\d+/preview", previewKey, previewValue, -3000);

    for (auto& [key, value] : inspectorTemplates) {
        value.sortProperties();
    }

    validationResults = nlohmann::json::object();

    validationSubscription = model::ProjectManager::getInstance()->model.validation.distinct_until_changed().observe_on(model::ProjectManager::getInstance()->uiWorker).subscribe([this](const nlohmann::json& report) {
        nlohmann::json transformedReport;
        if (!report.contains("/issues/messages"_json_pointer)) {
            return;
        }
        for (auto& [key, value] : report.at("/issues/messages"_json_pointer).items()) {
            std::string pointer = value.at("pointer");
            nlohmann::json data = value;
            data.erase("pointer");
            if (transformedReport.contains(pointer)) {
                transformedReport.at(pointer).push_back(data);
            } else {
                nlohmann::json newArray = nlohmann::json::array();
                newArray.push_back(data);
                transformedReport[pointer] = newArray;
            }
        }
        if (m_root == nullptr) {
            this->validationResults = transformedReport;
            return;
        }
        auto patch = nlohmann::json::diff(this->validationResults, transformedReport);
        this->validationResults = transformedReport;

        auto notify = [this](const std::string& path) {
            JsonTreeElement* changedElement = m_root->getPropertyRecursive(nlohmann::json::json_pointer(reactor::gltf::substituteIndices(path)));
            if (changedElement != nullptr) {
                QModelIndex changedIndex;
                int row = 0;
                if (changedElement->parent != nullptr) {
                    row = changedElement->parent->getIndex(changedElement->path);
                }
                changedIndex = createIndex(row, 0, changedElement);
                emit dataChanged(changedIndex, changedIndex, { Qt::DisplayRole });
            }
        };

        for (auto& op : patch) {
            std::string changedPath = op["path"];
            // Special case if validationResults is empty
            if (changedPath == "") {
                for (auto& [key, value] : this->validationResults.items()) {
                    notify(key);
                }
                return;
            } else {
                std::string path = reactor::json::getPointerPrefix(nlohmann::json::json_pointer(changedPath), 1).back();
                notify(path);
            }
        } }, [](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& ex) {
            core::logError("%s", ex.what());
        } });
}

InspectorModel::~InspectorModel()
{
    for (auto& [_, subscription] : m_subscriptions) {
        subscription.unsubscribe();
    }
    validationSubscription.unsubscribe();
    m_subscriptions.clear();
}

int InspectorModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

int InspectorModel::rowCount(const QModelIndex& parent) const
{
    if (m_root == nullptr) {
        return 0;
    }
    JsonTreeElement* parentElement = static_cast<JsonTreeElement*>(parent.internalPointer());
    if (parentElement != nullptr) {
        return parentElement->getPropertyCount();
    }
    return m_root->getPropertyCount();
}

bool InspectorModel::insertRows(int row, int count, const QModelIndex& parent)
{
    beginInsertRows(parent, row, row + count - 1);
    endInsertRows();
    return true;
}

bool InspectorModel::removeRows(int row, int count, const QModelIndex& parent)
{
    beginRemoveRows(parent, row, row + count - 1);
    endRemoveRows();
    return true;
}

QVariant InspectorModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::DecorationRole) || m_root == nullptr) {
        return QVariant();
    }
    JsonTreeElement* indexElement = static_cast<JsonTreeElement*>(index.internalPointer());
    if (indexElement == nullptr) {
        indexElement = m_root;
    }

    if (role == Qt::DecorationRole) {
        return QString::fromStdString(indexElement->key["type"].get<std::string>());
    }

    auto data = indexElement->value;
    data.update(indexElement->key);
    const std::string& type = data["type"];
    std::string elementPath;
    if (m_path.isEmpty()) {
        elementPath = indexElement->path;
        data["path"] = "/gltf" + elementPath;
    } else {
        std::string substitutedPath = reactor::gltf::substituteIndices(m_path.toStdString());
        elementPath = m_path.toStdString() + indexElement->path.substr(substitutedPath.size());
        data["path"] = "/gltf" + elementPath;
    }
    if (indexElement->key.contains("metaData")) {
        elementPath = "metaData" + elementPath;
    }

    if (data.contains("isExpanded") == false) {
        data["isExpanded"] = false;
    }

    auto it = m_values.find(elementPath);
    if (it != m_values.end()) {
        auto value = it->second;
        if (type == "array" && value.is_array()) {
            data["childCount"] = value.size();
            if (data["items"].contains("operations")) {
                data["operations"].update(data["items"]["operations"]);
            }
            data.erase("items");
            if (data.at("isExpanded").get<bool>()) {
                data["items"] = nlohmann::json::array();
                for (int i = 0; i < value.size(); ++i) {
                    auto item = nlohmann::json::object();
                    item["icon"] = indexElement->value["items"]["icon"];
                    item["type"] = indexElement->value["items"]["type"];
                    // value is of type object if this element is a pointer
                    if (value.at(i).is_object()) {
                        item.update(value.at(i));
                        if (value.at(i).contains("textureSource")) {
                            item["textureSource"] = value.at(i)["textureSource"];
                        }
                    } else {
                        item["value"] = value.at(i);
                    }

                    data["items"].push_back(std::move(item));
                }
            }
        } else if (type == "matrix" && value.is_array()) {
            transposeMatrix(value);
            data["value"] = value;
        } else {
            // value is of type object if this element is a pointer
            if (value.is_object()) {
                if (type == "pointer") {
                    data["value"] = value["value"];
                    data["uuid"] = value["uuid"];
                    data["localPath"] = value["localPath"];
                    data["name"] = value["name"];
                    data["referenceCandidates"] = value["referenceCandidates"];
                    if (value.contains("textureSource")) {
                        data["textureSource"] = value["textureSource"];
                    }
                    if (value.contains("size")) {
                        data["size"] = value["size"];
                    }
                } else if (type == "imagePreview") {
                    data["value"] = value;
                } else {
                    data["value"] = true;
                }
            } else {
                data["value"] = value;
            }
        }
    }
    if (validationResults.contains(elementPath)) {
        data["validationResults"] = validationResults[elementPath];
    }

    auto document = QJsonDocument::fromJson(QString(data.dump().c_str()).toUtf8());
    return document.object();
}

QModelIndex InspectorModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    JsonTreeElement* parentElement = static_cast<JsonTreeElement*>(parent.internalPointer());
    if (parentElement == nullptr) {
        if (m_root == nullptr) {
            return QModelIndex();
        }
        parentElement = m_root;
    }
    JsonTreeElement* child = parentElement->getProperty(row);
    if (child == nullptr) {
        return QModelIndex();
    }
    return createIndex(row, column, child);
}

QModelIndex InspectorModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    JsonTreeElement* indexElement = static_cast<JsonTreeElement*>(index.internalPointer());

    // for some reason this line can cause a crash
    try {
        if (indexElement == nullptr || indexElement->parent == nullptr || indexElement->parent->parent == nullptr) {
            return QModelIndex();
        }
    } catch (const std::exception& e) {
        core::debugLog(core::LogLevel::Error, "Error: %s", e.what());
        return QModelIndex();
    }
    int row = 0;
    if (indexElement->parent->parent != nullptr) {
        row = indexElement->parent->parent->getIndex(indexElement->parent->path);
    }
    return createIndex(row, 0, indexElement->parent);
}

void InspectorModel::setPath(const QString& path)
{
    m_path = path;
    setInspectorPath(m_path.toStdString());
    emit pathChanged();
}

QString InspectorModel::getPath()
{
    return m_path;
}

void InspectorModel::setIsRowExpanded(const bool isExpanded, const QModelIndex& index)
{
    JsonTreeElement* jsonTreeElem = static_cast<JsonTreeElement*>(index.internalPointer());
    if (jsonTreeElem == nullptr) {
        return;
    }
    jsonTreeElem->value["isExpanded"] = isExpanded;
    if (jsonTreeElem->key.value("type", "") == "array") {
        emit dataChanged(index, index, { Qt::DisplayRole });
    }
}

/**
 * @brief Updates the hierarchy by applying a diff between root and m_root
 * @param root updated hierarchy
 */
void InspectorModel::setInspectorPath(const std::string& path)
{
    beginResetModel();
    m_data.clear();
    std::string substituted = reactor::gltf::substituteIndices(path);
    if (inspectorTemplates.contains(substituted)) {
        m_root = &inspectorTemplates.at(substituted);
    } else {
        m_root = nullptr;
    }
    refreshSubscriptions(path);
    endResetModel();
}

void InspectorModel::addSubscriptionRecursive(JsonTreeElement* elem, const std::string& path)
{
    if (elem == nullptr) {
        return;
    }
    auto patternPath = elem->path;
    // reconstruct a json pointer from the path pattern in the JsonTreeElem so that we
    // can access the corresponding data in the glTF
    auto substitutedPath = reactor::gltf::substituteIndices(path);
    auto elementPath = path + patternPath.substr(substitutedPath.size());

    bool isPointer = false;
    bool hasAdditionalValues = false;
    bool isMetaData = elem->key.contains("metaData");
    bool isAdditionalValue = elem->value.contains("additionalValue");
    std::string type = "";
    if (elem->value.contains("type")) {
        type = elem->value["type"];
        if (elem->value["type"] == "pointer") {
            isPointer = true;
        } else if (elem->value["type"] == "array") {
            if (elem->value["items"]["type"] == "pointer") {
                isPointer = true;
            }
        } else if (elem->value["type"] == "object") {
            hasAdditionalValues = (!elem->value.contains("additionalProperties") || !elem->value.at("additionalProperties").is_boolean() || elem->value.at("additionalProperties").get<bool>());
        }
    }

    auto extractValue = [patternPath, elementPath, isPointer, hasAdditionalValues, isMetaData, isAdditionalValue, type](const std::shared_ptr<const nlohmann::json>& headPtr) -> std::optional<nlohmann::json> {
        // Fill data
        auto pointer = nlohmann::json::json_pointer(elementPath);
        auto& gltf = headPtr->at("gltf");
        if (isMetaData) {
            if (type == "imagePreview") {
                auto index = pointer.parent_pointer().back();
                nlohmann::json::json_pointer imagePointer = nlohmann::json::json_pointer() / "images" / index;
                if (elementPath.starts_with("/textures")) {
                    imagePointer = nlohmann::json::json_pointer() / "textures" / index;
                }
                nlohmann::json imageData = reactor::utils::extractImagePreviewFromData(imagePointer, headPtr, model::ProjectManager::getInstance()->model.m_dataStorage);
                if (!imageData.empty()) {
                    return imageData;
                }
            }
        }
        if (gltf.contains(pointer)) {
            nlohmann::json value = gltf.at(pointer);

            if (isPointer) {
                // Extract UUID and localPath from the pointer
                nlohmann::json::json_pointer pointerToParentObject = pointer;
                std::string localPath = "";
                std::string uuid = "";
                if (!(value.is_array() && gltf.contains(pointerToParentObject / 0 / "uuid"))) {
                    while (!pointerToParentObject.empty() && !gltf.contains(pointerToParentObject / "uuid")) {
                        // Back will throw an exceptions if the pointer is empty. This should never happen but it stops the loop
                        localPath = "/" + pointerToParentObject.back() + localPath;
                        pointerToParentObject.pop_back();
                    }
                    uuid = gltf.at(pointerToParentObject).at("uuid");
                }
                if (value.is_array() && !value.empty()) {
                    for (int i = 0; i < value.size(); ++i) {
                        // if object is a pointer add uuid to data (needed for selections)
                        nlohmann::json tmp;
                        const auto& itemValue = gltf.at(pointer / i);
                        tmp["value"] = itemValue.is_string() ? itemValue.get<std::string>() : (pointer / i).to_string();
                        if (localPath != "") {
                            tmp["localPath"] = localPath + "/" + std::to_string(i);
                        } else {
                            tmp["localPath"] = localPath;
                        }
                        if (uuid == "" && gltf.contains(pointerToParentObject / i / "uuid")) {
                            tmp["uuid"] = gltf.at(pointerToParentObject / i / "uuid");
                        } else {
                            tmp["uuid"] = uuid;
                        }

                        // Extract a displayable name for the item
                        if (itemValue.is_string()) {
                            // The item is a reference to another property, extract the name from the referenced property
                            tmp["name"] = reactor::getDisplayName(gltf, nlohmann::json::json_pointer(itemValue));
                        } else {
                            // The item is a property, extract the name directly
                            tmp["name"] = reactor::getDisplayName(gltf, pointer / i);
                        }

                        nlohmann::json imageData = reactor::utils::extractImagePreviewFromData(pointer / i, headPtr, model::ProjectManager::getInstance()->model.m_dataStorage);
                        if (!imageData.empty()) {
                            tmp["textureSource"] = imageData["source"];
                            tmp["size"] = imageData["size"];
                        }

                        value.at(i) = tmp;
                    }
                } else {
                    nlohmann::json tmp;
                    tmp["value"] = value;
                    tmp["uuid"] = uuid;
                    tmp["localPath"] = localPath;
                    tmp["name"] = reactor::getDisplayName(gltf, nlohmann::json::json_pointer(value));
                    tmp["referenceCandidates"] = reactor::getReferenceCandidatesJson(gltf, nlohmann::json::json_pointer(pointer));
                    value = tmp;
                }
            } else {
                // Convert arrays of objects to arrays of pointers
                if (value.is_array() && !value.empty() && value.at(0).is_object() && !isAdditionalValue) {
                    for (int i = 0; i < value.size(); ++i) {
                        value.at(i) = (pointer / i).to_string();
                    }
                } else if (value.is_object()) {
                    if (!hasAdditionalValues) {
                        value = true;
                    }
                }
            }

            if (type == "pointer" && gltf.contains(pointer)) {
                const nlohmann::json::json_pointer indexPointer(gltf.at(pointer).get<std::string>());
                nlohmann::json imageData = reactor::utils::extractImagePreviewFromData(indexPointer, headPtr, model::ProjectManager::getInstance()->model.m_dataStorage);
                if (!imageData.empty()) {
                    value["textureSource"] = imageData["source"];
                    value["size"] = imageData["size"];
                }
            }

            return value;
        }

        // The pointer is undefined, use the schema to find reference candidates
        if (isPointer) {

            const nlohmann::json& linearizedSchema = model::ProjectManager::getInstance()->model.m_linearizedSchema;
            const auto referencedPattern = linearizedSchema.at(patternPath);
            if (!referencedPattern.contains("ux3d_pathPrefix")) {
                return std::nullopt;
            }
            auto referenceCandidates = nlohmann::json::array();

            reactor::json::for_each_match(std::string(referencedPattern.at("ux3d_pathPrefix")) + "/\\d+", gltf, [&referenceCandidates, &gltf](const auto& pointer) {
                std::string name = reactor::getDisplayName(gltf, pointer);
                referenceCandidates.push_back({ { "path", pointer.to_string() },
                    { "name", name } });
            });
            nlohmann::json value;
            value["referenceCandidates"] = referenceCandidates;
            return value;
        }

        return std::nullopt;
    };

    auto subscription = model::ProjectManager::getInstance()->model.headShared
                            // extract the data for the pointer from the head
                            .map(extractValue)
                            // don't emit a dataChanged signal if nothing has changed
                            .distinct_until_changed()
                            .observe_on(model::ProjectManager::getInstance()->uiWorker)
                            .subscribe(
                                [patternPath, elementPath, this, hasAdditionalValues, path, isMetaData](const std::optional<nlohmann::json>& data) {
                                    auto elem = m_root->getPropertyRecursive(nlohmann::json::json_pointer(patternPath));

                                    if (elem == nullptr) {
                                        throw std::runtime_error("Inspector item does not exist");
                                    }

                                    QModelIndex changedIndex;
                                    int row = 0;
                                    if (elem->parent != nullptr) {
                                        row = elem->parent->getIndex(elem->path);
                                    }
                                    if (elementPath != path) {
                                        changedIndex = createIndex(row, 0, elem);
                                    }
                                    std::string lookUpPath = elementPath;
                                    if (isMetaData) {
                                        lookUpPath = "metaData" + elementPath;
                                    }
                                    std::vector<JsonTreeElement*> additionalSubscriptions;
                                    if (data) {
                                        if (hasAdditionalValues && (data.value().is_object() || (elem->value["type"] == "object"))) {
                                            // Collect  all new items not in schema
                                            int i = 0;
                                            for (auto& [key, value] : data.value().items()) {
                                                if (key == "uuid" || key == "ux3d_gestaltor") {
                                                    continue;
                                                }
                                                if (elem->getIndex(elem->path + "/" + key) == -1) {
                                                    nlohmann::json schema;
                                                    nlohmann::json jsonKey;
                                                    if (elem->value.contains("additionalProperties") && elem->value["additionalProperties"].is_object()) {
                                                        // Use schema of addtionalProperties if possible
                                                        collectProperties(elem->path + "/" + key, elem->value["additionalProperties"], jsonKey, schema);
                                                    }
                                                    if (!schema.contains("type")) {
                                                        // Deduct type from value
                                                        schema["type"] = value.type_name();
                                                        if (schema["type"] == "array") {
                                                            // Displays unkown arrays as objects with indicies as keys
                                                            schema["type"] = "object";
                                                        }
                                                        schema["operations"] = nlohmann::json::object();
                                                    }
                                                    std::string type = schema["type"];
                                                    if (type != "object" && type != "array" && type != "pointer") {
                                                        schema["operations"]["dangerous.setValue"] = true;
                                                    }
                                                    schema["additionalValue"] = true;
                                                    if (type != "readonly") {
                                                        schema["operations"]["dangerous.removeValue"] = true;
                                                    }
                                                    jsonKey["type"] = schema["type"];
                                                    jsonKey["title"] = key;
                                                    // Put unknown values last
                                                    additionalSubscriptions.emplace_back(&(elem->addProperty(elem->path + "/" + key, jsonKey, schema, i)));
                                                    --i;
                                                } else {
                                                }
                                            }
                                            // Add them to model
                                            if (!additionalSubscriptions.empty()) {
                                                elem->sortProperties();
                                                size_t firstElemIndex = elem->getIndex(additionalSubscriptions.front()->path);
                                                beginInsertRows(changedIndex, firstElemIndex, firstElemIndex + additionalSubscriptions.size() - 1);
                                                endInsertRows();
                                            }
                                        }
                                        m_values[lookUpPath] = data.value(); // Cache data
                                        for (auto item : additionalSubscriptions) {
                                            addSubscriptionRecursive(item, path);
                                        }
                                    } else {
                                        if (m_values.contains(lookUpPath)) {
                                            m_values.erase(lookUpPath);
                                        }
                                        if (elem->value.contains("additionalValue")) {
                                            m_subscriptions.at(elem->path).unsubscribe();
                                            m_subscriptions.erase(elem->path);
                                            auto parentIndex = changedIndex.parent();
                                            if (elem->parent != nullptr) {
                                                row = elem->parent->getIndex(elem->path);
                                            }
                                            beginRemoveRows(parentIndex, row, row);
                                            elem->parent->removeProperty(elem);
                                            endRemoveRows();
                                            return;
                                        }
                                    }
                                    emit dataChanged(changedIndex, changedIndex, { Qt::DisplayRole });
                                },
                                [](std::exception_ptr ep) {
                                    try {
                                        std::rethrow_exception(ep);
                                    } catch (const std::exception& ex) {
                                        core::debugLog(core::LogLevel::Error, "%s", ex.what());
                                    }
                                });

    for (auto iter = elem->begin(); elem->end() != iter; ++iter) {
        addSubscriptionRecursive(&std::get<1>(*iter), path);
    }
    m_subscriptions.emplace(elem->path, subscription);
}

void InspectorModel::refreshSubscriptions(const std::string& path)
{
    for (auto& [_, subscription] : m_subscriptions) {
        subscription.unsubscribe();
    }
    m_subscriptions.clear();

    std::string substitutedPath = reactor::gltf::substituteIndices(path);

    if (m_root != nullptr) {
        addSubscriptionRecursive(m_root, path);
    }
}

void InspectorModel::collectProperties(const std::string& pointer, const nlohmann::json& value, nlohmann::json& jsonKey, nlohmann::json& jsonValue) const
{
    jsonValue["operations"] = nlohmann::json::object();
    if (this->operations.contains("/gltf" + pointer)) {
        jsonValue["operations"] = this->operations.at("/gltf" + pointer);
    }

    if (value.contains("description")) {
        jsonKey["tooltip"] = value["description"];
        jsonValue["tooltip"] = value["description"];
    }
    if (value.contains("gltf_detailedDescription")) {
        jsonKey["gltf_detailedDescription"] = value["gltf_detailedDescription"];
        jsonValue["gltf_detailedDescription"] = value["gltf_detailedDescription"];
    }
    if (value.contains("type")) {
        jsonValue["type"] = value["type"];
    }
    if (value.contains("anyOf")) {
        // Handle enum
        jsonValue["type"] = "enum";
        nlohmann::json possibleValuesAsText = nlohmann::json::array();
        nlohmann::json possibleValues = nlohmann::json::array();
        for (auto& anyOfItem : value["anyOf"]) {
            possibleValues.push_back(anyOfItem.at("/enum/0"_json_pointer));
            if (!possibleValues.back().is_string() && anyOfItem.contains("description")) {
                possibleValuesAsText.push_back(anyOfItem.at("description"));
            }
        }
        jsonValue["options"] = possibleValues;
        if (!possibleValuesAsText.empty()) {
            jsonValue["optionsAsText"] = possibleValuesAsText;
        }
    }
    if (value.contains("minItems")) {
        jsonValue["minItems"] = value["minItems"];
    }
    if (value.contains("maxItems")) {
        jsonValue["maxItems"] = value["maxItems"];
    }
    if (value.contains("minItems") && value.contains("maxItems") && value["minItems"] == value["maxItems"]) {
        int itemCount = value["minItems"];
        if (itemCount == 9 || itemCount == 16) {
            jsonValue["type"] = "matrix";
        } else if (itemCount == 3 || itemCount == 4) {
            if (value.contains("description") && value["description"].get<std::string>().find("color") != std::string::npos || value.contains("tooltip") && value["tooltip"].get<std::string>().find("color") != std::string::npos || value.contains("description") && value["description"].get<std::string>().find("Color") != std::string::npos || value.contains("gltf_detailedDescription") && value["gltf_detailedDescription"].get<std::string>().find("color") != std::string::npos || value.contains("gltf_detailedDescription") && value["gltf_detailedDescription"].get<std::string>().find("Color") != std::string::npos || value.contains("tooltip") && value["tooltip"].get<std::string>().find("Color") != std::string::npos) {
                jsonValue["type"] = "color";
            } else {
                jsonValue["type"] = "vector";
            }
        } else {
            jsonValue["type"] = "vector";
        }
    }

    if (value.contains("minimum")) {
        jsonValue["minimum"] = value["minimum"];
    }
    if (value.contains("maximum")) {
        jsonValue["maximum"] = value["maximum"];
    }
    if (value.contains("multipleOf")) {
        jsonValue["multipleOf"] = value["multipleOf"];
    }
    if (value.contains("format")) {
        jsonValue["format"] = value["format"];
    }
    if (value.contains("default")) {
        jsonValue["default"] = value["default"];
    }
    if (value.contains("title") && value.at("title") == "glTF Id") {
        jsonValue["type"] = "pointer";
    }
    if (value.contains("ux3d_pathPrefix")) {
        jsonValue["icon"] = value["ux3d_pathPrefix"];
        jsonValue["type"] = "pointer";
    }
    if (value.contains("properties")) {
        int childCount = value["properties"].size();
        jsonValue["childCount"] = childCount;
        for (auto& [key, value] : value.at("properties").items()) {
            // Only save names of properties
            jsonValue["properties"][key] = true;
        }
    }

    if (value.contains("additionalProperties")) {
        jsonValue["additionalProperties"] = value["additionalProperties"];
    }

    if (value.contains("allOf")) {
        for (auto& allOfItem : value["allOf"]) {
            collectProperties(pointer, allOfItem, jsonKey, jsonValue);
        }
    }

    std::string type = jsonValue["type"];
    if (value.contains("ux3d_generateSetter") && !value["ux3d_generateSetter"].get<bool>() && type != "matrix" && type != "object" && type != "array" && type != "pointer") {
        jsonValue["type"] = "readonly";
    }
    jsonKey["type"] = jsonValue["type"];
}

void InspectorModel::transposeMatrix(nlohmann::json& array) const
{
    if (array.empty() && array.is_array() && (array.size() != 9 || array.size() != 16)) {
        return;
    }
    if (array.size() == 9) {
        auto v00 = array.at(0);
        auto v01 = array.at(1);
        auto v02 = array.at(2);
        auto v10 = array.at(3);
        auto v11 = array.at(4);
        auto v12 = array.at(5);
        auto v20 = array.at(6);
        auto v21 = array.at(7);
        auto v22 = array.at(8);

        array.at(0) = v00;
        array.at(1) = v10;
        array.at(2) = v20;
        array.at(3) = v01;
        array.at(4) = v11;
        array.at(5) = v21;
        array.at(6) = v02;
        array.at(7) = v12;
        array.at(8) = v22;
    } else if (array.size() == 16) {
        auto v00 = array.at(0);
        auto v01 = array.at(1);
        auto v02 = array.at(2);
        auto v03 = array.at(3);
        auto v10 = array.at(4);
        auto v11 = array.at(5);
        auto v12 = array.at(6);
        auto v13 = array.at(7);
        auto v20 = array.at(8);
        auto v21 = array.at(9);
        auto v22 = array.at(10);
        auto v23 = array.at(11);
        auto v30 = array.at(12);
        auto v31 = array.at(13);
        auto v32 = array.at(14);
        auto v33 = array.at(15);

        array.at(0) = v00;
        array.at(1) = v10;
        array.at(2) = v20;
        array.at(3) = v30;
        array.at(4) = v01;
        array.at(5) = v11;
        array.at(6) = v21;
        array.at(7) = v31;
        array.at(8) = v02;
        array.at(9) = v12;
        array.at(10) = v22;
        array.at(11) = v32;
        array.at(12) = v03;
        array.at(13) = v13;
        array.at(14) = v23;
        array.at(15) = v33;
    }
}
