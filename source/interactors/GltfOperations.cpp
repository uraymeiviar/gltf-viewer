#include "interactors/GltfOperations.h"
#include "model/ProjectManager.h"

using namespace ux3d;
using namespace ux3d::model;

bool gltf_operations::hasSkinning()
{
    return !ProjectManager::getInstance()->getGltf().getSkins().empty();
}

bool gltf_operations::hasAnimations()
{
    return !ProjectManager::getInstance()->getGltf().getAnimations().empty();
}

bool gltf_operations::hasEmitters()
{
    return !ProjectManager::getInstance()->getGltf().getAudioEmitters().empty();
}

bool gltf_operations::hasMorphing()
{
    for (auto& mesh : ProjectManager::getInstance()->getGltf().getMeshes()) {
        for (auto& primitive : mesh.getPrimitives()) {
            if (!primitive.getTargets().empty()) {
                return true;
            }
        }
    }
    return false;
}