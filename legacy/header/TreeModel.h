#pragma once

#include "core/Tree.h"
#include <QAbstractItemModel>
#include <QModelIndex>
#include <nlohmann/json.hpp>
#include <set>

namespace ux3d::editor {

/**
 * @brief TreeModel provides a base class for models that can be used with the
 * Qt Quick TreeView plugin and use JSON as data source
 *
 */
class TreeModel : public QAbstractItemModel {
    Q_OBJECT
public:
    struct NodeData {
        bool showChildren = false;
        nlohmann::json json;
    };
    using Node = core::tree::Node<NodeData>;

    TreeModel(QObject* parent = nullptr);

    void updateTree(const nlohmann::json& treeModel);

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    Node* nodeFromIndex(const QModelIndex& parent);
    const Node* nodeFromIndex(const QModelIndex& parent) const;

    QModelIndex index(const Node& node, int column) const;

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& index) const override;

    void fetchMore(const QModelIndex& parent) override;
    bool canFetchMore(const QModelIndex& parent) const override;

    bool hasChildren(const QModelIndex& parent) const override;

    void insert(Node& parent, int row, const nlohmann::json& data);
    void remove(Node& parent, int row);
    // It's not allow to insert / remove rows using the QAbstractItemModel interface
    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) final
    {
        throw std::logic_error("insertRows is not allowed on TreeModel");
    }
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) final
    {
        throw std::logic_error("removeRows is not allowed on TreeModel");
    }

signals:
    void itemPushed(const Node& node);
    void itemPopped(const Node& node);

protected:
    nlohmann::json treeData;
    Node root;
};

}
