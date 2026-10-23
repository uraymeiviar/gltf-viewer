#include "renderer/Skin.h"
#include "renderer/Mesh.h"
#include "renderer/TransformHierarchy.h"
#include "renderer/gl/Texture.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace {
// Last data written to each joints texture by computeJoints (P4.7). Keyed by
// texture, not by skin: copied skins share one texture, so a per-skin record
// could miss another skin's write.
struct UploadedJoints {
    float width = 0.0f;
    std::vector<float> data;
};
std::unordered_map<const ux3d::renderer::gl::Texture*, UploadedJoints>& uploadedJoints()
{
    static std::unordered_map<const ux3d::renderer::gl::Texture*, UploadedJoints> s_uploaded;
    return s_uploaded;
}
}

ux3d::renderer::Skin::JointUploadStats ux3d::renderer::Skin::s_jointUploadStats;
bool ux3d::renderer::Skin::s_jointUploadReuse = true;

ux3d::renderer::Skin::Skin(gl::GL* gl)
    : m_root(nullptr)
    , m_jointsSampler("u_jointsSampler")
    , m_jointsSamplerState()
{
    GLuint targetTexture;
    gl->genTextures(1, &targetTexture);
    m_jointsTexture = std::make_shared<gl::Texture>(gl, targetTexture, gl::TextureTarget::Texture2d);
    m_jointsSamplerState.magFilter = gl::TextureMagFilter::Nearest;
    m_jointsSamplerState.minFilter = gl::TextureMinFilter::Nearest;
    m_jointsSamplerState.wrapS = gl::TextureWrap::ClampToEdge;
    m_jointsSamplerState.wrapT = gl::TextureWrap::ClampToEdge;
    m_jointsSamplerState.wrapR = gl::TextureWrap::ClampToEdge;
    m_jointsSampler.samplerState = &m_jointsSamplerState;
    m_jointsSampler.texture = m_jointsTexture.get();
}

ux3d::renderer::Skin::Skin(const Skin& _Other)
    : m_inverseBindMatrices(_Other.m_inverseBindMatrices)
    , m_joints(_Other.m_joints)
    , m_root(_Other.m_root)
    , m_meshes(_Other.m_meshes)
    , m_jointsSamplerState(_Other.m_jointsSamplerState)
    , m_jointsSampler(_Other.m_jointsSampler)
    , m_jointsTexture(_Other.m_jointsTexture)
{
}

ux3d::renderer::Skin::Skin(Skin&& _Other) noexcept
    : m_inverseBindMatrices(std::move(_Other.m_inverseBindMatrices))
    , m_joints(std::move(_Other.m_joints))
    , m_root(_Other.m_root)
    , m_meshes(std::move(_Other.m_meshes))
    , m_jointsSamplerState(_Other.m_jointsSamplerState)
    , m_jointsSampler(_Other.m_jointsSampler)
    , m_jointsTexture(std::move(_Other.m_jointsTexture))
{
    _Other.m_root = nullptr;
    _Other.m_jointsTexture = nullptr;
}

ux3d::renderer::Skin::~Skin()
{
    if (m_jointsTexture != nullptr && m_jointsTexture.use_count() == 1) {
        uploadedJoints().erase(m_jointsTexture.get());
    }
    //Remove the skin from all meshes before deletion
    for (Mesh* mesh : m_meshes) {
        removeMesh(mesh);
    }
}

void ux3d::renderer::Skin::addJoint(TransformHierarchyNode* _Node)
{
    m_joints.push_back(_Node);
}

void ux3d::renderer::Skin::setRoot(TransformHierarchyNode* _Node)
{
    m_root = _Node;
}

void ux3d::renderer::Skin::addIBM(const math::mat4x4& _Mat)
{
    m_inverseBindMatrices.emplace_back(_Mat);
}

void ux3d::renderer::Skin::addMesh(Mesh* _Mesh)
{
    auto iter = std::find(m_meshes.begin(), m_meshes.end(), _Mesh);
    // only add mesh if it is not yet in the array
    if (iter == m_meshes.end()) {
        m_meshes.push_back(_Mesh);
    }
    _Mesh->jointsSampler = &m_jointsSampler;
}
void ux3d::renderer::Skin::removeMesh(const Mesh* _pMesh)
{
    auto iter = std::find(m_meshes.begin(), m_meshes.end(), _pMesh);
    if (iter != m_meshes.end()) {
        (*iter)->jointsSampler = nullptr;
        m_meshes.erase(iter);
    }
}

const ux3d::core::Vector<ux3d::renderer::Mesh*>& ux3d::renderer::Skin::getMeshes() const
{
    return m_meshes;
}

void ux3d::renderer::Skin::computeJoints()
{
    if (m_root == nullptr) {
        return;
    }

    if (m_joints.size() != m_inverseBindMatrices.size()) {
        core::debugLog(core::LogLevel::Error, "Joints and inverse bind matrices don't match in size (%d != %d). Something is wrong with the asset.", m_joints.size(), m_inverseBindMatrices.size());
        return;
    }

    auto width = math::ceil(math::sqrt(m_joints.size() * 8));
    m_jointTextureScratch.resize(width * width * 4);
    auto& textureData = m_jointTextureScratch;

    math::mat4x4 worldInverse = math::inverse(m_root->getWorldTransform());

    for (size_t i = 0u; i < m_joints.size(); ++i) {
        auto jointMatrix = worldInverse * m_joints[i]->getWorldTransform() * m_inverseBindMatrices[i];
        auto jointNormalMatrix = math::transpose(math::inverse(jointMatrix));
        std::copy(math::value_ptr(jointMatrix), math::value_ptr(jointMatrix) + 16, textureData.begin() + i * 32);
        std::copy(math::value_ptr(jointNormalMatrix), math::value_ptr(jointNormalMatrix) + 16, textureData.begin() + i * 32 + 16);
    }
    // Skip the GL upload when this texture already holds exactly this data (P4.7).
    auto& uploaded = uploadedJoints()[m_jointsTexture.get()];
    if (s_jointUploadReuse && uploaded.width == width && uploaded.data.size() == textureData.size()
        && std::equal(textureData.begin(), textureData.end(), uploaded.data.begin())) {
        ++s_jointUploadStats.suppressed;
        return;
    }
    m_jointsTexture->write(width, width, gl::Format::RGBA, gl::ComponentType::Float, textureData.data(), gl::InternalFormat::RGBA32F);
    ++s_jointUploadStats.uploads;
    uploaded.width = width;
    uploaded.data.assign(textureData.begin(), textureData.end());
}
