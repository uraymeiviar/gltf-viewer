// Copyright © 2023 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <list>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

namespace ux3d::core::tree {

struct NodePointer {
    std::vector<size_t> segments;
    size_t back() { return this->segments.back(); }
    NodePointer parent()
    {
        NodePointer parentPointer;
        parentPointer.segments = this->segments;
        parentPointer.segments.pop_back();
        return parentPointer;
    }
    bool empty() { return this->segments.empty(); }
};

template <typename T>
struct Node {
    std::list<Node<T>> children;
    Node<T>* parent = nullptr;
    T data;

    Node<T>& at(NodePointer pointer)
    {
        Node<T>* node = this;
        for (auto& index : pointer.segments) {
            auto childrenIt = node->children.begin();
            std::advance(childrenIt, index);
            if (childrenIt == node->children.end()) {
                throw std::runtime_error("Node pointer invalid for the tree");
            }
            node = &(*childrenIt);
        }
        return *node;
    }
};

template <typename T>
using Tree = Node<T>;
}
