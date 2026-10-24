// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Jim Eckerlein

#include "renderer/editor/EditorResourceManager.h"
#include "filesystem/File.h"
#include "filesystem/Path.h"
#include "gltf/Gltf.h"
#include "gltf/GltfParsing.h"
#include "gltf/ImportOptions.h"
#include "renderer/passes/OpaquePass.h"
#include "renderer/passes/OutlinePass.h"
#include "renderer/passes/PostprocessingPass.h"
#include "renderer/passes/TransparentPass.h"

#include <algorithm>

using namespace ux3d;
using namespace ux3d::renderer;

static GizmoPiece translationMeshNameToPiece(const core::string& name);
static GizmoPiece scaleMeshNameToPiece(const core::string& name);
static GizmoPiece rotationMeshNameToPiece(const core::string& name);
static OrientationGizmoPiece cameraOrientationMeshNameToPiece(const core::string& name);

EditorResourceManager::EditorResourceManager(gl::GL* gl, const filesystem::Path& editorGizmoModelsPath)
    : GltfResourceManager(gl)
    , m_translationGizmoGltf(new gltf::Gltf)
    , m_scaleGizmoGltf(new gltf::Gltf)
    , m_rotationGizmoGltf(new gltf::Gltf)
    , m_cameraOrientationGizmoGltf(new gltf::Gltf)
{
    m_foregroundPass = create<TransparentPass>(gl, RenderPassType::Foreground, "Foreground pass");
    m_foregroundPass->getPreCommands().emplace_back(Command::makeClearCommand(gl, gl::ClearTarget::DepthBuffer, 0, 0, 0, 0));
    m_foregroundMaskPass = create<TransparentPass>(gl, RenderPassType::Gizmos, "Mask pass");
    m_foregroundMaskPass->getPreCommands().emplace_back(Command::makeClearCommand(gl, gl::ClearTarget::DepthBuffer, 0, 0, 0, 0));
    m_foregroundMaskedPass = create<TransparentPass>(gl, RenderPassType::PostGizmos, "Masked pass");

    m_outlinePass = create<OutlinePass>(gl, RenderPassType::Outline, "Outline pass");
    m_postprocessingPass = create<PostprocessingPass>(RenderPassType::Postprocessing, "Postprocessing pass");

    initGizmo(*m_translationGizmoGltf, editorGizmoModelsPath / "Gizmo_Transform_Translation.glb", &translationMeshNameToPiece);
    initGizmo(*m_scaleGizmoGltf, editorGizmoModelsPath / "Gizmo_Transform_Scale.glb", &scaleMeshNameToPiece);
    initGizmo(*m_rotationGizmoGltf, editorGizmoModelsPath / "Gizmo_Transform_Rotation.glb", &rotationMeshNameToPiece);
    initGizmo(*m_cameraOrientationGizmoGltf, editorGizmoModelsPath / "orientation_gizmo.glb", &cameraOrientationMeshNameToPiece);

    m_silhouetteMaterial = createSilhouetteMaterial();
    m_silhouetteMaterial->isDoubleSided = true;
    m_silhouetteMaterialCulled = createSilhouetteMaterial();
    m_silhouetteMaterialCulled->isDoubleSided = false;
    m_outlineMaterial = createOutlineMaterial();
    m_fullscreenTriangleMesh = createFullscreenTriangleMesh();

    m_depthTestedPass = create<TransparentPass>(gl, RenderPassType::Grid, "Grid pass");

    initGrid();
    initOriginAxes();
    initBoundingBoxBuffers();
    initSkinHelperBuffers();
    initBoneHelperBuffers();
}

void EditorResourceManager::initGizmo(gltf::Gltf& outGltf, const filesystem::File& gltfFile, GizmoPiece (*meshNameToPiece)(const core::string& name))
{
    gltf::ImportOptions importOptions;
    importOptions.conversionUsage = gltf::ConversionUsage::ValidGltf;
    if (!gltf::GltfParsing::fromFile(outGltf, gltfFile, importOptions, false)) {
        return;
    }

    for (const gltf::Mesh& gltfMesh : outGltf.getMeshes()) {
        GizmoPiece piece = meshNameToPiece(gltfMesh.getName());
        if (piece == GizmoPiece::Invalid) {
            core::logError("Gizmo loading: Unknown mesh '%s' encountered", gltfMesh.getName().c_str());
            continue;
        }
        Mesh* mesh = create<Mesh>(m_gl);
        if (piece == GizmoPiece::RotationXCircle || piece == GizmoPiece::RotationYCircle || piece == GizmoPiece::RotationZCircle) {
            m_gizmoMeshMats[static_cast<size_t>(piece)] = {
                mesh,
                createGizmoPostMaterial()
            };
        } else {
            m_gizmoMeshMats[static_cast<size_t>(piece)] = {
                mesh,
                createGizmoMaterial()
            };
        }

        initGizmoMesh(gltfMesh, *mesh);
        mesh->nukeScale = true;
        if (piece > GizmoPiece::ScaleCenter || piece < GizmoPiece::ScaleAxisX) {
            mesh->nukeRotation = true;
        }
    }
}

