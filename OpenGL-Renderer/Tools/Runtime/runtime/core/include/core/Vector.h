// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <algorithm>
#include <initializer_list>
#include <utility>
#include <vector>

#include "Algorithms.h"
#include "ContainerReference.h"
#include "HeapAllocator.h"
#include "Logger.h"
#include "MemoryIterator.h"
#include "MemoryRange.h"
#include "Traits.h"

namespace ux3d {
namespace core {
    // T = element type
    // Allocator, must provide void* allocate(size_t _ByteSize, size_t _Alignment) function
    template <class T>
    class Vector {
    public:
        using value_type = T;
        using reference = T&;
        using const_reference = const T&;
        using value_type_noncv = std::remove_cv_t<T>;

        using allocator_type = IAllocator;
        using iterator = MemoryIterator<T>;
        using const_iterator = typename MemoryIterator<T>::const_iterator;

        using container_ref = ContainerReference<Vector, uint32_t>;
        using const_container_ref = ContainerReference<const Vector, uint32_t>;

        Vector(IAllocator* _allocator = HeapAllocator::defaultInstance());

        Vector(const std::initializer_list<T>& il, IAllocator* _allocator = HeapAllocator::defaultInstance());

        virtual ~Vector();

        Vector(const Vector& _Other);
        Vector(Vector&& _Other) noexcept;

        Vector& operator=(const Vector& _Other);
        Vector& operator=(Vector&& _Other) noexcept;

        Vector& operator=(const std::initializer_list<T>& il);

        container_ref refAt(size_t idx);
        const_container_ref refAt(size_t idx) const;

        T& at(size_t pos);
        const T& at(size_t pos) const;
        T& operator[](size_t idx);
        const T& operator[](size_t idx) const;

        /// Ensures that the vector owns memory of at least the given size.
        /// This function is intended to be used as a performance boost to reduce the number of allocations,
        /// for instance when pushing in a loop where the loop count can be inferred.
        /// No constructor is called upon the new elements,
        /// unlike @a resize() .
        /// Requires move constructor.

        bool reserve(size_t _Capacity, IAllocator* _Allocator = nullptr)
        {
            return reserve(_Capacity, DefaultAlignment, _Allocator);
        }

        bool reserve(size_t _Capacity, uint32_t _Alignment, IAllocator* _Allocator = nullptr);

        // requires copy constructor

        bool resize(size_t _Elements, IAllocator* _Allocator = nullptr)
        {
            return resize(_Elements, DefaultAlignment, _Allocator);
        }

        template <typename... Args>

        bool resize(size_t _Elements, uint32_t _Alignment, IAllocator* _Allocator, Args&&... _args)
        {
            return resizeWithValue(_Elements, _Alignment, _Allocator, std::forward<Args>(_args)...);
        }

        bool resize(size_t _Elements, uint32_t _Alignment, IAllocator* _Allocator = nullptr)
        {
            return resizeWithValue(_Elements, _Alignment, _Allocator);
        }

        template <typename... Args>

        bool resizeWithValue(size_t _Elements, uint32_t _Alignment, IAllocator* _Allocator, Args&&... _args);

        // requires copy constructor

        bool resizeWithValue(size_t _Elements, const T& _defaultValue, IAllocator* _Allocator = nullptr)
        {
            return resizeWithValue(_Elements, DefaultAlignment, _Allocator, _defaultValue);
        }

        // Slice will shrink if _Elements < m_Elements.
        // calls destructor for remainder elements
        void shrink(size_t _Elements);

        T* data() const;

        T& front();
        const T& front() const;

        T& back();
        const T& back() const;

        container_ref front_ref();
        const_container_ref front_ref() const;

        container_ref back_ref();
        const_container_ref back_ref() const;

        iterator begin() noexcept { return iterator(m_pData); }
        const_iterator begin() const noexcept { return const_iterator(m_pData); }
        const_iterator cbegin() const noexcept { return const_iterator(m_pData); }

