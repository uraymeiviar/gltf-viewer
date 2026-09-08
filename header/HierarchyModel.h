#pragma once

#include <QAbstractItemModel>
#include <QJsonObject>
#include <QModelIndex>
#include <QVariant>

#include <map>
#include <vector>

#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

// Hierarchy Model Testing
// Enable the QAbstractItemModelTester on this model.
// This can be helpful when debugging the model but comes
// with a severe performance penalty, so only enable it
// when needed.
#ifndef NDEBUG
//#define HIERARCHY_MODEL_TESTING
#endif
#ifdef HIERARCHY_MODEL_TESTING
#include <QAbstractItemModelTester>
#endif

#include "TreeModel.h"

namespace ux3d::editor {

/**
 * @brief HierarchyModel is an QAbstractItemModel which uses JSON as input and patches as updates
 * The structure of the input JSON is stored in m_root and has to look like this:
 * {
       children: [
            {
                children: []
                id: ""
            }
        ],
        id: ""
    }
 * In the hierarchy the ID consists of the UUID + local path.
 * If the item is an object, local path is empty. If it is a reference, the UUID of the parent is used together with the gltf path towards the child.
 * e.g. camera -> UUID of node + /camera
 *      root node -> UUID of scene + /nodes/0
 *      child node -> UUID of parent node + /children/0
 *      primitive -> UUID only (is not a reference)
 * 
 * The data is updated/generated via subscriptions, which are created for each ID. Data is cached in m_data.
 * QModelIndex consists of a parent pointer, row and column (column is always 0)
 * As parent pointer a pointer to a nlohmann::json::json_pointer is used. The pointers are stored in m_paths.
 * They point towards the parent in root (e.g. /children/0/children/1 is the parent of /children/0/children/1/children/0)
*/

class HierarchyModel : public TreeModel {

    Q_OBJECT
    Q_PROPERTY(QJsonObject hierarchy WRITE setHierarchy READ getHierarchy NOTIFY hierarchyChanged) //To access the hierarchy from QML

public:
    enum TreeModelRoles {
        TreeModelRoleName = Qt::UserRole + 1
    };

    HierarchyModel(QObject* parent = nullptr);
    ~HierarchyModel();

    // QAbstractItemModel interface start

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    // QAbstractItemModel interface end
    void setHierarchy(const QJsonObject& hierarchy);
    void setHierarchy(const nlohmann::json& hierarchy);

    Q_INVOKABLE static QJsonObject filter(const QJsonObject& gltf, const QJsonObject& input, const QString& filter);
    Q_INVOKABLE void init(const QString& observable);

    QJsonObject getHierarchy() const;

signals:
    void hierarchyChanged();
    void addSubscriber(const nlohmann::json::json_pointer& treePath);

private:
    enum class ChangedItemType {
        Array = 0,
        Object,
        String
    };

    std::map<uintptr_t, nlohmann::json> m_data; // data cache
    std::map<uintptr_t, rxcpp::subscription> m_subscriptions; // data subscriptions

    static bool filterHelper(const nlohmann::json& gltf, nlohmann::json& target, const std::string& filter, const std::string& currentPos);

    void addRecursive(const nlohmann::json::json_pointer& path, const QModelIndex& parent, int row);
    void removeRecursive(const nlohmann::json::json_pointer& path, const QModelIndex& parent, int row);
    void removeSubscriber(const QModelIndex& parent, int start, int end);

    rxcpp::subscription m_subscription;
    rxcpp::subscription m_newGltfSubscription;

#ifdef HIERARCHY_MODEL_TESTING
    const QAbstractItemModelTester tester;
#endif
};
}