void EditorResourceManager::initGizmo(gltf::Gltf& outGltf, const filesystem::File& gltfFile, OrientationGizmoPiece (*meshNameToPiece)(const core::string& name))
{
    gltf::ImportOptions importOptions;
    importOptions.conversionUsage = gltf::ConversionUsage::ValidGltf;
    if (!gltf::GltfParsing::fromFile(outGltf, gltfFile, importOptions, false)) {
        return;
    }

    // load images as textures
    std::map<const gltf::Image*, gl::Texture*> textureMapping;
    for (const gltf::Image& gltfImage : outGltf.getImages()) {
        textureMapping[&gltfImage] = nullptr;

        if (gltfImage.getEncodedBytes() != nullptr) {
            renderer::gl::Texture* texture = createWithGLContext<gl::Texture>(gl::Texture::extractTarget(gltfImage.getData()));
            textureMapping[&gltfImage] = texture;
        }
    }

    // place the nodes in the transform hierarchy
    TransformHierarchy gizmoTransformHierarchy;
    std::map<const gltf::Node*, TransformHierarchyNode*> transformHierarchyMapping;
    std::function<renderer::TransformHierarchyNode*(const gltf::Node*)> gatherTransformHierarchy;
    gatherTransformHierarchy = [&](const gltf::Node* gltfNode) -> renderer::TransformHierarchyNode* {
        auto node = gizmoTransformHierarchy.createNode(gltfNode->getTranslationOrDefault(), gltfNode->getRotationOrDefault(), gltfNode->getScaleOrDefault(), {});

        transformHierarchyMapping[gltfNode] = node;

        for (const gltf::Node* glTFChild : gltfNode->getChildren()) {
            node->addChild(gatherTransformHierarchy(glTFChild));
        }
        return node;
    };
    auto gltfScene = outGltf.getDefaultScene();
    for (const gltf::Node* glTFRootNode : gltfScene->getNodes()) {
        gizmoTransformHierarchy.addRootNode(gatherTransformHierarchy(glTFRootNode));
    }

    for (const gltf::Node& gltfNode : outGltf.getNodes()) {
        if (nullptr == gltfNode.getMesh()) {
            // this node contains no mesh, we can ignore it
            continue;
        }

        const gltf::Mesh& gltfMesh = *gltfNode.getMesh();

        OrientationGizmoPiece piece = meshNameToPiece(gltfMesh.getName());
        if (piece == OrientationGizmoPiece::Invalid) {
            core::logError("Gizmo loading: Unknown mesh '%s' encountered", gltfMesh.getName().c_str());
            continue;
        }
        Mesh* mesh = create<Mesh>(m_gl);
        Material* material = createGizmoMaterial();

        initGizmoMesh(gltfMesh, *mesh);

        mesh->defines.define("IS_SCREEN_SPACE_POSITIONED 1");
        mesh->setUniform("u_useClipSpacePosition", true);
        mesh->setUniform("u_screenSpacePosition", math::vec2(-100.0, -100.0));

        // add mesh to transform hierarchy
        transformHierarchyMapping[&gltfNode]->addMesh(mesh);

        // Creates a sampler with a given name from the given texture.
        // The sampler is only created if the given texture is not null.
        // Returns true if the sampler was actually created.
        auto createSampler = [this, &textureMapping, material](const core::string& baseName, const std::optional<gltf::TextureInfo>& texture, bool isLinear = true) -> bool {
            core::string capitalized;
            for (size_t i = 0; i < baseName.length(); ++i) {
                char c = baseName[i];
                if (i > 0 && std::isupper(c)) {
                    capitalized.push_back('_');
                }
                capitalized.push_back(std::toupper(c));
            }
            const core::string samplerEnabledDefine = "HAS_" + capitalized + "_MAP";

            material->defines.define(samplerEnabledDefine);

            if (texture.has_value() == false) {
                return false;
            }

            const gltf::TextureInfo& info(texture.value());
            const gltf::Texture* pTex = info.getTexture();

            if (pTex == nullptr) {
                return false;
            }

            auto texIt = textureMapping.end();

            const auto& oBasisU = pTex->getBasisU();
            if (oBasisU.has_value()) {
                texIt = textureMapping.find(oBasisU.value().getSource());
            }

            const auto& oWebP = pTex->getWebP();
            if (oWebP.has_value()) {
                texIt = textureMapping.find(oWebP.value().getSource());
            }

            const auto& oDds = pTex->getTextureDds();
            if (oDds.has_value()) {
                texIt = textureMapping.find(oDds.value().getSource());
            }

            if (texIt == textureMapping.end() || texIt->second == nullptr) {
                // fall back to standard texture source
                texIt = textureMapping.find(pTex->getSource());
                if (texIt == textureMapping.end() || texIt->second == nullptr) {
                    return false;
                }
            }

            renderer::gl::SamplerState* pSamplerState = create<gl::SamplerState>();
            pSamplerState->minFilter = gl::TextureMinFilter::Linear;

            // sampler states are not supported for the orientation gizmo
            //			auto samplerIt = m_samplerStateMapping.map.find(pTex->getSampler());
            //			if (samplerIt != m_samplerStateMapping.map.end())
            //			{
            //				pSamplerState = samplerIt->second;
            //			}
            texIt->second->write(texIt->first->getData(), isLinear);

            material->samplers.emplace_back(
                "u_" + baseName + "Sampler",
                texIt->second,
                pSamplerState);

            material->setUniform<uint32_t>("u_" + baseName + "UVSet", info.getEffectiveTexCoord());
            material->setUniform<math::mat3>("u_" + baseName + "UVTransform", info.calculateTransformMatrix());
            material->defines.define(samplerEnabledDefine); // set value to true because texture is present and not null

            return true;
        };

        const auto& primitives = gltfMesh.getPrimitives();
        if (primitives.size() != 1) {
            // this a requirement from initGizmoMesh, we should never reach this code path
            core::debugLog(core::LogLevel::Error, "Meshes with more than one primitive are not supported for the orientation gizmo");
            return;
        }

        if (const gltf::Material* gltfMaterial = primitives.front().getMaterial(); nullptr != gltfMaterial) {
            if (const auto& metallicRoughness = gltfMaterial->getPbrMetallicRoughness(); metallicRoughness.has_value()) {
                createSampler("BaseColor", metallicRoughness->getBaseColorTextureInfo(), false); // SRGB
                material->defines.define("MATERIAL_METALLICROUGHNESS");
            }
        }

        m_orientationGizmoMeshMats[piece] = {
            mesh,
            material
        };
    }

    gizmoTransformHierarchy.apply();
}

