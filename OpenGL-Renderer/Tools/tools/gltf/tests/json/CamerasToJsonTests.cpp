#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/CamerasToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("CamerasToJson: Perspective", "[tools.gltf.CamerasToJson]")
{
    Gltf gltf;

    Camera* pCamera = gltf.create<Camera>(Camera::ProjectionType::Perspective);
    REQUIRE(pCamera != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    CamerasToJson converter(indices, options);

    GIVEN("no values explicitly set")
    {
        WHEN("converted")
        {
            nlohmann::json jsonCamera;
            converter.convert(*pCamera, jsonCamera, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonCamera.contains("name"));
            }

            THEN("type is perspective")
            {
                REQUIRE(jsonCamera.contains("type"));
                CHECK(jsonCamera["type"].get<core::string>() == "perspective");
            }

            THEN("does not have orthographic object")
            {
                CHECK_FALSE(jsonCamera.contains("orthographic"));
            }

            THEN("has perspective object")
            {
                REQUIRE(jsonCamera.contains("perspective"));
                nlohmann::json jsonPerspective = jsonCamera["perspective"];

                AND_THEN("does not have aspectRatio")
                {
                    CHECK_FALSE(jsonPerspective.contains("aspectRatio"));
                }
                AND_THEN("does not have zfar")
                {
                    CHECK_FALSE(jsonPerspective.contains("zfar"));
                }
                AND_THEN("has default yfov")
                {
                    REQUIRE(jsonPerspective.contains("yfov"));
                    CHECK(jsonPerspective["yfov"] == 0.8);
                }
                AND_THEN("has default znear")
                {
                    REQUIRE(jsonPerspective.contains("znear"));
                    CHECK(jsonPerspective["znear"] == 0.001);
                }
            }
        }
    }

    GIVEN("all values explicitly set to non-default")
    {
        auto& perspective = pCamera->getCameraPerspective();
        REQUIRE(perspective.has_value());

        core::string name = "good name for cam";
        double aspectRatio = 123.0;
        double yFov = 753.0;
        double zFar = 159.0;
        double zNear = 0.456;

        pCamera->setName(name);
        perspective->setAspectRatio(aspectRatio);
        perspective->setYFov(yFov);
        perspective->setZFar(zFar);
        perspective->setZNear(zNear);

        WHEN("converted")
        {
            nlohmann::json jsonCamera;
            converter.convert(*pCamera, jsonCamera, {});

            THEN("has correct name")
            {
                REQUIRE(jsonCamera.contains("name"));
                CHECK(jsonCamera["name"].get<core::string>() == name);
            }

            THEN("type is perspective")
            {
                REQUIRE(jsonCamera.contains("type"));
                CHECK(jsonCamera["type"].get<core::string>() == "perspective");
            }

            THEN("does not have orthographic object")
            {
                CHECK_FALSE(jsonCamera.contains("orthographic"));
            }

            THEN("has perspective object")
            {
                REQUIRE(jsonCamera.contains("perspective"));
                nlohmann::json jsonPerspective = jsonCamera["perspective"];

                AND_THEN("has correct aspectRatio")
                {
                    REQUIRE(jsonPerspective.contains("aspectRatio"));
                    CHECK(jsonPerspective["aspectRatio"].get<double>() == aspectRatio);
                }
                AND_THEN("has correct zfar")
                {
                    REQUIRE(jsonPerspective.contains("zfar"));
                    CHECK(jsonPerspective["zfar"].get<double>() == zFar);
                }
                AND_THEN("has correct yfov")
                {
                    REQUIRE(jsonPerspective.contains("yfov"));
                    CHECK(jsonPerspective["yfov"].get<double>() == yFov);
                }
                AND_THEN("has correct znear")
                {
                    REQUIRE(jsonPerspective.contains("znear"));
                    CHECK(jsonPerspective["znear"].get<double>() == zNear);
                }
            }
        }
    }
}

