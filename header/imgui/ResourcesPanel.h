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
#include "gltf/Animation.h"

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
    Animation
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

    void selectNode(ux3d::gltf::Node* node) {
        type = SelectionType::Node;
        selectedNode = node;
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
};

class ResourcesPanel {
public:
    static void render(ux3d::gltf::Gltf& gltfModel, SelectionState& selection);
};

} // namespace ux3d::imgui