void EditorResourceManager::initGizmoMesh(const gltf::Mesh& gltfMesh, Mesh& mesh)
{
    // Initialize mesh.
    mesh.vertexShader = "gizmo.vert";
    mesh.primitiveTopology = gl::PrimitiveTopology::TriangleList;
    mesh.polygonMode = gl::PolygonMode::Fill;

    // Per specification, there is only a single primitive per gizmo mesh.
    if (gltfMesh.getPrimitives().size() != 1) {
        core::logError("Loading Gizmoes: Failed, mesh %s does not have one primitive", gltfMesh.getName().c_str());
        return;
    }

    const gltf::Primitive& gltfPrimitive = gltfMesh.getPrimitives().front();

    // Initialize index buffer.
    core::MemoryRange<const uint8_t> indexData;
    gltfPrimitive.getIndicesAccessor()->getBufferView()->getData(indexData);
    gl::Buffer* indexBuffer = create<gl::Buffer>(m_gl, indexData);

    switch (gltfPrimitive.getIndicesAccessor()->getComponentType()) {
    case gltf::ComponentType::UnsignedByte:
        mesh.indexFormat = gl::PrimitiveType::UnsignedByte;
        break;
    case gltf::ComponentType::UnsignedShort:
        mesh.indexFormat = gl::PrimitiveType::UnsignedShort;
        break;
    case gltf::ComponentType::UnsignedInt:
        mesh.indexFormat = gl::PrimitiveType::UnsignedInt;
        break;
    default:
        core::logWarning("Unknown index bit width");
        break;
    }

    mesh.indexCount = gltfPrimitive.getIndicesAccessor()->getCount();
    mesh.indexByteOffset = gltfPrimitive.getIndicesAccessor()->getByteOffset();
    mesh.indexBuffer = indexBuffer;
    mesh.defines.define("IS_FIXEDSIZE");
    mesh.setUniform("u_fixedSizeScale", GizmoMapping::getGizmoBaseScale());

    // Initialize position and normal attributes.
    if (auto iter = std::find_if(gltfPrimitive.getAttributes().begin(), gltfPrimitive.getAttributes().end(), [](const gltf::Attribute& attr) { return attr.getName() == "POSITION"; });
        iter != gltfPrimitive.getAttributes().end()) {
        gltf::Attribute& gltfAttr = iter->m_Data;

        core::MemoryRange<const uint8_t> data;
        gltfAttr.getAccessor()->getBufferView()->getBuffer()->getData(data);
        uint32_t byteOffset = gltfAttr.getAccessor()->getBufferView()->getByteOffset();
        uint32_t byteStride = gltfAttr.getAccessor()->getBufferView()->getByteStride().value_or(0u);

        gl::Buffer* buffer = create<gl::Buffer>(m_gl, data);

        gl::Attribute attr("a_position");
        attr.buffer = buffer;
        attr.offset = byteOffset;
        attr.stride = byteStride;
        attr.dimension = 3;
        attr.type = gl::ComponentType::Float;

        mesh.vertexCount = gltfAttr.getAccessor()->getCount();
        mesh.attributes.emplace_back(attr);
    } else {
        core::logError("Gizmo loading: Cannot retrieve mesh from gizmo, missing position attribute.");
    }

    if (auto iter = std::find_if(gltfPrimitive.getAttributes().begin(), gltfPrimitive.getAttributes().end(), [](const gltf::Attribute& attr) { return attr.getName() == "NORMAL"; });
        iter != gltfPrimitive.getAttributes().end()) {
        gltf::Attribute& gltfAttr = iter->m_Data;

        core::MemoryRange<const uint8_t> data;
        gltfAttr.getAccessor()->getBufferView()->getBuffer()->getData(data);
        uint32_t byteOffset = gltfAttr.getAccessor()->getBufferView()->getByteOffset();
        uint32_t byteStride = gltfAttr.getAccessor()->getBufferView()->getByteStride().value_or(0u);

        gl::Buffer* buffer = create<gl::Buffer>(m_gl, data);

        gl::Attribute attr("a_normal");
        attr.buffer = buffer;
        attr.offset = byteOffset;
        attr.stride = byteStride;
        attr.dimension = 3;
        attr.type = gl::ComponentType::Float;

        mesh.vertexCount = gltfAttr.getAccessor()->getCount();
        mesh.attributes.emplace_back(attr);
        mesh.defines.define("HAS_NORMAL_VEC3");
    } else {
        core::logError("Gizmo loading: Cannot retrieve mesh from gizmo, missing normal attribute.");
    }

    if (auto iter = std::find_if(gltfPrimitive.getAttributes().begin(), gltfPrimitive.getAttributes().end(), [](const gltf::Attribute& attr) { return attr.getName() == "TEXCOORD_0"; });
        iter != gltfPrimitive.getAttributes().end()) {
        gltf::Attribute& gltfAttr = iter->m_Data;

        core::MemoryRange<const uint8_t> data;
        gltfAttr.getAccessor()->getBufferView()->getBuffer()->getData(data);
        uint32_t byteOffset = gltfAttr.getAccessor()->getBufferView()->getByteOffset();
        uint32_t byteStride = gltfAttr.getAccessor()->getBufferView()->getByteStride().value_or(0u);

        gl::Buffer* buffer = create<gl::Buffer>(m_gl, data);

        gl::Attribute attr("a_texcoord_0");
        attr.buffer = buffer;
        attr.offset = byteOffset;
        attr.stride = byteStride;
        attr.dimension = 2;
        attr.type = gl::ComponentType::Float;

        mesh.vertexCount = gltfAttr.getAccessor()->getCount();
        mesh.attributes.emplace_back(attr);
        mesh.defines.define("HAS_TEXCOORD_0_VEC2");
    }
}