        iterator end() noexcept { return iterator(m_pData + m_Elements); }
        const_iterator end() const noexcept { return const_iterator(m_pData + m_Elements); }
        const_iterator cend() const noexcept { return const_iterator(m_pData + m_Elements); }

        const_container_ref begin_ref_const() const { return refAt(0); }
        container_ref begin_ref() { return refAt(0); }

        const_container_ref end_ref_const() const { return refAt(m_Elements); }
        container_ref end_ref() { return refAt(m_Elements); }

        MemoryRange<T> getMemoryRange() const { return MemoryRange<T>(data(), m_Elements); };
        operator MemoryRange<T>() const { return getMemoryRange(); };

        size_t size() const;
        size_t capacity() const;
        bool empty() const;

        /// Deletes the element at the given position by calling its destructor,
        /// than move constructs it again from the last element.
        /// Afterwards, the last element is destructed.
        iterator erase(iterator pos);

        /// Erases all elements in the given range.
        iterator erase(iterator first, iterator last);

        /// Deletes the element at the given position by calling its destructor,
        /// than move constructs it again from the next element.
        /// Repeated until the end of the vector is reached.
        /// Unlike [erase], this guarantees that the order of elements is not altered
        /// by deleting a single element.
        iterator erase_keep_order(iterator pos);

        /// Deletes the element at the given position by calling its destructor,
        /// than move constructs it again from the last element.
        /// Afterwards, the last element is destructed.
        container_ref erase(container_ref ref);

        /// Deletes the element at the given position by calling its destructor,
        /// than move constructs it again from the next element.
        /// Repeated until the end of the vector is reached.
        /// Unlike [erase], this guarantees that the order of elements is not altered
        /// by deleting a single element.
        container_ref erase_keep_order(container_ref ref);

        void clear();
        T pop_back();

        void reset();

        /// When custom alignment is desired, reserve first.
        template <class... Args>
        T* emplace_back(Args&&... args);

        /// When custom alignment is desired, reserve first.
        template <class... Args>
        container_ref emplace_back_ref(Args&&... args);

        /// When custom alignment is desired, reserve first.
        T* push_back(const T& value);

        /// When custom alignment is desired, reserve first.
        T* push_back(T&& value);

        // returns false if out of memory
        template <class InputIterator>
        bool assign(InputIterator _Begin, InputIterator _End, uint32_t _Alignment, IAllocator* _Allocator = nullptr);

        template <class InputIterator>
        bool assign(InputIterator _Begin, InputIterator _End, IAllocator* _Allocator = nullptr)
        {
            return assign(_Begin, _End, DefaultAlignment, _Allocator);
        }

        template <class Container>
        bool assign(const Container& _Container, uint32_t _Alignment, IAllocator* _Allocator = nullptr)
        {
            return assign(_Container.begin(), _Container.end(), _Alignment, _Allocator);
        }

        template <class Container>
        bool assign(const Container& _Container, IAllocator* _Allocator = nullptr)
        {
            return assign(_Container, DefaultAlignment, _Allocator);
        }

        bool assign(const std::initializer_list<T>& il, IAllocator* _Allocator = nullptr)
        {
            return assign(il.begin(), il.end(), DefaultAlignment, _Allocator);
        }

        // returns false if out of memory
        template <class InputIterator>
        bool append(InputIterator _Begin, InputIterator _End, uint32_t _Alignment, IAllocator* _Allocator = nullptr);

        template <class InputIterator>
        bool append(InputIterator _Begin, InputIterator _End, IAllocator* _Allocator = nullptr)
        {
            return append(_Begin, _End, DefaultAlignment, _Allocator);
        }

        // returns false if out of memory
        template <class Container>
        bool append(const Container& _Container, uint32_t _Alignment, IAllocator* _Allocator = nullptr)
        {
            return append(_Container.begin(), _Container.end(), _Alignment, _Allocator);
        }

        template <class Container>
        bool append(const Container& _Container, IAllocator* _Allocator = nullptr)
        {
            return append(_Container, DefaultAlignment, _Allocator);
        }

