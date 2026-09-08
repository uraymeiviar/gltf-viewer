// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/gl/Util.h"

using namespace ux3d;
using namespace ux3d::renderer;

EditorRenderLayer::EditorRenderLayer(gl::GL* gl, ux3d::renderer::GltfRenderLayer& gltfLayer, const filesystem::Path& editorTexturesPath, const filesystem::Path& editorGizmoModelsPath)
    : gltfLayer(gltfLayer)
    , m_editorResourceManager(gl, editorGizmoModelsPath)
    , m_lightGizmoMapping(*this, editorTexturesPath)
    , m_cameraGizmoMapping(*this, editorTexturesPath)
    , m_audioGizmoMapping(*this, editorTexturesPath)
    , m_gizmoMapping(*this)
    , m_orientationGizmoMapping(*this)
    , m_gizmoScaling(1.0f)
{
    m_orientationGizmoPieces[OrientationGizmoPiece::Top] = OrientationCubeInteractionMode::Default;
    m_orientationGizmoPieces[OrientationGizmoPiece::Bottom] = OrientationCubeInteractionMode::Default;
    m_orientationGizmoPieces[OrientationGizmoPiece::Right] = OrientationCubeInteractionMode::Default;
    m_orientationGizmoPieces[OrientationGizmoPiece::Left] = OrientationCubeInteractionMode::Default;
    m_orientationGizmoPieces[OrientationGizmoPiece::Front] = OrientationCubeInteractionMode::Default;
    m_orientationGizmoPieces[OrientationGizmoPiece::Back] = OrientationCubeInteractionMode::Default;

    m_wireframeMaterial = m_editorResourceManager.makeWireframeMaterial();
    m_wireframeMaterial->setUniform("u_WireframeColor", m_wireframeColor);
}

void EditorRenderLayer::update()
{
    // lights
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Light*>> gltfLights;

    for (auto& [lightKey, lightValue] : gltfLayer.m_lightMapping.map) {
        if (std::get<0>(lightKey)->getVisible()) {
            gltfLights.emplace_back(lightKey);
        }
    }
    m_lightGizmoMapping.setScale(m_gizmoScaling);
    m_lightGizmoMapping.update(gltfLights);

    // cameras
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Camera*>> gltfCameras;
    for (auto& [cameraKey, cameraValue] : gltfLayer.m_cameraMapping.map) {
        if (std::get<0>(cameraKey)->getVisible()) {
            gltfCameras.emplace_back(cameraKey);
        }
    }
    m_cameraGizmoMapping.scale = m_gizmoScaling;
    m_cameraGizmoMapping.update(gltfCameras);

    // audios
    core::LinkedList<const gltf::Node*> gltfAudios;
    for (auto [key, value] : gltfLayer.m_audioMapping.map) {
        const gltf::Node* gltfNode = std::get<0>(key);
        if (gltfNode->getVisible()) {
            gltfAudios.emplace_back(gltfNode);
        }
    }
    m_audioGizmoMapping.scale = m_gizmoScaling;
    m_audioGizmoMapping.update(gltfAudios);

    // gizmos
    auto foundIt = gltfLayer.m_transformHierarchyMapping.map.find(m_gizmoedNode);
    if (foundIt == gltfLayer.m_transformHierarchyMapping.map.end()) {
        // reset gizmoed node if it is not in the scene any more
        m_gizmoedNode = nullptr;
    }
    core::LinkedList<std::tuple<const gltf::Node*, const GizmoState*>> gizmoedGltfNodes;
    if (m_gizmoedNode != nullptr && m_gizmoState != nullptr) {
        gizmoedGltfNodes.emplace_back(m_gizmoedNode, m_gizmoState);
    }
    m_gizmoMapping.setScale(m_gizmoScaling);
    m_gizmoMapping.setNukeRotation(m_gizmoSpace == GizmoSpace::Global);
    m_gizmoMapping.update(gizmoedGltfNodes);

    core::LinkedList<OrientationGizmoMappingKey> orientationGizmoPieces;
    for (auto& [piece, interactionMode] : m_orientationGizmoPieces) {
        orientationGizmoPieces.emplace_back(piece, &interactionMode);
    }
    m_orientationGizmoMapping.update(orientationGizmoPieces);

    m_wireframeMaterial->setUniform("u_WireframeColor", m_wireframeColor);
}

