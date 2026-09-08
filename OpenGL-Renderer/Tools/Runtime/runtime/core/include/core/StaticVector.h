// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Logger.h"
#include "MemoryIterator.h"
#include "Traits.h"

namespace ux3d {
namespace core {
    template <class T, size_t N, size_t Alignment = alignof(T)>
    class StaticVector {
    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;

        using iterator = MemoryIterator<T>;
        using const_iterator = typename MemoryIterator<T>::const_iterator;

        constexpr StaticVector(const StaticVector& _Other);
        constexpr StaticVector(StaticVector&& _Other) noexcept;

        constexpr StaticVector& operator=(const StaticVector& _Other);
        constexpr StaticVector& operator=(StaticVector&& _Other) noexcept;

        StaticVector() = default;
        explicit StaticVector(size_t _elements, const T& _default = {});

        StaticVector(const std::initializer_list<T>& _List);

        template <std::size_t n = N>
        explicit StaticVector(T const (&arr)[n])
        {
            for (size_t i = 0; i < n; i++) {
                emplace_back(arr[i]);
            }
        }

        virtual ~StaticVector();

        T& at(size_t pos);
        const T& at(size_t pos) const;
        constexpr T& operator[](const size_t idx) { return m_data[idx]; }
        constexpr const T& operator[](const size_t idx) const { return m_data[idx]; }

        constexpr T* data() { return &m_data[0]; }
        constexpr const T* data() const { return &m_data[0]; }

        constexpr T& front() { return m_data[0]; }
        constexpr const T& front() const { return m_data[0]; }

        constexpr T& back() { return m_data[N - 1u]; }
        constexpr const T& back() const { return m_data[N - 1u]; }

        constexpr iterator begin() { return iterator(&m_data[0]); }
        constexpr const_iterator begin() const { return const_iterator(&m_data[0]); }
        constexpr const_iterator cbegin() const { return const_iterator(&m_data[0]); }

        constexpr iterator end() { return iterator(&m_data[0] + m_elements); }
        constexpr const_iterator end() const { return const_iterator(&m_data[0] + m_elements); }
        constexpr const_iterator cend() const { return const_iterator(&m_data[0] + m_elements); }

        constexpr size_t size() const { return m_elements; }
        constexpr size_t capacity() const { return N; }
        constexpr bool empty() const { return m_elements == 0u; }
        constexpr bool full() const { return m_elements == N; }

        template <class... Args>
        constexpr T* emplace_back(Args&&... args);
        constexpr T* push_back(const T& value);
        constexpr T* push_back(T&& value);

        void clear();
        void reset();
        constexpr T pop_back();

        iterator erase(iterator pos);
        iterator erase(iterator first, iterator last);
        iterator erase_keep_order(iterator pos);

        template <class InputIterator>
        void assign(InputIterator _Begin, InputIterator _End);

        template <class Container>
        void assign(const Container& _Container);

        template <class InputIterator>
        void append(InputIterator _Begin, InputIterator _End);

        template <class Container>
        void append(const Container& _Container);

    private:
        struct alignas(Alignment) {
            T m_data[N];
        };
        size_t m_elements = 0u;
    };

    template <class T, size_t N, size_t Alignment>
    inline constexpr StaticVector<T, N, Alignment>::StaticVector(const StaticVector& _Other)
    {
        for (size_t i = 0; i < _Other.m_elements; i++) {
            emplace_back(_Other[i]);
        }
    }

    template <class T, size_t N, size_t Alignment>
    inline constexpr StaticVector<T, N, Alignment>::StaticVector(StaticVector&& _Other) noexcept
    {
        for (size_t i = 0; i < _Other.m_elements; i++) {
            emplace_back(std::move(_Other[i]));
        }
    }

    template <class T, size_t N, size_t Alignment>
    inline constexpr StaticVector<T, N, Alignment>& StaticVector<T, N, Alignment>::operator=(const StaticVector& _Other)
    {
        if (this != &_Other) {
            for (size_t i = 0; i < _Other.m_elements; i++) {
                m_data[i] = _Other[i];
            }
            m_elements = _Other.m_elements;
        }

        return *this;
    }

    template <class T, size_t N, size_t Alignment>
    inline constexpr StaticVector<T, N, Alignment>& StaticVector<T, N, Alignment>::operator=(StaticVector&& _Other) noexcept
    {
        if (this != &_Other) {
            for (size_t i = 0; i < _Other.m_elements; i++) {
                m_data[i] = std::move(_Other[i]);
            }
            m_elements = _Other.m_elements;
        }

        return *this;
    }

    template <class T, size_t N, size_t Alignment>
    inline StaticVector<T, N, Alignment>::StaticVector(size_t _elements, const T& _default)
    {
        for (size_t i = 0; i < _elements; i++) {
            emplace_back(_default);
        }
    }