void EditorResourceManager::initGrid()
{
    std::vector<glm::vec3> grid1MVertices;
    std::vector<glm::vec3> grid1DMVertices;
    grid1MVertices.reserve(gridSize * 4);
    grid1DMVertices.reserve(gridSize * 32);

    for (int i = -gridSize / 2; i < 0; ++i) {
        grid1MVertices.emplace_back(i, 0, gridSize / 2);
        grid1MVertices.emplace_back(i, 0, -1 * gridSize / 2);
        grid1MVertices.emplace_back(gridSize / 2, 0, i);
        grid1MVertices.emplace_back(-1 * gridSize / 2, 0, i);
        for (int j = 1; j < 10; ++j) {
            grid1DMVertices.emplace_back(i + j * 0.1f, 0, gridSize / 2);
            grid1DMVertices.emplace_back(i + j * 0.1f, 0, -1 * gridSize / 2);
            grid1DMVertices.emplace_back(gridSize / 2, 0, i + j * 0.1f);
            grid1DMVertices.emplace_back(-1 * gridSize / 2, 0, i + j * 0.1f);
        }
    }

    // Leave out origin axes

    for (int i = 1; i <= gridSize / 2; ++i) {
        grid1MVertices.emplace_back(i, 0, gridSize / 2);
        grid1MVertices.emplace_back(i, 0, -1 * gridSize / 2);
        grid1MVertices.emplace_back(gridSize / 2, 0, i);
        grid1MVertices.emplace_back(-1 * gridSize / 2, 0, i);
        for (int j = 1; j < 10; ++j) {
            grid1DMVertices.emplace_back(i - j * 0.1f, 0, gridSize / 2);
            grid1DMVertices.emplace_back(i - j * 0.1f, 0, -1 * gridSize / 2);
            grid1DMVertices.emplace_back(gridSize / 2, 0, i - j * 0.1f);
            grid1DMVertices.emplace_back(-1 * gridSize / 2, 0, i - j * 0.1f);
        }
    }

    m_meterGridVertexBuffer = create<gl::Buffer>(m_gl, core::makeByteRange(grid1MVertices));
    m_decimeterGridVertexBuffer = create<gl::Buffer>(m_gl, core::makeByteRange(grid1DMVertices));

    m_meterGridMesh = create<Mesh>(m_gl);
    m_meterGridMesh->vertexShader = "grid.vert";
    m_meterGridMesh->primitiveTopology = gl::PrimitiveTopology::LineList;
    m_meterGridMesh->vertexCount = 4 * gridSize;
    glm::mat4 transform(1.0f);
    m_meterGridMesh->updateTransform(transform);

    gl::Attribute attribute("a_position");
    attribute.offset = 0u;
    attribute.stride = 0u;
    attribute.normalized = GL_FALSE;
    attribute.dimension = 3;
    attribute.type = gl::ComponentType::Float;
    attribute.buffer = m_meterGridVertexBuffer;
    m_meterGridMesh->attributes.emplace_back(attribute);

    m_meterGridMaterial = create<Material>(m_gl);
    m_meterGridMaterial->fragmentShader = "grid.frag";
    m_meterGridMaterial->setUniform("u_color", glm::vec4(0.72f, 0.72f, 0.72f, 1.0f));
    m_meterGridMaterial->renderPass = m_depthTestedPass;

    m_decimeterGridMesh = create<Mesh>(m_gl);
    m_decimeterGridMesh->vertexShader = "grid.vert";
    m_decimeterGridMesh->primitiveTopology = gl::PrimitiveTopology::LineList;
    m_decimeterGridMesh->vertexCount = 32 * gridSize;
    glm::mat4 transform2(1.0f);
    m_decimeterGridMesh->updateTransform(transform2);

    gl::Attribute attribute2("a_position");
    attribute2.offset = 0u;
    attribute2.stride = 0u;
    attribute2.normalized = GL_FALSE;
    attribute2.dimension = 3;
    attribute2.type = gl::ComponentType::Float;
    attribute2.buffer = m_decimeterGridVertexBuffer;
    m_decimeterGridMesh->attributes.emplace_back(attribute2);

    m_decimeterGridMaterial = create<Material>(m_gl);
    m_decimeterGridMaterial->fragmentShader = "grid.frag";
    m_decimeterGridMaterial->setUniform("u_color", glm::vec4(0.72f, 0.72f, 0.72f, 0.5f));
    m_decimeterGridMaterial->renderPass = m_depthTestedPass;
}

std::vector<std::tuple<Mesh*, Material*>> EditorResourceManager::getAxes() const
{
    return {
        { m_originAxesXMesh, m_originAxesXMaterial },
        { m_originAxesZMesh, m_originAxesZMaterial }
    };
}

std::tuple<Mesh*, Material*> EditorResourceManager::getBigGrid() const
{
    return { m_meterGridMesh, m_meterGridMaterial };
}

std::tuple<Mesh*, Material*> EditorResourceManager::getSmallGrid() const
{
    return { m_decimeterGridMesh, m_decimeterGridMaterial };
}

void EditorResourceManager::initOriginAxes()
{
    std::vector<glm::vec3> xAxis;
    std::vector<glm::vec3> zAxis;
    xAxis.emplace_back(-gridSize / 2, 0, -axesThickness / 2);
    xAxis.emplace_back(-gridSize / 2, 0, axesThickness / 2);
    xAxis.emplace_back(gridSize / 2, 0, -axesThickness / 2);
    xAxis.emplace_back(gridSize / 2, 0, axesThickness / 2);

    zAxis.emplace_back(-axesThickness / 2, 0, -gridSize / 2);
    zAxis.emplace_back(axesThickness / 2, 0, -gridSize / 2);
    zAxis.emplace_back(-axesThickness / 2, 0, gridSize / 2);
    zAxis.emplace_back(axesThickness / 2, 0, gridSize / 2);

    m_originAxisXVertexBuffer = create<gl::Buffer>(m_gl, core::makeByteRange(xAxis));
    m_originAxisZVertexBuffer = create<gl::Buffer>(m_gl, core::makeByteRange(zAxis));

    m_originAxesXMesh = create<Mesh>(m_gl);
    m_originAxesXMesh->vertexShader = "grid.vert";
    m_originAxesXMesh->primitiveTopology = gl::PrimitiveTopology::TriangleStrip;
    m_originAxesXMesh->vertexCount = 4;
    glm::mat4 transform(1.0f);
    m_originAxesXMesh->updateTransform(transform);

    gl::Attribute attribute("a_position");
    attribute.offset = 0u;
    attribute.stride = 0u;
    attribute.normalized = GL_FALSE;
    attribute.dimension = 3;
    attribute.type = gl::ComponentType::Float;
    attribute.buffer = m_originAxisXVertexBuffer;
    m_originAxesXMesh->attributes.emplace_back(attribute);

    m_originAxesXMaterial = create<Material>(m_gl);
    m_originAxesXMaterial->fragmentShader = "grid.frag";
    m_originAxesXMaterial->setUniform("u_color", colorAxisX);
    m_originAxesXMaterial->renderPass = m_depthTestedPass;
    m_originAxesXMaterial->isDoubleSided = true;

    m_originAxesZMesh = create<Mesh>(m_gl);
    m_originAxesZMesh->vertexShader = "grid.vert";
    m_originAxesZMesh->primitiveTopology = gl::PrimitiveTopology::TriangleStrip;
    m_originAxesZMesh->vertexCount = 4;

    glm::mat4 transform2(1.0f);
    m_originAxesZMesh->updateTransform(transform2);

    gl::Attribute attribute2("a_position");
    attribute2.offset = 0u;
    attribute2.stride = 0u;
    attribute2.normalized = GL_FALSE;
    attribute2.dimension = 3;
    attribute2.type = gl::ComponentType::Float;
    attribute2.buffer = m_originAxisZVertexBuffer;
    m_originAxesZMesh->attributes.emplace_back(attribute2);

    m_originAxesZMaterial = create<Material>(m_gl);
    m_originAxesZMaterial->fragmentShader = "grid.frag";
    m_originAxesZMaterial->setUniform("u_color", colorAxisZ);
    m_originAxesZMaterial->renderPass = m_depthTestedPass;
    m_originAxesZMaterial->isDoubleSided = true;
}

