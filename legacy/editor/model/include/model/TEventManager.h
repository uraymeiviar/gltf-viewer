// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <type_traits>

#include "TBinding.h"

namespace ux3d::model {
// TEventManager is a template base class for event managers. The template arguments are the
// derived class, the binding class that inherits from TBinding and the Presentation interface.
template <typename DerivedEventManager, typename Binding, typename Presentation>
class TEventManager {
    static_assert(std::is_base_of<TBinding<Presentation>, Binding>::value, "Event manager model bindings must inherit from IBinding");

protected:
    TEventManager() = default;
    // broadcast receivers are mainly there for backwards compatibility
    std::set<Presentation*> m_broadcastReceivers;
    std::multimap<typename Binding::ID, Binding*> m_modelIdRegistrations;

public:
    ~TEventManager() = default;
    TEventManager(TEventManager const&) = delete;
    void operator=(TEventManager const&) = delete;

    // registerModel registers the presentation associated with a binding as callback receiver
    void registerModel(Binding& _model)
    {
        m_modelIdRegistrations.emplace(_model.getID(), &_model);
    }
    // unregisterModel unregisters the presentation associated with a binding as callback receiver
    void unregisterModel(Binding& _model)
    {
        auto range = m_modelIdRegistrations.equal_range(_model.getID());

        auto it = std::find_if(range.first, range.second, [&_model](auto pair) { return pair.second == &_model; });
        if (it != range.second) {
            m_modelIdRegistrations.erase(it);
        }
    }

    // registerBroadcastReceiver registers a broadcast receiver presentation. It will receive all callbacks, regardless of the model data that triggered the event. Mainly there for backwards compatiblility
    void registerBroadcastReceiver(Presentation& _presentation)
    {
        m_broadcastReceivers.insert(&_presentation);
    }
    void unregisterBroadcastReceiver(Presentation& _presentation)
    {
        auto it = std::find(m_broadcastReceivers.begin(), m_broadcastReceivers.end(), &_presentation);
        if (it != m_broadcastReceivers.end()) {
            m_broadcastReceivers.erase(it);
        }
    }

    // broadcast calls all presentations that are registerd as broadcast receivers
    static void broadcast(const std::function<void(Presentation&)>& _withFunc)
    {
        DerivedEventManager::getInstance()->_broadcast(_withFunc);
    }

    // _broadcast calls all presentations that are registerd as broadcast receivers
    void _broadcast(const std::function<void(Presentation&)>& _withFunc)
    {
        for (auto it : m_broadcastReceivers) {
            _withFunc(*it);
        }
    }

    // present calls all presentations that are registerd to a model data object. This can include multiple bindings.
    static void present(typename Binding::ID _binding, const std::function<void(Presentation&)>& _withFunc)
    {
        DerivedEventManager::getInstance()->_present(_binding, _withFunc);
    }

    // _present calls all presentations that are registerd to a model data object. This can include multiple bindings.
    void _present(typename Binding::ID _binding, const std::function<void(Presentation&)>& _withFunc)
    {
        const auto itlow = m_modelIdRegistrations.lower_bound(_binding);
        const auto itup = m_modelIdRegistrations.upper_bound(_binding);
        for (auto it = itlow; it != itup; ++it) {
            if (it->second->isPresentable()) {
                _withFunc(*(it->second->getPresentation()));
            }
        }
    }

    // refresh only calls the presentation of a specific binding
    static void refresh(const Binding& _binding, const std::function<void(Presentation&)>& _withFunc)
    {
        DerivedEventManager::getInstance()->_refresh(_binding, _withFunc);
    }

    // _refresh only calls the presentation of a specific binding
    void _refresh(const Binding& _binding, const std::function<void(Presentation&)>& _withFunc)
    {
        if (_binding.isPresentable()) {
            _withFunc(*_binding.getPresentation());
        }
    }
};
}
