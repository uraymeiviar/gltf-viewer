// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/MeshMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/mesh/MaterialVariantsMapping.h"

#include <limits>

using namespace ux3d;
using namespace ux3d::gltf;

bool MeshMerger::resolveDependencies(const Mesh& _sourceMesh, Mesh& _mesh)
{
    resolveXMP(_sourceMesh, _mesh);
    resolvePrimitives(_sourceMesh, _mesh);
    return true;
}

void MeshMerger::resolvePrimitives(const Mesh& _sourceMesh, Mesh& _mesh)
{
    auto primitiveIter = _mesh.getPrimitives().begin();
    for (const Primitive& sourcePrimitive : _sourceMesh.getPrimitives()) {
        Primitive& primitive = *primitiveIter;

        resolveMaterials(sourcePrimitive, primitive);
        resolveAccessors(sourcePrimitive, primitive);
        resolveTargets(sourcePrimitive, primitive);
        resolveDracoMeshCompression(sourcePrimitive, primitive);
        resolveMaterialVariants(sourcePrimitive, primitive);

        primitiveIter++;
    }
}

void MeshMerger::resolveXMP(const Mesh& source, Mesh& dest)
{
    if (source.hasXMPPacket()) {
        const Packet* pSourcePacket = source.getXMPPacket().value().m_packet;
        const size_t packetIndex = m_indices[pSourcePacket];

        if (m_destination.hasXMP() == false) {
            m_destination.createXMP();
        }

        dest.setXMPPacket(&(m_destination.getXMP().value().getPackets().at(packetIndex)));
    }
}

void MeshMerger::resolveMaterials(const Primitive& _sourcePrimitive, Primitive& _primitive)
{
    const Material* pSourceMaterial = _sourcePrimitive.getMaterial();
    if (pSourceMaterial != nullptr) {
        const size_t materialIndex = m_indices[pSourceMaterial];
        Material& material = m_destination.getMaterials().at(materialIndex);
        _primitive.setMaterial(material);
    }
}

void MeshMerger::resolveAccessors(const Primitive& _sourcePrimitive, Primitive& _primitive)
{
    {
        const Accessor* pSourceAccessor = _sourcePrimitive.getIndicesAccessor();
        if (pSourceAccessor != nullptr) {
            const size_t accessorIndex = m_indices[pSourceAccessor];
            Accessor& accessor = m_destination.getAccessors().at(accessorIndex);
            _primitive.setIndicesAccessor(accessor);
        }
    }
    auto attributeIter = _primitive.getAttributes().begin();
    for (const Attribute& sourceAttribute : _sourcePrimitive.getAttributes()) {
        const Accessor* pSourceAccessor = sourceAttribute.getAccessor();
        const size_t accessorIndex = m_indices[pSourceAccessor];
        Accessor& accessor = m_destination.getAccessors().at(accessorIndex);
        (*attributeIter).setAccessor(&accessor);

        attributeIter++;
    }
}

void MeshMerger::resolveTargets(const Primitive& _sourcePrimitive, Primitive& _primitive)
{
    auto targetIt = _primitive.getTargets().begin();
    for (const MorphTarget& sourceTarget : _sourcePrimitive.getTargets()) {
        MorphTarget& target = *targetIt;

        const Accessor* pSourcePositionAccessor = sourceTarget.getPositionAccessor();
        if (pSourcePositionAccessor) {
            const size_t accessorIndex = m_indices[pSourcePositionAccessor];
            Accessor& accessor = m_destination.getAccessors().at(accessorIndex);
            target.setPositionAccessor(accessor);
        }

        const Accessor* pSourceNormalAccessor = sourceTarget.getNormalAccessor();
        if (pSourceNormalAccessor) {
            const size_t accessorIndex = m_indices[pSourceNormalAccessor];
            Accessor& accessor = m_destination.getAccessors().at(accessorIndex);
            target.setNormalAccessor(accessor);
        }

        const Accessor* pSourceTangentAccessor = sourceTarget.getTangentAccessor();
        if (pSourceTangentAccessor) {
            const size_t accessorIndex = m_indices[pSourceTangentAccessor];
            Accessor& accessor = m_destination.getAccessors().at(accessorIndex);
            target.setTangentAccessor(accessor);
        }

        targetIt++;
    }
}

