// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Allocator.h"
#include <new> // placement new

namespace ux3d {
namespace core {
    template <class T>
    struct LinkedListCell {
        T m_Data;
        LinkedListCell* m_pNext = nullptr;
    };

    template <class T, class... Args>
    LinkedListCell<T>* makeLinkedListCell(IAllocator* allocator, Args&&... args)
    {
        auto* ptr = reinterpret_cast<LinkedListCell<T>*>(
            allocator->allocate(sizeof(LinkedListCell<T>), alignof(LinkedListCell<T>)));
        if (ptr != nullptr) {
            ptr->m_pNext = nullptr;

            if constexpr (std::is_constructible_v<T, Args&&...>) {
                new (&ptr->m_Data) T(std::forward<Args>(args)...);
            } else {
                new (&ptr->m_Data) T { std::forward<Args>(args)... };
            }
        }
        return ptr;
    }
}
}
