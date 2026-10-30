// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// GltfRenderLayer scene traversal: static bounds, frustum culling, the shared per-frame traversal (isolation,
// inherited visibility, ASOBO options) and the renderer scene with its reverse mapping.

#include "GltfRenderLayerInternal.h"

namespace {

// The primitive's local POSITION box, or false when it cannot bound what the vertex
// shader draws: skinning and morphing move vertices, and min/max of a non-float
// (quantized) accessor are not in model units.
bool getStaticPositionBounds(const gltf::Primitive& primitive, const Mesh& mesh, math::vec3& lo, math::vec3& hi)
{
    if (!primitive.getTargets().empty() || mesh.defines.isDefined("USE_SKINNING") || mesh.defines.isDefined("USE_MORPHING")
        || mesh.vertexShader != "primitive.vert") {
        return false;
    }
    for (const gltf::Attribute& attribute : primitive.getAttributes()) {
        if (attribute.getName() != "POSITION") {
            continue;
        }
        const gltf::Accessor* accessor = attribute.getAccessor();
        if (accessor == nullptr || accessor->getComponentType() != gltf::ComponentType::Float
            || accessor->getMin().size() != 3 || accessor->getMax().size() != 3) {
            return false;
        }
        lo = math::vec3(static_cast<float>(accessor->getMin()[0]), static_cast<float>(accessor->getMin()[1]), static_cast<float>(accessor->getMin()[2]));
        hi = math::vec3(static_cast<float>(accessor->getMax()[0]), static_cast<float>(accessor->getMax()[1]), static_cast<float>(accessor->getMax()[2]));
        return true;
    }
    return false;
}

// True when all eight corners of the box, in clip space, lie strictly outside one of
// the six clip half-spaces (-w <= x, y, z <= w). Each half-space is linear in
// homogeneous coordinates, so this holds for the whole box, including corners behind
// the camera, under perspective, orthographic and infinite projections and any model
// matrix (negative or non-uniform scale). A small relative margin keeps boxes that
// touch a plane.
bool boxOutsideClip(const math::mat4x4& clipFromModel, const math::vec3& lo, const math::vec3& hi)
{
    constexpr float kMargin = 1e-4f;
    uint32_t outside[6] = { 0, 0, 0, 0, 0, 0 };
    for (int corner = 0; corner < 8; ++corner) {
        const math::vec4 p = clipFromModel * math::vec4((corner & 1) ? hi.x : lo.x, (corner & 2) ? hi.y : lo.y, (corner & 4) ? hi.z : lo.z, 1.0f);
        const float w = p.w + kMargin * std::abs(p.w);
        outside[0] += p.x < -w;
        outside[1] += p.x > w;
        outside[2] += p.y < -w;
        outside[3] += p.y > w;
        outside[4] += p.z < -w;
        outside[5] += p.z > w;
    }
    for (uint32_t count : outside) {
        if (count == 8) {
            return true;
        }
    }
    return false;
}

} // namespace

size_t GltfRenderLayer::cullToClip(core::LinkedList<std::tuple<Mesh*, Material*>>& drawables, const ReverseMapping& reverseMapping,
                                   const math::mat4x4& clipFromWorld) const
{
    size_t removed = 0;
    for (auto it = drawables.begin(); it != drawables.end();) {
        Mesh* mesh = std::get<Mesh*>(*it);
        const auto nodeIt = mesh != nullptr ? reverseMapping.find(mesh) : reverseMapping.end();
        const gltf::Mesh* gltfMesh = nodeIt != reverseMapping.end() ? nodeIt->second->getMesh() : nullptr;
        bool outside = false;
        if (gltfMesh != nullptr) {
            for (const auto& gltfPrimitive : gltfMesh->getPrimitives()) {
                const auto meshIt = m_meshMapping.map.find(std::tuple(nodeIt->second, gltfMesh, &gltfPrimitive));
                if (meshIt == m_meshMapping.map.end() || meshIt->second != mesh) {
                    continue;
                }
                math::vec3 lo;
                math::vec3 hi;
                outside = getStaticPositionBounds(gltfPrimitive, *mesh, lo, hi)
                    && boxOutsideClip(clipFromWorld * mesh->getTransform(), lo, hi);
                break;
            }
        }
        if (outside) {
            it = drawables.erase(it);
            ++removed;
        } else {
            ++it;
        }
    }
    return removed;
}