void MeshMerger::resolveDracoMeshCompression(const Primitive& _sourcePrimitive, Primitive& _primitive)
{
    if (false == _primitive.hasDracoMeshCompression()) {
        return;
    }

    const size_t bufferViewIndex = m_indices[_sourcePrimitive.getDracoMeshCompression()->getBufferView()];
    BufferView& bufferView = m_destination.getBufferViews().at(bufferViewIndex);
    _primitive.getDracoMeshCompression()->setBufferView(bufferView);
}

void MeshMerger::resolveMaterialVariants(const Primitive& _sourcePrimitive, Primitive& _primitive)
{
    if (false == _primitive.hasMaterialVariantsMapping() || false == _sourcePrimitive.hasMaterialVariantsMapping()) {
        return;
    }

    const MaterialVariantsPrimitiveMapping& pSourceMaterialVariants = _sourcePrimitive.getMaterialVariantsMapping().value();
    MaterialVariantsPrimitiveMapping& pDestMaterialVariants = _primitive.getMaterialVariantsMapping().value();
    pDestMaterialVariants.resetVariants();

    for (size_t i = 0; i < pSourceMaterialVariants.getMappings().size(); ++i) {
        const MaterialVariantPrimitive& sourceVariantPrimitive = pSourceMaterialVariants.getMappings().at(i);
        gltf::Material* destMaterial = getDestMaterialFromSourceMaterial(sourceVariantPrimitive.material);
        if (destMaterial == nullptr) {
            continue;
        }

        for (size_t j = 0; j < sourceVariantPrimitive.pVariants.size(); ++j) {
            const MaterialVariant* destVariant = getDestVariantFromSourceVariant(sourceVariantPrimitive.pVariants.at(j));
            if (destVariant == nullptr) {
                continue;
            }
            pDestMaterialVariants.addVariant(destVariant, destMaterial);
        }
    }
}

const MaterialVariant* MeshMerger::getDestVariantFromSourceVariant(const MaterialVariant* variant)
{
    const auto& sourceVariants = m_source.getMaterialVariants();
    const auto& destVariants = m_destination.getMaterialVariants();

    size_t originalMVCount = destVariants.size() - sourceVariants.size();

    size_t sourceIndex = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < sourceVariants.size(); ++i) {
        if (&(sourceVariants.at(i)) == variant) {
            sourceIndex = i;
        }
    }

    if (sourceIndex == std::numeric_limits<size_t>::max()) {
        return nullptr;
    }

    size_t destIndex = static_cast<size_t>(originalMVCount) + sourceIndex;
    if (destIndex < 0 || destIndex >= destVariants.size()) {
        return nullptr;
    }

    return &destVariants.at(destIndex);
}

gltf::Material* MeshMerger::getDestMaterialFromSourceMaterial(gltf::Material* material)
{
    const auto& sourceMaterials = m_source.getMaterials();
    auto& destMaterials = m_destination.getMaterials();

    size_t originalMaterialCount = destMaterials.size() - sourceMaterials.size();

    size_t sourceIndex = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < sourceMaterials.size(); ++i) {
        if (&(sourceMaterials.at(i)) == material) {
            sourceIndex = i;
        }
    }

    if (sourceIndex == std::numeric_limits<size_t>::max()) {
        return nullptr;
    }

    size_t destIndex = static_cast<size_t>(originalMaterialCount) + sourceIndex;
    if (destIndex < 0 || destIndex >= destMaterials.size()) {
        return nullptr;
    }

    return &destMaterials.at(destIndex);
}
