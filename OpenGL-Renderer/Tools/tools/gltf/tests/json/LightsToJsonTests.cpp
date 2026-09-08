// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/LightsToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("LightsToJson: Directional", "[tools.gltf.LightsToJson]")
{
    Gltf gltf;

    Light* pLight = gltf.create<Light>(Light::Type::Directional);
    REQUIRE(pLight != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    LightsToJson converter(indices, options);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonLight;
            converter.convert(*pLight, jsonLight, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonLight.contains("name"));
            }
            THEN("has no color")
            {
                CHECK_FALSE(jsonLight.contains("color"));
            }
            THEN("has no intensity")
            {
                CHECK_FALSE(jsonLight.contains("intensity"));
            }
            THEN("has correct type")
            {
                REQUIRE(jsonLight.contains("type"));
                CHECK(jsonLight["type"].get<core::string>() == "directional");
            }
            THEN("has no range")
            {
                CHECK_FALSE(jsonLight.contains("range"));
            }
            THEN("has no spot")
            {
                CHECK_FALSE(jsonLight.contains("spot"));
            }
        }
    }

    GIVEN("all values explicitly set")
    {
        core::string name = "good name for a light";
        math::dvec3 color(0.2, 0.4, 0.6);
        double intensity = 0.1234;
        double range = 4.321;
        pLight->setName(name);
        pLight->setColor(color);
        pLight->setIntensity(intensity);
        pLight->setRange(range);

        WHEN("converted")
        {
            nlohmann::json jsonLight;
            converter.convert(*pLight, jsonLight, {});

            THEN("has correct name")
            {
                REQUIRE(jsonLight.contains("name"));
                CHECK(jsonLight["name"].get<core::string>() == name);
            }
            THEN("has correct color")
            {
                REQUIRE(jsonLight.contains("color"));
                nlohmann::json jsonColor = jsonLight["color"].get<nlohmann::json::array_t>();
                REQUIRE(jsonColor.size() == 3);
                CHECK(jsonColor[0] == color.r);
                CHECK(jsonColor[1] == color.g);
                CHECK(jsonColor[2] == color.b);
            }
            THEN("has correct intensity")
            {
                REQUIRE(jsonLight.contains("intensity"));
                CHECK(jsonLight["intensity"].get<double>() == intensity);
            }
            THEN("has correct type")
            {
                REQUIRE(jsonLight.contains("type"));
                CHECK(jsonLight["type"].get<core::string>() == "directional");
            }
            THEN("has correct range")
            {
                REQUIRE(jsonLight.contains("range"));
                CHECK(jsonLight["range"].get<double>() == range);
            }
            THEN("has no spot")
            {
                CHECK_FALSE(jsonLight.contains("spot"));
            }
        }
    }
}

SCENARIO("LightsToJson: Point", "[tools.gltf.LightsToJson]")
{
    Gltf gltf;

    Light* pLight = gltf.create<Light>(Light::Type::Point);
    REQUIRE(pLight != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    LightsToJson converter(indices, options);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonLight;
            converter.convert(*pLight, jsonLight, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonLight.contains("name"));
            }
            THEN("has no color")
            {
                CHECK_FALSE(jsonLight.contains("color"));
            }
            THEN("has no intensity")
            {
                CHECK_FALSE(jsonLight.contains("intensity"));
            }
            THEN("has correct type")
            {
                REQUIRE(jsonLight.contains("type"));
                CHECK(jsonLight["type"].get<core::string>() == "point");
            }
            THEN("has no range")
            {
                CHECK_FALSE(jsonLight.contains("range"));
            }
            THEN("has no spot")
            {
                CHECK_FALSE(jsonLight.contains("spot"));
            }
        }
    }

    GIVEN("all values explicitly set")
    {
        core::string name = "good name for a light";
        math::dvec3 color(0.2, 0.4, 0.6);
        double intensity = 0.1234;
        double range = 4.321;
        pLight->setName(name);
        pLight->setColor(color);
        pLight->setIntensity(intensity);
        pLight->setRange(range);

        WHEN("converted")
        {
            nlohmann::json jsonLight;
            converter.convert(*pLight, jsonLight, {});

            THEN("has correct name")
            {
                REQUIRE(jsonLight.contains("name"));
                CHECK(jsonLight["name"].get<core::string>() == name);
            }
            THEN("has correct color")
            {
                REQUIRE(jsonLight.contains("color"));
                nlohmann::json jsonColor = jsonLight["color"].get<nlohmann::json::array_t>();
                REQUIRE(jsonColor.size() == 3);
                CHECK(jsonColor[0] == color.r);
                CHECK(jsonColor[1] == color.g);
                CHECK(jsonColor[2] == color.b);
            }
            THEN("has correct intensity")
            {
                REQUIRE(jsonLight.contains("intensity"));
                CHECK(jsonLight["intensity"].get<double>() == intensity);
            }
            THEN("has correct type")
            {
                REQUIRE(jsonLight.contains("type"));
                CHECK(jsonLight["type"].get<core::string>() == "point");
            }
            THEN("has correct range")
            {
                REQUIRE(jsonLight.contains("range"));
                CHECK(jsonLight["range"].get<double>() == range);
            }
            THEN("has no spot")
            {
                CHECK_FALSE(jsonLight.contains("spot"));
            }
        }
    }
}

