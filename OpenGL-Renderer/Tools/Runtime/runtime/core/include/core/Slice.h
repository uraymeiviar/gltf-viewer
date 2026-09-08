// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "HeapAllocator.h"
#include "Logger.h"
#include "MemoryIterator.h"
#include "MemoryRange.h"
#include "Traits.h"

namespace ux3d {
namespace core {
    // Slice can only be (delay) allocated once
    // Copy and move construction and move assignment work, copy assignment is deleted

    // T = element type
    // Allocator, must provide void* allocate(size_t _ByteSize, size_t _Alignment) function
    template <class T>
    class Slice {
        template <class U>
        friend class Slice;

    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;

        using allocator_type = IAllocator;
        using iterator = MemoryIterator<T>;
        using const_iterator = typename MemoryIterator<T>::const_iterator;

        Slice(IAllocator* _Allocator = HeapAllocator::defaultInstance());

        //explicit Slice(Allocator* _Allocator, size_t _Elements = 0u);
        //explicit Slice(Allocator* _Allocator, size_t _Elements, const T& _DefaultValue);
        //explicit Slice(Allocator* _Allocator, const std::initializer_list<T>& _List);
        //explicit Slice(Allocator* _Allocator, const T* _pData, size_t _Elements);

        //explicit Slice(size_t _Alignment, Allocator* _Allocator, size_t _Elements = 0u);
        //explicit Slice(size_t _Alignment, Allocator* _Allocator, size_t _Elements, const T& _DefaultValue);
        //explicit Slice(size_t _Alignment, Allocator* _Allocator, const std::initializer_list<T>& _List);
        //explicit Slice(size_t _Alignment, Allocator* _Allocator, const T* _pData, size_t _Elements);

        virtual ~Slice();

        Slice(Slice&& _Other) noexcept;

        /*, typename std::enable_if<sizeof(T) % sizeof(Elem) == 0>::type* = 0*/
        template <class Elem>
        explicit Slice(Slice<Elem>&& _Other);

        Slice& operator=(const Slice& _Other) = delete;
        Slice& operator=(Slice&& _Other) noexcept;

        T& at(size_t idx);
        const T& at(size_t idx) const;

        T& operator[](size_t idx);
        const T& operator[](size_t idx) const;

        // can only be called once, either on construction with non-zero _Elements or directly ('delayed single shot allocation')
        // does not call any constructor, just allocates raw memory
        bool allocate(size_t _Elements, uint32_t _Alignment, IAllocator* _Allocator = nullptr);
        bool allocate(size_t _Elements, IAllocator* _Allocator = nullptr) { return allocate(_Elements, DefaultAlignment, _Allocator); }

        // will call allocate(_Elements) if slice has been constructed with _Elements == 0
        // otherwise, Slice will shrink if _Elements < m_Elements. Slice can not grow after first allocation
        // calls default constructor for elements for newly allocated elements
        // calling resize with _Elements == 0 leads to a unusable empty Slice
        bool resize(size_t _Elements, uint32_t _Alignment, IAllocator* _Allocator = nullptr);
        bool resize(size_t _Elements, IAllocator* _Allocator = nullptr) { return resize(_Elements, DefaultAlignment, _Allocator); }

        // same as resize, copy constructs newly added elements with Args&&... _args
        template <typename... Args>
        bool resizeWithValue(size_t _Elements, uint32_t _Alignment, IAllocator* _Allocator, Args&&... _args);

        // same as resize, copy constructs newly added elements with _defaultValue
        bool resizeWithValue(size_t _Elements, uint32_t _Alignment, const T& _defaultValue, IAllocator* _Allocator = nullptr) { return resizeWithValue(_Elements, _Alignment, _Allocator, _defaultValue); }
        bool resizeWithValue(size_t _Elements, const T& _defaultValue, IAllocator* _Allocator = nullptr) { return resizeWithValue(_Elements, DefaultAlignment, _defaultValue, _Allocator); }

        // Slice will shrink if _Elements < m_Elements.
        // calls destructor for remainder elements
        bool shrink(size_t _Elements);

        T* data() const;

        T& front();
        const T& front() const;

        T& back();
        const T& back() const;

        iterator begin() noexcept { return iterator(m_pData); }
        const_iterator begin() const noexcept { return const_iterator(m_pData); }
        const_iterator cbegin() const noexcept { return const_iterator(m_pData); }

        iterator end() noexcept { return iterator(m_pData + m_Elements); }
        const_iterator end() const noexcept { return const_iterator(m_pData + m_Elements); }
        const_iterator cend() const noexcept { return const_iterator(m_pData + m_Elements); }

        size_t size() const;
        bool empty() const;

        MemoryRange<T> getMemoryRange() const { return MemoryRange<T>(data(), m_Elements); };
        operator MemoryRange<T>() const { return getMemoryRange(); };