std::unique_ptr<Material> EditorResourceManager::makeBoundingBoxMaterial()
{
    auto material = std::make_unique<Material>(m_gl);
    material->fragmentShader = "generic_helper.frag";
    material->renderPass = m_depthTestedPass;
    material->setUniform("u_Color", glm::vec3(1, 1, 0));
    return material;
}

void EditorResourceManager::initBoundingBoxBuffers()
{
    std::vector<glm::vec3> positions = {
        { 0, 0, 0 },
        { 1, 0, 0 },
        { 0, 1, 0 },
        { 0, 0, 1 },
        { 0, 1, 1 },
        { 1, 1, 0 },
        { 1, 0, 1 },
        { 1, 1, 1 },
    };
    std::vector<uint16_t> indices = {
        0, 1, 1, 5, 5, 2, 2, 0,
        3, 6, 6, 7, 7, 4, 4, 3,
        0, 3, 1, 6, 5, 7, 2, 4
    };

    m_boundingBoxIndexCount = indices.size();
    m_boundingBoxVertexCount = positions.size();

    m_boundingBoxIndexBuffer = create<gl::Buffer>(m_gl, core::makeByteRange(indices));
    m_boundingBoxVertexBuffer = create<gl::Buffer>(m_gl, core::makeByteRange(positions));
}

std::unique_ptr<Mesh> EditorResourceManager::makeBoundingBoxMesh()
{
    gl::Attribute attribute("a_position");
    attribute.buffer = m_boundingBoxVertexBuffer;

    auto mesh = std::make_unique<Mesh>(m_gl);
    mesh->vertexShader = "generic_helper.vert";
    mesh->primitiveTopology = gl::PrimitiveTopology::LineList;
    mesh->vertexCount = m_boundingBoxVertexCount;
    mesh->attributes.emplace_back(attribute);
    mesh->indexBuffer = m_boundingBoxIndexBuffer;
    mesh->indexCount = m_boundingBoxIndexCount;
    mesh->updateTransform(glm::identity<glm::mat4>());
    return mesh;
}

std::unique_ptr<Material> EditorResourceManager::makeSkinHelperMaterial()
{
    auto material = std::make_unique<Material>(m_gl);
    material->fragmentShader = "generic_helper.frag";
    material->renderPass = m_foregroundPass;
    return material;
}

std::unique_ptr<Material> EditorResourceManager::makeBoneHelperMaterial()
{
    auto material = std::make_unique<Material>(m_gl);
    material->fragmentShader = "generic_helper.frag";
    material->renderPass = m_foregroundPass;
    return material;
}

Material* EditorResourceManager::makeWireframeMaterial()
{
    auto material = create<Material>(m_gl);
    material->renderPass = m_opaquePass;
    material->isDoubleSided = true; // disable back face culling. Beware, this is set for shared materials!
    material->fragmentShader = "wireframe_material.frag";
    return material;
}

void EditorResourceManager::initSkinHelperBuffers()
{
    std::vector<glm::vec3> positions;

    size_t N = 32;
    float circleOuterRadius = 0.8;
    float circleInnerRadius = 0.67;

    positions.reserve(N * 6);

    for (size_t i = 0; i < N; ++i) {
        float alpha1 = static_cast<float>(i) / static_cast<float>(N) * 2 * glm::pi<float>();
        float alpha2 = static_cast<float>(i + 1) / static_cast<float>(N) * 2 * glm::pi<float>();

        auto a = glm::rotateZ(glm::vec3(0, circleOuterRadius, 0), alpha1);
        auto b = glm::rotateZ(glm::vec3(0, circleOuterRadius, 0), alpha2);
        auto c = glm::rotateZ(glm::vec3(0, circleInnerRadius, 0), alpha1);
        auto d = glm::rotateZ(glm::vec3(0, circleInnerRadius, 0), alpha2);

        positions.push_back(a);
        positions.push_back(d);
        positions.push_back(b);

        positions.push_back(a);
        positions.push_back(c);
        positions.push_back(d);
    }

    m_skinHelperVertexCount = positions.size();
    m_skinHelperBuffer = create<gl::Buffer>(m_gl, core::makeByteRange(positions));
}