size_t GltfRenderLayer::boundDrawables(const core::LinkedList<std::tuple<Mesh*, Material*>>& drawables,
                                       const ReverseMapping& reverseMapping, math::vec3& lo, math::vec3& hi,
                                       bool& bounded) const
{
    size_t unbounded = 0;
    bounded = false;
    for (const auto& meshMat : drawables) {
        Mesh* mesh = std::get<Mesh*>(meshMat);
        const auto nodeIt = mesh != nullptr ? reverseMapping.find(mesh) : reverseMapping.end();
        if (nodeIt == reverseMapping.end()) {
            continue; // not a glTF primitive (cube map, transmission helpers)
        }
        const gltf::Mesh* gltfMesh = nodeIt->second->getMesh();
        bool found = false;
        if (gltfMesh != nullptr) {
            for (const auto& gltfPrimitive : gltfMesh->getPrimitives()) {
                const auto meshIt = m_meshMapping.map.find(std::tuple(nodeIt->second, gltfMesh, &gltfPrimitive));
                if (meshIt == m_meshMapping.map.end() || meshIt->second != mesh) {
                    continue;
                }
                math::vec3 boxLo;
                math::vec3 boxHi;
                if (getStaticPositionBounds(gltfPrimitive, *mesh, boxLo, boxHi)) {
                    found = true;
                    const math::mat4x4& transform = mesh->getTransform();
                    for (int corner = 0; corner < 8; ++corner) {
                        const math::vec3 point(transform * math::vec4((corner & 1) ? boxHi.x : boxLo.x,
                                                                      (corner & 2) ? boxHi.y : boxLo.y,
                                                                      (corner & 4) ? boxHi.z : boxLo.z, 1.0f));
                        lo = bounded ? math::min(lo, point) : point;
                        hi = bounded ? math::max(hi, point) : point;
                        bounded = true;
                    }
                }
                break;
            }
        }
        if (!found) {
            ++unbounded;
        }
    }
    return unbounded;
}

