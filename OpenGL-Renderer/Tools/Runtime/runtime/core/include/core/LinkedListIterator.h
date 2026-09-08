// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "LinkedListCell.h"
#include <iterator> // forward_iterator_tag

namespace ux3d {
namespace core {
    template <class T>
    class LinkedListIterator {
        template <class LinkedListT>
        friend class LinkedList;
        LinkedListCell<T>* m_pCurrent;

    public:
        using value_type = T;
        using iterator_category = std::forward_iterator_tag;
        using pointer = T*;
        using reference = T&;
        using difference_type = std::ptrdiff_t;

        //using const_iterator = LinkedListIterator<const typename std::remove_const<T>::type>;

        constexpr LinkedListIterator(const LinkedListIterator& _other)
            : m_pCurrent(_other.m_pCurrent)
        {
        }

        constexpr LinkedListIterator(LinkedListIterator&& _other) noexcept
            : m_pCurrent(_other.m_pCurrent)
        {
            _other.m_pCurrent = nullptr;
        }

        constexpr LinkedListIterator(LinkedListCell<T>* _pCurrent = nullptr)
            : m_pCurrent(_pCurrent)
        {
        }

        LinkedListIterator& operator=(const LinkedListIterator& _other)
        {
            m_pCurrent = _other.m_pCurrent;
            return *this;
        }

        LinkedListIterator& operator=(LinkedListIterator&& _other) noexcept
        {
            m_pCurrent = _other.m_pCurrent;
            _other.m_pCurrent = nullptr;
            return *this;
        }

        //constexpr operator const_iterator() const { return const_iterator(m_pCurrent); }
        //constexpr const_iterator getConst() const { return const_iterator(m_pCurrent); }

        LinkedListIterator& operator++()
        {
            m_pCurrent = m_pCurrent->m_pNext;
            return *this;
        }

        LinkedListIterator<T> operator++(int)
        {
            LinkedListIterator<T> ret(m_pCurrent);
            m_pCurrent = m_pCurrent->m_pNext;
            return ret;
        }

        T& operator*()
        {
            return m_pCurrent->m_Data;
        }

        const T& operator*() const
        {
            return m_pCurrent->m_Data;
        }

        constexpr LinkedListIterator operator+(const size_t n) const
        {
            LinkedListIterator result(m_pCurrent);
            for (size_t i = 0; i < n; ++i) {
                ++result;
            }
            return result;
        }

        constexpr bool operator==(const LinkedListIterator& _Other) const
        {
            return m_pCurrent == _Other.m_pCurrent;
        }

        constexpr bool operator!=(const LinkedListIterator& rhs) const
        {
            return m_pCurrent != rhs.m_pCurrent;
        }

        constexpr bool operator==(const std::nullptr_t) const
        {
            return m_pCurrent == nullptr;
        }

        constexpr bool operator!=(const std::nullptr_t) const
        {
            return m_pCurrent != nullptr;
        }

        constexpr LinkedListCell<T>* operator->()
        {
            return m_pCurrent;
        }

        constexpr const LinkedListCell<T>* operator->() const
        {
            return m_pCurrent;
        }
    };
} // !core
} // !ux3d

namespace std {
template <class T>
struct iterator_traits<ux3d::core::LinkedListIterator<T>> {
    using value_type = typename ux3d::core::LinkedListIterator<T>::value_type;
    using difference_type = typename ux3d::core::LinkedListIterator<T>::difference_type;
    using pointer = typename ux3d::core::LinkedListIterator<T>::pointer;
    using reference = typename ux3d::core::LinkedListIterator<T>::reference;
    using iterator_category = typename ux3d::core::LinkedListIterator<T>::iterator_category;
};
}