std::vector<std::tuple<Mesh*, Material*>> EditorRenderLayer::getSelectedTuples()
{
    std::vector<std::tuple<Mesh*, Material*>> meshMaterials;

    // set mesh highlight for the selected node, mesh, primitive
    for (auto [gltfTuple, mesh] : gltfLayer.m_meshMapping.map) {
        std::tuple<Mesh*, Material*> meshMaterial;
        auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;

        bool nodeMeshPrimitiveSelected = true;
        if (m_selectedItems.contains(gltfNode) == false && m_selectedItems.contains(gltfMesh) == false && m_selectedItems.contains(gltfPrimitive) == false) {
            nodeMeshPrimitiveSelected = false;
        }

        // if materials are selected we need to check if the primitive uses one of the selected materials
        // thefore we cant skip the primitive material for loop below if a material is selected
        if (nodeMeshPrimitiveSelected == false && m_selectedMaterials.size() == 0) {
            continue;
        }

        // set mesh:
        std::get<Mesh*>(meshMaterial) = mesh;

        // set material by searching for the material that is used in the primitive:
        for (auto [gltfMaterial, rendererMaterial] : gltfLayer.m_materialMapping.map) {
            if (gltfMaterial == gltfPrimitive->getMaterial()) {
                // arr to selection if either the node, mesh or primitive is selected or the material is selected
                if (nodeMeshPrimitiveSelected || m_selectedMaterials.contains(gltfMaterial)) {
                    std::get<Material*>(meshMaterial) = rendererMaterial;
                    meshMaterials.emplace_back(meshMaterial);
                }
            }
        }
    }

    return meshMaterials;
}

