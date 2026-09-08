// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "../TestAssets.h"
#include "catch2/catch.hpp"
#include "gltf/Gltf.h"

namespace ux3d::gltf::tests {
namespace {
    constexpr auto TestNamespace = "[tools.gltf.GltfToJson]";
    const gltf::Gltf& resolveTestAsset(TestAsset& _Asset) { return _Asset.getRoundtrip(); }
}

TEST_CASE("GltfToJson: scene", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& scenes = gltf.getScenes();
    REQUIRE(scenes.size() == 1);
    const gltf::Scene& scene = scenes.front();

    SECTION("name")
    {
        CHECK(scene.getName() == "Main Scene");
    }

    SECTION("nodes")
    {
        const auto& sceneNodes = scene.getNodes();
        REQUIRE(sceneNodes.size() == 1);
        CHECK(sceneNodes.at(0) != nullptr);
    }

    SECTION("default scene")
    {
        const gltf::Scene* defaultScene = gltf.getDefaultScene();
        REQUIRE(defaultScene != nullptr);
        CHECK(defaultScene == &scene);
    }
}

TEST_CASE("GltfToJson: nodes", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& nodes = gltf.getNodes();
    REQUIRE(nodes.size() == 6);

    SECTION("names")
    {
        CHECK(nodes.at(0).getName() == "BoomBox");
        CHECK(nodes.at(1).getName() == "CoordinateSystem");
        CHECK(nodes.at(2).getName() == "X_axis");
        CHECK(nodes.at(3).getName() == "Y_axis");
        CHECK(nodes.at(4).getName() == "Z_axis");
        CHECK(nodes.at(5).getName() == "BoomBox_Coordinates");
    }

    SECTION("children")
    {
        CHECK(nodes.at(0).getChildren().empty());
        CHECK(nodes.at(1).getChildren().empty());
        CHECK(nodes.at(2).getChildren().empty());
        CHECK(nodes.at(3).getChildren().empty());
        CHECK(nodes.at(4).getChildren().empty());
        CHECK(nodes.at(5).getChildren().size() == 5);
    }

    SECTION("scales")
    {
        CHECK(nodes.at(0).getScaleOrDefault().x == 1.0);
        CHECK(nodes.at(0).getScaleOrDefault().y == 1.0);
        CHECK(nodes.at(0).getScaleOrDefault().z == 1.0);

        CHECK(nodes.at(2).getScaleOrDefault().x == 0.06);
        CHECK(nodes.at(2).getScaleOrDefault().y == 0.06);
        CHECK(nodes.at(2).getScaleOrDefault().z == 0.06);
    }

    SECTION("rotations")
    {
        CHECK(nodes.at(0).getRotationOrDefault().x == 0.0);
        CHECK(nodes.at(0).getRotationOrDefault().y == 0.0);
        CHECK(nodes.at(0).getRotationOrDefault().z == 0.0);
        CHECK(nodes.at(0).getRotationOrDefault().w == 1.0);

        CHECK(nodes.at(5).getRotationOrDefault().x == 0.0);
        CHECK(nodes.at(5).getRotationOrDefault().y == 1.0);
        CHECK(nodes.at(5).getRotationOrDefault().z == 0.0);
        CHECK(nodes.at(5).getRotationOrDefault().w == 0.0);
    }

    SECTION("meshes")
    {
        CHECK(nodes.at(0).getMesh() != nullptr);
        CHECK(nodes.at(1).getMesh() != nullptr);
        CHECK(nodes.at(2).getMesh() != nullptr);
        CHECK(nodes.at(3).getMesh() != nullptr);
        CHECK(nodes.at(4).getMesh() != nullptr);
        CHECK(nodes.at(5).getMesh() == nullptr);
    }

    SECTION("cameras")
    {
        CHECK(nodes.at(0).getCamera() == nullptr);
        CHECK(nodes.at(1).getCamera() != nullptr);
        CHECK(nodes.at(2).getCamera() == nullptr);
        CHECK(nodes.at(3).getCamera() == nullptr);
        CHECK(nodes.at(4).getCamera() == nullptr);
        CHECK(nodes.at(5).getCamera() == nullptr);
    }

    SECTION("lights")
    {
        CHECK(nodes.at(0).getLight() != nullptr);
        CHECK(nodes.at(1).getLight() == nullptr);
    }
}

TEST_CASE("GltfToJson: cameras", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& cameras = gltf.getCameras();
    REQUIRE(cameras.size() == 2);

    SECTION("perspective")
    {
        const gltf::Camera& camera = cameras.at(0);
        CHECK(camera.getType() == gltf::Camera::ProjectionType::Perspective);
        CHECK_FALSE(camera.getCameraOrthographic().has_value());
        const auto& perspectiveCamera = camera.getCameraPerspective();
        REQUIRE(perspectiveCamera.has_value());
        REQUIRE(perspectiveCamera->getAspectRatio().has_value());
        CHECK(perspectiveCamera->getAspectRatio().value() == 2.3);
        CHECK(perspectiveCamera->getYFov() == 0.8);
        CHECK(perspectiveCamera->getZFar() == 101.0);
        CHECK(perspectiveCamera->getZNear() == 7.0);
    }

    SECTION("orthographic")
    {
        const gltf::Camera& camera = cameras.at(1);
        CHECK(camera.getType() == gltf::Camera::ProjectionType::Orthographic);
        CHECK_FALSE(camera.getCameraPerspective().has_value());
        const auto& orthographicCamera = camera.getCameraOrthographic();
        REQUIRE(orthographicCamera.has_value());
        CHECK(orthographicCamera->getXMag() == 0.7);
        CHECK(orthographicCamera->getYMag() == 0.6);
        CHECK(orthographicCamera->getZFar() == 102.0);
        CHECK(orthographicCamera->getZNear() == 8.0);
    }
}

TEST_CASE("GltfToJson: lights", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& lights = gltf.getLights();
    REQUIRE(lights.size() == 3);

    SECTION("directional")
    {
        const gltf::Light& light = lights.at(0);
        CHECK(light.getType() == gltf::Light::Type::Directional);
        CHECK(light.getName() == "L_sun");
        CHECK(light.getIntensityOrDefault() == 1.337);
        CHECK_FALSE(light.getSpot().has_value());
        CHECK(light.getColorOrDefault().r == 0.1);
        CHECK(light.getColorOrDefault().g == 0.2);
        CHECK(light.getColorOrDefault().b == 0.3);
    }

    SECTION("spot")
    {
        const gltf::Light& light = lights.at(1);
        CHECK(light.getType() == gltf::Light::Type::Spot);
        CHECK(light.getName() == "L_spot");
        CHECK(light.getIntensityOrDefault() == 1.0);
        CHECK(light.getColorOrDefault().r == 0.9);
        CHECK(light.getColorOrDefault().g == 0.8);
        CHECK(light.getColorOrDefault().b == 0.7);

        const auto& spot = light.getSpot();
        REQUIRE(spot.has_value());
        CHECK(spot->getInnerConeAngleOrDefault() == 0.1);
        CHECK(spot->getOuterConeAngleOrDefault() == 1.1);
    }

    SECTION("point")
    {
        const gltf::Light& light = lights.at(2);
        CHECK(light.getType() == gltf::Light::Type::Point);
        CHECK(light.getName() == "L_point");
        CHECK(light.getIntensityOrDefault() == 3.14);
        CHECK_FALSE(light.getSpot().has_value());
        CHECK(light.getColorOrDefault().r == 1.0);
        CHECK(light.getColorOrDefault().g == 1.0);
        CHECK(light.getColorOrDefault().b == 1.0);
    }
}

TEST_CASE("GltfToJson: meshes", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& meshes = gltf.getMeshes();
    REQUIRE(meshes.size() == 5);

    SECTION("names")
    {
        CHECK(meshes.at(0).getName() == "BoomBox");
        CHECK(meshes.at(1).getName() == "CoordinateSystem");
        CHECK(meshes.at(2).getName() == "X_axis");
        CHECK(meshes.at(3).getName() == "Y_axis");
        CHECK(meshes.at(4).getName() == "Z_axis");
    }

    SECTION("primitives")
    {
        for (const gltf::Mesh& mesh : meshes) {
            const auto& primitives = mesh.getPrimitives();
            REQUIRE(primitives.size() == 1);

            for (const gltf::Primitive& primitive : primitives) {
                CHECK(primitive.getIndicesAccessor() != nullptr);
                CHECK(primitive.getMaterial() != nullptr);

                const auto& attributes = primitive.getAttributes();
                REQUIRE(attributes.size() == 4);
                CHECK(attributes.at(0).getName() == "NORMAL");
                CHECK(attributes.at(1).getName() == "POSITION");
                CHECK(attributes.at(2).getName() == "TANGENT");
                CHECK(attributes.at(3).getName() == "TEXCOORD_0");

                for (const gltf::Attribute& attribute : attributes) {
                    CHECK(attribute.getAccessor() != nullptr);
                }
            }
        }
    }
}

TEST_CASE("GltfToJson: buffers", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& buffers = gltf.getBuffers();
    REQUIRE(buffers.size() == 1);
    const gltf::Buffer& buffer = buffers.at(0);

    SECTION("uri")
    {
        CHECK(buffer.getUri() == "BoomBoxWithAxes.bin");
    }

    SECTION("byte length")
    {
        core::MemoryRange<const uint8_t> bufferData;
        buffer.getData(bufferData);

        CHECK(bufferData.size() == 591168);
    }
}

TEST_CASE("GltfToJson: buffer views", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& bufferViews = gltf.getBufferViews();
    REQUIRE(bufferViews.size() == 25);

    uint32_t accumulatedLength = 0;
    for (const gltf::BufferView& bufferView : bufferViews) {
        CHECK(bufferView.getByteOffset() == accumulatedLength);
        accumulatedLength += bufferView.getByteLength();
    }
}

TEST_CASE("GltfToJson: accessors", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& accessors = gltf.getAccessors();
    REQUIRE(accessors.size() == 25);
    const Accessor& accessor = accessors.at(3);

    SECTION("min")
    {
        const core::Vector<double>& min = accessor.getMin();
        REQUIRE(min.size() == 3);
        CHECK(min[0] == -0.009921154);
        CHECK(min[1] == -0.00977163);
        CHECK(min[2] == -0.0100762453);
    }

    SECTION("max")
    {
        const core::Vector<double>& max = accessor.getMax();
        REQUIRE(max.size() == 3);
        CHECK(max[0] == 0.009921154);
        CHECK(max[1] == 0.00977163);
        CHECK(max[2] == 0.0100762453);
    }
}

TEST_CASE("GltfToJson: materials", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& materials = gltf.getMaterials();
    REQUIRE(materials.size() == 6);

    SECTION("M_BoomBox")
    {
        const gltf::Material& boomBoxMaterial = materials.at(0);
        CHECK(boomBoxMaterial.getName() == "M_BoomBox");
        REQUIRE(boomBoxMaterial.getNormalTextureInfo().has_value());
        CHECK(boomBoxMaterial.getNormalTextureInfo()->getTexture() != nullptr);
        REQUIRE(boomBoxMaterial.getEmissiveTextureInfo().has_value());
        CHECK(boomBoxMaterial.getEmissiveTextureInfo()->getTexture() != nullptr);
        CHECK(false == boomBoxMaterial.getOcclusionTextureInfo().has_value());
        CHECK(boomBoxMaterial.getEmissiveFactorOrDefault().r == 1.0);
        CHECK(boomBoxMaterial.getEmissiveFactorOrDefault().g == 1.0);
        CHECK(boomBoxMaterial.getEmissiveFactorOrDefault().b == 1.0);

        const auto& metallicRoughness = boomBoxMaterial.getPbrMetallicRoughness();
        REQUIRE(metallicRoughness.has_value());
        REQUIRE(metallicRoughness->getBaseColorTextureInfo().has_value());
        CHECK(metallicRoughness->getBaseColorTextureInfo()->getTexture() != nullptr);
        CHECK(metallicRoughness->getBaseColorTextureInfo()->getEffectiveTexCoord() == 0);
        REQUIRE(metallicRoughness->getMetallicRoughnessTextureInfo().has_value());
        CHECK(metallicRoughness->getMetallicRoughnessTextureInfo()->getTexture() != nullptr);
        CHECK(metallicRoughness->getRoughnessFactorOrDefault() == 1.0);
        CHECK(metallicRoughness->getMetallicFactorOrDefault() == 1.0);
    }

    SECTION("M_Coordinates")
    {
        const gltf::Material& coordinatesMaterial = materials.at(1);
        CHECK(coordinatesMaterial.getName() == "M_Coordinates");
        CHECK(false == coordinatesMaterial.getNormalTextureInfo().has_value());
        CHECK(false == coordinatesMaterial.getEmissiveTextureInfo().has_value());
        CHECK(false == coordinatesMaterial.getOcclusionTextureInfo().has_value());
        CHECK(coordinatesMaterial.getEmissiveFactorOrDefault().r == 0.0);
        CHECK(coordinatesMaterial.getEmissiveFactorOrDefault().g == 0.0);
        CHECK(coordinatesMaterial.getEmissiveFactorOrDefault().b == 0.0);

        const auto& metallicRoughness = coordinatesMaterial.getPbrMetallicRoughness();
        REQUIRE(metallicRoughness.has_value());
        REQUIRE(metallicRoughness->getBaseColorTextureInfo().has_value());
        CHECK(metallicRoughness->getBaseColorTextureInfo()->getTexture() != nullptr);
        CHECK(false == metallicRoughness->getMetallicRoughnessTextureInfo().has_value());
        CHECK(metallicRoughness->getMetallicFactorOrDefault() == 0.0);
        CHECK(metallicRoughness->getRoughnessFactorOrDefault() == 0.735);
    }

    SECTION("M_Useless_Unlit_Material")
    {
        const gltf::Material& unlitMaterial = materials.at(2);
        CHECK(unlitMaterial.getName() == "M_Useless_Unlit_Material");
        CHECK(false == unlitMaterial.getNormalTextureInfo().has_value());
        CHECK(false == unlitMaterial.getEmissiveTextureInfo().has_value());
        CHECK(false == unlitMaterial.getOcclusionTextureInfo().has_value());
        CHECK(false == unlitMaterial.getPbrMetallicRoughness().has_value());
        CHECK(unlitMaterial.getEmissiveFactorOrDefault().r == 0.0);
        CHECK(unlitMaterial.getEmissiveFactorOrDefault().g == 0.0);
        CHECK(unlitMaterial.getEmissiveFactorOrDefault().b == 0.0);
    }

    SECTION("M_Useless_SpecularGlossiness_Material")
    {
        const gltf::Material& specularGlossinessMaterial = materials.at(3);
        CHECK(specularGlossinessMaterial.getName() == "M_Useless_SpecularGlossiness_Material");
        CHECK(false == specularGlossinessMaterial.getNormalTextureInfo().has_value());
        CHECK(false == specularGlossinessMaterial.getEmissiveTextureInfo().has_value());
        CHECK(false == specularGlossinessMaterial.getOcclusionTextureInfo().has_value());
        CHECK(specularGlossinessMaterial.getEmissiveFactorOrDefault().r == 0.0);
        CHECK(specularGlossinessMaterial.getEmissiveFactorOrDefault().g == 0.0);
        CHECK(specularGlossinessMaterial.getEmissiveFactorOrDefault().b == 0.0);

        const auto& specularGlossiness = specularGlossinessMaterial.getPbrSpecularGlossiness();
        REQUIRE(specularGlossiness.has_value());
        REQUIRE(specularGlossiness->getDiffuseTextureInfo().has_value());
        CHECK(specularGlossiness->getDiffuseTextureInfo()->getTexture() != nullptr);
        REQUIRE(specularGlossiness->getSpecularGlossinessTextureInfo().has_value());
        CHECK(specularGlossiness->getSpecularGlossinessTextureInfo()->getTexture() != nullptr);
        CHECK(specularGlossiness->getGlossinessFactorOrDefault() == 0.1);
        CHECK(specularGlossiness->getDiffuseFactorOrDefault().r == 0.1);
        CHECK(specularGlossiness->getDiffuseFactorOrDefault().g == 0.2);
        CHECK(specularGlossiness->getDiffuseFactorOrDefault().b == 0.3);
        CHECK(specularGlossiness->getDiffuseFactorOrDefault().a == 0.4);
        CHECK(specularGlossiness->getSpecularFactorOrDefault().r == 0.1);
        CHECK(specularGlossiness->getSpecularFactorOrDefault().g == 0.2);
        CHECK(specularGlossiness->getSpecularFactorOrDefault().b == 0.3);
    }

    SECTION("varnish")
    {
        const gltf::Material& clearcoatMaterial = materials.at(4);
        const auto& clearcoat = clearcoatMaterial.getClearcoat();
        REQUIRE(clearcoat.has_value());
        CHECK(clearcoat->getClearcoatFactorOrDefault() == 1.0f);
    }

    SECTION("specular")
    {
        const Material& specularMaterial = materials.at(5);
        const auto& specular = specularMaterial.getSpecular();
        REQUIRE(specular.has_value());
        CHECK(specular->getSpecularTextureInfo().has_value());
        CHECK(specular->getSpecularFactorOrDefault() == 0.7);
        CHECK(specular->getSpecularColorFactorOrDefault().r == 0.0);
        CHECK(specular->getSpecularColorFactorOrDefault().g == 0.0);
        CHECK(specular->getSpecularColorFactorOrDefault().b == 0.0);
    }

    SECTION("sheen")
    {
        const Material& sheenMaterial = materials.at(5);
        const auto& sheen = sheenMaterial.getSheen();
        REQUIRE(sheen.has_value());
        CHECK(sheen->getSheenColorTextureInfo().has_value());
        CHECK(sheen->getSheenRoughnessTextureInfo().has_value());
        CHECK(sheen->getSheenRoughnessFactorOrDefault() == 0.7);
        CHECK(sheen->getSheenColorFactorOrDefault().r == 1.0);
        CHECK(sheen->getSheenColorFactorOrDefault().g == 1.0);
        CHECK(sheen->getSheenColorFactorOrDefault().b == 1.0);
    }

    SECTION("ior")
    {
        const Material& iorMaterial = materials.at(5);
        const auto& ior = iorMaterial.getIor();
        REQUIRE(ior.has_value());
        CHECK(ior->getIorOrDefault() == 1.0);
    }
}

TEST_CASE("GltfToJson: textures", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& textures = gltf.getTextures();
    REQUIRE(textures.size() == 6);
    CHECK(textures.at(0).getSource() != nullptr);
    CHECK(textures.at(0).getSampler() != nullptr); // this one is special
    CHECK(textures.at(1).getSource() != nullptr);
    CHECK(textures.at(1).getSampler() == nullptr);
    CHECK(textures.at(2).getSource() != nullptr);
    CHECK(textures.at(2).getSampler() == nullptr);
    CHECK(textures.at(3).getSource() != nullptr);
    CHECK(textures.at(3).getSampler() == nullptr);
    CHECK(textures.at(4).getSource() != nullptr);
    CHECK(textures.at(4).getSampler() == nullptr);
    REQUIRE(textures.at(5).getBasisU().has_value());
    CHECK(textures.at(5).getBasisU()->getSource() != nullptr);
}

TEST_CASE("GltfToJson: images", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& images = gltf.getImages();
    REQUIRE(images.size() == 9);

    SECTION("uris")
    {
        CHECK(images.at(0).getUri() == "BoomBoxWithAxes_baseColor.png");
        CHECK(images.at(1).getUri() == "BoomBoxWithAxes_roughnessMetallic.png");
        CHECK(images.at(2).getUri() == "BoomBoxWithAxes_normal.png");
        CHECK(images.at(3).getUri() == "BoomBoxWithAxes_emissive.png");
        CHECK(images.at(4).getUri() == "BoomBoxWithAxes_baseColor1.png");
        CHECK(images.at(5).getUri() == "image_basisu.ktx2");
        CHECK(images.at(6).getUri() == "image_astc.ktx2");
        CHECK(images.at(7).getUri() == "image_bc6h.ktx2");
        CHECK(images.at(8).getUri() == "texture.ktx2");
    }

    SECTION("get data")
    {
        CHECK_FALSE(images.front().getData().getPixels().empty());
    }
}

TEST_CASE("GltfToJson: mesh weights", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(MorphCube);
    const auto& meshes = gltf.getMeshes();
    REQUIRE_FALSE(meshes.empty());

    const auto& weights = meshes.front().getWeights();
    REQUIRE(weights.size() == 2);
    CHECK(weights.at(0) == 0.0);
    CHECK(weights.at(1) == 0.5);
}

TEST_CASE("GltfToJson: primitive targets", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(MorphCube);
    const auto& meshes = gltf.getMeshes();
    REQUIRE_FALSE(meshes.empty());

    const auto& primitives = meshes.front().getPrimitives();
    REQUIRE_FALSE(primitives.empty());

    const auto& targets = primitives.front().getTargets();
    REQUIRE_FALSE(targets.empty());

    const MorphTarget& target = targets.front();
    CHECK(target.getPositionAccessor() != nullptr);
    CHECK(target.getNormalAccessor() == nullptr);
    CHECK(target.getTangentAccessor() != nullptr);
}

TEST_CASE("GltfToJson: node weights", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(MorphCube);
    const auto& nodes = gltf.getNodes();
    REQUIRE_FALSE(nodes.empty());

    const auto& weights = nodes.front().getWeights();
    REQUIRE(weights.size() == 2);

    CHECK(weights.at(0) == 0.0);
    CHECK(weights.at(1) == 0.5);
}

TEST_CASE("GltfToJson: samplers", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& samplers = gltf.getSamplers();
    REQUIRE_FALSE(samplers.empty());

    const Sampler& sampler = samplers.front();

    SECTION("explicit values")
    {
        CHECK(sampler.getWrapSOrDefault() == TextureWrappingMode::MirroredRepeat);
        REQUIRE(sampler.getMagFilter().has_value());
        CHECK(sampler.getMagFilter().value() == TextureFilterType::Linear);
    }

    SECTION("implicit values")
    {
        CHECK(sampler.getWrapTOrDefault() == TextureWrappingMode::Repeat);
        CHECK_FALSE(sampler.getMinFilter().has_value());
    }
}

TEST_CASE("GltfToJson: animations", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(AnimatedCube);
    const auto& animations = gltf.getAnimations();
    REQUIRE(animations.size() == 1);

    const gltf::Animation& animation = animations.front();
    CHECK(animation.getName() == "animation_AnimatedCube");
    CHECK(animation.getChannels().size() == 1);
    CHECK(animation.getSamplers().size() == 1);

    SECTION("channels")
    {
        REQUIRE_FALSE(animation.getChannels().empty());
        const gltf::AnimationChannel& channel = animation.getChannels().front();
        CHECK(channel.getTarget().getPath() == AnimationPath::Rotation);
        CHECK(channel.getTarget().getNode() != nullptr);
        CHECK(channel.getSampler() != nullptr);
    }

    SECTION("samplers")
    {
        REQUIRE_FALSE(animation.getSamplers().empty());
        const gltf::AnimationSampler& sampler = animation.getSamplers().front();
        CHECK(sampler.getInterpolation() == AnimationInterpolation::CubicSpline);
        CHECK(sampler.getInputAccessor() != nullptr);
        CHECK(sampler.getOutputAccessor() != nullptr);
    }
}

TEST_CASE("GltfToJson: skins", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(Monster);
    const auto& skins = gltf.getSkins();
    REQUIRE(skins.size() == 1);
    const gltf::Skin& skin = skins.front();
    CHECK(skin.getName() == "Armature");
    CHECK(skin.getInverseBindMatricesAccessor() != nullptr);
    CHECK(skin.getSkeletonRoot() != nullptr);
    CHECK(skin.getJoints().size() == 32);
    for (const auto* joint : skin.getJoints()) {
        CHECK(joint != nullptr);
    }
}

TEST_CASE("GltfToJson: node with skin", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(Monster);
    const auto& nodes = gltf.getNodes();
    const auto& skins = gltf.getSkins();
    REQUIRE(nodes.size() >= 3);
    REQUIRE(false == skins.empty());
    CHECK(nodes.at(2).getSkin() == &skins.front());
}

TEST_CASE("GltfToJson: texture transform", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(BoomBoxWithAxes);
    const auto& materials = gltf.getMaterials();
    REQUIRE(materials.size() == 6);

    const gltf::Material& boomBoxMaterial = materials.at(0);
    REQUIRE(boomBoxMaterial.getEmissiveTextureInfo().has_value());
    CHECK(boomBoxMaterial.getEmissiveTextureInfo()->getTexture() != nullptr);
    CHECK(boomBoxMaterial.getEmissiveTextureInfo()->getEffectiveTexCoord() == 0);
    CHECK(boomBoxMaterial.getEmissiveTextureInfo()->getEffectiveUVScale().x == 1);
    CHECK(boomBoxMaterial.getEmissiveTextureInfo()->getEffectiveUVScale().y == -1);
    CHECK(boomBoxMaterial.getEmissiveTextureInfo()->getEffectiveUVOffset().x == 0);
    CHECK(boomBoxMaterial.getEmissiveTextureInfo()->getEffectiveUVOffset().y == 1);

    math::dmat3 transformation = boomBoxMaterial.getEmissiveTextureInfo()->calculateTransformMatrix();
    CHECK(transformation[0][0] == 1.0);
    CHECK(transformation[0][1] == 0.0);
    CHECK(transformation[0][2] == 0.0);
    CHECK(transformation[1][0] == 0.0);
    CHECK(transformation[1][1] == -1.0);
    CHECK(transformation[1][2] == 0.0);
    CHECK(transformation[2][0] == 0.0);
    CHECK(transformation[2][1] == 1.0);
    CHECK(transformation[2][2] == 1.0);
}

TEST_CASE("GltfToJson: material variants", TestNamespace)
{
    const gltf::Gltf& gltf = resolveTestAsset(ExtMaterialVariants);

    SECTION("global values")
    {
        REQUIRE(gltf.hasMaterialVariants());
        REQUIRE(gltf.getMaterialVariants().size() == 4);
    }

    SECTION("primitive values")
    {
        const core::LinkedList<Mesh>& meshes = gltf.getMeshes();
        const core::LinkedList<Material>& materials = gltf.getMaterials();
        const core::LinkedList<Primitive>& primitives = meshes.at(0).getPrimitives();
        const core::LinkedList<MaterialVariant>& materialVariants = gltf.getMaterialVariants();
        const Primitive& prim = primitives.at(0);

        REQUIRE(prim.hasMaterialVariantsMapping());
        const gltf::MaterialVariantsPrimitiveMapping& mapping = prim.getMaterialVariantsMapping().value();
        const core::Vector<MaterialVariantPrimitive>& variants = mapping.getMappings();
        CHECK(variants.size() == 3);
        CHECK(mapping.getMappingsForMaterial(&materials.at(0))->size() == 2);
        CHECK(mapping.getMappingsForMaterial(&materials.at(1))->size() == 1);
        CHECK(mapping.getMappingsForMaterial(&materials.at(2))->size() == 1);
    }
}

}