SCENARIO("CamerasToJson: Orthograpic", "[tools.gltf.CamerasToJson]")
{
    Gltf gltf;

    Camera* pCamera = gltf.create<Camera>(Camera::ProjectionType::Orthographic);
    REQUIRE(pCamera != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    CamerasToJson converter(indices, options);

    GIVEN("no values explicitly set")
    {
        WHEN("converted")
        {
            nlohmann::json jsonCamera;
            converter.convert(*pCamera, jsonCamera, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonCamera.contains("name"));
            }

            THEN("type is perspective")
            {
                REQUIRE(jsonCamera.contains("type"));
                CHECK(jsonCamera["type"].get<core::string>() == "orthographic");
            }

            THEN("does not have perspective object")
            {
                CHECK_FALSE(jsonCamera.contains("perspective"));
            }

            THEN("has orthographic object")
            {
                REQUIRE(jsonCamera.contains("orthographic"));
                nlohmann::json jsonOrthographic = jsonCamera["orthographic"];

                AND_THEN("has default xmag")
                {
                    REQUIRE(jsonOrthographic.contains("xmag"));
                    CHECK(jsonOrthographic["xmag"].get<double>() == 1.0);
                }
                AND_THEN("has default ymag")
                {
                    REQUIRE(jsonOrthographic.contains("ymag"));
                    CHECK(jsonOrthographic["ymag"].get<double>() == 1.0);
                }
                AND_THEN("has default zfar")
                {
                    REQUIRE(jsonOrthographic.contains("zfar"));
                    CHECK(jsonOrthographic["zfar"].get<double>() == 1.0);
                }
                AND_THEN("has default znear")
                {
                    REQUIRE(jsonOrthographic.contains("znear"));
                    CHECK(jsonOrthographic["znear"].get<double>() == 0.0);
                }
            }
        }
    }

    GIVEN("all values explicitly set to non-default")
    {
        auto& orthographic = pCamera->getCameraOrthographic();
        REQUIRE(orthographic.has_value());

        core::string name = "good name for cam";
        double xMag = 123.0;
        double yMag = 753.0;
        double zFar = 159.0;
        double zNear = 0.456;

        pCamera->setName(name);
        orthographic->setXMag(xMag);
        orthographic->setYMag(yMag);
        orthographic->setZFar(zFar);
        orthographic->setZNear(zNear);

        WHEN("converted")
        {
            nlohmann::json jsonCamera;
            converter.convert(*pCamera, jsonCamera, {});

            THEN("has correct name")
            {
                REQUIRE(jsonCamera.contains("name"));
                CHECK(jsonCamera["name"].get<core::string>() == name);
            }

            THEN("type is orthographic")
            {
                REQUIRE(jsonCamera.contains("type"));
                CHECK(jsonCamera["type"].get<core::string>() == "orthographic");
            }

            THEN("does not have perspective object")
            {
                CHECK_FALSE(jsonCamera.contains("perspective"));
            }

            THEN("has orthographic object")
            {
                REQUIRE(jsonCamera.contains("orthographic"));
                nlohmann::json jsonOrthographic = jsonCamera["orthographic"];

                AND_THEN("has correct xmag")
                {
                    REQUIRE(jsonOrthographic.contains("xmag"));
                    CHECK(jsonOrthographic["xmag"].get<double>() == xMag);
                }
                AND_THEN("has correct ymag")
                {
                    REQUIRE(jsonOrthographic.contains("ymag"));
                    CHECK(jsonOrthographic["ymag"].get<double>() == yMag);
                }
                AND_THEN("has correct zfar")
                {
                    REQUIRE(jsonOrthographic.contains("zfar"));
                    CHECK(jsonOrthographic["zfar"].get<double>() == zFar);
                }
                AND_THEN("has correct znear")
                {
                    REQUIRE(jsonOrthographic.contains("znear"));
                    CHECK(jsonOrthographic["znear"].get<double>() == zNear);
                }
            }
        }
    }
}
}
