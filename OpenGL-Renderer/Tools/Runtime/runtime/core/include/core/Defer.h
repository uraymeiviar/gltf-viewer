#pragma once

#include <functional>

namespace ux3d::core {
/**
 * @brief Defer execution of a function until the current scope is left.
 * This is helpful to implement cleanup functionality.
 * In C++ this is traditionally called a "Scope Guard" (you can also use ux3d::ScopeGuard instead)
 *
 * E.g.
 *
 * auto* someResource = createResource();
 * if (not doSomethingThatCanFail()) {
 *  destroyResource(someResource);
 *  return false;
 * }
 * ...
 * destroyResource(someResource);
 *
 * can be rewritten to
 *
 * auto* someResource = createResource();
 * Defer([]{ destroyResource(someResource); })
 * if (not doSomethingThatCanFail()) {
 *  return false;
 * }
 * ...
 *
 */
class Defer {
private:
    // non copyable
    auto operator=(const Defer&) -> Defer& = delete;
    Defer(const Defer&) = delete;

public:
    template <class F>
    Defer(F const& func)
        : _func(func)
    {
    }

    Defer(std::function<void()>&& func)
        : _func(func)
    {
    }

    ~Defer()
    {
        _func();
    }

    void dismiss()
    {
        this->_func = [] {};
    }

    Defer(Defer&& other)
        : _func(std::move(other._func))
    {
        other.dismiss();
    }

private:
    std::function<void()> _func;
};

// Ensure the C++ lingo is also supported
using ScopeGuard = Defer;
}