        bool append(const std::initializer_list<T>& il, IAllocator* _Allocator = nullptr)
        {
            return append(il.begin(), il.end(), DefaultAlignment, _Allocator);
        }

        IAllocator* getAllocator() const { return m_pAllocator; }

    private:
        IAllocator* m_pAllocator = nullptr;
        size_t m_Capacity = 0u;
        size_t m_Elements = 0u;
        uint32_t m_Alignment = DefaultAlignment;

        // The pointer removes const-volatile qualification,
        // as the vector always owns the memory pointed to by this pointer,
        // and therefore is never const nor volatile.
        // On the other hand, it is possible that the elements of a vector are const to the user,
        // so `value_type` may be const qualified.
        // This is the usual case when using an obtained inner-const reference to a vector.
        value_type_noncv* m_pData = nullptr;
    };

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    //// Implementation
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    template <class T>
    using VectorRef = typename Vector<T>::container_ref;

    template <class T>
    using VectorConstRef = typename Vector<T>::const_container_ref;

    template <class T>
    Vector<T>::Vector(const std::initializer_list<T>& il, IAllocator* _Allocator)
        : m_pAllocator(_Allocator)
    {
        auto inputLength = il.size();

        if (reserve(inputLength, nullptr)) {
            // Allocation succeeded.
            m_Elements = inputLength;

            size_t i = 0;
            for (auto iter = il.begin(); iter != il.end(); ++iter, ++i) {
                // Construct elements in place using the types copy constructor.
                new (&m_pData[i]) T { *iter };
            }
        }
    }

    template <class T>
    Vector<T>::Vector(IAllocator* _Allocator)
        : m_pAllocator(_Allocator)
    {
    }

    template <class T>
    Vector<T>::~Vector()
    {
        if (m_pData != nullptr && m_pAllocator != nullptr) {
            clear();
            deallocateIfExists(m_pAllocator, reinterpret_cast<void*>(m_pData), m_Capacity * sizeof(T));

            m_pData = nullptr;
            m_pAllocator = nullptr;
            m_Capacity = 0u;
            m_Elements = 0u;
        }
    }

    template <class T>
    Vector<T>::Vector(const Vector& _Other)
        : m_pAllocator(_Other.m_pAllocator)
        , m_Elements(_Other.m_Elements)
        , m_Alignment(_Other.m_Alignment)
    {
        if (m_pAllocator == nullptr) {
            // uninitialized --> nothing left to do
            return;
        }
        if (reserve(m_Elements, m_Alignment, m_pAllocator)) {
            for (size_t i = 0u; i < m_Elements; ++i) {
                new (m_pData + i) T(_Other[i]);
            }
        } else {
            // TODO: Fatal
        }
    }

    template <class T>
    Vector<T>::Vector(Vector<T>&& _Other) noexcept
        : m_pAllocator(_Other.m_pAllocator)
        , m_Capacity(_Other.m_Capacity)
        , m_Elements(_Other.m_Elements)
        , m_Alignment(_Other.m_Alignment)
        , m_pData(_Other.m_pData)
    {
        _Other.m_pAllocator = nullptr;
        _Other.m_pData = nullptr;
        _Other.m_Capacity = 0u;
        _Other.m_Elements = 0u;
        _Other.m_Alignment = 0u;
    }

    template <class T>
    Vector<T>& Vector<T>::operator=(const Vector& _Other)
    {
        if (this == &_Other) {
            return *this;
        }
        if (m_pAllocator == nullptr) {
            // uninitialized --> nothing left to do
            return *this;
        }

        if (resize(_Other.m_Elements, _Other.m_Alignment, m_pAllocator != nullptr ? m_pAllocator : _Other.m_pAllocator)) {
            m_Elements = _Other.m_Elements;

            for (size_t i = 0u; i < m_Elements; ++i) {
                this->operator[](i) = _Other[i];
            }
        } else {
            // TODO: FATAL
        }

        return *this;
    }

