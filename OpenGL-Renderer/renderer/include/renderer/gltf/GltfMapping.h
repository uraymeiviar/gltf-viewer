// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "renderer/Mapping.h"

#include <functional>
#include <vector>

namespace ux3d::renderer {
class GltfRenderLayer;
}
namespace ux3d::renderer {
// GltfToRendererMapping is a helper to avoid code duplication in the mapping between gltf objects
// and their corresponding renderer objects.
template <class GLTF, class RENDERER>
class GltfMapping : public Mapping<GLTF, RENDERER> {
public:
    virtual ~GltfMapping() = 0;
    GltfMapping(GltfRenderLayer& scene)
        : m_gltfRenderLayer(scene)
    {
    }

protected:
    GltfRenderLayer& m_gltfRenderLayer;
};

template <class GLTF, class RENDERER>
inline GltfMapping<GLTF, RENDERER>::~GltfMapping()
{
    // compulsory virtual destructor definition
}

// TODO: remove the following once all mappings are using the GltfMapping class

// GltfToRendererMapping is a helper to avoid code duplication in the mapping between gltf objects
// and their corresponding renderer objects.
template <class GLTF, class RENDERER>
struct GltfToRendererMapping {
    // updateRendererobjectFromGltf can be used for uniform updates based on changes in the gltf object. It is called every frame for every object
    std::function<bool(GLTF, RENDERER)> updateRendererobjectFromGltf = [](GLTF, RENDERER) -> bool { return true; };

    // createRendererObjectFromGltf and destroyRendererObjectFromGltf must be implemented to create and destroy the object in the renderer
    std::function<RENDERER(GLTF)> createRendererObjectFromGltf;
    std::function<void(GLTF, RENDERER)> destroyRendererobjectFromGltf;

    // needsRecreation returns true when the renderer object should be destroyed and recreated. This would be the case, e.g. when the permutaion changes
    std::function<bool(RENDERER, GLTF)> needsRecreation = [](RENDERER, GLTF) { return false; };

    // map stores the actual mapping
    std::unordered_map<GLTF, RENDERER, typename core::Hash<GLTF>> map;

    // call update with a list of all gltf objects. It creates new renderer objects when new gltf objects are found, recreates the renderer object
    // if needsRecreation returns true, calls updateRenderobjectFromGltf for all renderobjects and destroys any renderer object for which the gltf
    // object was destroyed.

    template <typename>
    struct is_tuple : std::false_type {
    };
    template <typename... T>
    struct is_tuple<std::tuple<T...>> : std::true_type {
    };
    template <typename... T>
    struct is_tuple<const std::tuple<T...>> : std::true_type {
    };

    template <typename GLTF_WITHOUT_PTR = typename std::remove_pointer<GLTF>::type>
    GLTF getKey(GLTF_WITHOUT_PTR& _key)
    {
        if constexpr (is_tuple<GLTF_WITHOUT_PTR>::value || std::is_pointer<GLTF_WITHOUT_PTR>::value) {
            return _key;
        } else if constexpr (false == is_tuple<GLTF_WITHOUT_PTR>::value) {
            return &_key;
        }
    }

    template <typename GLTF_WITHOUT_PTR = typename std::remove_pointer<GLTF>::type>
    void update(const core::LinkedList<GLTF_WITHOUT_PTR>& gltfProperties)
    {
        std::vector<GLTF> erasedProperties;
        erasedProperties.reserve(map.size());
        std::unordered_set<GLTF, typename core::Hash<GLTF>> alive;
        alive.reserve(map.size());
        std::vector<GLTF> toBeCreated;
        toBeCreated.reserve(gltfProperties.size());
        std::vector<std::tuple<GLTF, RENDERER>> toBeUpdated;
        toBeUpdated.reserve(gltfProperties.size());
        std::vector<std::tuple<GLTF, RENDERER>> toBeRecreated;

        for (const auto& [key, _] : map) {
            erasedProperties.emplace_back(key);
            alive.insert(key);
        }

        for (const auto& updateProperty : gltfProperties) {
            auto key = getKey(updateProperty);
            auto mapIt = map.find(key);
            // add the renderer object if the property is new
            if (mapIt == map.end()) {
                toBeCreated.emplace_back(key);
            }
            // recreate the object if for example the permutation changed
            else if (needsRecreation(mapIt->second, key)) {
                toBeRecreated.emplace_back(mapIt->first, mapIt->second);
            }
            // update for example uniforms
            else {
                toBeUpdated.emplace_back(mapIt->first, mapIt->second);
            }

            // Objects still present in the glTF are no longer candidates for removal.
            alive.erase(key);
        }

        // destroy the render objects for gltf objects that are not part of the gltf any more
        for (const auto& erase : erasedProperties) {
            if (!alive.contains(erase)) {
                continue;
            }
            auto mapIt = map.find(erase);
            if (mapIt != map.end()) {
                destroyRendererobjectFromGltf(mapIt->first, mapIt->second);
                map.erase(mapIt);
            }
        }

        for (const auto& [key, value] : toBeRecreated) {
            destroyRendererobjectFromGltf(key, value);
            map[key] = createRendererObjectFromGltf(key);
        }

        for (const auto& key : toBeCreated) {
            map[key] = createRendererObjectFromGltf(key);
        }

        for (const auto& [key, value] : toBeUpdated) {
            updateRendererobjectFromGltf(key, value);
        }
    }
};

}