const GltfRenderLayer::SceneTraversal& GltfRenderLayer::traverseScene(const gltf::Scene* gltfScene)
{
    SceneTraversal& t = m_traversal;
    if (m_sceneFrameToken != 0 && t.frameToken == m_sceneFrameToken && t.scene == gltfScene
        && t.asoboOptions == asoboOptions && t.materialExtensionOptions == materialExtensionOptions
        && t.materialVariantID == m_materialVariantID
        && t.isolationRoot == m_isolationRoot && t.isolationIncludeChildren == m_isolationIncludeChildren) {
        ++m_traversalReuses;
        return t;
    }
    t.primitives.clear();
    t.lights.clear();
    t.nodesVisited = 0;
    t.primitivesVisited = 0;
    t.containsTransmissiveMaterial = false;
    t.skyPortals.clear();
    t.hasInteriorVolume = false;
    t.scene = gltfScene;
    t.asoboOptions = asoboOptions;
    t.materialExtensionOptions = materialExtensionOptions;
    t.materialVariantID = m_materialVariantID;
    t.isolationRoot = m_isolationRoot;
    t.isolationIncludeChildren = m_isolationIncludeChildren;
    t.frameToken = m_sceneFrameToken;
    const bool hideInvisible = asoboOptions.applies("ASOBO_material_invisible");
    const bool hideOccluders = asoboOptions.applies("ASOBO_material_environment_occluder");

    std::function<void(const gltf::Node*, bool, bool)> addMeshMaterialRecursive;
    addMeshMaterialRecursive = [&](const gltf::Node* gltfNode, bool insideIsolation, bool parentHidden) {
        ++t.nodesVisited;
        const bool isIsolationRoot = (gltfNode == m_isolationRoot);
        const bool insideIsolatedSubtree = (m_isolationRoot == nullptr) || insideIsolation || isIsolationRoot;
        // Visibility is inherited: hiding a node hides every descendant. Isolation is
        // explicit, so the isolated root ignores hidden ancestors but still respects its
        // own visible flag.
        const bool hidden = (isIsolationRoot ? false : parentHidden) || !gltfNode->getVisible();

        // Keep walking children of nodes outside the isolated subtree: the isolated node
        // can be nested at any depth below a scene root. Collection is gated below.
        const bool childInsideIsolation = (m_isolationRoot == nullptr)
            ? false
            : (isIsolationRoot ? m_isolationIncludeChildren : insideIsolation);
        for (const auto* child : gltfNode->getChildren()) {
            addMeshMaterialRecursive(child, childInsideIsolation, hidden);
        }

        // A3.7: a portal is part of the environment of whatever is drawn, so isolating a part of the cabin
        // keeps the window light; hiding the portal node (or an ancestor) switches it off.
        if (!hidden && !m_skyPortalSources.empty() && m_skyPortalSources.count(gltfNode) != 0) {
            t.skyPortals.push_back(gltfNode);
        }

        if (!insideIsolatedSubtree || hidden) {
            return;
        }

        const gltf::Light* gltfLight = gltfNode->getLight();
        if (gltfLight != nullptr) {
            auto lightIt = m_lightMapping.map.find(std::tuple(gltfNode, gltfLight));
            if (lightIt != m_lightMapping.map.end()) {
                t.lights.push_back(std::get<Light*>(lightIt->second));
            }
        }

        const auto* gltfMesh = gltfNode->getMesh();
        if (gltfMesh == nullptr)
            return;

        if (!gltfMesh->getVisible())
            return;

        for (auto& gltfPrimitive : gltfMesh->getPrimitives()) {
            ++t.primitivesVisited;
            // TODO: use new callback to sort commands into render passes
            if (!gltfPrimitive.getVisible())
                continue;

            auto gltfTuple = std::tuple(gltfNode, gltfMesh, &gltfPrimitive);
            auto meshIt = m_meshMapping.map.find(gltfTuple);
            if (meshIt == m_meshMapping.map.end())
                continue;

            const gltf::Material* gltfMaterial;
            if (gltfPrimitive.hasMaterialVariantsMapping()
                && m_materialVariantID.has_value()
                && gltfPrimitive.getMaterialVariantsMapping().value().hasVariant(m_materialVariantID.value())) {
                gltfMaterial = gltfPrimitive.getMaterialVariantsMapping()->getMaterial(m_materialVariantID.value());
            } else {
                gltfMaterial = gltfPrimitive.getMaterial();
            }

            auto matIt = m_materialMapping.map.find(gltfMaterial);
            if (matIt == m_materialMapping.map.end())
                continue;

            if (gltfMaterial != nullptr) {
                if (gltfMaterial->getTransmission().has_value()
                    && materialExtensionOptions.applies(gltfMaterial->m_ID, "KHR_materials_transmission")) {
                    t.containsTransmissiveMaterial = true;
                }
            }

            auto* mesh = meshIt->second;
            auto* material = matIt->second;
            if (material != nullptr && material->environmentOccluder) {
                math::vec3 lo, hi;
                if (getStaticPositionBounds(gltfPrimitive, *mesh, lo, hi)) {
                    for (int corner = 0; corner < 8; ++corner) {
                        const math::vec3 point(mesh->getTransform() * math::vec4(
                            (corner & 1) ? hi.x : lo.x, (corner & 2) ? hi.y : lo.y,
                            (corner & 4) ? hi.z : lo.z, 1.0f));
                        if (!t.hasInteriorVolume) {
                            t.interiorMin = t.interiorMax = point;
                        } else {
                            t.interiorMin = glm::min(t.interiorMin, point);
                            t.interiorMax = glm::max(t.interiorMax, point);
                        }
                        t.hasInteriorVolume = true;
                    }
                }
            }
            if (material != nullptr && ((material->invisible && hideInvisible)
                    || (material->environmentOccluder && hideOccluders))) {
                continue;
            }

            SceneTraversal::Primitive entry;
            entry.mesh = mesh;
            entry.material = material;
            entry.node = gltfNode;
            entry.bounded = getStaticPositionBounds(gltfPrimitive, *mesh, entry.lo, entry.hi);
            t.primitives.push_back(entry);
        }
    };

    if (gltfScene != nullptr) {
        for (const gltf::Node* gltfNode : gltfScene->getNodes()) {
            addMeshMaterialRecursive(gltfNode, false, false);
        }
    }
    return t;
}

