// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Attribute.h"
#include "DracoMeshCompression.h"
#include "MorphTarget.h"
#include "core/LinkedList.h"
#include "core/String.h"
#include "gltf/GlTypes.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfResult.h"
#include "gltf/HelperFilled.h"
#include "gltf/HelperVisible.h"
#include "gltf/StorageFormats.h"
#include "gltf/mesh/MaterialVariantsMapping.h"
#include <atomic>
#include <cstdint>
#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#primitive

namespace ux3d::gltf {
class Gltf;
class Accessor;
class Material;
class Mesh;
class DracoEncoder;
class DracoDecoder;

class Primitive : public HelperVisible, public HelperFilled, public GltfObject {
private:
    PrimitiveMode m_mode = PrimitiveMode::Default;
    Accessor* m_pIndicesAccessor = nullptr;
    Material* m_pMaterial = nullptr;

    core::LinkedList<Attribute> m_attributes;
    core::LinkedList<MorphTarget> m_targets;

    std::optional<DracoMeshCompression> m_dracoMeshCompression;
    std::optional<MaterialVariantsPrimitiveMapping> m_materialVariantsMapping;

public:
    const Mesh* parentMesh;
    Primitive(Primitive&&) = delete;

    explicit Primitive(const Mesh& pParent)
        : parentMesh(&pParent)
    {
    }

    Primitive(const Primitive&) = default;
    Primitive(const Gltf& _rootGltf, const Primitive& _source);

    GltfResult setIndicesAccessor(Accessor& _Accessor);
    GltfResult setPrimitiveMode(PrimitiveMode _Mode);
    GltfResult setMaterial(Material& _Material);

    Attribute* addAttribute() { return m_attributes.emplace_back(); }
    GltfResult removeAttribute(const Attribute& _Attribute);

    MorphTarget* addTarget() { return m_targets.emplace_back(); }
    GltfResult removeTarget(MorphTarget& _Target);

    PrimitiveMode getPrimitiveMode() const { return m_mode; }
    size_t getPrimitiveCount() const;

    core::LinkedList<Attribute>& getAttributes() { return m_attributes; }
    const core::LinkedList<Attribute>& getAttributes() const { return m_attributes; }

    core::LinkedList<MorphTarget>& getTargets() { return m_targets; }
    const core::LinkedList<MorphTarget>& getTargets() const { return m_targets; }

    Accessor* getIndicesAccessor() { return m_pIndicesAccessor; }
    const Accessor* getIndicesAccessor() const { return m_pIndicesAccessor; }

    Material* getMaterial() { return m_pMaterial; }
    const Material* getMaterial() const { return m_pMaterial; }

    static uint64_t getMaterialRevision() { return s_materialRevision.load(std::memory_order_relaxed); }

    core::LinkedList<Accessor*> getAllReferencedAccessors();

    GltfResult enableDracoMeshCompression(DracoEncoder& _encoder, DracoDecoder& _decoder);
    GltfResult disableDracoMeshCompression(Gltf& _gltf);
    void setAttributesStorageFormat(StorageFormat _storageFormat);
    void setIndicesAccessorStorageFormat(StorageFormat _storageFormat);
    void setDracoStorageFormat(StorageFormat _storageFormat);
    bool hasDracoMeshCompression() const { return m_dracoMeshCompression.has_value(); }
    bool hasMaterialVariantsMapping() const { return m_materialVariantsMapping.has_value(); }

    std::optional<DracoMeshCompression>& getDracoMeshCompression() { return m_dracoMeshCompression; }
    const std::optional<DracoMeshCompression>& getDracoMeshCompression() const { return m_dracoMeshCompression; }
    std::optional<MaterialVariantsPrimitiveMapping>& getMaterialVariantsMapping() { return m_materialVariantsMapping; }
    const std::optional<MaterialVariantsPrimitiveMapping>& getMaterialVariantsMapping() const { return m_materialVariantsMapping; }
    GltfResult createDracoMeshCompression();
    GltfResult createMaterialVariantsMapping();
    void unsetMaterialVariantsMapping() { m_materialVariantsMapping.reset(); }

    void unsetMaterial()
    {
        m_pMaterial = nullptr;
        bumpMaterialRevision();
    }
    void unsetIndicesAccessor() { m_pIndicesAccessor = nullptr; }

    bool isValid() const;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const;

    uint32_t getMeshByteSize() const;

private:
    static void bumpMaterialRevision() { s_materialRevision.fetch_add(1, std::memory_order_relaxed); }

    inline static std::atomic<uint64_t> s_materialRevision{ 0 };
};
}
