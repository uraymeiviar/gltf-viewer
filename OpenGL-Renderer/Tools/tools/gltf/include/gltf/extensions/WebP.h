#pragma once

#include "gltf/GltfObject.h"

namespace ux3d::gltf {
class Image;

class WebP : public GltfObject {
public:
    Image* getSource() const;
    void setSource(Image* _pSource);

private:
    Image* m_pSource = nullptr;
};
}