    template <class T>
    Vector<T>& Vector<T>::operator=(Vector&& _Other) noexcept
    {
        if (this == &_Other) {
            return *this;
        }

        if (m_pData != nullptr && m_pAllocator != nullptr) {
            clear(); // call destructor
            deallocateIfExists(m_pAllocator, reinterpret_cast<void*>(m_pData), m_Capacity * sizeof(T));
        }

        // adapt from other
        m_pAllocator = _Other.m_pAllocator;
        m_pData = _Other.m_pData;
        m_Elements = _Other.m_Elements;
        m_Capacity = _Other.m_Capacity;

        // invalidate other
        _Other.m_pAllocator = nullptr;
        _Other.m_pData = nullptr;
        _Other.m_Elements = 0u;
        _Other.m_Capacity = 0u;

        return *this;
    }

    template <class T>
    Vector<T>& Vector<T>::operator=(const std::initializer_list<T>& il)
    {
        (void)assign(il.begin(), il.end(), nullptr);
        return *this;
    }

    template <class T>
    typename Vector<T>::const_container_ref Vector<T>::refAt(size_t idx) const
    {
        return const_container_ref(*this, static_cast<uint32_t>(idx));
    }

    template <class T>
    typename Vector<T>::container_ref Vector<T>::refAt(size_t idx)
    {
        return container_ref(*this, static_cast<uint32_t>(idx));
    }

    template <class T>
    T& Vector<T>::at(const size_t pos)
    {
        if (assertion(pos < m_Elements, "Vector access out of range")) {
            return *(m_pData + pos);
        }

        return *m_pData;
    }

    template <class T>
    const T& Vector<T>::at(const size_t pos) const
    {
        if (assertion(pos < m_Elements, "Vector access out of range")) {
            return *(m_pData + pos);
        }

        return *m_pData;
    }

    template <class T>
    T& Vector<T>::operator[](const size_t idx)
    {
        return *(m_pData + idx);
    }

    template <class T>
    const T& Vector<T>::operator[](const size_t idx) const
    {
        return *(m_pData + idx);
    }

    template <class T>
    bool Vector<T>::reserve(size_t _Capacity, uint32_t _Alignment, IAllocator* _Allocator)
    {
        if (_Allocator == nullptr) {
            _Allocator = m_pAllocator;
        }

        if (assertion(_Allocator != nullptr, "No allocator specified") == false) {
            return false;
        }

        // dont shrink capacity if we stay on the same allocator
        if (_Allocator == m_pAllocator && _Capacity <= m_Capacity) {
            return true;
        }

        // dont shrink capacity if we switch the allocator
        _Capacity = std::max(_Capacity, m_Capacity);

        // allocate storage of at least requested capacity on the requested allocator
        value_type_noncv* pNewData = nullptr;

        if (_Capacity != 0u) {
            pNewData = reinterpret_cast<value_type_noncv*>(_Allocator->allocate(sizeof(T) * _Capacity, _Alignment));
            if (pNewData == nullptr) {
                return false;
            }
        }

        // copy old data to new allocation
        if (m_pData != nullptr && m_pAllocator != nullptr) {
            for (size_t i = 0u; i < m_Elements; ++i) {
                new (&pNewData[i]) T(std::move(m_pData[i]));
                destructIfExists<T>(m_pData + i);
            }

            deallocateIfExists(m_pAllocator, reinterpret_cast<void*>(m_pData), m_Capacity * sizeof(T));
        }

        m_pData = pNewData;
        m_Capacity = _Capacity;
        m_pAllocator = _Allocator;
        m_Alignment = _Alignment;

        return true;
    }

