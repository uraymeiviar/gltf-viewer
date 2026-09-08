// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class Mesh;
class Material;
class Primitive;
class IndexLookup;
class MaterialVariant;

class MeshMerger {
private:
    Gltf& m_destination;
    const Gltf& m_source;
    const IndexLookup& m_indices;

    void resolvePrimitives(const Mesh& _sourceMesh, Mesh& _mesh);
    void resolveXMP(const Mesh& _sourceMesh, Mesh& _mesh);
    void resolveMaterials(const Primitive& _sourcePrimitive, Primitive& _primitive);
    void resolveAccessors(const Primitive& _sourcePrimitive, Primitive& _primitive);
    void resolveTargets(const Primitive& _sourcePrimitive, Primitive& _primitive);
    void resolveDracoMeshCompression(const Primitive& _sourcePrimitive, Primitive& _primitive);
    void resolveMaterialVariants(const Primitive& _sourcePrimitive, Primitive& _primitive);

    const MaterialVariant* getDestVariantFromSourceVariant(const MaterialVariant* variant);
    gltf::Material* getDestMaterialFromSourceMaterial(gltf::Material* material);

public:
    MeshMerger(Gltf& _destination, const Gltf& _source, const IndexLookup& _indexLookup)
        : m_destination(_destination)
        , m_source(_source)
        , m_indices(_indexLookup)
    {
    }

    bool resolveDependencies(const Mesh& _sourceMesh, Mesh& _mesh);
};
}
