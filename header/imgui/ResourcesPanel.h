// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/SceneRevisions.h"
#include "gltf/Node.h"
#include "gltf/Material.h"
#include "gltf/Mesh.h"
#include "gltf/Camera.h"
#include "gltf/Light.h"
#include "gltf/Texture.h"
#include "gltf/Image.h"
#include "gltf/Sampler.h"
#include "gltf/Animation.h"
#include "gltf/Skin.h"
#include "gltf/MaterialVariant.h"
#include "gltf/Scene.h"
#include "gltf/Accessor.h"
#include "gltf/BufferView.h"
#include "gltf/Buffer.h"
#include "imgui/SelfCheckResult.h"
#include <string>

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

enum class SelectionType {
    None,
    Node,
    Material,
    Mesh,
    Camera,
    Light,
    Texture,
    Image,
    Animation,
    Skin,
    MaterialVariant,
    Scene,
    Sampler,
    Accessor,
    BufferView,
    Buffer,
    Viewport
};

struct SelectionState {
    SelectionType type = SelectionType::None;
    ux3d::gltf::Node* selectedNode = nullptr;
    ux3d::gltf::Material* selectedMaterial = nullptr;
    ux3d::gltf::Mesh* selectedMesh = nullptr;
    ux3d::gltf::Camera* selectedCamera = nullptr;
    ux3d::gltf::Light* selectedLight = nullptr;
    ux3d::gltf::Texture* selectedTexture = nullptr;
    ux3d::gltf::Image* selectedImage = nullptr;
    ux3d::gltf::Animation* selectedAnimation = nullptr;
    ux3d::gltf::Skin* selectedSkin = nullptr;
    ux3d::gltf::MaterialVariant* selectedVariant = nullptr;
    ux3d::gltf::Scene* selectedScene = nullptr;
    ux3d::gltf::Sampler* selectedSampler = nullptr;
    ux3d::gltf::Accessor* selectedAccessor = nullptr;
    ux3d::gltf::BufferView* selectedBufferView = nullptr;
    ux3d::gltf::Buffer* selectedBuffer = nullptr;
    uint32_t selectedViewportId = 0;
    uint64_t revision = 0;

    void selectViewport(uint32_t vpId) {
        type = SelectionType::Viewport;
        selectedViewportId = vpId;
        ++revision;
    }

    void selectNode(const ux3d::gltf::Node* node) {
        type = SelectionType::Node;
        selectedNode = const_cast<ux3d::gltf::Node*>(node);
        ++revision;
    }

    void selectMaterial(ux3d::gltf::Material* mat) {
        type = SelectionType::Material;
        selectedMaterial = mat;
        ++revision;
    }

    void selectMesh(ux3d::gltf::Mesh* mesh) {
        type = SelectionType::Mesh;
        selectedMesh = mesh;
        ++revision;
    }

    void selectCamera(ux3d::gltf::Camera* cam) {
        type = SelectionType::Camera;
        selectedCamera = cam;
        ++revision;
    }

    void selectLight(ux3d::gltf::Light* light) {
        type = SelectionType::Light;
        selectedLight = light;
        ++revision;
    }

    void selectTexture(ux3d::gltf::Texture* tex) {
        type = SelectionType::Texture;
        selectedTexture = tex;
        ++revision;
    }

    void selectImage(ux3d::gltf::Image* img) {
        type = SelectionType::Image;
        selectedImage = img;
        ++revision;
    }

    void selectAnimation(ux3d::gltf::Animation* anim) {
        type = SelectionType::Animation;
        selectedAnimation = anim;
        ++revision;
    }

    void selectSkin(ux3d::gltf::Skin* skin) {
        type = SelectionType::Skin;
        selectedSkin = skin;
        ++revision;
    }

    void selectVariant(ux3d::gltf::MaterialVariant* variant) {
        type = SelectionType::MaterialVariant;
        selectedVariant = variant;
        ++revision;
    }

    void selectScene(ux3d::gltf::Scene* scene) {
        type = SelectionType::Scene;
        selectedScene = scene;
        ++revision;
    }

    void selectSampler(ux3d::gltf::Sampler* sampler) {
        type = SelectionType::Sampler;
        selectedSampler = sampler;
        ++revision;
    }

    void selectAccessor(ux3d::gltf::Accessor* accessor) {
        type = SelectionType::Accessor;
        selectedAccessor = accessor;
        ++revision;
    }

    void selectBufferView(ux3d::gltf::BufferView* bv) {
        type = SelectionType::BufferView;
        selectedBufferView = bv;
        ++revision;
    }

    void selectBuffer(ux3d::gltf::Buffer* buf) {
        type = SelectionType::Buffer;
        selectedBuffer = buf;
        ++revision;
    }
};

class ResourcesPanel {
public:
    static void render(ux3d::gltf::Gltf& gltfModel,
                       SelectionState& selection,
                       ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr,
                       SceneRevisions* revisions = nullptr,
                       bool* pOpen = nullptr);
    static void resetSearch();
    static void setSearchFilter(const std::string& filter);

    // Focused P2.2 warm-cache invalidation check used by the command-line
    // regression run. It calls the same category cache used by render().
    static SelfCheckResult runCacheInvalidationSelfCheck(bool forceFailure = false);
};

} // namespace ux3d::imgui
