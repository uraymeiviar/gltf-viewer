// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
// Created by jim on 14.05.19.
//

#pragma once

#include "Allocator.h"
#include "HeapAllocator.h"
#include "LinkedListCell.h"
#include "LinkedListIterator.h"
#include "Traits.h"

namespace ux3d {
namespace core {
    template <class T>
    class LinkedList {
    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;

        using allocator_type = IAllocator;
        using iterator = LinkedListIterator<T>;

        LinkedList(IAllocator* _Allocator = HeapAllocator::defaultInstance())
            : m_pAllocator(_Allocator)
        {
        }

        LinkedList(const LinkedList& _other);
        LinkedList& operator=(const LinkedList& _other);

        LinkedList(LinkedList&& _other) noexcept;
        LinkedList& operator=(LinkedList&& _other) noexcept;

        virtual ~LinkedList();

        T& front();
        const T& front() const;

        T& back();
        const T& back() const;

        iterator erase(iterator pos);

        void clear();

        size_t size() const;
        bool empty() const;

        T& at(size_t n);
        const T& at(size_t n) const;

        /// Copies cells of rhs into this list.
        /// The current cells are retained
        /// The newly created cells call the copy constructor on the new elements.
        /// No data is shared between this and the other list afterwards,
        /// so no destructor is called twice on an object because both lists get destructed.
        bool concat(const LinkedList<T>& rhs);

        // TODO: Use concat() overload with r-value ref?
        /// Moves the elements from the given list into this own.
        /// This function does not need to make any allocations,
        /// to call move constructors.
        void take(LinkedList<T>& rhs);

        template <class... Args>
        T* emplace(LinkedListIterator<T> _pos, Args&&... _args);

        template <class... Args>
        T* emplace_back(Args&&... args);

        iterator begin() const noexcept { return iterator(m_pBegin); }
        //const_iterator begin() const noexcept { return iterator(m_pBegin); }
        //const_iterator cbegin() const noexcept { return const_iterator(m_pBegin); }

        iterator end() const noexcept { return iterator(nullptr); }
        //const_iterator end() const noexcept { return const_iterator(nullptr); }
        //const_iterator cend() const noexcept { return const_iterator(nullptr); }

    private:
        IAllocator* m_pAllocator = nullptr;
        LinkedListCell<T>* m_pBegin = nullptr;
        LinkedListCell<T>* m_pEnd = nullptr;
        size_t m_Elements = 0u;
    };

    template <class T>
    inline LinkedList<T>::LinkedList(const LinkedList& _other)
        : m_pAllocator(_other.m_pAllocator)
    {
        concat(_other);
    }

    template <class T>
    inline LinkedList<T>& LinkedList<T>::operator=(const LinkedList& _other)
    {
        if (this != &_other) {
            clear();
            concat(_other);
        }
        return *this;
    }

    template <class T>
    inline LinkedList<T>::LinkedList(LinkedList&& _other) noexcept
        : m_pAllocator(_other.m_pAllocator)
        , m_pBegin(_other.m_pBegin)
        , m_pEnd(_other.m_pEnd)
        , m_Elements(_other.m_Elements)
    {
        _other.m_pAllocator = nullptr;
        _other.m_pBegin = nullptr;
        _other.m_pEnd = nullptr;
        _other.m_Elements = 0u;
    }

    template <class T>
    inline LinkedList<T>& LinkedList<T>::operator=(LinkedList&& _other) noexcept
    {
        if (this != &_other) {
            clear();

            m_pAllocator = _other.m_pAllocator;
            m_pBegin = _other.m_pBegin;
            m_pEnd = _other.m_pEnd;
            m_Elements = _other.m_Elements;

            _other.m_pAllocator = nullptr;
            _other.m_pBegin = nullptr;
            _other.m_pEnd = nullptr;
            _other.m_Elements = 0u;
        }

        return *this;
    }

    template <class T>
    inline LinkedList<T>::~LinkedList()
    {
        clear();
        m_pAllocator = nullptr;
    }

    template <class T>
    bool LinkedList<T>::concat(const LinkedList<T>& rhs)
    {
        for (const T& el : rhs) {
            if (!emplace_back(el)) {
                return false;
            }
        }
        return true;
    }

    template <class T>
    void LinkedList<T>::take(LinkedList<T>& rhs)
    {
        if (!empty()) {
            m_pEnd->m_pNext = rhs.m_pBegin;
            m_pEnd = rhs.m_pEnd;
        } else {
            m_pBegin = rhs.m_pBegin;
            m_pEnd = rhs.m_pEnd;
        }

        rhs.m_pBegin = nullptr;
        rhs.m_pEnd = nullptr;
        m_Elements += rhs.m_Elements;
        rhs.m_Elements = 0;
    }

