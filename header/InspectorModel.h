#pragma once

#include "JsonTreeElement.h"

#include <QAbstractItemModel>
#include <QJsonObject>
#include <QModelIndex>
#include <QVariant>

#include <map>
#include <vector>

#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

namespace ux3d::editor {

class InspectorModel : public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY(QString path WRITE setPath READ getPath NOTIFY pathChanged)

public:
    enum TreeModelRoles {
        TreeModelRoleName = Qt::UserRole + 1
    };

    InspectorModel(QObject* parent = nullptr);
    InspectorModel(const InspectorModel&) = delete;
    ~InspectorModel() override;

    // QAbstractItemModel interface start
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    // bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;
    // QAbstractItemModel interface end

    void setPath(const QString& path);
    QString getPath();

    Q_INVOKABLE void setIsRowExpanded(const bool isExpanded, const QModelIndex& index);
signals:
    void pathChanged();

private:
    // Map of default inspector panels. std::tuple is path to parent, key/value pair from gltf
    std::unordered_map<std::string, JsonTreeElement> inspectorTemplates;

    QString m_path;
    mutable JsonTreeElement* m_root = nullptr; // inpsector structure
    std::map<std::string, nlohmann::json> m_values;

    mutable std::map<std::string, nlohmann::json> m_data; // data cache
    mutable std::map<std::string, rxcpp::subscription> m_subscriptions; // data subscriptions

    nlohmann::json operations;
    nlohmann::json validationResults;
    rxcpp::subscription validationSubscription;

    void setInspectorPath(const std::string& path);

    // rxcpp::subscription m_subscription;
    // rxcpp::subscription m_newGltfSubscription;
    void refreshSubscriptions(const std::string& path);
    void collectProperties(const std::string& pointer, const nlohmann::json& value, nlohmann::json& resultKey, nlohmann::json& resultValue) const;
    void addSubscriptionRecursive(JsonTreeElement* elem, const std::string& path);

    /**
     * @brief transposes the input array in place
     *
     * @param array
     */
    void transposeMatrix(nlohmann::json& array) const;
};
}
