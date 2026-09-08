// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/IndexLookup.h"
#include "core/Algorithms.h"
#include "core/LinkedList.h"
#include "gltf/Gltf.h"
#include "gltf/XMP.h"

using namespace ux3d;
using namespace ux3d::gltf;

IndexLookup::IndexLookup(const Gltf& _gltf, const core::Vector<const ChildOfRoot*>& _pointersToKeep, const core::Vector<const GltfObject*>& _gltfObjsToKeep)
{
    fillIndices(_gltf.getScenes(), m_SceneIndices, _pointersToKeep);
    fillIndices(_gltf.getNodes(), m_NodeIndices, _pointersToKeep);
    fillIndices(_gltf.getCameras(), m_CameraIndices, _pointersToKeep);
    fillIndices(_gltf.getLights(), m_LightIndices, _pointersToKeep);
    fillIndices(_gltf.getMeshes(), m_MeshIndices, _pointersToKeep);
    fillIndices(_gltf.getMaterials(), m_MaterialIndices, _pointersToKeep);
    fillIndices(_gltf.getTextures(), m_TextureIndices, _pointersToKeep);
    fillIndices(_gltf.getSamplers(), m_SamplerIndices, _pointersToKeep);
    fillIndices(_gltf.getImages(), m_ImageIndices, _pointersToKeep);
    fillIndices(_gltf.getAccessors(), m_AccessorIndices, _pointersToKeep);
    fillIndices(_gltf.getBufferViews(), m_BufferViewIndices, _pointersToKeep);
    fillIndices(_gltf.getBuffers(), m_BufferIndices, _pointersToKeep);
    fillIndices(_gltf.getAnimations(), m_AnimationIndices, _pointersToKeep);
    fillIndices(_gltf.getSkins(), m_SkinIndices, _pointersToKeep);
    fillIndices(_gltf.getMaterialVariants(), m_MaterialVariantIndices, _pointersToKeep);
    fillIndices(_gltf.getAudioSources(), m_AudioSourceIndices, _pointersToKeep);
    fillIndices(_gltf.getAudioEmitters(), m_AudioEmitterIndices, _pointersToKeep);

    if (_gltf.hasXMP()) {
        fillIndices(_gltf.getXMP().value().getPackets(), m_XMPPacketsIndices, _gltfObjsToKeep);
    }
}

IndexLookup::IndexLookup(const Gltf& _gltf, const Gltf& _offsets)
{
    fillIndices(_gltf.getScenes(), m_SceneIndices, _offsets.getScenes().size());
    fillIndices(_gltf.getNodes(), m_NodeIndices, _offsets.getNodes().size());
    fillIndices(_gltf.getCameras(), m_CameraIndices, _offsets.getCameras().size());
    fillIndices(_gltf.getLights(), m_LightIndices, _offsets.getLights().size());
    fillIndices(_gltf.getMeshes(), m_MeshIndices, _offsets.getMeshes().size());
    fillIndices(_gltf.getMaterials(), m_MaterialIndices, _offsets.getMaterials().size());
    fillIndices(_gltf.getTextures(), m_TextureIndices, _offsets.getTextures().size());
    fillIndices(_gltf.getSamplers(), m_SamplerIndices, _offsets.getSamplers().size());
    fillIndices(_gltf.getImages(), m_ImageIndices, _offsets.getImages().size());
    fillIndices(_gltf.getAccessors(), m_AccessorIndices, _offsets.getAccessors().size());
    fillIndices(_gltf.getBufferViews(), m_BufferViewIndices, _offsets.getBufferViews().size());
    fillIndices(_gltf.getBuffers(), m_BufferIndices, _offsets.getBuffers().size());
    fillIndices(_gltf.getAnimations(), m_AnimationIndices, _offsets.getAnimations().size());
    fillIndices(_gltf.getSkins(), m_SkinIndices, _offsets.getSkins().size());
    fillIndices(_gltf.getMaterialVariants(), m_MaterialVariantIndices, _offsets.getMaterialVariants().size());
    fillIndices(_gltf.getAudioSources(), m_AudioSourceIndices, _offsets.getAudioSources().size());
    fillIndices(_gltf.getAudioEmitters(), m_AudioEmitterIndices, _offsets.getAudioEmitters().size());

    if (_gltf.hasXMP()) {
        if (_offsets.hasXMP()) {
            fillIndices(_gltf.getXMP().value().getPackets(), m_XMPPacketsIndices, _offsets.getXMP().value().getPackets().size());
        } else {
            fillIndices(_gltf.getXMP().value().getPackets(), m_XMPPacketsIndices, 0);
        }
    }
}

template <typename T>
void IndexLookup::fillIndices(const core::LinkedList<T>& _source, IndexMap<T>& _destination, const core::Vector<const ChildOfRoot*>& _pointersToKeep)
{
    size_t index = 0;
    for (const T& element : _source) {
        if (_pointersToKeep.empty() || core::contains(_pointersToKeep, &element)) {
            _destination[&element] = index;
            ++index;
        }
    }
}

template <typename T>
void IndexLookup::fillIndices(const core::LinkedList<T>& _source, IndexMap<T>& _destination, const core::Vector<const GltfObject*>& _gltfObjsToKeep)
{
    size_t index = 0;
    for (const T& element : _source) {
        if (_gltfObjsToKeep.empty() || core::contains(_gltfObjsToKeep, &element)) {
            _destination[&element] = index;
            ++index;
        }
    }
}

template <typename T>
void IndexLookup::fillIndices(const core::LinkedList<T>& _source, IndexMap<T>& _destination, size_t _offset)
{
    size_t index = _offset;
    for (const auto& element : _source) {
        _destination[&element] = index;
        ++index;
    }
}
