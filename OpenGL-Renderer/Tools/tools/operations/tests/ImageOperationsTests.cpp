
#include "Gltfs.h"
#include "Helpers.h"
#include "operations/Library.h"
#include "reactor/GltfEnums.h"
#include "gtest/gtest.h"

#include <gtc/constants.hpp>

using namespace ux3d;
using namespace ux3d::operations;

class ImageOperationsTests : public OperationsTests {
};

TEST_F(ImageOperationsTests, applyTextureTransform)
{
    ScenarioResource resource = Avocado;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();
    uint32_t textureCountBefore = resource.state["gltf"]["textures"].size();

    nlohmann::json::json_pointer texture { "/gltf/materials/0/pbrMetallicRoughness/baseColorTexture" };

    resource.state[texture]["extensions"]["KHR_texture_transform"] = {
        { "offset", std::vector<double> { 0.2, 0.0 } },
        { "scale", std::vector<double> { 2.0, 4.0 } },
        { "rotation", glm::pi<double>() / 4.0 },
    };

    resource.state["gltf"]["samplers"].push_back({
        { "wrapS", reactor::gltf::TextureWrappingMode::Repeat },
        { "wrapT", reactor::gltf::TextureWrappingMode::MirroredRepeat },
        { "filter", reactor::gltf::TextureFilter::Linear },
    });

    size_t index = resource.state[texture]["index"];
    resource.state["gltf"]["textures"][index]["sampler"] = resource.state["gltf"]["samplers"].size() - 1;

    nlohmann::json taskDescription {
        {
            "apply_texture_transform",
            {
                { "path", texture / "extensions" / "KHR_texture_transform" },
            },
        }
    };

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    uint32_t textureCountAfterwards = resource.state["gltf"]["textures"].size();
    EXPECT_EQ(imageCountBefore + 1, imageCountAfterwards);
    EXPECT_EQ(textureCountBefore + 1, textureCountAfterwards);
}

TEST_F(ImageOperationsTests, transform)
{
    ScenarioResource resource = Avocado;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();

    nlohmann::json taskDescription {
        {
            "transform_image",
            {
                { "path", "/gltf/images/0" },
                { "offset", std::vector<double> { 0.2, 0.0 } },
                { "scale", std::vector<double> { 2.0, 4.0 } },
                { "rotation", glm::pi<double>() / 4.0 },
                { "wrapS", reactor::gltf::TextureWrappingMode::Repeat },
                { "wrapT", reactor::gltf::TextureWrappingMode::MirroredRepeat },
                { "filter", reactor::gltf::TextureFilter::Linear },
            },
        }
    };

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore, imageCountAfterwards);
}

TEST_F(ImageOperationsTests, hsv)
{
    ScenarioResource resource = Avocado;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();
    uint32_t textureCountBefore = resource.state["gltf"]["textures"].size();

    nlohmann::json taskDescription {
        {
            "image_hsv",
            {
                { "path", "/gltf/images/0" },
                { "hue", -60.0 },
                { "saturation", 2.0 },
                { "brightness", 0.8 },
            },
        }
    };

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore, imageCountAfterwards);
}

TEST_F(ImageOperationsTests, blur)
{
    ScenarioResource resource = Avocado;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();
    uint32_t textureCountBefore = resource.state["gltf"]["textures"].size();

    nlohmann::json taskDescription {
        {
            "image_blur",
            {
                { "path", "/gltf/images/0" },
                { "radius", 5 },
            },
        }
    };

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore, imageCountAfterwards);
}

TEST_F(ImageOperationsTests, sharpen)
{
    ScenarioResource resource = Avocado;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();
    uint32_t textureCountBefore = resource.state["gltf"]["textures"].size();

    nlohmann::json taskDescription {
        {
            "image_sharpen",
            {
                { "path", "/gltf/images/0" },
                { "intensity", 5 },
            },
        }
    };

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore, imageCountAfterwards);
}

TEST_F(ImageOperationsTests, laplace)
{
    ScenarioResource resource = Avocado;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();
    uint32_t textureCountBefore = resource.state["gltf"]["textures"].size();

    nlohmann::json taskDescription {
        {
            "image_laplace",
            {
                { "path", "/gltf/images/0" },
            },
        }
    };

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore, imageCountAfterwards);
}

TEST_F(ImageOperationsTests, invert)
{
    ScenarioResource resource = Avocado;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();
    uint32_t textureCountBefore = resource.state["gltf"]["textures"].size();

    nlohmann::json taskDescription {
        {
            "image_invert",
            {
                { "path", "/gltf/images/0" },
            },
        }
    };

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore, imageCountAfterwards);
}

TEST_F(ImageOperationsTests, colorSpace)
{
    ScenarioResource resource = Avocado;

    uint32_t imageCountBefore = resource.state["gltf"]["images"].size();
    uint32_t textureCountBefore = resource.state["gltf"]["textures"].size();

    nlohmann::json taskDescription {
        {
            "convert_color_space",
            {
                { "path", "/gltf/images/0" },
                { "source", "srgb" },
                { "target", "rgb" },
            },
        }
    };

    EXPECT_NO_THROW(resource.state = g_library.call(std::move(taskDescription), std::move(resource.state), resource.dataStorage));

    uint32_t imageCountAfterwards = resource.state["gltf"]["images"].size();
    EXPECT_EQ(imageCountBefore, imageCountAfterwards);
}
