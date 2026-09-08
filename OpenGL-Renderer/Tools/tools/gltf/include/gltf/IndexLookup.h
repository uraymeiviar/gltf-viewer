// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <core/Vector.h>
#include <unordered_map>

namespace ux3d::core {
template <typename T>
class LinkedList;
}

namespace ux3d::gltf {
template <typename T>
using IndexMap = std::unordered_map<const T*, size_t>;

class Gltf;
class ChildOfRoot;
class GltfObject;
class Scene;
class Node;
class Camera;
class Light;
class Mesh;
class Material;
class Texture;
class Sampler;
class Image;
class Accessor;
class BufferView;
class Buffer;
class Animation;
class Skin;
class MaterialVariant;
class AudioSource;
class AudioEmitter;
class Packet;

class IndexLookup {
public:
    explicit IndexLookup(const Gltf& _gltf, const core::Vector<const ChildOfRoot*>& _pointersToKeep = {}, const core::Vector<const GltfObject*>& _gltfObjsToKeep = {});
    explicit IndexLookup(const Gltf& _gltf, const Gltf& _offsets);

    size_t operator[](const Scene* _pKey) const { return m_SceneIndices.at(_pKey); }
    size_t operator[](const Node* _pKey) const { return m_NodeIndices.at(_pKey); }
    size_t operator[](const Camera* _pKey) const { return m_CameraIndices.at(_pKey); }
    size_t operator[](const Light* _pKey) const { return m_LightIndices.at(_pKey); }
    size_t operator[](const Mesh* _pKey) const { return m_MeshIndices.at(_pKey); }
    size_t operator[](const Material* _pKey) const { return m_MaterialIndices.at(_pKey); }
    size_t operator[](const Texture* _pKey) const { return m_TextureIndices.at(_pKey); }
    size_t operator[](const Sampler* _pKey) const { return m_SamplerIndices.at(_pKey); }
    size_t operator[](const Image* _pKey) const { return m_ImageIndices.at(_pKey); }
    size_t operator[](const Accessor* _pKey) const { return m_AccessorIndices.at(_pKey); }
    size_t operator[](const BufferView* _pKey) const { return m_BufferViewIndices.at(_pKey); }
    size_t operator[](const Buffer* _pKey) const { return m_BufferIndices.at(_pKey); }
    size_t operator[](const Animation* _pKey) const { return m_AnimationIndices.at(_pKey); }
    size_t operator[](const Skin* _pKey) const { return m_SkinIndices.at(_pKey); }
    size_t operator[](const MaterialVariant* _pKey) const { return m_MaterialVariantIndices.at(_pKey); }

    size_t operator[](const AudioSource* _pKey) const
    {
        return m_AudioSourceIndices.at(_pKey);
    }
    size_t operator[](const AudioEmitter* _pKey) const { return m_AudioEmitterIndices.at(_pKey); }
    size_t operator[](const Packet* _pKey) const { return m_XMPPacketsIndices.at(_pKey); }

private:
    IndexMap<Scene> m_SceneIndices;
    IndexMap<Node> m_NodeIndices;
    IndexMap<Camera> m_CameraIndices;
    IndexMap<Light> m_LightIndices;
    IndexMap<Mesh> m_MeshIndices;
    IndexMap<Material> m_MaterialIndices;
    IndexMap<Texture> m_TextureIndices;
    IndexMap<Sampler> m_SamplerIndices;
    IndexMap<Image> m_ImageIndices;
    IndexMap<Accessor> m_AccessorIndices;
    IndexMap<BufferView> m_BufferViewIndices;
    IndexMap<Buffer> m_BufferIndices;
    IndexMap<Animation> m_AnimationIndices;
    IndexMap<Skin> m_SkinIndices;
    IndexMap<MaterialVariant> m_MaterialVariantIndices;

    IndexMap<AudioSource> m_AudioSourceIndices;
    IndexMap<AudioEmitter> m_AudioEmitterIndices;
    IndexMap<Packet> m_XMPPacketsIndices;

    template <typename T>
    void fillIndices(const core::LinkedList<T>& _source, IndexMap<T>& _destination, const core::Vector<const ChildOfRoot*>& _pointersToKeep);

    template <typename T>
    void fillIndices(const core::LinkedList<T>& _source, IndexMap<T>& _destination, const core::Vector<const GltfObject*>& _gltfObjsToKeep);

    template <typename T>
    void fillIndices(const core::LinkedList<T>& _source, IndexMap<T>& _destination, size_t _offset);
};
}
