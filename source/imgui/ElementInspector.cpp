// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ElementInspector.h"
#include "imgui/MaterialInspector.h"
#include "imageformats/imageformats.h"
#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <cstring>
#include <cmath>

namespace ux3d::imgui {

void ElementInspector::render(SelectionState& selection, ux3d::gltf::Gltf& gltfModel, bool& isSceneDirty)
{
    if (selection.type == SelectionType::None) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Select an element from the Hierarchy or Resources panel.");
        return;
    }

    // -----------------------------------------------------------------
    // Node Inspector
    // -----------------------------------------------------------------
    if (selection.type == SelectionType::Node && selection.selectedNode != nullptr) {
        auto* node = selection.selectedNode;

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Node ID: %u", node->m_ID);

        char nameBuf[256] = {};
        if (node->hasName()) {
            std::strncpy(nameBuf, node->getName().c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            node->setName(nameBuf);
            isSceneDirty = true;
        }

        ImGui::Separator();

        // Transform (TRS)
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Translation
            auto t = node->getTranslationOrDefault();
            float trans[3] = { static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z) };
            if (ImGui::DragFloat3("Translation", trans, 0.05f)) {
                node->setTranslation(ux3d::math::dvec3(trans[0], trans[1], trans[2]));
                isSceneDirty = true;
            }

            // Rotation (Euler angles in degrees)
            auto r = node->getRotationOrDefault();
            glm::quat q(static_cast<float>(r.w), static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.z));
            glm::vec3 euler = glm::degrees(glm::eulerAngles(q));
            float rot[3] = { euler.x, euler.y, euler.z };
            if (ImGui::DragFloat3("Rotation (deg)", rot, 0.5f)) {
                glm::quat newQ = glm::quat(glm::radians(glm::vec3(rot[0], rot[1], rot[2])));
                node->setRotation(ux3d::math::dquat(newQ.w, newQ.x, newQ.y, newQ.z));
                isSceneDirty = true;
            }

            // Scale
            auto s = node->getScaleOrDefault();
            float scale[3] = { static_cast<float>(s.x), static_cast<float>(s.y), static_cast<float>(s.z) };
            if (ImGui::DragFloat3("Scale", scale, 0.02f, 0.001f, 100.0f)) {
                node->setScale(ux3d::math::dvec3(scale[0], scale[1], scale[2]));
                isSceneDirty = true;
            }

