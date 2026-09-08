// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/ResourceManager.h"

using namespace ux3d;
using namespace ux3d::renderer;

ResourceManager::ResourceManager(gl::GL* gl)
    : m_gl(gl)
{
}

template <>
std::list<gl::SamplerState>& ResourceManager::listByType<gl::SamplerState>()
{
    return m_samplerStates;
}

template <>
std::list<Material>& ResourceManager::listByType<Material>()
{
    return m_materials;
}

template <>
std::list<Mesh>& ResourceManager::listByType<Mesh>()
{
    return m_meshes;
}

template <>
std::list<gl::Texture>& ResourceManager::listByType<gl::Texture>()
{
    return m_textures;
}

template <>
std::list<gl::Buffer>& ResourceManager::listByType<gl::Buffer>()
{
    return m_buffers;
}

template <>
std::list<EnvironmentMap>& ResourceManager::listByType<EnvironmentMap>()
{
    return m_environmentMaps;
}

template <>
std::list<Camera>& ResourceManager::listByType<Camera>()
{
    return m_cameras;
}

template <>
const std::list<gl::SamplerState>& ResourceManager::listByType<gl::SamplerState>() const
{
    return m_samplerStates;
}

template <>
const std::list<Material>& ResourceManager::listByType<Material>() const
{
    return m_materials;
}

template <>
const std::list<Mesh>& ResourceManager::listByType<Mesh>() const
{
    return m_meshes;
}

template <>
const std::list<gl::Texture>& ResourceManager::listByType<gl::Texture>() const
{
    return m_textures;
}

template <>
const std::list<gl::Buffer>& ResourceManager::listByType<gl::Buffer>() const
{
    return m_buffers;
}

template <>
const std::list<EnvironmentMap>& ResourceManager::listByType<EnvironmentMap>() const
{
    return m_environmentMaps;
}

template <>
const std::list<Camera>& ResourceManager::listByType<Camera>() const
{
    return m_cameras;
}
