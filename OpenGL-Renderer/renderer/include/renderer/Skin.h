// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gl/Sampler.h"
#include "math/MatrixTypes.h"

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
        void addMesh(Mesh* _Mesh);
        void removeMesh(const Mesh* _pMesh);
        const core::Vector<Mesh*>& getMeshes() const;
        void computeJoints();

    private:
        core::Vector<math::mat4x4> m_inverseBindMatrices;
        core::Vector<TransformHierarchyNode*> m_joints;
        core::Vector<Mesh*> m_meshes;
        TransformHierarchyNode* m_root; //Node in which the skin is used

        std::shared_ptr<gl::Texture> m_jointsTexture = nullptr;
        gl::Sampler m_jointsSampler;
        gl::SamplerState m_jointsSamplerState;
    };
}
}