void EditorResourceManager::initBoneHelperBuffers()
{
    float arrowFootRadius = 0.01;
    float arrowFootWidth = 0.025;
    float arrowFootLength = 0.85;
    float arrowHeadWidth = 0.2;
    float arrowHeadRadius = 0.01;

    float arrowHeadLength = 1 - arrowFootLength;
    float arrowFootMiddleWidth = arrowFootWidth - 2 * arrowFootRadius;

    glm::vec3 arrowHeadNormal = arrowHeadRadius * glm::normalize(glm::vec3(arrowHeadWidth / 2, arrowHeadLength, 0));
    glm::vec3 arrowHeadNormalDown = arrowHeadRadius * glm::normalize(glm::vec3(arrowHeadWidth / 2, -arrowHeadLength, 0));

    std::vector<glm::vec3> positions = {
        { arrowFootRadius, arrowFootWidth / 2, 0 },
        { arrowFootLength, arrowFootWidth / 2, 0 },
        { arrowFootRadius, -arrowFootWidth / 2, 0 },
        { arrowFootLength, arrowFootWidth / 2, 0 },
        { arrowFootLength, -arrowFootWidth / 2, 0 },
        { arrowFootRadius, -arrowFootWidth / 2, 0 },

        { 0, arrowFootMiddleWidth / 2, 0 },
        { arrowFootRadius, arrowFootMiddleWidth / 2, 0 },
        { 0, -arrowFootMiddleWidth / 2, 0 },
        { arrowFootRadius, arrowFootMiddleWidth / 2, 0 },
        { arrowFootRadius, -arrowFootMiddleWidth / 2, 0 },
        { 0, -arrowFootMiddleWidth / 2, 0 },

        { arrowFootLength + arrowHeadRadius, arrowHeadWidth / 2 - arrowHeadRadius, 0 },
        { 1 - arrowHeadRadius, 0, 0 },
        { arrowFootLength + arrowHeadRadius, -arrowHeadWidth / 2 + arrowHeadRadius, 0 },

        { arrowFootLength, arrowHeadWidth / 2 - arrowHeadRadius, 0 },
        { arrowFootLength + arrowHeadRadius, arrowHeadWidth / 2 - arrowHeadRadius, 0 },
        { arrowFootLength, -arrowHeadWidth / 2 + arrowHeadRadius, 0 },
        { arrowFootLength + arrowHeadRadius, arrowHeadWidth / 2 - arrowHeadRadius, 0 },
        { arrowFootLength + arrowHeadRadius, -arrowHeadWidth / 2 + arrowHeadRadius, 0 },
        { arrowFootLength, -arrowHeadWidth / 2 + arrowHeadRadius, 0 },

        { arrowFootLength + arrowHeadRadius, arrowHeadWidth / 2 - arrowHeadRadius, 0 },
        glm::vec3(arrowFootLength + arrowHeadRadius, arrowHeadWidth / 2 - arrowHeadRadius, 0) + arrowHeadNormal,
        { 1 - arrowHeadRadius, 0, 0 },
        glm::vec3(arrowFootLength + arrowHeadRadius, arrowHeadWidth / 2 - arrowHeadRadius, 0) + arrowHeadNormal,
        glm::vec3(1 - arrowHeadRadius, 0, 0) + arrowHeadNormal,
        { 1 - arrowHeadRadius, 0, 0 },

        { arrowFootLength + arrowHeadRadius, -arrowHeadWidth / 2 + arrowHeadRadius, 0 },
        { 1 - arrowHeadRadius, 0, 0 },
        glm::vec3(arrowFootLength + arrowHeadRadius, -arrowHeadWidth / 2 + arrowHeadRadius, 0) + arrowHeadNormalDown,
        glm::vec3(arrowFootLength + arrowHeadRadius, -arrowHeadWidth / 2 + arrowHeadRadius, 0) + arrowHeadNormalDown,
        { 1 - arrowHeadRadius, 0, 0 },
        glm::vec3(1 - arrowHeadRadius, 0, 0) + arrowHeadNormalDown,
    };

    size_t N = 8;

    // Generate rounded arrow bottom
    for (size_t i = 0; i < N; ++i) {
        float alpha1 = static_cast<float>(i) / static_cast<float>(N) * 0.5 * glm::pi<float>();
        float alpha2 = static_cast<float>(i + 1) / static_cast<float>(N) * 0.5 * glm::pi<float>();
        glm::vec3 top(arrowFootRadius, arrowFootMiddleWidth / 2, 0);
        glm::vec3 bottom(arrowFootRadius, -arrowFootMiddleWidth / 2, 0);
        positions.push_back(top + glm::rotateZ(glm::vec3(0, arrowFootRadius, 0), alpha1));
        positions.push_back(top);
        positions.push_back(top + glm::rotateZ(glm::vec3(0, arrowFootRadius, 0), alpha2));
        positions.push_back(bottom + glm::rotateZ(glm::vec3(0, -arrowFootRadius, 0), -alpha1));
        positions.push_back(bottom + glm::rotateZ(glm::vec3(0, -arrowFootRadius, 0), -alpha2));
        positions.push_back(bottom);
    }

    // Generate rounded arrow head
    float halfAngle = atan(arrowHeadNormal.y / arrowHeadNormal.x);
    for (size_t i = 0; i < N; ++i) {
        float alpha1 = static_cast<float>(i) / static_cast<float>(N) * 2 * halfAngle - halfAngle;
        float alpha2 = static_cast<float>(i + 1) / static_cast<float>(N) * 2 * halfAngle - halfAngle;
        glm::vec3 base(1 - arrowHeadRadius, 0, 0);
        positions.push_back(base + glm::rotateZ(glm::vec3(arrowHeadRadius, 0, 0), alpha2));
        positions.push_back(base + glm::rotateZ(glm::vec3(arrowHeadRadius, 0, 0), alpha1));
        positions.push_back(base);
    }
    for (size_t i = 0; i < N; ++i) {
        float alpha1 = static_cast<float>(i) / static_cast<float>(N) * (glm::pi<float>() - halfAngle);
        float alpha2 = static_cast<float>(i + 1) / static_cast<float>(N) * (glm::pi<float>() - halfAngle);
        glm::vec3 top(arrowFootLength + arrowHeadRadius, arrowHeadWidth / 2 - arrowHeadRadius, 0);
        positions.push_back(top + glm::rotateZ(glm::vec3(-arrowHeadRadius, 0, 0), -alpha1));
        positions.push_back(top + glm::rotateZ(glm::vec3(-arrowHeadRadius, 0, 0), -alpha2));
        positions.push_back(top);
        glm::vec3 bottom(arrowFootLength + arrowHeadRadius, -arrowHeadWidth / 2 + arrowHeadRadius, 0);
        positions.push_back(bottom + glm::rotateZ(glm::vec3(-arrowHeadRadius, 0, 0), alpha2));
        positions.push_back(bottom + glm::rotateZ(glm::vec3(-arrowHeadRadius, 0, 0), alpha1));
        positions.push_back(bottom);
    }

    m_boneHelperVertexCount = positions.size();
    m_boneHelperBuffer = create<gl::Buffer>(m_gl, core::makeByteRange(positions));
}

