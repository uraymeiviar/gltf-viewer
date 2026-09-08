#include "TreeModel.h"

#include "reactor/Json.h"
#include <iostream>

using namespace ux3d;
using namespace ux3d::editor;

TreeModel::TreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    this->treeData = nlohmann::json::object({ { "children", nlohmann::json::array() } });
    this->root.data.showChildren = true;
}

core::tree::NodePointer fromJsonPointer(nlohmann::json::json_pointer jsonPointer)
{
    std::stack<size_t> stack;
    while (!jsonPointer.empty()) {
        stack.push(std::stoi(jsonPointer.back()));
        jsonPointer = jsonPointer.parent_pointer().parent_pointer();
    }

    core::tree::NodePointer nodePointer;
    while (!stack.empty()) {
        nodePointer.segments.push_back(stack.top());
        stack.pop();
    }

    return nodePointer;
}

void TreeModel::updateTree(const nlohmann::json& updatedTreeData)
{
    auto diff = reactor::json::tree_diff(this->treeData, updatedTreeData);

    for (const auto& difference : diff) {
        // extract the path and resolve escaped characters to obtain a json pointer for
        // the non flat tree
        auto pointer = nlohmann::json::json_pointer(difference["path"]);
        auto treePointer = fromJsonPointer(pointer);
        if (treePointer.empty()) {
            throw std::runtime_error("Empty diff path encountered");
        }

        auto parentPointer = treePointer.parent();
        auto& parentNode = this->root.at(parentPointer);
        int parentRow = 0;
        if (!parentPointer.empty()) {
            parentRow = static_cast<int>(parentPointer.back());
        }

        if (difference["op"] == "add") {
            int row = std::stoi(pointer.back());
            insert(parentNode, row, difference["value"]);
        }

        else if (difference["op"] == "remove") {
            int row = std::stoi(pointer.back());
            remove(parentNode, row);
        }
    }

    this->treeData = updatedTreeData;
}

int TreeModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

int TreeModel::rowCount(const QModelIndex& parent) const
{
    const Node* parentNode = nodeFromIndex(parent);

    if (parentNode->data.showChildren == false) {
        return 0;
    }

    return static_cast<int>(parentNode->children.size());
}

TreeModel::Node* TreeModel::nodeFromIndex(const QModelIndex& index)
{
    Node* node;
    if (!index.isValid()) {
        node = &this->root;
    } else {
        node = reinterpret_cast<Node*>(index.internalPointer());
    }
    return node;
}

const TreeModel::Node* TreeModel::nodeFromIndex(const QModelIndex& index) const
{
    const Node* node;
    if (!index.isValid()) {
        node = &this->root;
    } else {
        node = reinterpret_cast<Node*>(index.internalPointer());
    }
    return node;
}

QModelIndex TreeModel::index(const Node& node, int column) const
{
    if (&node == &this->root) {
        return QModelIndex();
    }

    auto it = std::find_if(node.parent->children.begin(), node.parent->children.end(), [&node](const auto& child) {
        return &child == &node;
    });

    if (it == node.parent->children.end()) {
        throw std::runtime_error("Node not found in its parents");
    }

    int row = static_cast<int>(std::distance(node.parent->children.begin(), it));
    return createIndex(row, column, &node);
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex& parent) const
{
    const Node* parentNode = nodeFromIndex(parent);

    auto childIterator = parentNode->children.begin();
    if (row < 0 || row >= static_cast<int>(parentNode->children.size())) {
        return QModelIndex();
    }
    std::advance(childIterator, row);
    auto* node = &(*childIterator);

    return createIndex(row, column, node);
}

QModelIndex TreeModel::parent(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }

    auto* node = reinterpret_cast<Node*>(index.internalPointer());
    auto* parent = node->parent;

    if (parent == &this->root) {
        return QModelIndex();
    }

    const auto grandparent = node->parent->parent;
    auto it = std::find_if(grandparent->children.begin(), grandparent->children.end(), [&parent](const auto& child) {
        return &child == parent;
    });
    if (it == grandparent->children.end()) {
        throw std::runtime_error("Node not found in its parents");
    }
    int row = static_cast<int>(std::distance(grandparent->children.begin(), it));

    return createIndex(row, index.column(), parent);
}

void TreeModel::fetchMore(const QModelIndex& parent)
{
    Node* node = nodeFromIndex(parent);

    if (node->children.empty() || node->data.showChildren) {
        return;
    }

    beginInsertRows(parent, 0, static_cast<int>(node->children.size()) - 1);
    node->data.showChildren = true;
    endInsertRows();

    for (auto& child : node->children) {
        emit itemPushed(child);
    }
}

bool TreeModel::canFetchMore(const QModelIndex& parent) const
{
    const Node* node = nodeFromIndex(parent);

    return !node->data.showChildren && !node->children.empty();
}

bool TreeModel::hasChildren(const QModelIndex& parent) const
{
    const Node* node = nodeFromIndex(parent);

    return !node->children.empty();
}

void TreeModel::insert(Node& parentNode, int row, const nlohmann::json& value)
{
#ifndef NDEBUG
    if (value.contains("children") && !value["children"].empty()) {
        throw std::invalid_argument("TreeModel insert encountered value containing 'children' key. Children should be added with their own insert calls instead.");
    }
#endif

    auto parent = index(parentNode, 0);

    if (parentNode.data.showChildren) {
        beginInsertRows(parent, row, row);
    }

    std::list<Node>::iterator insertIterator = parentNode.children.begin();
    std::advance(insertIterator, row);
    auto newNodeIt = parentNode.children.emplace(insertIterator);
    newNodeIt->parent = &parentNode;
    newNodeIt->data = {
        .showChildren = false,
        .json = value,
    };

    if (parentNode.data.showChildren) {
        endInsertRows();
        emit itemPushed(*newNodeIt);
    }
}

void TreeModel::remove(Node& parentNode, int row)
{
    auto parent = index(parentNode, 0);

    if (parentNode.data.showChildren) {
        beginRemoveRows(parent, row, row);
    }

    auto removeIterator = parentNode.children.begin();
    std::advance(removeIterator, row);
    emit itemPopped(*removeIterator);
    parentNode.children.erase(removeIterator);
    if (parentNode.data.showChildren) {
        endRemoveRows();
    }
}
