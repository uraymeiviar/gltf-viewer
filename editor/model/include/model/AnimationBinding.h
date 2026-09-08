#pragma once

#include "core/LinkedList.h"
#include "core/String.h"
namespace ux3d::gltf {
class Animation;
}

namespace ux3d::model {

// Yes this class is now a shadow of its former self and rather useless.
// I left it like this to avoid long drawnout refactoring while getting rid of a lot of bindings.

class AnimationBinding {
public:
    AnimationBinding(gltf::Animation* animation);

    void setPlaying(bool _isPlaying);

private:
    gltf::Animation* getGltfObject() const;

    uint32_t m_gltfId;
};

}