        template <class InputIterator>
        bool assign(InputIterator _Begin, InputIterator _End, uint32_t _Alignment, IAllocator* _Allocator = nullptr);

        template <class InputIterator>
        bool assign(InputIterator _Begin, InputIterator _End, IAllocator* _Allocator = nullptr) { return assign(_Begin, _End, DefaultAlignment, _Allocator); }

        template <class Container>
        bool assign(const Container& _Container, uint32_t _Alignment, IAllocator* _Allocator = nullptr) { return assign(_Container.begin(), _Container.end(), _Alignment, _Allocator); }

        template <class Container>
        bool assign(const Container& _Container, IAllocator* _Allocator = nullptr) { return assign(_Container.begin(), _Container.end(), DefaultAlignment, _Allocator); }

        template <class... Args>
        T& constructAt(size_t idx, Args&&... _Args);

        IAllocator* getAllocator() const { return m_pAllocator; }

    private:
        void constructElements();
        void destructElements();

    private:
        IAllocator* m_pAllocator = nullptr;
        T* m_pData = nullptr;
        size_t m_Elements = 0u;
        uint32_t m_Alignment = DefaultAlignment;
    };

    //template<class T, class Allocator>
    //inline Slice<T, Allocator>::Slice(Allocator* _Allocator, const std::initializer_list<T>& _List)
    //{
    //	if (_List.size() > 0u)
    //	{
    //		if (allocate(_List.size(), _Allocator))
    //		{
    //			auto it = _List.begin();
    //			for (T& v : *this)
    //			{
    //				new(&v) T(*it);
    //				++it;
    //			}
    //		}
    //		else
    //		{
    //			// TODO: FATAL
    //		}
    //	}
    //}

    template <class T>
    inline Slice<T>::Slice(IAllocator* _Allocator)
        : m_pAllocator(_Allocator)
    {
    }

    template <class T>
    inline Slice<T>::~Slice()
    {
        if (m_pData != nullptr && m_pAllocator != nullptr) {
            destructElements();
            deallocateIfExists(m_pAllocator, reinterpret_cast<void*>(m_pData), m_Elements * sizeof(T));

            m_pAllocator = nullptr;
            m_pData = nullptr;
            m_Elements = 0u;
            m_Alignment = DefaultAlignment;
        }
    }

    template <class T>
    Slice<T>::Slice(Slice<T>&& _Other) noexcept
        : m_pAllocator(_Other.m_pAllocator)
        , m_pData(_Other.m_pData)
        , m_Elements(_Other.m_Elements)
        , m_Alignment(_Other.m_Alignment)
    {
        _Other.m_pAllocator = nullptr;
        _Other.m_pData = nullptr;
        _Other.m_Elements = 0u;
        _Other.m_Alignment = DefaultAlignment;
    }

    template <class T>
    inline Slice<T>& Slice<T>::operator=(Slice&& _Other) noexcept
    {
        if (this == &_Other) {
            return *this;
        }

        // release own memory if possible
        if (m_pData != nullptr && m_pAllocator != nullptr) {
            destructElements();
            deallocateIfExists(m_pAllocator, reinterpret_cast<void*>(m_pData), m_Elements * sizeof(T));
        }

        // adapt from other
        m_pAllocator = _Other.m_pAllocator;
        m_pData = _Other.m_pData;
        m_Elements = _Other.m_Elements;
        m_Alignment = _Other.m_Alignment;

        // invalidate other
        _Other.m_pAllocator = nullptr;
        _Other.m_pData = nullptr;
        _Other.m_Elements = 0u;

        return *this;
    }

    template <class T>
    inline T& Slice<T>::at(const size_t idx)
    {
        if (assertion(idx < m_Elements, "Slice access out of range")) {
            return *(m_pData + idx);
        }

        return *m_pData;
    }

    template <class T>
    inline const T& Slice<T>::at(const size_t idx) const
    {
        if (assertion(idx < m_Elements, "Slice access out of range")) {
            return *(m_pData + idx);
        }

        return *m_pData;
    }

    template <class T>
    T& Slice<T>::operator[](const size_t idx)
    {
        return *(m_pData + idx);
    }

    template <class T>
    const T& Slice<T>::operator[](const size_t idx) const
    {
        return *(m_pData + idx);
    }

    template <class T>
    inline bool Slice<T>::allocate(const size_t _Elements, const uint32_t _Alignment, IAllocator* _Allocator)
    {
        if (_Elements == 0u) {
            return true;
        }

        if (assertion(m_pData == nullptr, "Slice can only be allocated once")) {
            if (_Allocator != nullptr) {
                m_pAllocator = _Allocator; // assign new allocator
            }

            if (assertion(m_pAllocator != nullptr, "No allocator specified")) {
                m_pData = reinterpret_cast<T*>(m_pAllocator->allocate(sizeof(T) * _Elements, _Alignment));
                m_Elements = m_pData != nullptr ? _Elements : 0u;
                m_Alignment = _Alignment;
            }
        }

        return m_pData != nullptr;
    }

