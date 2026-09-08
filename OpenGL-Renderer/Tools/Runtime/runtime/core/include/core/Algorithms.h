// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Flag.h"
#include "Traits.h"
#include <stddef.h>

namespace ux3d::core {
template <class Container, class T>
size_t findIndexOf(const Container& container, const T* rhs)
{
    size_t i = 0;
    //for (const T& el : container)
    for (auto iter = container.begin(); iter != container.end(); iter++) {
        const T* el = &*iter;
        if (el == rhs) {
            return i;
        }
        i++;
    }
    return ~0u;
}

template <class Container, class Index, class T = typename Container::value_type>
inline auto safeAt(Container& _Container, const Index& _Index)
{
    bool invalidIndex = _Index < 0 || _Index >= static_cast<Index>(_Container.size());

    if constexpr (std::is_pointer<T>::value) {
        if (invalidIndex) {
            return static_cast<T>(nullptr);
        }
        return _Container.at(static_cast<size_t>(_Index));
    } else {
        if (invalidIndex) {
            return static_cast<T*>(nullptr);
        }
        return &_Container.at(static_cast<size_t>(_Index));
    }
}

template <class Enum, typename Base = typename std::underlying_type_t<Enum>>
bool hasFlag(Flag<Enum> _Enum, Enum _Flag)
{
    return (static_cast<Base>(_Enum) & static_cast<Base>(_Flag)) == static_cast<Base>(_Flag);
}

template <class Container, class Element>
bool remove(Container& _Container, const Element& _Element)
{
    const auto begin = _Container.begin();
    const auto end = _Container.end();

    for (auto it = begin; it != end; ++it) {
        if (&*it == &_Element) {
            _Container.erase(it);
            return true;
        }
    }

    return false;
}

template <class Iterator, class Element>
Iterator find(Iterator _it, Iterator _end, const Element& _Element)
{
    for (; _it != _end && (*_it != _Element); ++_it) {
        // no body
    }

    return _it;
}

template <class Iterator, class Predicate>
Iterator find_if(Iterator _it, Iterator _end, const Predicate& _pred)
{
    for (; _it != _end && _pred(*_it) == false; ++_it) {
        // no body
    }

    return _it;
}

template <class Container, class Element>
decltype(auto) find(Container& _container, const Element& _element)
{
    return find(_container.begin(), _container.end(), _element);
}

template <class Container, class Predicate>
decltype(auto) find_if(Container& _container, const Predicate& _predicate)
{
    return find_if(_container.begin(), _container.end(), _predicate);
}

template <class Container, class Element>
bool contains(const Container& _container, const Element& _element)
{
    return find(_container, _element) != _container.end();
}

template <typename Container>
bool equal(const Container& _a, const Container& _b)
{
    if (_a.size() != _b.size()) {
        return false;
    }

    auto aIt = _a.begin();
    auto bIt = _b.begin();
    auto aEnd = _a.end();
    auto bEnd = _b.end();
    for (; aIt != aEnd && bIt != bEnd; ++aIt, ++bIt) {
        if (*aIt != *bIt) {
            return false;
        }
    }

    return true;
}
}