SCENARIO("LightsToJson: Spot", "[tools.gltf.LightsToJson]")
{
    Gltf gltf;

    Light* pLight = gltf.create<Light>(Light::Type::Spot);
    REQUIRE(pLight != nullptr);

    IndexLookup indices(gltf);
    ExportOptions options;
    LightsToJson converter(indices, options);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonLight;
            converter.convert(*pLight, jsonLight, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonLight.contains("name"));
            }
            THEN("has no color")
            {
                CHECK_FALSE(jsonLight.contains("color"));
            }
            THEN("has no intensity")
            {
                CHECK_FALSE(jsonLight.contains("intensity"));
            }
            THEN("has correct type")
            {
                REQUIRE(jsonLight.contains("type"));
                CHECK(jsonLight["type"].get<core::string>() == "spot");
            }
            THEN("has no range")
            {
                CHECK_FALSE(jsonLight.contains("range"));
            }
            THEN("has spot")
            {
                REQUIRE(jsonLight.contains("spot"));
                nlohmann::json jsonSpot = jsonLight["spot"];

                AND_THEN("has no innerConeAngle")
                {
                    CHECK_FALSE(jsonSpot.contains("innerConeAngle"));
                }
                AND_THEN("has no outerConeAngle")
                {
                    CHECK_FALSE(jsonSpot.contains("outerConeAngle"));
                }
            }
        }
    }

    GIVEN("all values explicitly set")
    {
        auto& spot = pLight->getSpot();
        REQUIRE(spot.has_value());

        core::string name = "good name for a light";
        math::dvec3 color(0.2, 0.4, 0.6);
        double intensity = 0.1234;
        double range = 4.321;
        double innerConeAngle = 0.98765;
        double outerConeAngle = 1.11111;
        pLight->setName(name);
        pLight->setColor(color);
        pLight->setIntensity(intensity);
        pLight->setRange(range);
        spot->setInnerConeAngle(innerConeAngle);
        spot->setOuterConeAngle(outerConeAngle);

        WHEN("converted")
        {
            nlohmann::json jsonLight;
            converter.convert(*pLight, jsonLight, {});

            THEN("has correct name")
            {
                REQUIRE(jsonLight.contains("name"));
                CHECK(jsonLight["name"].get<core::string>() == name);
            }
            THEN("has correct color")
            {
                REQUIRE(jsonLight.contains("color"));
                nlohmann::json jsonColor = jsonLight["color"].get<nlohmann::json::array_t>();
                REQUIRE(jsonColor.size() == 3);
                CHECK(jsonColor[0] == color.r);
                CHECK(jsonColor[1] == color.g);
                CHECK(jsonColor[2] == color.b);
            }
            THEN("has correct intensity")
            {
                REQUIRE(jsonLight.contains("intensity"));
                CHECK(jsonLight["intensity"].get<double>() == intensity);
            }
            THEN("has correct type")
            {
                REQUIRE(jsonLight.contains("type"));
                CHECK(jsonLight["type"].get<core::string>() == "spot");
            }
            THEN("has correct range")
            {
                REQUIRE(jsonLight.contains("range"));
                CHECK(jsonLight["range"].get<double>() == range);
            }
            THEN("has spot")
            {
                REQUIRE(jsonLight.contains("spot"));
                nlohmann::json jsonSpot = jsonLight["spot"];

                AND_THEN("has correct innerConeAngle")
                {
                    REQUIRE(jsonSpot.contains("innerConeAngle"));
                    CHECK(jsonSpot["innerConeAngle"].get<double>() == innerConeAngle);
                }
                AND_THEN("has correct outerConeAngle")
                {
                    REQUIRE(jsonSpot.contains("outerConeAngle"));
                    CHECK(jsonSpot["outerConeAngle"].get<double>() == outerConeAngle);
                }
            }
        }
    }
}
}