    template <class T, size_t N, size_t Alignment>
    inline StaticVector<T, N, Alignment>::StaticVector(const std::initializer_list<T>& _List)
    {
        for (size_t i = 0; i < N && i < _List.size(); i++) {
            emplace_back(*(_List.begin() + i));
        }
    }

    template <class T, size_t N, size_t Alignment>
    inline StaticVector<T, N, Alignment>::~StaticVector()
    {
        clear();
    }

    template <class T, size_t N, size_t Alignment>
    inline T& StaticVector<T, N, Alignment>::at(const size_t pos)
    {
        if (assertion(pos < m_elements, "Vector access out of range")) {
            return m_data[pos];
        }

        return front();
    }

    template <class T, size_t N, size_t Alignment>
    inline const T& StaticVector<T, N, Alignment>::at(const size_t pos) const
    {
        if (assertion(pos < m_elements, "Vector access out of range")) {
            return m_data[pos];
        }

        return front();
    }

    template <class T, size_t N, size_t Alignment>
    inline constexpr T* StaticVector<T, N, Alignment>::push_back(const T& value)
    {
        return emplace_back(value);
    }

    template <class T, size_t N, size_t Alignment>
    inline constexpr T* StaticVector<T, N, Alignment>::push_back(T&& value)
    {
        return emplace_back(std::forward<T>(value));
    }

    template <class T, size_t N, size_t Alignment>
    inline void StaticVector<T, N, Alignment>::clear()
    {
        for (size_t i = 0; i < m_elements; i++) {
            destructIfExists<T>(m_data + i);
        }
        m_elements = 0u;
    }

    template <class T, size_t N, size_t Alignment>
    inline void StaticVector<T, N, Alignment>::reset()
    {
        m_elements = 0u;
    }

    template <class T, size_t N, size_t Alignment>
    inline constexpr T StaticVector<T, N, Alignment>::pop_back()
    {
        T backElem(m_data[--m_elements]); // copy last element
        destructIfExists<T>(m_data + m_elements); // destruct last element
        return backElem;
    }

    template <class T, size_t N, size_t Alignment>
    inline typename StaticVector<T, N, Alignment>::iterator StaticVector<T, N, Alignment>::erase(iterator pos)
    {
        --m_elements;

        auto last = end();

        if (pos != last) {
            // move the element at the back to the point of erase
            *pos = std::move(*last);
            destructIfExists<T>(&*last);
        } else {
            destructIfExists<T>(&*pos);
        }

        return pos;
    }

    template <class T, size_t N, size_t Alignment>
    inline typename StaticVector<T, N, Alignment>::iterator StaticVector<T, N, Alignment>::erase(iterator first, iterator last)
    {
        auto current = last;
        while (current != first) {
            --current;
            current = erase(current);
        }
        return current;
    }

    template <class T, size_t N, size_t Alignment>
    inline typename StaticVector<T, N, Alignment>::iterator StaticVector<T, N, Alignment>::erase_keep_order(iterator pos)
    {
        // call destructor
        destructIfExists<T>(&*pos);
        --m_elements;

        auto beginIt = pos;
        auto endIt = end();

        for (; beginIt != endIt; ++beginIt) {
            // shift all elements after the point of erase to the left
            *beginIt = std::move(*(beginIt + 1));
        }

        return pos;
    }

    template <class T, size_t N, size_t Alignment>
    template <class... Args>
    inline constexpr T* StaticVector<T, N, Alignment>::emplace_back(Args&&... args)
    {
        return new (&m_data[m_elements++]) T(std::forward<Args>(args)...);
    }

    template <class T, size_t N, size_t Alignment>
    template <class InputIterator>
    inline void StaticVector<T, N, Alignment>::assign(InputIterator _begin, InputIterator _end)
    {
        clear();
        append(_begin, _end);
    }

    template <class T, size_t N, size_t Alignment>
    template <class Container>
    inline void StaticVector<T, N, Alignment>::assign(const Container& _container)
    {
        assign(_container.begin(), _container.end());
    }

    template <class T, size_t N, size_t Alignment>
    template <class InputIterator>
    inline void StaticVector<T, N, Alignment>::append(InputIterator _begin, InputIterator _end)
    {
        for (; _begin != _end; ++_begin) {
            emplace_back(*_begin);
        }
    }

    template <class T, size_t N, size_t Alignment>
    template <class Container>
    inline void StaticVector<T, N, Alignment>::append(const Container& _container)
    {
        append(_container.begin(), _container.end());
    }

    template <class T, size_t N, size_t Alignment = alignof(T)>
    StaticVector<T, N, Alignment> makeStaticVector(T const (&arr)[N]) { return StaticVector<T, N, Alignment>(arr); }
} // !core
} // !ux3d