std::tuple<RendererScene, ReverseMapping> GltfRenderLayer::createScene(const gltf::Scene* gltfScene)
{
    RendererScene scene;
    ReverseMapping reverseMapping;
    bool containsTransmissiveMaterial = false;
    m_gltfScene = gltfScene;
    m_lastSceneStats = SceneProfileStats();
    const bool cull = frustumCulling && m_activeCamera != nullptr;
    const math::mat4x4 clipFromWorld = cull ? m_activeCamera->getProjectionMatrix() * m_activeCamera->getViewMatrix() : math::mat4x4(1.0f);

    const SceneTraversal& traversal = traverseScene(gltfScene);
    m_lastSceneStats.nodesVisited = traversal.nodesVisited;
    m_lastSceneStats.primitivesVisited = traversal.primitivesVisited;
    containsTransmissiveMaterial = traversal.containsTransmissiveMaterial;
    for (Light* light : traversal.lights) {
        scene.lights.emplace_back(light);
    }
    // A3.7: the portals in world space (node +Z = into the cabin; the disc lies in the node's XY plane), the
    // kMaxSkyPortals nearest to the camera when there are more. A viewport that does not apply the extension, or a
    // file without portals, gets none, and its materials compile without ASOBO_SKY_PORTAL.
    if (!traversal.skyPortals.empty() && asoboOptions.applies("ASOBO_sky_portal")) {
        for (const gltf::Node* node : traversal.skyPortals) {
            const SkyPortalSource& source = m_skyPortalSources.at(node);
            const math::mat4 world = getWorldTransform(node);
            const math::vec3 axisZ(world[2]);
            const float axisLength = math::length(axisZ);
            if (source.radiusMetres <= 0.0f || axisLength <= 1e-8f) {
                continue;
            }
            SkyPortalLight portal;
            portal.position = math::vec3(world[3]);
            portal.normal = axisZ / axisLength;
            portal.radius = source.radiusMetres * 0.5f * (math::length(math::vec3(world[0])) + math::length(math::vec3(world[1])));
            portal.cosInner = source.cosInner;
            portal.cosOuter = source.cosOuter;
            scene.skyPortals.push_back(portal);
        }
        if (scene.skyPortals.size() > kMaxSkyPortals && m_activeCamera != nullptr) {
            const math::vec3 eye(math::inverse(m_activeCamera->getViewMatrix())[3]);
            std::stable_sort(scene.skyPortals.begin(), scene.skyPortals.end(),
                             [&eye](const SkyPortalLight& a, const SkyPortalLight& b) {
                                 return math::length(a.position - eye) < math::length(b.position - eye);
                             });
        }
        if (scene.skyPortals.size() > kMaxSkyPortals) {
            scene.skyPortals.resize(kMaxSkyPortals);
        }
    }
    const bool skyPortalsActive = !scene.skyPortals.empty();
    for (const SceneTraversal::Primitive& entry : traversal.primitives) {
        if (cull) {
            if (!entry.bounded) {
                ++m_lastSceneStats.uncullable;
            } else if (boxOutsideClip(clipFromWorld * entry.mesh->getTransform(), entry.lo, entry.hi)) {
                ++m_lastSceneStats.culled;
                continue;
            }
        }
        // A1.4: MSFS sorts transparent decals, glass and windshield by the mesh's bounding
        // box centre, not the node pivot. Skinned, morphed and quantized primitives cannot
        // be bounded statically, so they keep the pivot as the fallback.
        const bool drawOrderActive = m_assetDrawOrderDeclared
            && asoboOptions.applies("ASOBO_material_draw_order");
        if (entry.material != nullptr) {
            // Restore the authored pass on each viewport before applying display policy.
            // Shared materials are configured and consumed sequentially, like ASOBO.
            if (entry.material->defines.isDefined("MATERIAL_TRANSMISSION")) {
                const bool transmissionApplied = materialExtensionOptions.applies(entry.material->gltfMaterialId,
                    "KHR_materials_transmission");
                entry.material->renderPass = transmissionApplied
                    ? static_cast<RenderPass*>(m_resourceManager.getTransmissivePass()) : entry.material->nonTransmissivePass;
                entry.material->defines.defineIf(!transmissionApplied && entry.material->decalReceiverWithoutTransmission,
                    "ASOBO_DECAL_RECEIVER");
            }
            const auto* extensions = entry.material->asobo.get();
            const auto active = [&](const char* name) -> const gltf::asobo::ExtensionData* {
                const auto* ext = extensions != nullptr ? extensions->find(name) : nullptr;
                return ext != nullptr && asoboOptions.applies(name) ? ext : nullptr;
            };
            const auto* glass = active("ASOBO_material_glass_v2");
            const auto* legacyGlass = active("ASOBO_material_glass");
            if (legacyGlass == nullptr) legacyGlass = active("ASOBO_material_kitty_glass");
            const auto* windshieldV3 = active("ASOBO_material_windshield_v3");
            const bool legacyWindshield = active("ASOBO_material_windshield") != nullptr;
            // An explicit type takes precedence over extras, including when its display
            // switch is off. Otherwise extras alone identifies old packaged windshields.
            const bool explicitType = extensions != nullptr && (extensions->find("ASOBO_material_glass_v2")
                || extensions->find("ASOBO_material_glass") || extensions->find("ASOBO_material_kitty_glass")
                || extensions->find("ASOBO_material_windshield_v3") || extensions->find("ASOBO_material_windshield"));
            const bool codeActive = !explicitType && asoboOptions.applies("ASOBO_material_code");
            const bool codeGlass = codeActive && entry.material->asoboMaterialCode == Material::AsoboMaterialCode::Glass;
            const bool codeWindshield = codeActive && entry.material->asoboMaterialCode == Material::AsoboMaterialCode::Windshield;
            const bool refract = glass != nullptr || legacyGlass != nullptr || codeGlass;
            const bool reflect = refract || windshieldV3 != nullptr || legacyWindshield || codeWindshield;
            const bool windshield = reflect && !refract;
            // A7.1: ASOBO_material_anisotropic_v2 per viewport; off compiles the isotropic core variant.
            const bool asoboAnisotropy = entry.material->asoboAnisotropy && !entry.material->khrAnisotropy
                && asoboOptions.applies("ASOBO_material_anisotropic_v2");
            entry.material->defines.defineIf(asoboAnisotropy, "ASOBO_ANISOTROPY");
            entry.material->defines.defineIf(asoboAnisotropy || entry.material->khrAnisotropy, "MATERIAL_ANISOTROPY");
            // A3.7: every material of a file with portals (the portal's own file is the interior), per viewport.
            entry.material->defines.defineIf(skyPortalsActive, "ASOBO_SKY_PORTAL");
            // A3.4: ASOBO_material_SSS per viewport; off compiles the core variant.
            entry.material->defines.defineIf(entry.material->asoboSss && asoboOptions.applies("ASOBO_material_SSS"),
                "ASOBO_SSS");
            entry.material->defines.defineIf(windshield, "ASOBO_MATERIAL_WINDSHIELD");
            entry.material->defines.defineIf(windshield && windshieldV3 != nullptr, "ASOBO_MATERIAL_WINDSHIELD_V3");
            entry.material->defines.defineIf(windshield && windshieldV3 == nullptr, "ASOBO_MATERIAL_WINDSHIELD_LEGACY");
            entry.material->defines.defineIf(legacyGlass != nullptr, "ASOBO_GLASS_LEGACY");
            if (legacyGlass != nullptr) {
                entry.material->setUniform<float>("u_AsoboGlassReflectionMask",
                    static_cast<float>(std::clamp(legacyGlass->number("glassReflectionMaskFactor").value_or(0.0), 0.0, 1.0)));
            }
            entry.material->defines.defineIf(windshield, "ASOBO_WINDSHIELD_SSR");
            if (windshield) {
                scene.windshieldSsr = true;
                containsTransmissiveMaterial = true;
                scene.asoboGlassCopy = true;
                entry.material->setUniform<float>("u_SsrAttenuation", static_cast<float>(windshieldV3 != nullptr
                    ? windshieldV3->number("ssrAttenuation").value_or(1.0) : 1.0));
                entry.material->setUniform<int>("u_HasInteriorVolume", traversal.hasInteriorVolume ? 1 : 0);
                entry.material->setUniform<math::vec3>("u_InteriorMin", traversal.interiorMin);
                entry.material->setUniform<math::vec3>("u_InteriorMax", traversal.interiorMax);
            }
            const bool blend = entry.material->renderPass->getId() == Transparent;
            entry.material->defines.defineIf(blend && reflect, "ASOBO_GLASS_REFLECTION");
            entry.material->defines.defineIf(blend && refract, "ASOBO_GLASS_REFRACTION");
            if (blend && refract) {
                containsTransmissiveMaterial = true;
                scene.asoboGlassCopy = true;
                // The exporter's UI value is in millimetres and is written divided by 100
                // (E2 screen glass: UI 2 mm -> 0.02), so metres = stored value / 10. A 2 mm
                // slab matches the subtle refraction in MSFS; reading 0.02 as metres smears it.
                entry.material->setUniform<float>("u_AsoboGlassWidth",
                    glass != nullptr ? static_cast<float>(std::max(0.0, glass->number("glassWidth").value_or(0.0)) * 0.1) : 0.0f);
            }
            // Kitty glass and extras had no glass draw-order class before this mapping.
            // Turning their display switch off must restore that core depth/sort path.
            const bool oldGlassClass = extensions != nullptr && (extensions->find("ASOBO_material_glass_v2")
                || extensions->find("ASOBO_material_glass") || extensions->find("ASOBO_material_windshield_v3")
                || extensions->find("ASOBO_material_windshield"));
            entry.material->drawOrderActive = drawOrderActive
                && (entry.material->drawOrderClass != DrawOrderClass::Glass || oldGlassClass || reflect);
            entry.material->decalBiasActive = asoboOptions.applies("ASOBO_material_geometry_decal");
            if (drawOrderActive && entry.material->drawOrderClass != DrawOrderClass::None) {
                if (entry.bounded) {
                    const math::vec3 localCenter = (entry.lo + entry.hi) * 0.5f;
                    entry.mesh->setBoundingCenter(math::vec3(entry.mesh->getTransform() * math::vec4(localCenter, 1.0f)));
                } else {
                    entry.mesh->setBoundingCenter(entry.mesh->getPosition());
                }
            }
        }
        scene.drawables.emplace_back(entry.mesh, entry.material);
        reverseMapping[entry.mesh] = entry.node;
    }

    scene.camera = m_activeCamera;
    scene.environmentMap = getActiveEnvironmentMap();

    for (const auto [mesh, material] : scene.drawables) {
        if (mesh->defines.isDefined("USE_MORPHING")) {
            mesh->defines.defineIf(renderSettings.enableMorphing, "USE_MORPHING");
        }
        if (!mesh->defines.isDefined("WEIGHT_COUNT")) {
            mesh->defines.define("WEIGHT_COUNT", 1); // Must be positive (with 0 there are compilation errors)
        }
        mesh->defines.define("DEBUG", static_cast<int>(RenderMode::None));
        mesh->polygonMode = gl::PolygonMode::Fill;
        material->fragmentShader = "pbr.frag";
        material->renderSettings = renderSettings;
        if (!materialExtensionOptions.disabled.empty()) {
            material->renderSettings.enableSpecularGlossiness &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_pbrSpecularGlossiness");
            material->renderSettings.enableUnlit &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_unlit");
            material->renderSettings.enableClearCoat &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_clearcoat");
            material->renderSettings.enableSheen &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_sheen");
            material->renderSettings.enableTransmission &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_transmission");
            material->renderSettings.enableVolume &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_volume");
            material->renderSettings.enableIridescence &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_iridescence");
            material->renderSettings.enableIOR &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_ior");
            material->renderSettings.enableSpecular &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_specular");
            material->renderSettings.enableEmissiveStrength &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_emissive_strength");
            material->renderSettings.enableAnisotropy &= materialExtensionOptions.applies(material->gltfMaterialId, "KHR_materials_anisotropy");
        }
        material->defines.define("LINEAR_OUTPUT", 1);
    }

    // add CubeMap
    if (m_cubeMapEnabled && m_activeCamera != nullptr && m_activeCamera->getType() == Camera::Type::Perspective) {
        scene.drawables.emplace_back(m_resourceManager.getCubeMapTuple());
    }

    // The scene target is already linear RGBA16F; capture the behind copy directly
    // from the HDR scene before Transmissive / glass Transparent passes without
    // duplicating drawables into a second LINEAR_OUTPUT pass.
    m_postTransmissionDrawables.clear();
    if (containsTransmissiveMaterial) {
        scene.drawables.emplace_back(m_resourceManager.getEmptyTransmissionBlit());
    }

    m_lastSceneStats.drawables = static_cast<uint64_t>(scene.drawables.size());
    return { scene, reverseMapping };
}

