// Copyright (c) 2021 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class IndexLookup;
class Image;

class ImageMerger {
private:
    Gltf& m_destination;
    const IndexLookup& m_indices;

public:
    ImageMerger(Gltf& _destination, const IndexLookup& _indexLookup)
        : m_destination(_destination)
        , m_indices(_indexLookup)
    {
    }

    void resolveXMP(const Image& _srcImage, Image& _dstImage);
    bool resolveDependencies(const Image& _srcImage, Image& _dstImage);
};
}
