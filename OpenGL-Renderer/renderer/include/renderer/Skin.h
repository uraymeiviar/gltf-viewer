// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gl/Sampler.h"
#include "math/MatrixTypes.h"

#include <cstdint>
#include <memory>

namespace ux3d {
namespace renderer {
    class Mesh;
    class TransformHierarchyNode;

    class Skin {
    public:
        Skin(gl::GL* gl);
        Skin(const Skin&);
        Skin(Skin&&) noexcept;
        ~Skin();

        void addJoint(TransformHierarchyNode* _Node);
        void setRoot(TransformHierarchyNode* _Node);
        void addIBM(const math::mat4x4& _Mat);
        size_t getInverseBindMatrixCount() const { return m_inverseBindMatrices.size(); }
        const math::mat4x4& getInverseBindMatrix(size_t index) const { return m_inverseBindMatrices[index]; }
        void addMesh(Mesh* _Mesh);
        void removeMesh(const Mesh* _pMesh);
        const core::Vector<Mesh*>& getMeshes() const;
        void computeJoints();

        // Mesh-node-relative bind (Asobo/MSFS exports): the inverse bind matrices do not carry
        // the mesh node transform, so at rest every jointWorld x IBM equals one shared matrix C
        // that is not the mesh node's world matrix. The joint deltas then deform the placed
        // mesh, as Blender's armature does: world = jointWorld x IBM x correction x v with
        // correction = inverse(C) x meshRestWorld, instead of strict glTF's jointWorld x IBM x v.
        void setBindCorrection(const math::mat4x4& correction)
        {
            m_bindCorrection = correction;
            m_hasBindCorrection = true;
        }
        bool hasBindCorrection() const { return m_hasBindCorrection; }

        // Joint texture uploads issued and suppressed as unchanged (P4.7).
        struct JointUploadStats {
            uint64_t uploads = 0;
            uint64_t suppressed = 0;
        };
        static JointUploadStats s_jointUploadStats;
        static bool s_jointUploadReuse;

    private:
        core::Vector<math::mat4x4> m_inverseBindMatrices;
        core::Vector<TransformHierarchyNode*> m_joints;
        core::Vector<Mesh*> m_meshes;
        TransformHierarchyNode* m_root; //Node in which the skin is used
        math::mat4x4 m_bindCorrection = math::mat4x4(1.0f);
        bool m_hasBindCorrection = false;

        std::shared_ptr<gl::Texture> m_jointsTexture = nullptr;
        // Reused between frames. Never part of the skin's observable state; its
        // capacity may remain at the largest joint count seen so far.
        core::Vector<float> m_jointTextureScratch;
        gl::Sampler m_jointsSampler;
        gl::SamplerState m_jointsSamplerState;
    };
}
}
