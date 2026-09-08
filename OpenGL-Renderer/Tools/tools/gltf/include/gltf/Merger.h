// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "IndexLookup.h"

#include "merge/AccessorMerger.h"
#include "merge/AnimationMerger.h"
#include "merge/AudioEmitterMerger.h"
#include "merge/AudioSourceMerger.h"
#include "merge/BufferViewMerger.h"
#include "merge/ImageMerger.h"
#include "merge/MaterialMerger.h"
#include "merge/MeshMerger.h"
#include "merge/NodeMerger.h"
#include "merge/SceneMerger.h"
#include "merge/SkinMerger.h"
#include "merge/TextureMerger.h"

namespace ux3d::gltf {
class Gltf;

class Merger {
public:
    Merger(const Gltf& _source, Gltf& _destination);
    bool merge();

private:
    Gltf& m_Destination;
    const Gltf& m_Source;

    const IndexLookup m_Indices;
    const size_t m_OriginalSceneCount = 0;

    SceneMerger m_SceneMerger;
    NodeMerger m_NodeMerger;
    MeshMerger m_MeshMerger;
    MaterialMerger m_MaterialMerger;
    TextureMerger m_TextureMerger;
    AccessorMerger m_AccessorMerger;
    BufferViewMerger m_BufferViewMerger;
    AnimationMerger m_AnimationMerger;
    SkinMerger m_SkinMerger;
    ImageMerger m_ImageMerger;
    AudioEmitterMerger m_AudioEmitterMerger;
    AudioSourceMerger m_AudioSourceMerger;

    template <class ChildOfRoot>
    bool createCopies();

    template <class ChildOfRoot, class Resolver>
    bool resolve(Resolver& _resolver);

    bool createElements();
    bool resolveDependencies();

    void resolveDefaultScene();
    void resolveXMP();
    void resolveExtensionsExtras();

    bool fixURIs();
};
}