    template <class T>
    inline T& LinkedList<T>::front()
    {
        return m_pBegin->m_Data;
    }

    template <class T>
    inline const T& LinkedList<T>::front() const
    {
        return m_pBegin->m_Data;
    }

    template <class T>
    inline T& LinkedList<T>::back()
    {
        return m_pEnd->m_Data;
    }

    template <class T>
    inline const T& LinkedList<T>::back() const
    {
        return m_pEnd->m_Data;
    }

    template <class T>
    typename LinkedList<T>::iterator LinkedList<T>::erase(iterator pos)
    {
        if (m_pBegin != nullptr && m_pAllocator != nullptr) {
            LinkedListCell<T>* pParentCell = nullptr;
            LinkedListCell<T>* pCell = m_pBegin;
            LinkedListCell<T>* pTarget = pos.operator->();

            //search for cell to be erased
            while (pCell != nullptr && pCell != pTarget) {
                pParentCell = pCell;
                pCell = pCell->m_pNext;
            }
            if (pCell == nullptr) {
                return nullptr;
            }

            //check if first cell should be erased
            if (pParentCell == nullptr) {
                m_pBegin = (m_Elements > 1) ? pCell->m_pNext : nullptr;
            } else {
                pParentCell->m_pNext = pCell->m_pNext;
            }
            if (pCell->m_pNext == nullptr) {
                m_pEnd = pParentCell;
            }

            destructIfExists<T>(&pCell->m_Data);
            deallocateIfExists(m_pAllocator, reinterpret_cast<void*>(pCell), sizeof(LinkedListCell<T>));
            --m_Elements;
            return LinkedListIterator<T>((pParentCell != nullptr) ? pParentCell->m_pNext : m_pBegin);
        }
        return nullptr;
    }

    template <class T>
    void LinkedList<T>::clear()
    {
        if (m_pBegin != nullptr && m_pAllocator != nullptr) {
            LinkedListCell<T>* pCell = m_pBegin;
            while (pCell != nullptr) {
                LinkedListCell<T>* pNext = pCell->m_pNext;
                destructIfExists<T>(&pCell->m_Data);
                deallocateIfExists(m_pAllocator, reinterpret_cast<void*>(pCell), sizeof(LinkedListCell<T>));
                pCell = pNext;
            }

            m_Elements = 0u;
        }

        m_pBegin = nullptr;
        m_pEnd = nullptr;
    }

    template <class T>
    inline size_t LinkedList<T>::size() const
    {
        return m_Elements;
    }

    template <class T>
    inline bool LinkedList<T>::empty() const
    {
        return m_Elements == 0u;
    }

    template <class T>
    inline T& LinkedList<T>::at(const size_t n)
    {
        return *(begin() + n);
    }

    template <class T>
    inline const T& LinkedList<T>::at(const size_t n) const
    {
        return *(begin() + n);
    }

    template <class T>
    template <class... Args>
    inline T* LinkedList<T>::emplace(LinkedListIterator<T> _pos, Args&&... _args)
    {
        LinkedListCell<T>* pCell = makeLinkedListCell<T, Args...>(
            m_pAllocator, std::forward<Args>(_args)...);
        if (pCell == nullptr) {
            return nullptr;
        }
        if (m_pBegin == nullptr) {
            m_pEnd = m_pBegin = pCell;
        } else {
            LinkedListCell<T>* pIndex = _pos.m_pCurrent;
            if (_pos == m_pBegin) {
                pCell->m_pNext = pIndex;
                m_pBegin = pCell;
            } else {
                LinkedListCell<T>* pPrev = m_pBegin;
                while (pPrev->m_pNext != pIndex) {
                    pPrev = pPrev->m_pNext;
                }
                pPrev->m_pNext = pCell;
                if (_pos == m_pEnd) {
                    m_pEnd = pCell;
                } else {
                    pCell->m_pNext = pIndex;
                }
            }
        }
        ++m_Elements;
        return &pCell->m_Data;
    }

    template <class T>
    template <class... Args>
    inline T* LinkedList<T>::emplace_back(Args&&... args)
    {
        LinkedListCell<T>* pCell = makeLinkedListCell<T, Args...>(
            m_pAllocator, std::forward<Args>(args)...);
        if (pCell == nullptr) {
            return nullptr;
        }
        if (m_pBegin == nullptr) {
            m_pEnd = m_pBegin = pCell;
        } else {
            m_pEnd->m_pNext = pCell;
            m_pEnd = pCell;
        }

        ++m_Elements;
        return &pCell->m_Data;
    }
} // !core
} // !namespace