std::unique_ptr<Mesh> EditorResourceManager::makeSkinHelperMesh()
{
    gl::Attribute attribute("a_position");
    attribute.buffer = m_skinHelperBuffer;
    attribute.dimension = 3;
    attribute.type = gl::ComponentType::Float;

    auto mesh = std::make_unique<Mesh>(m_gl);
    mesh->vertexShader = "generic_helper.vert";
    mesh->primitiveTopology = gl::PrimitiveTopology::TriangleList;
    mesh->attributes.emplace_back(attribute);
    mesh->updateTransform(glm::identity<glm::mat4>());
    mesh->vertexCount = m_skinHelperVertexCount;

    return mesh;
}

std::unique_ptr<Mesh> EditorResourceManager::makeBoneHelperMesh()
{
    gl::Attribute attribute("a_position");
    attribute.buffer = m_boneHelperBuffer;

    auto mesh = std::make_unique<Mesh>(m_gl);
    mesh->vertexShader = "generic_helper.vert";
    mesh->primitiveTopology = gl::PrimitiveTopology::TriangleList;
    mesh->attributes.emplace_back(attribute);
    mesh->updateTransform(glm::identity<glm::mat4>());
    mesh->vertexCount = m_boneHelperVertexCount;

    return mesh;
}

Mesh* EditorResourceManager::createFullscreenTriangleMesh()
{
    Mesh* mesh = create<Mesh>(m_gl);
    mesh->vertexShader = "fullscreen.vert";
    mesh->primitiveTopology = gl::PrimitiveTopology::TriangleList;
    mesh->vertexCount = 3;
    return mesh;
}

Material* EditorResourceManager::createGizmoPostMaterial()
{
    Material* material = createGizmoMaterial();
    material->renderPass = m_foregroundMaskedPass;
    return material;
}

Material* EditorResourceManager::createGizmoMaterial()
{
    renderer::Material* material = create<Material>(m_gl);
    material->fragmentShader = "gizmo_material.frag";
    material->renderPass = m_foregroundMaskPass;
    material->setUniform("u_BaseColorFactor", math::vec4(1.0f));
    material->isDoubleSided = true;

    return material;
}

Material* EditorResourceManager::createSilhouetteMaterial()
{
    renderer::Material* material = create<Material>(m_gl);
    material->fragmentShader = "pbr.frag";
    material->defines.define("ALPHAMODE", 0);
    material->defines.define("ALPHAMODE_OPAQUE", static_cast<int>(gltf::AlphaMode::Opaque));
    material->defines.define("ALPHAMODE_MASK", static_cast<int>(gltf::AlphaMode::Mask));
    material->defines.define("ALPHAMODE_BLEND", static_cast<int>(gltf::AlphaMode::Blend));
    material->defines.define("DEBUG", 0);
    material->renderPass = m_outlinePass;

    return material;
}

Material* EditorResourceManager::createOutlineMaterial()
{
    renderer::Material* material = create<Material>(m_gl);
    material->fragmentShader = "outline.frag";
    material->renderPass = m_postprocessingPass;

    return material;
}

static OrientationGizmoPiece cameraOrientationMeshNameToPiece(const core::string& name)
{
    if (name == "Top") {
        return OrientationGizmoPiece::Top;
    } else if (name == "Bottom") {
        return OrientationGizmoPiece::Bottom;
    } else if (name == "Left") {
        return OrientationGizmoPiece::Left;
    } else if (name == "Right") {
        return OrientationGizmoPiece::Right;
    } else if (name == "Front") {
        return OrientationGizmoPiece::Front;
    } else if (name == "Back") {
        return OrientationGizmoPiece::Back;
    } else {
        return OrientationGizmoPiece::Invalid;
    }
}

static GizmoPiece translationMeshNameToPiece(const core::string& name)
{
    if (name == "AxisX") {
        return GizmoPiece::TranslationAxisX;
    } else if (name == "AxisY") {
        return GizmoPiece::TranslationAxisY;
    } else if (name == "AxisZ") {
        return GizmoPiece::TranslationAxisZ;
    } else if (name == "PlaneX") {
        return GizmoPiece::TranslationPlaneX;
    } else if (name == "PlaneY") {
        return GizmoPiece::TranslationPlaneY;
    } else if (name == "PlaneZ") {
        return GizmoPiece::TranslationPlaneZ;
    } else if (name == "PlaneX_Highlight") {
        return GizmoPiece::TranslationHighlightPlaneX;
    } else if (name == "PlaneY_Highlight") {
        return GizmoPiece::TranslationHighlightPlaneY;
    } else if (name == "PlaneZ_Highlight") {
        return GizmoPiece::TranslationHighlightPlaneZ;
    } else {
        return GizmoPiece::Invalid;
    }
}

static GizmoPiece scaleMeshNameToPiece(const core::string& name)
{
    if (name == "AxisX") {
        return GizmoPiece::ScaleAxisX;
    } else if (name == "AxisY") {
        return GizmoPiece::ScaleAxisY;
    } else if (name == "AxisZ") {
        return GizmoPiece::ScaleAxisZ;
    } else if (name == "PlaneX") {
        return GizmoPiece::ScalePlaneX;
    } else if (name == "PlaneY") {
        return GizmoPiece::ScalePlaneY;
    } else if (name == "PlaneZ") {
        return GizmoPiece::ScalePlaneZ;
    } else if (name == "PlaneX_Highlight") {
        return GizmoPiece::ScaleHighlightPlaneX;
    } else if (name == "PlaneY_Highlight") {
        return GizmoPiece::ScaleHighlightPlaneY;
    } else if (name == "PlaneZ_Highlight") {
        return GizmoPiece::ScaleHighlightPlaneZ;
    } else if (name == "Center") {
        return GizmoPiece::ScaleCenter;
    } else {
        return GizmoPiece::Invalid;
    }
}