    template <class T>
    template <typename... Args>
    inline bool Vector<T>::resizeWithValue(size_t _Elements, uint32_t _Alignment, IAllocator* _Allocator, Args&&... args)
    {
        // If n is smaller than the current container size, the content is reduced to its first n elements,
        // removing those beyond (and destroying them).
        shrink(_Elements);

        // checks if capacity sufficient, reallocates and moves otherwise
        if (reserve(_Elements, _Alignment, _Allocator)) {
            // If n is greater than the current container size, the content is expanded by inserting at the
            // end as many elements as needed to reach a size of n.
            // If val is specified, the new elements are initialized as copies of val,
            // otherwise, they are value - initialized.
            for (size_t i = m_Elements; i < _Elements; ++i) {
                if constexpr (std::is_constructible_v<T, Args&&...>) {
                    new (&m_pData[i]) T(std::forward<Args>(args)...);
                } else {
                    new (&m_pData[i]) T { std::forward<Args>(args)... };
                }
            }

            m_Elements = _Elements;

            return true;
        }

        return false;
    }

    template <class T>
    void Vector<T>::shrink(size_t _Elements)
    {
        while (m_Elements > _Elements) {
            erase(end() - 1); // swap erase, calls destructor for T
        }
    }

    template <class T>
    T* Vector<T>::data() const
    {
        return m_pData;
    }

    template <class T>
    T& Vector<T>::front()
    {
        return *m_pData;
    }

    template <class T>
    const T& Vector<T>::front() const
    {
        return *m_pData;
    }

    template <class T>
    T& Vector<T>::back()
    {
        return this->operator[](m_Elements - 1u);
    }

    template <class T>
    const T& Vector<T>::back() const
    {
        return this->operator[](m_Elements - 1u);
    }

    template <class T>
    typename Vector<T>::container_ref Vector<T>::front_ref()
    {
        return refAt(0);
    }

    template <class T>
    typename Vector<T>::const_container_ref Vector<T>::front_ref() const
    {
        return refAt(0);
    }

    template <class T>
    typename Vector<T>::container_ref Vector<T>::back_ref()
    {
        return refAt(m_Elements - 1u);
    }

    template <class T>
    typename Vector<T>::const_container_ref Vector<T>::back_ref() const
    {
        return refAt(m_Elements - 1u);
    }

    template <class T>
    size_t Vector<T>::size() const
    {
        return m_Elements;
    }

    template <class T>
    size_t Vector<T>::capacity() const
    {
        return m_Capacity;
    }

    template <class T>
    bool Vector<T>::empty() const
    {
        return m_Elements == 0u;
    }

    // Erase the specified element from the vector. Invalidates all iterators at and after the point of erase
    template <class T>
    typename Vector<T>::iterator Vector<T>::erase(iterator pos)
    {
        // call destructor
        --m_Elements;

        auto last = end();

        if (pos != last) {
            // Move the element at the back to the point of erase.
            // Destruct the original value and move-construct it from the last value.
            destructIfExists<T>(&*pos);
            new (&*pos) T(std::move(*last));

            destructIfExists<T>(&*last);
        } else {
            destructIfExists<T>(&*pos);
        }

        return pos;
    }

    template <class T>
    typename Vector<T>::iterator
    Vector<T>::erase(iterator first, iterator last)
    {
        auto current = last;
        while (current != first) {
            --current;
            current = erase(current);
        }
        return current;
    }

    template <class T>
    typename Vector<T>::iterator
    Vector<T>::erase_keep_order(iterator pos)
    {
        // call destructor
        destructIfExists<T>(&*pos);
        --m_Elements;

        auto beginIt = pos;
        auto endIt = end();

        for (; beginIt != endIt; ++beginIt) {
            // shift all elements after the point of erase to the left
            destructIfExists<T>(&*beginIt);
            new (&*beginIt) T(std::move(*(beginIt + 1)));
        }

        return pos;
    }

    // Erase the specified element from the vector. Invalidates all iterators at and after the point of erase
    template <class T>
    typename Vector<T>::container_ref Vector<T>::erase(container_ref ref)
    {
        // call destructor
        --m_Elements;

        auto endRef = end_ref();

        if (ref != endRef) {
            // move the element at the back to the point of erase
            destructIfExists<T>(&*ref);
            new (&*ref) T(std::move(*endRef));
        } else {
            destructIfExists<T>(&*ref);
        }

        return ref;
    }