            if (ImGui::Button("Reset Transform")) {
                node->setTranslation(ux3d::math::dvec3(0.0));
                node->setRotation(ux3d::math::dquat(1.0, 0.0, 0.0, 0.0));
                node->setScale(ux3d::math::dvec3(1.0));
                isSceneDirty = true;
            }
        }

        // Attached Components
        if (node->getMesh() != nullptr) {
            ImGui::Separator();
            auto* mesh = node->getMesh();
            std::string meshName = mesh->hasName() ? mesh->getName().c_str() : ("Mesh " + std::to_string(mesh->m_ID));
            ImGui::Text("Attached Mesh: %s", meshName.c_str());

            if (ImGui::Button("Inspect Mesh Properties")) {
                selection.selectMesh(mesh);
            }

            // Quick material jumps for primitives
            int primIdx = 0;
            for (auto& prim : mesh->getPrimitives()) {
                if (prim.getMaterial() != nullptr) {
                    auto* mat = prim.getMaterial();
                    std::string matLabel = "Inspect Material: " + (mat->hasName() ? mat->getName().c_str() : ("Material " + std::to_string(mat->m_ID)));
                    ImGui::PushID(primIdx);
                    if (ImGui::Button(matLabel.c_str())) {
                        selection.selectMaterial(mat);
                    }
                    ImGui::PopID();
                }
                ++primIdx;
            }
        }

        if (node->getCamera() != nullptr) {
            ImGui::Separator();
            if (ImGui::Button("Inspect Attached Camera")) {
                selection.selectCamera(node->getCamera());
            }
        }

        if (node->getLight() != nullptr) {
            ImGui::Separator();
            if (ImGui::Button("Inspect Attached Light")) {
                selection.selectLight(node->getLight());
            }
        }
    }

    // -----------------------------------------------------------------
    // Material Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
        if (selection.selectedNode != nullptr) {
            if (ImGui::Button("< Back to Selected Node")) {
                selection.type = SelectionType::Node;
            }
            ImGui::Separator();
        }

        MaterialInspector::render(selection.selectedMaterial, gltfModel, isSceneDirty);
    }

    // -----------------------------------------------------------------
    // Mesh Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Mesh && selection.selectedMesh != nullptr) {
        auto* mesh = selection.selectedMesh;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Mesh ID: %u", mesh->m_ID);

        char nameBuf[256] = {};
        if (mesh->hasName()) {
            std::strncpy(nameBuf, mesh->getName().c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            mesh->setName(nameBuf);
        }

        ImGui::Separator();

        auto& prims = mesh->getPrimitives();
        ImGui::Text("Primitives: %zu", prims.size());

        int idx = 0;
        for (auto& prim : prims) {
            std::string primTitle = "Primitive " + std::to_string(idx);
            if (ImGui::TreeNode(primTitle.c_str())) {
                // Indices & vertices
                if (prim.getIndicesAccessor() != nullptr) {
                    ImGui::Text("Triangles: %u", prim.getIndicesAccessor()->getCount() / 3);
                    ImGui::Text("Indices: %u", prim.getIndicesAccessor()->getCount());
                }

                // Attributes
                if (ImGui::TreeNode("Vertex Attributes")) {
                    for (const auto& attr : prim.getAttributes()) {
                        std::string attrName = attr.getName().c_str();
                        uint32_t count = attr.getAccessor() ? attr.getAccessor()->getCount() : 0;
                        ImGui::BulletText("%s (%u vertices)", attrName.c_str(), count);
                    }
                    ImGui::TreePop();
                }

                // Material selector for primitive
                auto* mat = prim.getMaterial();
                std::string currentMatName = mat ? (mat->hasName() ? mat->getName().c_str() : ("Material " + std::to_string(mat->m_ID))) : "None";

                if (ImGui::BeginCombo("Material", currentMatName.c_str())) {
                    for (auto& m : gltfModel.getElements<ux3d::gltf::Material>()) {
                        bool isSel = (mat == &m);
                        std::string mLabel = m.hasName() ? m.getName().c_str() : ("Material " + std::to_string(m.m_ID));
                        if (ImGui::Selectable(mLabel.c_str(), isSel)) {
                            prim.setMaterial(m);
                            isSceneDirty = true;
                        }
                    }
                    ImGui::EndCombo();
                }

                if (mat != nullptr && ImGui::Button("Inspect Material")) {
                    selection.selectMaterial(mat);
                }

                ImGui::TreePop();
            }
            ++idx;
        }
    }

    // -----------------------------------------------------------------
    // Camera Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Camera && selection.selectedCamera != nullptr) {
        auto* cam = selection.selectedCamera;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera ID: %u", cam->m_ID);

        bool isPerspective = (cam->getType() == ux3d::gltf::Camera::ProjectionType::Perspective);
        const char* camTypes[] = { "Perspective", "Orthographic" };
        int typeIdx = isPerspective ? 0 : 1;
        if (ImGui::Combo("Type", &typeIdx, camTypes, 2)) {
            cam->setType(typeIdx == 0 ? ux3d::gltf::Camera::ProjectionType::Perspective : ux3d::gltf::Camera::ProjectionType::Orthographic);
            isSceneDirty = true;
        }

        if (isPerspective && cam->getCameraPerspective().has_value()) {
            auto& p = cam->getCameraPerspective().value();
            float fov = static_cast<float>(glm::degrees(p.getYFov()));
            if (ImGui::SliderFloat("Vertical FoV (deg)", &fov, 10.0f, 120.0f, "%.1f")) {
                p.setYFov(glm::radians(fov));
                isSceneDirty = true;
            }
            float znear = static_cast<float>(p.getZNear());
            if (ImGui::DragFloat("Near Plane", &znear, 0.01f, 0.001f, 100.0f)) {
                p.setZNear(znear);
                isSceneDirty = true;
            }
            float zfar = p.getZFar().has_value() ? static_cast<float>(p.getZFar().value()) : 1000.0f;
            if (ImGui::DragFloat("Far Plane", &zfar, 1.0f, 1.0f, 10000.0f)) {
                p.setZFar(zfar);
                isSceneDirty = true;
            }
        }
    }

    // -----------------------------------------------------------------
    // Light Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Light && selection.selectedLight != nullptr) {
        auto* light = selection.selectedLight;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Light ID: %u", light->m_ID);

        // Light color
        auto c = light->getColorOrDefault();
        float color[3] = { static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b) };
        if (ImGui::ColorEdit3("Color", color)) {
            light->setColor(ux3d::math::dvec3(color[0], color[1], color[2]));
            isSceneDirty = true;
        }

        // Intensity
        float intensity = static_cast<float>(light->getIntensityOrDefault());
        if (ImGui::DragFloat("Intensity", &intensity, 0.5f, 0.0f, 100000.0f, "%.1f")) {
            light->setIntensity(intensity);
            isSceneDirty = true;
        }

        // Range
        float range = static_cast<float>(light->getRangeOrDefault());
        if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 1000.0f, "%.1f")) {
            light->setRange(range);
            isSceneDirty = true;
        }
    }

    // -----------------------------------------------------------------
    // Texture / Image Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Texture && selection.selectedTexture != nullptr) {
        auto* tex = selection.selectedTexture;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Texture ID: %u", tex->m_ID);
        if (tex->getSource() != nullptr) {
            ImGui::Text("Source Image ID: %u", tex->getSource()->m_ID);
            if (!tex->getSource()->getUri().empty()) {
                ImGui::Text("URI: %s", tex->getSource()->getUri().c_str());
            }
            if (ImGui::Button("Inspect Image")) {
                selection.selectImage(tex->getSource());
            }
        }
    } else if (selection.type == SelectionType::Image && selection.selectedImage != nullptr) {
        auto* img = selection.selectedImage;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Image ID: %u", img->m_ID);
        if (!img->getUri().empty()) {
            ImGui::Text("URI: %s", img->getUri().c_str());
        }
        auto mimeStr = ux3d::imageformats::imageEncodingToString(img->getMimeType());
        if (!mimeStr.empty()) {
            ImGui::Text("MIME Type: %s", mimeStr.c_str());
        }
        size_t byteSize = img->getData().getPixels().size();
        if (byteSize == 0 && img->getEncodedBytes() != nullptr) {
            byteSize = img->getEncodedBytes()->size();
        }
        ImGui::Text("Data Size: %zu bytes", byteSize);
        auto res = img->getResolution();
        if (res.size() >= 2) {
            ImGui::Text("Resolution: %u x %u", res[0], res[1]);
        }
    }

    // -----------------------------------------------------------------
    // Animation Inspector
    // -----------------------------------------------------------------
    else if (selection.type == SelectionType::Animation && selection.selectedAnimation != nullptr) {
        auto* anim = selection.selectedAnimation;
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Animation ID: %u", anim->m_ID);

        char nameBuf[256] = {};
        if (anim->hasName()) {
            std::strncpy(nameBuf, anim->getName().c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            anim->setName(nameBuf);
        }

        ImGui::Separator();
        ImGui::Text("Channels: %zu", anim->getChannels().size());
        ImGui::Text("Samplers: %zu", anim->getSamplers().size());

        if (ImGui::TreeNode("Channels")) {
            for (const auto& ch : anim->getChannels()) {
                std::string targetNode = ch.getTarget().getNode() ? (ch.getTarget().getNode()->hasName() ? ch.getTarget().getNode()->getName().c_str() : ("Node " + std::to_string(ch.getTarget().getNode()->m_ID))) : "Unknown";
                ImGui::BulletText("Target: %s", targetNode.c_str());
            }
            ImGui::TreePop();
        }
    }
}

} // namespace ux3d::imgui
