// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class Texture;
class IndexLookup;

class TextureMerger {
private:
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    void resolveSampler(const Texture& _SourceTexture, Texture& _Texture);
    void resolveImage(const Texture& sourceTexture, Texture& texture);
    void resolveBasisU(const Texture& sourceTexture, Texture& texture);
    void resolveWebP(const Texture& sourceTexture, Texture& texture);
    void resolveTextureDds(const Texture& sourceTexture, Texture& texture);

public:
    TextureMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const Texture& _SourceTexture, Texture& _Texture);
};
}
