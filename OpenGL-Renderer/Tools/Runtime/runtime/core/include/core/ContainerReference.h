// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once
#include <stdint.h>

namespace ux3d {
namespace core {

    /// A reference to an indexed element inside a container.
    /// The reference is stable over the container's re-allocations.

    template <class Container, class Index>
    class ContainerReference {
    public:
        using value_type = typename Container::value_type;
        using index_type = Index;

        /// Creates a container reference without referencing a container.
        /// Subsequent dereferencing will result in null pointer dereferencing.
        ContainerReference();

        ContainerReference(const std::nullptr_t);

        ContainerReference(const ContainerReference& _other);

        ContainerReference(ContainerReference&& _other) noexcept;

        explicit ContainerReference(Container* _container, const Index _index = 0);

        explicit ContainerReference(Container& _container, const Index _index = 0);

        ContainerReference& operator=(const ContainerReference& rhs);

        ContainerReference& operator=(ContainerReference&& rhs) noexcept;

        const value_type& operator*() const;
        const value_type* operator->() const;

        std::conditional_t<std::is_const_v<Container>, const value_type&, value_type&>
        operator*() { return *(m_pContainer->begin() + m_Index); }

        std::conditional_t<std::is_const_v<Container>, const value_type*, value_type*>
        operator->() { return &(*(m_pContainer->begin() + m_Index)); }

        /// Coercion into a container reference holding a constant pointer to the container.
        operator value_type() const;

        operator bool() const;

        bool operator==(const ContainerReference& rhs) const;

        bool operator!=(const ContainerReference& rhs) const;

        bool operator==(std::nullptr_t) const;

        bool operator!=(std::nullptr_t) const;

        Container& container();

        ContainerReference& operator++();

        bool has_same_container(const ContainerReference& rhs) const;

        operator ContainerReference<const Container, Index>() const;

    private:
        Container* m_pContainer;
        Index m_Index;
    };

    template <class Container, class Index>
    ContainerReference<Container, Index>::ContainerReference()
        : m_pContainer { nullptr }
        , m_Index { 0 }
    {
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>::ContainerReference(const std::nullptr_t)
        : ContainerReference()
    {
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>::ContainerReference(const ContainerReference& _other)
        : m_pContainer(_other.m_pContainer)
        , m_Index(_other.m_Index)
    {
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>::ContainerReference(ContainerReference&& _other) noexcept
        : m_pContainer(_other.m_pContainer)
        , m_Index(_other.m_Index)
    {
        _other.m_pContainer = nullptr;
        _other.m_Index = 0u;
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>::ContainerReference(Container* _container, const Index _index)
        : m_pContainer(_container)
        , m_Index(_index)
    {
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>::ContainerReference(Container& _container, const Index _index)
        : m_pContainer(&_container)
        , m_Index(_index)
    {
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>&
    ContainerReference<Container, Index>::operator=(const ContainerReference& rhs)
    {
        if (this != &rhs) {
            m_pContainer = rhs.m_pContainer;
            m_Index = rhs.m_Index;
        }
        return *this;
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>&
    ContainerReference<Container, Index>::operator=(ContainerReference&& rhs) noexcept
    {
        if (this != &rhs) {
            m_pContainer = rhs.m_pContainer;
            m_Index = rhs.m_Index;
            rhs.m_pContainer = nullptr;
            rhs.m_Index = 0u;
        }

        return *this;
    }

    template <class Container, class Index>
    const typename ContainerReference<Container, Index>::value_type&
    ContainerReference<Container, Index>::operator*() const { return *(m_pContainer->begin() + m_Index); }

    template <class Container, class Index>
    const typename ContainerReference<Container, Index>::value_type*
    ContainerReference<Container, Index>::operator->() const { return &(*(m_pContainer->begin() + m_Index)); }

    template <class Container, class Index>
    ContainerReference<Container, Index>::operator value_type() const { return *(m_pContainer->begin() + m_Index); }

    template <class Container, class Index>
    ContainerReference<Container, Index>::operator bool() const { return m_pContainer != nullptr; }

    template <class Container, class Index>
    bool ContainerReference<Container, Index>::operator==(const ContainerReference& rhs) const
    {
        return m_pContainer == rhs.m_pContainer && m_Index == rhs.m_Index;
    }

    template <class Container, class Index>
    bool ContainerReference<Container, Index>::operator!=(const ContainerReference& rhs) const
    {
        return m_pContainer != rhs.m_pContainer || m_Index != rhs.m_Index;
    }

    template <class Container, class Index>
    bool ContainerReference<Container, Index>::operator==(std::nullptr_t) const
    {
        return m_pContainer == nullptr;
    }

    template <class Container, class Index>
    bool ContainerReference<Container, Index>::operator!=(std::nullptr_t) const
    {
        return m_pContainer != nullptr;
    }

    template <class Container, class Index>
    Container& ContainerReference<Container, Index>::container()
    {
        return *m_pContainer;
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>& ContainerReference<Container, Index>::operator++()
    {
        ++m_Index;
        return *reinterpret_cast<ContainerReference*>(this);
    }

    template <class Container, class Index>
    bool ContainerReference<Container, Index>::has_same_container(const ContainerReference& rhs) const
    {
        return m_pContainer == rhs.m_pContainer;
    }

    template <class Container, class Index>
    ContainerReference<Container, Index>::operator ContainerReference<const Container, Index>() const
    {
        return ContainerReference<const Container, Index> {
            static_cast<const Container*>(m_pContainer),
            m_Index
        };
    }

} // !core
} // !ux3d
