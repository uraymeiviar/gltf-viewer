// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include <tuple>
#include <type_traits>
#include <unordered_map>

#include "core/Algorithms.h"
#include "core/Hash.h"
#include "core/LinkedList.h"
#include "core/TupleHash.h"

namespace ux3d::renderer {
// GltfToRendererMapping is a helper to avoid code duplication in the mapping between gltf objects
// and their corresponding renderer objects.
template <class KEY, class VALUE>
class Mapping {
public:
    virtual ~Mapping() = 0;
    Mapping() = default;

    // call update with a list of all gltf objects. It creates new renderer objects when new gltf objects are found, recreates the renderer object
    // if needsRecreation returns true, calls updateRenderobjectFromGltf for all renderobjects and destroys any renderer object for which the gltf
    // object was destroyed.
    template <typename KEY_WITHOUT_PTR = typename std::remove_pointer<KEY>::type>
    void update(const core::LinkedList<KEY_WITHOUT_PTR>& gltfProperties);

protected:
    // onUpdateMapping can for example be used for uniform updates based on changes in the gltf object. It is called every frame for every object
    virtual bool onUpdateMapping(KEY, VALUE)
    {
        return true;
    }

    // onCreateMapping and onDestroyMapping must be implemented to create and destroy the object in the renderer
    virtual VALUE onCreateMapping(KEY) = 0;
    virtual void onDestroyMapping(KEY, VALUE) = 0;

    // needsRecreation returns true when the renderer object should be destroyed and recreated. This would be the case, e.g. when the permutaion changes
    virtual bool needsRecreation(VALUE, KEY)
    {
        return false;
    };

    template <typename>
    struct is_tuple : std::false_type {
    };
    template <typename... T>
    struct is_tuple<std::tuple<T...>> : std::true_type {
    };
    template <typename... T>
    struct is_tuple<const std::tuple<T...>> : std::true_type {
    };

    template <typename KEY_WITHOUT_PTR = typename std::remove_pointer<KEY>::type>
    KEY getKey(KEY_WITHOUT_PTR& _key);

public:
    // map stores the actual mapping
    std::unordered_map<KEY, VALUE, typename core::Hash<KEY>> map;
};

template <class KEY, class VALUE>
template <typename KEY_WITHOUT_PTR>
inline KEY Mapping<KEY, VALUE>::getKey(KEY_WITHOUT_PTR& _key)
{
    if constexpr (is_tuple<KEY_WITHOUT_PTR>::value || std::is_pointer<KEY_WITHOUT_PTR>::value) {
        return _key;
    } else if constexpr (false == is_tuple<KEY_WITHOUT_PTR>::value) {
        return &_key;
    }
}

template <class KEY, class VALUE>
template <typename KEY_WITHOUT_PTR>
inline void Mapping<KEY, VALUE>::update(const core::LinkedList<KEY_WITHOUT_PTR>& gltfProperties)
{
    core::LinkedList<KEY> erasedProperties;
    core::LinkedList<KEY> createdProperties;

    for (const auto& [key, _] : map) {
        erasedProperties.emplace_back(key);
    }

    for (const auto& updateProperty : gltfProperties) {
        auto key = getKey(updateProperty);
        auto mapIt = map.find(key);
        // add the renderer object if the property is new
        if (mapIt == map.end()) {
            // create the properties later to allow cleanup in destroy properties to happen before creation
            createdProperties.emplace_back(key);
        }
        // recreate the object if for example the permutation changed
        else if (needsRecreation(mapIt->second, key)) {
            onDestroyMapping(mapIt->first, mapIt->second);
            map[key] = onCreateMapping(key);
        }
        // update; for example uniforms
        else {
            onUpdateMapping(mapIt->first, mapIt->second);
        }

        // tick off objects that are still present in the gltf. Only objects that are not in the gltf any more will be left
        // in erasedProperties
        auto eraseIt = ux3d::core::find(erasedProperties.begin(), erasedProperties.end(), key);
        if (eraseIt != erasedProperties.end()) {
            erasedProperties.erase(eraseIt);
        }
    }

    // destroy the render objects for gltf objects that are not part of the gltf any more
    for (const auto erase : erasedProperties) {
        auto erasePtr = map[erase];
        onDestroyMapping(erase, erasePtr);
        map.erase(erase);
    }

    // create the objects that were added to the gltf
    for (const auto createKey : createdProperties) {
        map[createKey] = onCreateMapping(createKey);
    }
}

template <class KEY, class VALUE>
inline Mapping<KEY, VALUE>::~Mapping()
{
    // compulsory virtual destructor definition
}
}
