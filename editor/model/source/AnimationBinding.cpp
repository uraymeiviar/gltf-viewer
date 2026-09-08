#include "model/AnimationBinding.h"
#include "gltf/Animation.h"
#include "model/ProjectManager.h"

using namespace ux3d;
using namespace ux3d::model;

AnimationBinding::AnimationBinding(gltf::Animation* animation)
    : m_gltfId(animation->m_ID)
{
}

gltf::Animation* AnimationBinding::getGltfObject() const
{
    return ProjectManager::getInstance()->getGltf().getByID<gltf::Animation>(m_gltfId);
}

void AnimationBinding::setPlaying(bool _isPlaying)
{
    getGltfObject()->setPlaying(_isPlaying);
}
