// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "renderer/AudioBuffer.h"
#include "renderer/AudioEmitter.h"
#include "renderer/Camera.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"
#include "renderer/RenderPass.h"
#include "renderer/TransformHierarchy.h"
#include "renderer/gl/Buffer.h"
#include "renderer/gl/Texture.h"

#include <algorithm>
#include <list>
#include <memory>

namespace ux3d::renderer {
/// A factory for resources used in rendering.
class ResourceManager {
public:
    ResourceManager(gl::GL* gl);
    virtual ~ResourceManager() = default;

    /// Creates a resource in-place, forwarding the arguments to its constructor.
    template <class T, class... Args>
    T* create(Args&&... args);

    template <class T, class... Args>
    T* createWithGLContext(Args&&... args);

    /// Destroys a given resource.
    /// Resources are compared by memory address.
    template <class T>
    void destroy(const T* resource);

    /// Destroys all resources of a given type.
    template <class T>
    void destroyAllOf();

    /// Returns true if the given resource is held by this resource manager.
    /// Resources are compared by memory address.
    template <class T>
    bool holds(const T* resource) const;

private:
    /// Returns a resource list depending on the given resource type.
    template <class T>
    std::list<T>& listByType();

    /// Returns a read-only resource list depending on the given resource type.
    template <class T>
    const std::list<T>& listByType() const;

private:
    // TODO: replace lists with tables. Requires a replacement of pointers as well
    // TODO: add more resources
    std::list<gl::SamplerState> m_samplerStates;
    std::list<gl::Texture> m_textures;
    std::list<gl::Buffer> m_buffers;
    std::list<EnvironmentMap> m_environmentMaps;
    std::list<Mesh> m_meshes;
    std::list<Material> m_materials;
    std::list<Camera> m_cameras;
    std::list<std::unique_ptr<RenderPass>> m_renderPasses;

    // TODO: make protected
public:
    TransformHierarchy m_transformHierarchy; // TODO: replace by a list of transform hierarchy nodes
    gl::GL* m_gl;
};

template <class T, class... Args>
T* ResourceManager::create(Args&&... args)
{
    if constexpr (std::is_base_of_v<RenderPass, T>) {
        T* pass = new T(std::forward<Args>(args)...);
        m_renderPasses.emplace_back(pass);
        return pass;
    } else {
        std::list<T>& list = listByType<T>();
        return &list.emplace_back(std::forward<Args>(args)...);
    }
}

template <class T, class... Args>
T* ResourceManager::createWithGLContext(Args&&... args)
{
    return create<T, gl::GL*&, Args...>(m_gl, std::forward<Args>(args)...);
}

template <class T>
void ResourceManager::destroy(const T* resource)
{
    if constexpr (std::is_base_of_v<RenderPass, T>) {
        auto eraseIt = std::find_if(m_renderPasses.begin(), m_renderPasses.end(), [resource](const auto& pass) { return pass.get() == resource; });
        if (eraseIt != m_renderPasses.end()) {
            m_renderPasses.erase(eraseIt);
        }
    } else {
        std::list<T>& list = listByType<T>();
        auto eraseIt = std::find_if(list.begin(), list.end(), [resource](const auto& x) { return &x == resource; });
        if (eraseIt == list.end()) {
            core::debugLog(core::LogLevel::Warning, "Cannot delete resource because it is not held by this resource manager.");
            return;
        }
        list.erase(eraseIt);
    }
}

template <class T>
void ResourceManager::destroyAllOf()
{
    if constexpr (std::is_base_of_v<RenderPass, T>) {
        m_renderPasses.clear();
    } else {
        std::list<T>& list = listByType<T>();
        list.clear();
    }
}

template <class T>
bool ResourceManager::holds(const T* resource) const
{
    if constexpr (std::is_base_of_v<RenderPass, T>) {
        auto iter = std::find_if(m_renderPasses.begin(), m_renderPasses.end(), [resource](const RenderPass* pass) { return pass == resource; });
        return iter != m_renderPasses.end();
    } else {
        const std::list<T>& list = listByType<T>();
        const auto iter = std::find_if(list.begin(), list.end(), [resource](const auto& x) { return &x == resource; });
        return iter != list.end();
    }
}

template <>
std::list<gl::SamplerState>& ResourceManager::listByType<gl::SamplerState>();

template <>
std::list<Material>& ResourceManager::listByType<Material>();

template <>
std::list<Mesh>& ResourceManager::listByType<Mesh>();

template <>
std::list<gl::Texture>& ResourceManager::listByType<gl::Texture>();

template <>
std::list<gl::Buffer>& ResourceManager::listByType<gl::Buffer>();

template <>
std::list<EnvironmentMap>& ResourceManager::listByType<EnvironmentMap>();

template <>
std::list<Camera>& ResourceManager::listByType<Camera>();

template <>
const std::list<gl::SamplerState>& ResourceManager::listByType<gl::SamplerState>() const;

template <>
const std::list<Material>& ResourceManager::listByType<Material>() const;

template <>
const std::list<Mesh>& ResourceManager::listByType<Mesh>() const;

template <>
const std::list<gl::Texture>& ResourceManager::listByType<gl::Texture>() const;

template <>
const std::list<gl::Buffer>& ResourceManager::listByType<gl::Buffer>() const;

template <>
const std::list<EnvironmentMap>& ResourceManager::listByType<EnvironmentMap>() const;

template <>
const std::list<Camera>& ResourceManager::listByType<Camera>() const;
}