    template <class T>
    bool Slice<T>::resize(const size_t _Elements, const uint32_t _Alignment, IAllocator* _Allocator)
    {
        // shrink if _Elements < m_Elements
        if (!shrink(_Elements)) {
            if (allocate(_Elements, _Alignment, _Allocator)) {
                constructElements();

                return true;
            }

            return false;
        }

        return true;
    }

    template <class T>
    template <typename... Args>
    inline bool Slice<T>::resizeWithValue(size_t _Elements, uint32_t _Alignment, IAllocator* _Allocator, Args&&... _args)
    {
        if (!shrink(_Elements)) {
            if (allocate(_Elements, _Alignment, _Allocator)) {
                for (size_t i = 0; i < m_Elements; ++i) {
                    if constexpr (std::is_constructible_v<T, Args&&...>) {
                        new (m_pData + i) T(std::forward<Args>(_args)...);
                    } else {
                        new (m_pData + i) T { std::forward<Args>(_args)... };
                    }
                }

                return true;
            }

            return false;
        }

        return true;
    }

    template <class T>
    inline bool Slice<T>::shrink(const size_t _Elements)
    {
        if (_Elements > m_Elements) {
            return false;
        }

        // call destructor for remaining elements
        for (size_t i = _Elements; i < m_Elements; ++i) {
            destructIfExists<T>(m_pData + i);
        }

        m_Elements = _Elements;

        return true;
    }

    template <class T>
    inline T* Slice<T>::data() const
    {
        return m_pData;
    }

    template <class T>
    inline T& Slice<T>::front()
    {
        return *m_pData;
    }

    template <class T>
    inline const T& Slice<T>::front() const
    {
        return *m_pData;
    }

    template <class T>
    inline T& Slice<T>::back()
    {
        return this->operator[](m_Elements - 1u);
    }

    template <class T>
    inline const T& Slice<T>::back() const
    {
        return this->operator[](m_Elements - 1u);
    }

    template <class T>
    inline size_t Slice<T>::size() const
    {
        return m_Elements;
    }

    template <class T>
    inline bool Slice<T>::empty() const
    {
        return m_Elements == 0u;
    }

    template <class T>
    inline void Slice<T>::destructElements()
    {
        if (std::is_destructible<T>::value) {
            for (size_t i = 0u; i < m_Elements; ++i) {
                destructIfExists<T>(m_pData + i);
            }
        }
    }

    template <class T>
    inline void Slice<T>::constructElements()
    {
        if (std::is_constructible<T>::value) {
            for (size_t i = 0u; i < m_Elements; ++i) {
                constructIfExists<T>(m_pData + i);
            }
        }
    }

    template <class T>
    template <class Elem>
    inline Slice<T>::Slice(Slice<Elem>&& _Other)
    {
        if ((_Other.m_Elements * sizeof(Elem)) % sizeof(T) == 0) {
            m_pAllocator = _Other.m_pAllocator;
            m_pData = reinterpret_cast<T*>(_Other.m_pData);
            m_Elements = (_Other.m_Elements * sizeof(Elem)) / sizeof(T);
            m_Alignment = _Other.m_Alignment;

            _Other.m_pAllocator = nullptr;
            _Other.m_pData = nullptr;
            _Other.m_Elements = 0u;
        }
    }

    template <class T>
    template <class InputIterator>
    inline bool Slice<T>::assign(InputIterator _Begin, const InputIterator _End, const uint32_t _Alignment, IAllocator* _Allocator)
    {
        auto inputLength = _End - _Begin;

        if (static_cast<size_t>(inputLength) > m_Elements) {
            if (!allocate(inputLength, _Alignment, _Allocator)) {
                return false;
            }
        }

        for (auto dst = begin(); _Begin < _End; ++_Begin, ++dst) {
            *dst = *_Begin;
        }

        return true;
    }

    template <class T>
    template <class... Args>
    inline T& Slice<T>::constructAt(const size_t idx, Args&&... _Args)
    {
        return *new (m_pData + idx) T(std::forward<Args>(_Args)...);
    }

    //template <class T, class Allocator>
    //Slice<T, Allocator> makeSlice(Allocator& _Allocator, const size_t _Elements = 0u) { return Slice<T, Allocator>(_Allocator, _Elements); }

    //template <class T, class Allocator>
    //Slice<T, Allocator> makeSlice(Allocator& _Allocator, const size_t _Elements, const T& _DefaultValue) { return Slice<T, Allocator>(_Allocator, _Elements, _DefaultValue); }

    template <class T, class U>
    Slice<T> asSlice(Slice<U>&& _Other) { return Slice<T>(std::forward<Slice<U>>(_Other)); }
} // !core
} // !ux3d
