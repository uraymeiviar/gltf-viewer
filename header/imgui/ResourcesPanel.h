// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
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
    Buffer
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

    void selectNode(const ux3d::gltf::Node* node) {
        type = SelectionType::Node;
        selectedNode = const_cast<ux3d::gltf::Node*>(node);
    }

    void selectMaterial(ux3d::gltf::Material* mat) {
        type = SelectionType::Material;
        selectedMaterial = mat;
    }

    void selectMesh(ux3d::gltf::Mesh* mesh) {
        type = SelectionType::Mesh;
        selectedMesh = mesh;
    }

    void selectCamera(ux3d::gltf::Camera* cam) {
        type = SelectionType::Camera;
        selectedCamera = cam;
    }

    void selectLight(ux3d::gltf::Light* light) {
        type = SelectionType::Light;
        selectedLight = light;
    }

    void selectTexture(ux3d::gltf::Texture* tex) {
        type = SelectionType::Texture;
        selectedTexture = tex;
    }

    void selectImage(ux3d::gltf::Image* img) {
        type = SelectionType::Image;
        selectedImage = img;
    }

    void selectAnimation(ux3d::gltf::Animation* anim) {
        type = SelectionType::Animation;
        selectedAnimation = anim;
    }

    void selectSkin(ux3d::gltf::Skin* skin) {
        type = SelectionType::Skin;
        selectedSkin = skin;
    }

    void selectVariant(ux3d::gltf::MaterialVariant* variant) {
        type = SelectionType::MaterialVariant;
        selectedVariant = variant;
    }

    void selectScene(ux3d::gltf::Scene* scene) {
        type = SelectionType::Scene;
        selectedScene = scene;
    }

    void selectSampler(ux3d::gltf::Sampler* sampler) {
        type = SelectionType::Sampler;
        selectedSampler = sampler;
    }

    void selectAccessor(ux3d::gltf::Accessor* accessor) {
        type = SelectionType::Accessor;
        selectedAccessor = accessor;
    }

    void selectBufferView(ux3d::gltf::BufferView* bv) {
        type = SelectionType::BufferView;
        selectedBufferView = bv;
    }

    void selectBuffer(ux3d::gltf::Buffer* buf) {
        type = SelectionType::Buffer;
        selectedBuffer = buf;
    }
};

class ResourcesPanel {
public:
    static void render(ux3d::gltf::Gltf& gltfModel,
                       SelectionState& selection,
                       ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr,
                       bool* isSceneDirty = nullptr,
                       bool* pOpen = nullptr);
    static void resetSearch();
    static void setSearchFilter(const std::string& filter);
};

} // namespace ux3d::imgui
