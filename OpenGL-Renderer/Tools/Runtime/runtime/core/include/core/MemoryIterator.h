// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

// needed for std::random_access_iterator_tag
#include <iterator>

namespace ux3d {
namespace core {
    template <class T>
    class MemoryIterator {
    public:
        using value_type = T;
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t; //  using difference_type = typename std::iterator<std::random_access_iterator_tag, Type>::difference_type;
        using pointer = T*;
        using reference = T&;

        using const_iterator = MemoryIterator<const typename std::remove_const<T>::type>;

        constexpr MemoryIterator()
            : m_pCurrent(nullptr)
        {
        }

        constexpr explicit MemoryIterator(T* _pCurrent)
            : m_pCurrent(_pCurrent)
        {
        }

        MemoryIterator(const MemoryIterator& _Other)
            : m_pCurrent(_Other.m_pCurrent)
        {
        }

        constexpr MemoryIterator(MemoryIterator&& _Other) noexcept
            : m_pCurrent(_Other.m_pCurrent)
        {
            _Other.m_pCurrent = nullptr;
        }

        operator const_iterator() const { return const_iterator(m_pCurrent); }

        const_iterator getConst() const { return const_iterator(m_pCurrent); }

        MemoryIterator& operator++()
        {
            ++m_pCurrent;
            return *this;
        }

        MemoryIterator operator++(int)
        {
            MemoryIterator Ret(*this);
            ++m_pCurrent;
            return Ret;
        }

        MemoryIterator& operator--()
        {
            --m_pCurrent;
            return *this;
        }

        MemoryIterator operator--(int)
        {
            MemoryIterator Ret(*this);
            --m_pCurrent;
            return Ret;
        }

        MemoryIterator& operator=(const MemoryIterator& rhs)
        {
            m_pCurrent = rhs.m_pCurrent;
            return *this;
        }

        MemoryIterator& operator=(MemoryIterator&& rhs) noexcept
        {
            m_pCurrent = rhs.m_pCurrent;
            rhs.m_pCurrent = nullptr;
            return *this;
        }

        constexpr MemoryIterator operator+(difference_type n) const
        {
            return MemoryIterator(m_pCurrent + n);
        }

        MemoryIterator& operator+=(difference_type n)
        {
            m_pCurrent += n;
            return *this;
        }

        constexpr MemoryIterator operator-(difference_type n) const
        {
            return MemoryIterator(m_pCurrent - n);
        }

        MemoryIterator& operator-=(difference_type n)
        {
            m_pCurrent -= n;
            return *this;
        }

        constexpr bool operator==(const MemoryIterator& _Other) const
        {
            return m_pCurrent == _Other.m_pCurrent;
        }

        constexpr bool operator!=(const MemoryIterator& _Other) const
        {
            return m_pCurrent != _Other.m_pCurrent;
        }

        constexpr bool operator<(const MemoryIterator& _Other) const
        {
            return m_pCurrent < _Other.m_pCurrent;
        }

        constexpr bool operator>(const MemoryIterator& _Other) const
        {
            return m_pCurrent > _Other.m_pCurrent;
        }

        constexpr bool operator>=(const MemoryIterator& _Other) const
        {
            return m_pCurrent >= _Other.m_pCurrent;
        }

        constexpr bool operator<=(const MemoryIterator& _Other) const
        {
            return m_pCurrent <= _Other.m_pCurrent;
        }

        constexpr difference_type operator-(const MemoryIterator& _Other) const
        {
            return m_pCurrent - _Other.m_pCurrent;
        }

        constexpr reference operator*() const
        {
            return *m_pCurrent;
        }

        constexpr pointer operator->() const
        {
            return m_pCurrent;
        }

        constexpr reference operator[](difference_type n) const
        {
            return m_pCurrent[n];
        }

        constexpr pointer ptr() const
        {
            return m_pCurrent;
        }

    private:
        T* m_pCurrent = nullptr;
    };
} // !core
} // !ux3d

namespace std {
template <class T>
struct iterator_traits<ux3d::core::MemoryIterator<T>> {
    using value_type = typename ux3d::core::MemoryIterator<T>::value_type;
    using difference_type = typename ux3d::core::MemoryIterator<T>::difference_type;
    using pointer = typename ux3d::core::MemoryIterator<T>::pointer;
    using reference = typename ux3d::core::MemoryIterator<T>::reference;
    using iterator_category = typename ux3d::core::MemoryIterator<T>::iterator_category;
};
}