RendererScene EditorRenderLayer::applyLayer(ux3d::renderer::RendererScene&& scene)
{
    gl::checkGLError(m_editorResourceManager.m_gl, "EditorRenderLayer: Error");

    // apply the debug output channel to all gltf layer drawables
    for (auto& [mesh, material] : scene.drawables) {
        mesh->defines.define("DEBUG", static_cast<int>(m_renderMode));
    }

    // Wireframe mode rendering
    for (auto& [gltfTuple, mesh] : gltfLayer.m_meshMapping.map) {
        auto& [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;
        if ((m_renderMode == RenderMode::Wireframe || !gltfMesh->getFilled()) && gltfNode->getVisible() && gltfMesh->getVisible()) {
            mesh->polygonMode = gl::PolygonMode::Line;
            mesh->defines.define("DEBUG", static_cast<int>(RenderMode::Wireframe));

            auto materialIt = gltfLayer.m_materialMapping.map.find(gltfPrimitive->getMaterial());
            if (materialIt != gltfLayer.m_materialMapping.map.end()) {
                // Remove the drawable tuple with the original material (we want to only display the wireframe)
                auto drawableIt = std::find(scene.drawables.begin(), scene.drawables.end(), std::tuple<Mesh*, Material*>(mesh, materialIt->second));
                if (drawableIt != scene.drawables.end()) {
                    scene.drawables.erase(drawableIt);
                }

                // Create a new drawable pair with the wireframe material
                scene.drawables.emplace_back(std::tuple<Mesh*, Material*>(mesh, m_wireframeMaterial));
            }
        }
    }

    bool selectionApplied = false;

    // cleanup the temporarily cloned meshes
    for (auto& selectedMeshClone : this->m_selectedMeshClones) {
        this->m_editorResourceManager.destroy(selectedMeshClone);
    }
    this->m_selectedMeshClones.clear();

    // apply selection to the drawables
    for (auto [selectedMesh, selectedMaterial] : getSelectedTuples()) {
        if (selectedMesh != nullptr && selectedMaterial != nullptr) {
            Mesh* selectedMeshClone = m_editorResourceManager.create<Mesh>(*selectedMesh);
            selectedMeshClone->polygonMode = gl::PolygonMode::Fill;

            scene.drawables.emplace_back(std::tuple<Mesh*, Material*>(
                selectedMeshClone,
                selectedMaterial->isDoubleSided ? m_editorResourceManager.getSilhouetteMaterial() : m_editorResourceManager.getSilhouetteMaterialCulled()));

            // put temporary clones into a list to be able to destroy them after drawing
            m_selectedMeshClones.emplace_back(selectedMeshClone);

            selectionApplied = true;
        }
    }

    // add specific editor layer drawables
    for (auto& [gltfTuple, gizmoTuple] : m_lightGizmoMapping.map) {
        auto& [gltfNode, gltfLight] = gltfTuple;
        if (m_selectedItems.contains(gltfLight) || m_selectedItems.contains(gltfNode)) {
            // Create duplicate of gizmoTuple with outline material:
            std::tuple<Mesh*, Material*> meshMatDuplicate = gizmoTuple;
            std::get<Material*>(meshMatDuplicate) = m_editorResourceManager.getSilhouetteMaterial();

            selectionApplied = true;

            scene.drawables.emplace_back(meshMatDuplicate);
        }
        scene.drawables.emplace_back(gizmoTuple);
    }
    for (auto& [gltfTuple, gizmoTuple] : m_cameraGizmoMapping.map) {
        auto& [gltfNode, gltfCamera] = gltfTuple;
        if (m_selectedItems.contains(gltfCamera) || m_selectedItems.contains(gltfNode)) {
            // Create duplicate of gizmoTuple with outline material:
            std::tuple<Mesh*, Material*> meshMatDuplicate = gizmoTuple;
            std::get<Material*>(meshMatDuplicate) = m_editorResourceManager.getSilhouetteMaterial();

            selectionApplied = true;

            scene.drawables.emplace_back(meshMatDuplicate);
        }
        scene.drawables.emplace_back(gizmoTuple);
    }
    for (auto& [gltfNode, gizmoTuple] : m_audioGizmoMapping.map) {
        if (m_selectedItems.contains(gltfNode)) {
            // Create duplicate of gizmoTuple with outline material:
            std::tuple<Mesh*, Material*> meshMatDuplicate = gizmoTuple;
            std::get<Material*>(meshMatDuplicate) = m_editorResourceManager.getSilhouetteMaterial();

            selectionApplied = true;

            scene.drawables.emplace_back(meshMatDuplicate);
        }
        scene.drawables.emplace_back(gizmoTuple);
    }
    for (auto& [gltfTuple, gizmoTuple] : m_gizmoMapping.map) {
        for (const std::tuple<Mesh*, Material*>& meshMat : *gizmoTuple) {
            scene.drawables.emplace_back(meshMat);
        }
    }
    for (auto& [gltfTuple, meshMat] : m_orientationGizmoMapping.map) {
        scene.drawables.emplace_back(meshMat);
    }

    if (showBoundingBoxes) {
        std::function<void(const gltf::Node*)> traverse;
        traverse = [&](const gltf::Node* gltfNode) {
            for (const auto* child : gltfNode->getChildren()) {
                traverse(child);
            }

            const auto* gltfMesh = gltfNode->getMesh();
            if (gltfMesh == nullptr || gltfMesh->getPrimitives().empty())
                return;

            auto bounds = gltfMesh->getPositionBounds();
            if (!bounds)
                return;

            // Each bounding box needs a new mesh, because the transform is baked into it. The material can be shared across all boxes.
            static auto material = m_editorResourceManager.makeBoundingBoxMaterial();
            material->setUniform("u_Color", m_boundingBoxColor);
            auto mesh = m_editorResourceManager.makeBoundingBoxMesh();

            // Transform unit cube from [0, 1] to [min, max]
            glm::mat4 matrix = glm::translate(bounds->min) * glm::scale(bounds->max - bounds->min);
            mesh->updateTransform(gltfLayer.getTransformHierarchy().map.at(gltfNode)->getWorldTransform() * matrix);

            scene.drawables.emplace_back(mesh.get(), material.get());

            // Store bounding box mesh mat, because scene.drawables needs a non-owning reference to it far after this function returns
            m_boundingBoxMeshes.emplace_back(std::move(mesh));
        };
        m_boundingBoxMeshes.clear(); // Delete bounding box meshes from last frame
        std::for_each(gltfLayer.getGltfScene()->getNodes().begin(), gltfLayer.getGltfScene()->getNodes().end(), traverse);
    }

    if (showSkinsAndBones) {
        m_skeletonMeshes.clear();
        if (gltfLayer.recalculateBones) {
            m_bones.clear();
            m_skins.clear();
            gltfLayer.recalculateBones = false;

            std::function<void(const gltf::Node*)> traverseSkin;
            traverseSkin = [&](const gltf::Node* node) {
                const auto* skin = node->getSkin();
                if (skin == nullptr) {
                    // No skin is instanciated at this node.
                    return;
                }

                // Find the "bones" (joint pairs) this skin constitutes:
                // Traverse into sub-nodes and build a DAG by connecting all node pairs which are both joints and form
                // a possibly indirect parent-child relation ship.
                // Instead of the full DAG, only the minimialized graph is built under the assumption that its edges are transitive.
                // This data structure does not need to be re-built every frame, but only when the glTF skin changes.
                auto wl = skin->getJoints();

                std::function<void(const gltf::Node*, const gltf::Node*)> traverseJoint;
                traverseJoint = [&](const gltf::Node* parentJoint, const gltf::Node* node) {
                    // Check it this node is a joint (by checking the working list) and remove it if so.
                    auto iter = std::find(wl.begin(), wl.end(), node);
                    bool nodeIsJoint = false;
                    if (iter != wl.end()) {
                        wl.erase(iter);
                        nodeIsJoint = true;
                    }

                    // This node might be a joint, and thus be a boundary between two bones.
                    if (nodeIsJoint) {
                        // Step 1: Finish the current bone.
                        if (parentJoint != nullptr && std::find(m_bones.begin(), m_bones.end(), std::make_pair(parentJoint, node)) == m_bones.end()) {
                            m_bones.emplace_back(parentJoint, node);
                        }
                        // Step 2: Begin a new bone.
                        parentJoint = node;
                    }

                    if (!wl.empty()) {
                        for (const auto* child : node->getChildren()) {
                            traverseJoint(parentJoint, child);
                        }
                    }
                };

                if (skin->getSkeletonRoot() != nullptr) {
                    m_skins.push_back(skin->getSkeletonRoot());
                    traverseJoint(nullptr, skin->getSkeletonRoot());
                } else {
                    m_skins.push_back(node);
                    for (const auto skeletonRoot : gltfLayer.getGltfScene()->getNodes()) {
                        traverseJoint(nullptr, skeletonRoot);
                    }
                }
            };

            // There might be skins inside skins, thus we always have to traverse the whole hierarchy.
            std::function<void(const gltf::Node*)> traverseNode;
            traverseNode = [&](const gltf::Node* node) {
                traverseSkin(node);
                for (const auto* child : node->getChildren()) {
                    traverseNode(child);
                }
            };

            // Kick off traversal for each root node.
            if (gltfLayer.getGltfScene() != nullptr) {
                for (const auto* rootNode : gltfLayer.getGltfScene()->getNodes()) {
                    traverseNode(rootNode);
                }
            }
        }

        // Generate bone helper mesh-mats
        for (auto [parent, child] : m_bones) {
            static auto material = m_editorResourceManager.makeBoneHelperMaterial();
            material->setUniform("u_Color", m_skeletonColor);
            auto mesh = m_editorResourceManager.makeBoneHelperMesh();
            auto parentPosition = gltfLayer.getTransformHierarchy().map.at(parent)->getWorldTranslation();
            auto childPosition = gltfLayer.getTransformHierarchy().map.at(child)->getWorldTranslation();
            float scale = glm::distance(parentPosition, childPosition);

            // Compute orthogonal frame along parent-child line while facing the camera position
            // In case of a degeneracy (arrow points right into the camera), the bone will not be visible
            glm::vec3 side = childPosition - parentPosition;
            glm::vec3 up = scale * glm::normalize(glm::cross(side, scene.camera->getPositon() - parentPosition));
            glm::vec3 forward = scale * glm::normalize(glm::cross(up, side));
            glm::mat4 matrix = glm::mat3(side, up, forward);
            matrix[3] = glm::vec4(parentPosition, 1);
            mesh->updateTransform(matrix);

            scene.drawables.emplace_back(mesh.get(), material.get());
            m_skeletonMeshes.emplace_back(std::move(mesh));
        }

        // Generate skin helper mesh-mats
        double averageBoneLength = 0;
        for (auto [parent, child] : m_bones) {
            auto parentPosition = gltfLayer.getTransformHierarchy().map.at(parent)->getWorldTranslation();
            auto childPosition = gltfLayer.getTransformHierarchy().map.at(child)->getWorldTranslation();
            averageBoneLength += static_cast<double>(glm::distance(parentPosition, childPosition));
        }
        averageBoneLength /= static_cast<double>(m_bones.size());
        for (const auto* skin : m_skins) {
            auto mesh = m_editorResourceManager.makeSkinHelperMesh();
            auto position = gltfLayer.getTransformHierarchy().map.at(skin)->getWorldTranslation();

            // Build frame which is orthogonal to camera view direction
            glm::vec3 forward = glm::normalize(scene.camera->getPositon() - position);
            glm::vec3 side = -glm::normalize(glm::cross(glm::vec3(0, 1, 0), forward));
            glm::vec3 up = glm::normalize(glm::cross(side, forward));
            glm::mat4 matrix = glm::mat3(side, up, forward);
            matrix *= 0.2 * std::max(averageBoneLength, 0.2);
            matrix[3] = glm::vec4(position, 1);
            mesh->updateTransform(matrix);

            static auto material = m_editorResourceManager.makeSkinHelperMaterial();
            material->setUniform("u_Color", 0.67f * m_skeletonColor);
            scene.drawables.emplace_back(mesh.get(), material.get());
            m_skeletonMeshes.emplace_back(std::move(mesh));
        }
    }

    if (selectionApplied && m_outlineThickness > 0) {
        Material* outlineMaterial = m_editorResourceManager.getOutlineMaterial();
        outlineMaterial->setUniform("u_outlineColor", m_outlineColor);
        outlineMaterial->setUniform("u_outlineThickness", m_outlineThickness);
        scene.drawables.emplace_back(m_editorResourceManager.getFullscreenTriangleMesh(), outlineMaterial);
    }

    if (showGrid) {
        glm::vec4 color = m_gridColor;
        glm::vec4 colorSmall = m_gridColor;
        glm::vec4 colorAxisX = m_editorResourceManager.colorAxisX;
        glm::vec4 colorAxisZ = m_editorResourceManager.colorAxisZ;
        float scaling = 0.1f;
        float maxdistance = 6;
        float blenddistance = 3;
        auto smallGrid = m_editorResourceManager.getSmallGrid();
        auto bigGrid = m_editorResourceManager.getBigGrid();
        glm::vec3 translation(0.0f);
        glm::vec3 orthographicTranslation(0.0f);
        glm::mat4 transform(1.0f);
        short rotation = 0; // 0 = normal grid, 1 = front/back, 2 = left/right

        if (scene.camera != nullptr) {
            auto cameraPos = scene.camera->getPositon();
            // Transform the grid if orthogonal camera is aligned with the world axes
            math::vec3 cameraLookDirection = scene.camera->getView();
            constexpr float epsilon = 1.0e-4;
            if (scene.camera->getType() == Camera::Type::Orthographic
                && (glm::all(math::epsilonEqual(cameraLookDirection, math::vec3(0, 0, 1), math::vec3(epsilon)))
                    || glm::all(math::epsilonEqual(cameraLookDirection, math::vec3(0, 0, -1), math::vec3(epsilon)))
                    || glm::all(math::epsilonEqual(cameraLookDirection, math::vec3(1, 0, 0), math::vec3(epsilon)))
                    || glm::all(math::epsilonEqual(cameraLookDirection, math::vec3(-1, 0, 0), math::vec3(epsilon)))
                    || glm::all(math::epsilonEqual(cameraLookDirection, math::vec3(0, 1, 0), math::vec3(epsilon)))
                    || glm::all(math::epsilonEqual(cameraLookDirection, math::vec3(0, -1, 0), math::vec3(epsilon))))) {
                cameraLookDirection = math::round(cameraLookDirection);
                transform = scene.camera->getViewMatrix();
                transform[3] = math::vec4(0, 0, 0, 1);
                math::vec3 rightDirection = transform[0];
                // This rotates the grid 90 degress upwards in this mode the z axis will always point towards the camera
                transform = math::rotate(math::pi<float>() / 2.0f, rightDirection) * transform;

                math::vec3 absCameraLookDirection = math::abs(cameraLookDirection);
                orthographicTranslation = (scene.camera->getZFarPlane() * 0.9f) * -cameraLookDirection + cameraPos * absCameraLookDirection;

                if (glm::all(math::equal(absCameraLookDirection, math::vec3(0, 0, 1)))) {
                    rotation = 1;
                    colorAxisZ = m_editorResourceManager.colorAxisY;
                } else if (glm::all(math::equal(absCameraLookDirection, math::vec3(1, 0, 0)))) {
                    rotation = 2;
                    colorAxisZ = m_editorResourceManager.colorAxisY;
                    colorAxisX = m_editorResourceManager.colorAxisZ;
                }
            }

            float elevation = cameraPos.y;
            // Dark color from below
            if (elevation < 0 && scene.camera->getType() != Camera::Type::Orthographic) {
                color = { color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, color.a };
            }

            // Calculate scaling of grids (at 10, 100, 1000)
            elevation = std::abs(scene.camera->getZoom());
            if (elevation > 1) {
                scaling *= static_cast<float>(std::pow(10, 1 + static_cast<int>(std::log10(elevation))));
            } else {
                scaling *= 1 / static_cast<float>(std::pow(10, (static_cast<int>(std::log10(1 / elevation)))));
            }

            // Horizon should be dependent on camera zoom
            if (scene.camera->getType() != Camera::Type::Orthographic) {
                maxdistance *= elevation * 0.5f;
                blenddistance *= elevation * 0.5f;
            }

            // The alpha of the small grid is dependent on the percentual elevation of the camera in respect to the current scale
            colorSmall.a = 0.8f - (elevation - scaling) / (scaling * 10 - scaling);

            auto axes = m_editorResourceManager.getAxes();

            glm::mat4 transformX = glm::scale(transform, glm::vec3(scaling, 0, elevation / 10));
            glm::mat4 transformZ = glm::scale(transform, glm::vec3(elevation / 10, 0, scaling));

            // Move grid if out of bounds
            float cameraPosX = cameraPos.x;
            float cameraPosZ = cameraPos.z;

            if (rotation == 1) {
                cameraPosZ = cameraPos.y;
            } else if (rotation == 2) {
                cameraPosX = cameraPos.y;
            }

            float translationX = getGridTranslationOffset(cameraPosX, scaling, maxdistance);
            float translationZ = getGridTranslationOffset(cameraPosZ, scaling, maxdistance);

            if (rotation == 0) {
                translation.x = translationX;
                translation.z = translationZ;
                transformX[3][0] += translationX;
                transformZ[3][2] += translationZ;
            } else if (rotation == 1) {
                translation.x = translationX;
                translation.y = translationZ;
                transformX[3][0] += translationX;
                transformZ[3][1] += translationZ;
            } else {
                translation.y = translationX;
                translation.z = translationZ;
                transformX[3][1] += translationX;
                transformZ[3][2] += translationZ;
            }

            transformX[3] += math::vec4(orthographicTranslation, 0);
            std::get<Mesh*>(axes[0])->updateTransform(transformX);

            transformZ[3] += math::vec4(orthographicTranslation, 0);
            std::get<Mesh*>(axes[1])->updateTransform(transformZ);
        }
        Material* smallGridMat = std::get<Material*>(smallGrid);
        Material* bigGridMat = std::get<Material*>(bigGrid);
        smallGridMat->setUniform("u_color", colorSmall);
        smallGridMat->setUniform("u_maxDistance", maxdistance);
        smallGridMat->setUniform("u_blendDistance", blenddistance);
        bigGridMat->setUniform("u_color", color);
        bigGridMat->setUniform("u_maxDistance", maxdistance);
        bigGridMat->setUniform("u_blendDistance", blenddistance);

        glm::mat4 gridTransform = glm::scale(transform, glm::vec3(scaling, scaling, scaling));
        gridTransform[3] += glm::vec4(translation, 0.0f);

        gridTransform[3] += math::vec4(orthographicTranslation, 0);
        std::get<Mesh*>(smallGrid)->updateTransform(gridTransform);
        std::get<Mesh*>(bigGrid)->updateTransform(gridTransform);

        scene.drawables.emplace_back(bigGrid);
        scene.drawables.emplace_back(smallGrid);

        // Set material for x Axis
        Material* mat = std::get<Material*>(m_editorResourceManager.getAxes()[0]);
        mat->setUniform("u_maxDistance", maxdistance);
        mat->setUniform("u_blendDistance", blenddistance);
        mat->setUniform("u_color", colorAxisX);

        // Set material fox y Axis
        mat = std::get<Material*>(m_editorResourceManager.getAxes()[1]);
        mat->setUniform("u_maxDistance", maxdistance);
        mat->setUniform("u_blendDistance", blenddistance);
        mat->setUniform("u_color", colorAxisZ);

        for (auto drawable : m_editorResourceManager.getAxes()) {
            scene.drawables.emplace_back(drawable);
        }
    }
    gl::checkGLError(m_editorResourceManager.m_gl, "EditorRenderLayer: Error");
    return std::move(scene);
}

float EditorRenderLayer::getGridTranslationOffset(float cameraPos, float scaling, float maxDistance)
{
    if (std::abs(cameraPos) > scaling * m_editorResourceManager.gridSize / 3 - maxDistance) {
        if (cameraPos > 0) {
            return std::floor(cameraPos) - m_editorResourceManager.gridSize * scaling / 4;
        } else {
            return std::floor(cameraPos) + m_editorResourceManager.gridSize * scaling / 4;
        }
    }
    return 0.0f;
}

ReverseMapping EditorRenderLayer::applyLayer(ux3d::renderer::ReverseMapping&& reverseMapping)
{
    for (auto [gltfTuple, gizmoTuple] : m_lightGizmoMapping.map) {
        reverseMapping[std::get<renderer::Mesh*>(gizmoTuple)] = std::get<const gltf::Node*>(gltfTuple);
    }

    for (auto [gltfTuple, gizmoTuple] : m_cameraGizmoMapping.map) {
        reverseMapping[std::get<renderer::Mesh*>(gizmoTuple)] = std::get<const gltf::Node*>(gltfTuple);
    }

    for (auto [node, gizmoTuple] : m_audioGizmoMapping.map) {
        reverseMapping[std::get<renderer::Mesh*>(gizmoTuple)] = node;
    }

    return std::move(reverseMapping);
}

void EditorRenderLayer::setGizmoedNode(const gltf::Node* node, const GizmoState* state)
{
    m_gizmoedNode = node;
    m_gizmoState = state;
}

const gltf::Node* EditorRenderLayer::getGizmoedNode() const
{
    return m_gizmoedNode;
}

bool EditorRenderLayer::hasActiveGizmo() const
{
    auto it = m_gizmoMapping.map.find({ m_gizmoedNode, m_gizmoState });
    if (it != m_gizmoMapping.map.end()) {
        return true;
    }
    return false;
}
