// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/LinkedList.h"
#include "core/Vector.h"

namespace ux3d::gltf {
class ChildOfRoot;
class GltfObject;
class Gltf;

class Scene;
class Node;
class Camera;
class Mesh;
class Material;
class Texture;
class Image;
class Accessor;
class BufferView;
class Skin;
class Animation;
class ImageBasedLight;
class Light;
class AudioEmitter;
class AudioSource;

class ChildCollector {
public:
    ChildCollector(const Gltf& _root);
    ChildCollector(const core::Vector<const ChildOfRoot*>& _initialList);

    template <typename ChildOfRootType>
    void addInitialElements(const core::LinkedList<ChildOfRootType>& _elements);

    void collect(core::Vector<const ChildOfRoot*>& _outCompleteList);
    void collectGltfObjects(core::Vector<const GltfObject*>& _outCompleteList);

private:
    void iterativeCollect();
    void addChildrenToWorkList(const ChildOfRoot* parent);
    void addToWorkList(const core::Vector<const ChildOfRoot*>& _elementList);
    void addToWorkList(const ChildOfRoot* _element);

    void addToGltfObjectList(const GltfObject* _element);

    void addSceneChildren(const Scene* scene);
    void addNodeChildren(const Node* node);
    void addCameraChildren(const Camera* _camera);
    void addMeshChildren(const Mesh* mesh);
    void addMaterialChildren(const Material* material);
    void addTextureChildren(const Texture* texture);
    void addImageChildren(const Image* image);
    void addAccessorChildren(const Accessor* accessor);
    void addBufferViewChildren(const BufferView* bufferView);
    void addSkinChildren(const Skin* skin);
    void addAnimationChildren(const Animation* animation);
    void addLightChildren(const Light* _light);
    void addAudioEmitterChildren(const AudioEmitter* _emitter);
    void addAudioSourceChildren(const AudioSource* _source);
    void addRootGltfObjects(const Gltf* _gltf);

    core::Vector<const ChildOfRoot*> m_childOfRootList;
    core::Vector<const GltfObject*> m_gltfObjectList;

    bool m_dirtyState = true;
};
}