    template <class T>
    typename Vector<T>::container_ref
    Vector<T>::erase_keep_order(container_ref ref)
    {
        // call destructor
        destructIfExists<T>(*ref);
        --m_Elements;

        auto beginIt = ref;
        auto endIt = end_ref();

        for (; beginIt != endIt; ++beginIt) {
            // shift all elements after the point of erase to the left
            destructIfExists<T>(&*beginIt);
            new (&*beginIt) T(std::move(*(beginIt + 1)));
        }

        return ref;
    }

    template <class T>
    void Vector<T>::clear()
    {
        if constexpr (has_nonprimitve_destructor_v<T>) {
            for (auto& elem : *this) {
                elem.~T();
            }
        }

        reset();
    }

    // reset only clears the element count without calling the destructors of elements
    template <class T>
    void Vector<T>::reset()
    {
        m_Elements = 0u;
    }

    template <class T>
    T Vector<T>::pop_back()
    {
        T backElem(m_pData[--m_Elements]); // copy last element
        destructIfExists<T>(m_pData + m_Elements); // destruct last element
        return backElem;
    }

    template <class T>
    template <class... Args>
    T* Vector<T>::emplace_back(Args&&... args)
    {
        if (m_Elements >= m_Capacity) {
            // TODO: come up with a better strategy
            if (!reserve(m_Capacity + (1u + m_Capacity / 2u), nullptr)) {
                // Allocation failed.
                return nullptr;
            }
        }

        auto ptr = m_pData + m_Elements++;
        if constexpr (std::is_constructible_v<T, Args&&...>) {
            new (ptr) T(std::forward<Args>(args)...);
        } else {
            new (ptr) T { std::forward<Args>(args)... };
        }

        return ptr;
    }

    template <class T>
    template <class... Args>
    typename Vector<T>::container_ref Vector<T>::emplace_back_ref(Args&&... _args)
    {
        container_ref ref(*this, static_cast<uint32_t>(m_Elements));
        (void)emplace_back(std::forward<Args>(_args)...);
        return ref;
    }

    template <class T>
    template <class InputIterator>
    bool Vector<T>::assign(InputIterator _Begin, const InputIterator _End, const uint32_t _Alignment, IAllocator* _Allocator)
    {
        // TODO: check with enable_if if InputIterator implements -operator to be able to calculate the needed capacity upfront

        size_t inputLength = static_cast<size_t>(_End - _Begin);

        if (reserve(inputLength, _Alignment, _Allocator)) {
            shrink(inputLength); // call destructor on elements between inputLength and m_Elements
            m_Elements = inputLength;

            for (auto dst = begin(); _Begin < _End; ++_Begin, ++dst) {
                *dst = *_Begin;
            }

            return true;
        }

        return false;
    }

    template <class T>
    template <class InputIterator>
    bool Vector<T>::append(InputIterator _Begin, const InputIterator _End, const uint32_t _Alignment, IAllocator* _Allocator)
    {
        auto inputLength = m_Elements + static_cast<uint64_t>(_End - _Begin);

        if (reserve(inputLength, _Alignment, _Allocator)) {
            for (size_t i = m_Elements; i < inputLength; ++i, ++_Begin) {
                new (&m_pData[i]) T(*_Begin);
            }

            m_Elements = inputLength;

            return true;
        }

        return false;
    }

    template <class T>
    T* Vector<T>::push_back(const T& value)
    {
        return emplace_back(value);
    }

    template <class T>
    T* Vector<T>::push_back(T&& value)
    {
        return emplace_back(std::forward<T>(value));
    }

    template <class T>
    std::vector<T> convertToStd(const core::Vector<T>& v)
    {
        std::vector<T> result;
        for (const auto& x : v) {
            result.push_back(x);
        }
        return result;
    }
} // !core
} // !ux3d
