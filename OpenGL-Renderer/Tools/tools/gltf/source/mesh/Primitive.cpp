// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/mesh/Primitive.h"
#include "gltf/Accessor.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/DracoDecoder.h"
#include "gltf/DracoEncoder.h"
#include "gltf/Gltf.h"
#include "gltf/Material.h"

using namespace ux3d;
using namespace ux3d::gltf;

Primitive::Primitive(const Gltf& _rootGltf, const Primitive& _source)
    : GltfObject(_source)
    , m_mode(_source.m_mode)
    , m_dracoMeshCompression(_source.m_dracoMeshCompression)
    , parentMesh(_source.parentMesh)
{
    for (const Attribute& sourceAttribute : _source.getAttributes()) {
        Attribute* attribute = addAttribute();
        attribute->setName(sourceAttribute.getName());
    }

    for (auto it = _source.getTargets().begin(), end = _source.getTargets().end(); it != end; ++it) {
        (void)(addTarget());
    }

    if (m_dracoMeshCompression.has_value()) {
        m_dracoMeshCompression->unsetBufferView();
    }
}

size_t Primitive::getPrimitiveCount() const
{
    if (m_pIndicesAccessor == nullptr && (m_attributes.empty() || m_attributes.front().getAccessor() == nullptr)) {
        return 0;
    }
    size_t count = m_pIndicesAccessor != nullptr ? m_pIndicesAccessor->getCount() : m_attributes.front().getAccessor()->getCount();
    switch (m_mode) {
    case PrimitiveMode::Lines: {
        return count / 2;
    }
    case PrimitiveMode::LineStrip: {
        return count == 0 ? 0 : count - 1;
    }
    case PrimitiveMode::LineLoop: {
        return count == 1 ? 0 : count;
    }
    case PrimitiveMode::Points: {
        return count;
    }
    case PrimitiveMode::Triangles: {
        return count / 3;
    }
    case PrimitiveMode::TriangleStrip:
    case PrimitiveMode::TriangleFan: {
        return count < 3 ? 0 : count - 2;
    }
    default:
        return 0;
    }
}

GltfResult Primitive::setIndicesAccessor(Accessor& _Accessor)
{
    m_pIndicesAccessor = &_Accessor;
    return GltfResult::Success;
}

GltfResult Primitive::setPrimitiveMode(PrimitiveMode _Mode)
{
    m_mode = _Mode;
    return GltfResult::Success;
}

GltfResult Primitive::setMaterial(Material& _Material)
{
    m_pMaterial = &_Material;
    return GltfResult::Success;
}

GltfResult Primitive::removeAttribute(const Attribute& _Attribute)
{
    for (auto it = m_attributes.begin(); it != m_attributes.end(); ++it) {
        if (&*it == &_Attribute) {
            m_attributes.erase(it);
            return GltfResult::Success;
        }
    }
    return GltfError::ObjectNotFound;
}

GltfResult Primitive::removeTarget(MorphTarget& _Target)
{
    for (auto it = m_targets.begin(); it != m_targets.end(); ++it) {
        if (&*it == &_Target) {
            m_targets.erase(it);
            return GltfResult::Success;
        }
    }
    return GltfError::ObjectNotFound;
}

core::LinkedList<Accessor*> Primitive::getAllReferencedAccessors()
{
    core::LinkedList<Accessor*> accessorList;

    if (m_pIndicesAccessor != nullptr) {
        accessorList.emplace_back(m_pIndicesAccessor);
    }

    for (Attribute& attribute : m_attributes) {
        if (attribute.getAccessor() != nullptr) {
            accessorList.emplace_back(attribute.getAccessor());
        }
    }

    return accessorList;
}

GltfResult Primitive::enableDracoMeshCompression(DracoEncoder& _encoder, DracoDecoder& _decoder)
{
    if (m_dracoMeshCompression.has_value()) {
        // Primitive is already compressed.
        return GltfResult::Success;
    }

    GltfResult result = _encoder.encodePrimitive(*this);

    if (result == GltfResult::Success) {
        //disable uncompressed buffers:
        setAttributesStorageFormat(StorageFormat::NotStored);
        setIndicesAccessorStorageFormat(StorageFormat::NotStored);

        //mark compressed buffer for writeout:
        setDracoStorageFormat(StorageFormat::File);

        //decode mesh to update renderer
        result = _decoder.decodePrimitive(*this);
    }

    if (result == GltfResult::Success) {
        for (Accessor* accessor : getAllReferencedAccessors()) {
            accessor->recalculateMinMax();
        }
    }

    return result;
}

GltfResult Primitive::disableDracoMeshCompression(Gltf& _gltf)
{
    //reactivate uncompressed buffers:
    setAttributesStorageFormat(StorageFormat::File);
    setIndicesAccessorStorageFormat(StorageFormat::File);

    if (m_dracoMeshCompression.has_value() == false) {
        return GltfResult::Success;
    }

    m_dracoMeshCompression.reset();
    return GltfResult::Success;
}

GltfResult Primitive::createDracoMeshCompression()
{
    if (!m_dracoMeshCompression) {
        m_dracoMeshCompression.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

GltfResult Primitive::createMaterialVariantsMapping()
{
    if (!m_materialVariantsMapping) {
        m_materialVariantsMapping.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

void Primitive::setAttributesStorageFormat(StorageFormat _storageFormat)
{
    for (auto& attribute : m_attributes) {
        if (attribute.getAccessor() != nullptr) {
            attribute.getAccessor()->setStorageFormat(_storageFormat);
        }
    }
}

void Primitive::setIndicesAccessorStorageFormat(StorageFormat _storageFormat)
{
    if (m_pIndicesAccessor != nullptr) {
        m_pIndicesAccessor->setStorageFormat(_storageFormat);
    }
}

void Primitive::setDracoStorageFormat(StorageFormat _storageFormat)
{
    if (!m_dracoMeshCompression) {
        return;
    }

    if (m_dracoMeshCompression->getBufferView() != nullptr && m_dracoMeshCompression->getBufferView()->getBuffer() != nullptr) {
        m_dracoMeshCompression->getBufferView()->getBuffer()->setStorageFormat(_storageFormat);
    }
}

bool Primitive::isValid() const
{
    return !m_attributes.empty();
}

GltfResult Primitive::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    if (m_attributes.empty()) {
        if (_InvalidProperties.push_back("attributes") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}

uint32_t Primitive::getMeshByteSize() const
{
    if (m_dracoMeshCompression.has_value() && m_dracoMeshCompression->getBufferView() != nullptr) {
        return m_dracoMeshCompression->getBufferView()->getByteLength();
    }

    uint32_t usedBytes = 0;
    {
        if (m_pIndicesAccessor != nullptr) {
            usedBytes += static_cast<uint32_t>(m_pIndicesAccessor->getTotalSizeInBytes());
        }

        for (Attribute& attribute : m_attributes) {
            if (attribute.getAccessor() == nullptr) {
                continue;
            }

            usedBytes += static_cast<uint32_t>(attribute.getAccessor()->getTotalSizeInBytes());
        }
    }

    return usedBytes;
}
