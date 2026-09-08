// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <type_traits>

namespace ux3d {
namespace core {
    // https://en.cppreference.com/w/cpp/types/conditional
    //template <bool B, typename Func, typename ...Args, typename Result = typename std::enable_if<B, std::result_of<Func && (Args&&...)>::type>>
    //typename std::enable_if<B, Result>::type execIf(Func&& t, Args&& ...args) { return t(std::forward<Args>(args)...); }
    //template <bool B, typename ...Args>
    //typename std::enable_if<!B>::type execIf(Args&& ...args) { }

    template <typename T>
    constexpr bool has_nonprimitve_destructor_v = std::is_destructible_v<T> && (std::is_class_v<T> || std::is_union_v<T>);

    template <typename T>
    typename std::enable_if<has_nonprimitve_destructor_v<T>>::type destructIfExists(T* t) { t->~T(); }
    template <typename T>
    typename std::enable_if<!has_nonprimitve_destructor_v<T>>::type destructIfExists(T* t) { (void)t; }

    template <typename T>
    typename std::enable_if<std::is_constructible<T>::value>::type constructIfExists(T* t) { new (t) T(); }
    template <typename T>
    typename std::enable_if<!std::is_constructible<T>::value>::type constructIfExists(T* t) { (void)t; }
}
} // !ux3d