static GizmoPiece rotationMeshNameToPiece(const core::string& name)
{
    if (name == "XCircle") {
        return GizmoPiece::RotationXCircle;
    } else if (name == "YCircle") {
        return GizmoPiece::RotationYCircle;
    } else if (name == "ZCircle") {
        return GizmoPiece::RotationZCircle;
    } else if (name == "DepthStencil") {
        return GizmoPiece::RotationDepthStencil;
    } else {
        return GizmoPiece::Invalid;
    }
}

std::optional<GizmoTransformMode> EditorResourceManager::mapGizmoMeshToTransformMode(Mesh* mesh) const
{
    if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationAxisX)])) {
        return renderer::GizmoTransformMode::TranslateX;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationAxisY)])) {
        return renderer::GizmoTransformMode::TranslateY;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationAxisZ)])) {
        return renderer::GizmoTransformMode::TranslateZ;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationPlaneX)])
        || mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationHighlightPlaneX)])) {
        return renderer::GizmoTransformMode::TranslatePlaneX;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationPlaneY)])
        || mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationHighlightPlaneY)])) {
        return renderer::GizmoTransformMode::TranslatePlaneY;
    } else if ((mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationPlaneZ)]))
        || mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::TranslationHighlightPlaneZ)])) {
        return renderer::GizmoTransformMode::TranslatePlaneZ;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScaleAxisX)])) {
        return renderer::GizmoTransformMode::ScaleX;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScaleAxisY)])) {
        return renderer::GizmoTransformMode::ScaleY;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScaleAxisZ)])) {
        return renderer::GizmoTransformMode::ScaleZ;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScalePlaneX)]) || mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScaleHighlightPlaneX)])) {
        return renderer::GizmoTransformMode::ScalePlaneX;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScalePlaneY)]) || mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScaleHighlightPlaneY)])) {
        return renderer::GizmoTransformMode::ScalePlaneY;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScalePlaneZ)]) || mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScaleHighlightPlaneZ)])) {
        return renderer::GizmoTransformMode::ScalePlaneZ;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::ScaleCenter)])) {
        return renderer::GizmoTransformMode::ScaleUniform;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::RotationXCircle)])) {
        return renderer::GizmoTransformMode::RotateX;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::RotationYCircle)])) {
        return renderer::GizmoTransformMode::RotateY;
    } else if (mesh == std::get<renderer::Mesh*>((m_gizmoMeshMats)[static_cast<size_t>(renderer::GizmoPiece::RotationZCircle)])) {
        return renderer::GizmoTransformMode::RotateZ;
    } else {
        return {};
    }
}

std::optional<OrientationGizmoPiece> EditorResourceManager::mapOrientationGizmoMeshToPiece(Mesh* mesh) const
{
    if (mesh == std::get<Mesh*>(m_orientationGizmoMeshMats.at(OrientationGizmoPiece::Top))) {
        return OrientationGizmoPiece::Top;
    } else if (mesh == std::get<Mesh*>(m_orientationGizmoMeshMats.at(OrientationGizmoPiece::Bottom))) {
        return OrientationGizmoPiece::Bottom;
    } else if (mesh == std::get<Mesh*>(m_orientationGizmoMeshMats.at(OrientationGizmoPiece::Front))) {
        return OrientationGizmoPiece::Front;
    } else if (mesh == std::get<Mesh*>(m_orientationGizmoMeshMats.at(OrientationGizmoPiece::Back))) {
        return OrientationGizmoPiece::Back;
    } else if (mesh == std::get<Mesh*>(m_orientationGizmoMeshMats.at(OrientationGizmoPiece::Right))) {
        return OrientationGizmoPiece::Right;
    } else if (mesh == std::get<Mesh*>(m_orientationGizmoMeshMats.at(OrientationGizmoPiece::Left))) {
        return OrientationGizmoPiece::Left;
    }

    return std::nullopt;
}

void ux3d::renderer::EditorResourceManager::createFramebufferTexture(GLuint textureID)
{
    if ((m_framebufferTexture != nullptr || m_framebufferSamplerState != nullptr) && m_framebufferTexture->handle == textureID) {
        return;
    }

    if (m_framebufferTexture != nullptr) {
        // The framebuffer texture are is by Qt, so we cannot delete it.
        m_framebufferTexture->handle = gl::invalid;
        destroy(m_framebufferTexture);
    }
    if (m_framebufferSamplerState != nullptr) {
        destroy(m_framebufferSamplerState);
    }

    gl::SamplerState samplerState;
    samplerState.minFilter = gl::TextureMinFilter::Linear;
    samplerState.magFilter = gl::TextureMagFilter::Linear;
    samplerState.wrapS = gl::TextureWrap::ClampToEdge;
    samplerState.wrapT = gl::TextureWrap::ClampToEdge;

    m_framebufferSamplerState = create<gl::SamplerState>(std::move(samplerState));
    m_framebufferTexture = create<gl::Texture>(m_gl, textureID, gl::TextureTarget::Texture2d);

    // remove previous u_framebuffer sampler from material
    auto framebufferIt = std::find_if(m_outlineMaterial->samplers.begin(), m_outlineMaterial->samplers.end(), [](auto& sampler) {
        return sampler.name == "u_framebuffer";
    });
    if (framebufferIt != m_outlineMaterial->samplers.end()) {
        m_outlineMaterial->samplers.erase(framebufferIt);
    }

    m_outlineMaterial->samplers.emplace_back("u_framebuffer", m_framebufferTexture, m_framebufferSamplerState);
}

void ux3d::renderer::EditorResourceManager::setBlitFramebufferFunction(std::function<void()> blitFramebuffer, math::vec4 clearColor)
{
    m_outlinePass->setBlitFramebufferFunction(blitFramebuffer, clearColor);
};
