// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"

namespace ux3d::gltf {
class Node;
class Scene;
class Mesh;
class Buffer;
class BufferView;
class Accessor;
class Image;
class Material;
class Texture;
class Sampler;
class Camera;
class Light;
class Animation;
class Skin;
class ImageBasedLight;
class MaterialVariant;
class AudioSource;
class AudioEmitter;

struct PointerLookup {
    core::Vector<Node*> Nodes;
    core::Vector<Scene*> Scenes;
    core::Vector<Mesh*> Meshes;
    core::Vector<Buffer*> Buffers;
    core::Vector<BufferView*> BufferViews;
    core::Vector<Accessor*> Accessors;
    core::Vector<Image*> Images;
    core::Vector<Material*> Materials;
    core::Vector<Texture*> Textures;
    core::Vector<Sampler*> Samplers;
    core::Vector<Camera*> Cameras;
    core::Vector<Light*> Lights;
    core::Vector<Animation*> Animations;
    core::Vector<Skin*> Skins;
    core::Vector<ImageBasedLight*> ImageBasedLights;
    core::Vector<MaterialVariant*> MaterialVariants;
    core::Vector<AudioSource*> AudioSources;
    core::Vector<AudioEmitter*> AudioEmitters;

    bool add(Node* _Pointer) { return Nodes.emplace_back(_Pointer); }
    bool add(Scene* _Pointer) { return Scenes.emplace_back(_Pointer); }
    bool add(Mesh* _Pointer) { return Meshes.emplace_back(_Pointer); }
    bool add(Buffer* _Pointer) { return Buffers.emplace_back(_Pointer); }
    bool add(BufferView* _Pointer) { return BufferViews.emplace_back(_Pointer); }
    bool add(Accessor* _Pointer) { return Accessors.emplace_back(_Pointer); }
    bool add(Image* _Pointer) { return Images.emplace_back(_Pointer); }
    bool add(Material* _Pointer) { return Materials.emplace_back(_Pointer); }
    bool add(Texture* _Pointer) { return Textures.emplace_back(_Pointer); }
    bool add(Sampler* _Pointer) { return Samplers.emplace_back(_Pointer); }
    bool add(Camera* _Pointer) { return Cameras.emplace_back(_Pointer); }
    bool add(Light* _Pointer) { return Lights.emplace_back(_Pointer); }
    bool add(Animation* _Pointer) { return Animations.emplace_back(_Pointer); }
    bool add(Skin* _Pointer) { return Skins.emplace_back(_Pointer); }
    bool add(ImageBasedLight* _Pointer) { return ImageBasedLights.emplace_back(_Pointer); }
    bool add(MaterialVariant* _Pointer) { return MaterialVariants.emplace_back(_Pointer); }
    bool add(AudioSource* _Pointer) { return AudioSources.emplace_back(_Pointer); }
    bool add(AudioEmitter* _Pointer) { return AudioEmitters.emplace_back(_Pointer); }
};
}
