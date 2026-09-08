// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/json/MaterialsToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("MaterialsToJson", "[tools.gltf.MaterialsToJson]")
{
    GIVEN("Specular material and no values set")
    {
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonMaterial.contains("name"));
            }
            THEN("has empty pbrMetallicRoughness properties")
            {
                CHECK_FALSE(jsonMaterial.contains("pbrMetallicRoughness"));
            }
            THEN("has no normalTexture")
            {
                CHECK_FALSE(jsonMaterial.contains("normalTexture"));
            }
            THEN("has no occlusionTexture")
            {
                CHECK_FALSE(jsonMaterial.contains("occlusionTexture"));
            }
            THEN("has no emissiveTexture")
            {
                CHECK_FALSE(jsonMaterial.contains("emissiveTexture"));
            }
            THEN("has no emissiveFactor")
            {
                CHECK_FALSE(jsonMaterial.contains("emissiveFactor"));
            }
            THEN("has no alphaMode")
            {
                CHECK_FALSE(jsonMaterial.contains("alphaMode"));
            }
            THEN("has no alphaCutoff")
            {
                CHECK_FALSE(jsonMaterial.contains("alphaCutoff"));
            }
            THEN("has no doubleSided")
            {
                CHECK_FALSE(jsonMaterial.contains("doubleSided"));
            }
            THEN("has no specular glossiness material extension")
            {
                CHECK_FALSE(jsonMaterial["extensions"].contains("KHR_materials_pbrSpecularGlossiness"));
            }
            THEN("has no unlit material extension")
            {
                CHECK_FALSE(jsonMaterial["extensions"].contains("KHR_materials_unlit"));
            }
            THEN("has no specular material extension")
            {
                CHECK_FALSE(jsonMaterial["extensions"].contains("KHR_materials_specular"));
            }
            THEN("has no sheen material extension")
            {
                CHECK_FALSE(jsonMaterial["extensions"].contains("KHR_materials_sheen"));
            }
            THEN("has no ior material extension")
            {
                CHECK_FALSE(jsonMaterial["extensions"].contains("KHR_materials_ior"));
            }
            THEN("has no clearcoat material extension")
            {
                CHECK_FALSE(jsonMaterial["extensions"].contains("KHR_materials_clearcoat"));
            }
            THEN("has no transmission material extension")
            {
                CHECK_FALSE(jsonMaterial["extensions"].contains("KHR_materials_transmission"));
            }
        }
    }

    GIVEN("PbrMetallicRoughness material and all values explicitly set")
    {
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createPbrMetallicRoughness();

        Texture* pTexture = gltf.create<Texture>();

        core::string name = "good name for a material";
        pMaterial->setName(name);

        nlohmann::json extrasJson = nlohmann::json::object();
        extrasJson["extraEntry0"] = 5.0;
        extrasJson["extraEntry1"] = "some text";
        core::string extrasString;
        extrasString = extrasJson.dump(1, '\t', false, nlohmann::json::error_handler_t::replace);
        pMaterial->setExtras(extrasString);

        const uint32_t texCoordTest = 3;
        const double scaleTest = 0.1;
        const double strengthTest = 0.4;

        //// Begin PbrMetallicRoughness ////
        std::optional<PbrMetallicRoughness>& pbrMetallicRoughnessOptional = pMaterial->getPbrMetallicRoughness();
        REQUIRE(pbrMetallicRoughnessOptional.has_value());
        PbrMetallicRoughness& pbrMetallicRoughness = pbrMetallicRoughnessOptional.value();

        math::dvec4 testBaseColorFactor(0.1, 0.2, 0.3, 0.4);
        pbrMetallicRoughness.setBaseColorFactor(testBaseColorFactor);

        REQUIRE(pbrMetallicRoughness.createBaseColorTextureInfo() == GltfResult::Success);
        std::optional<TextureInfo>& baseColorTextureInfoOptional = pbrMetallicRoughness.getBaseColorTextureInfo();
        REQUIRE(baseColorTextureInfoOptional.has_value());
        baseColorTextureInfoOptional.value().setTexCoord(texCoordTest);
        baseColorTextureInfoOptional.value().setTexture(*pTexture);

        REQUIRE(pbrMetallicRoughness.createMetallicRoughnessTextureInfo() == GltfResult::Success);
        //Test if TextureInfo without Texture is handled correctly

        const double testMetallicFactor = 0.4;
        pbrMetallicRoughness.setMetallicFactor(testMetallicFactor);

        const double testRoughnessFactor = 0.2;
        pbrMetallicRoughness.setRoughnessFactor(testRoughnessFactor);
        //// End PbrMetallicRoughness ////

        REQUIRE(pMaterial->createNormalTextureInfo() == GltfResult::Success);
        std::optional<TextureInfo>& oNnormalTexture = pMaterial->getNormalTextureInfo();
        REQUIRE(oNnormalTexture.has_value());
        oNnormalTexture->setTexCoord(texCoordTest);
        oNnormalTexture->setScale(scaleTest);

        REQUIRE(pMaterial->createOcclusionTextureInfo() == GltfResult::Success);
        std::optional<TextureInfo>& oOcclusionTexture = pMaterial->getOcclusionTextureInfo();
        REQUIRE(oOcclusionTexture.has_value());
        oOcclusionTexture->setTexCoord(texCoordTest);
        oOcclusionTexture->setStrength(strengthTest);

        REQUIRE(pMaterial->createEmissiveTextureInfo() == GltfResult::Success);
        std::optional<TextureInfo>& oEmissiveTexture = pMaterial->getEmissiveTextureInfo();
        REQUIRE(oEmissiveTexture.has_value());
        oEmissiveTexture->setTexCoord(texCoordTest);

        const math::dvec3 testEmissiveFactor(0.1, 0.2, 0.3);
        pMaterial->setEmissiveFactor(testEmissiveFactor);

        const std::string testAlphaMode = "BLEND";
        AlphaMode alphaMode = alphaModeFromString(testAlphaMode);
        pMaterial->setAlphaMode(alphaMode);

        const double testAlphaCutoff = 0.6;
        pMaterial->setAlphaCutoff(testAlphaCutoff);

        const bool testDoubleSided = true;
        pMaterial->setDoubleSided(testDoubleSided);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("has correct name")
            {
                REQUIRE(jsonMaterial.contains("name"));
                CHECK(jsonMaterial["name"].get<core::string>() == name);
            }
            THEN("has pbrMetallicRoughness properties")
            {
                REQUIRE(jsonMaterial.contains("pbrMetallicRoughness"));

                REQUIRE(jsonMaterial["pbrMetallicRoughness"].contains("baseColorFactor"));
                REQUIRE(jsonMaterial["pbrMetallicRoughness"].contains("baseColorTexture"));
                REQUIRE(jsonMaterial["pbrMetallicRoughness"].contains("metallicRoughnessTexture"));
                REQUIRE(jsonMaterial["pbrMetallicRoughness"].contains("metallicFactor"));
                REQUIRE(jsonMaterial["pbrMetallicRoughness"].contains("roughnessFactor"));

                for (size_t i = 0; i < 4; i++) {
                    CHECK(jsonMaterial["pbrMetallicRoughness"]["baseColorFactor"][i] == testBaseColorFactor[math::dvec4::length_type(i)]);
                }

                size_t testTextureIndex = indices[baseColorTextureInfoOptional.value().getTexture()];
                CHECK(jsonMaterial["pbrMetallicRoughness"]["baseColorTexture"]["index"] == testTextureIndex);
                CHECK(jsonMaterial["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"] == texCoordTest);

                CHECK(jsonMaterial["pbrMetallicRoughness"]["metallicFactor"] == testMetallicFactor);

                CHECK(jsonMaterial["pbrMetallicRoughness"]["roughnessFactor"] == testRoughnessFactor);
                CHECK(jsonMaterial["pbrMetallicRoughness"]["metallicFactor"] == testMetallicFactor);
            }
            THEN("has normalTexture properties")
            {
                REQUIRE(jsonMaterial.contains("normalTexture"));
                CHECK(jsonMaterial["normalTexture"]["texCoord"] == texCoordTest);
                CHECK(jsonMaterial["normalTexture"]["scale"] == scaleTest);
            }
            THEN("has occlusionTexture properties")
            {
                REQUIRE(jsonMaterial.contains("occlusionTexture"));
                CHECK(jsonMaterial["occlusionTexture"]["texCoord"] == texCoordTest);
                CHECK(jsonMaterial["occlusionTexture"]["strength"] == strengthTest);
            }
            THEN("has emissiveTexture properties")
            {
                REQUIRE(jsonMaterial.contains("emissiveTexture"));
                CHECK(jsonMaterial["emissiveTexture"]["texCoord"] == texCoordTest);
                CHECK_FALSE(jsonMaterial["emissiveTexture"].contains("scale"));
            }
            THEN("has emissiveFactor properties")
            {
                REQUIRE(jsonMaterial.contains("emissiveFactor"));
                for (size_t i = 0; i < 3; i++) {
                    CHECK(jsonMaterial["emissiveFactor"][i] == testEmissiveFactor[math::dvec3::length_type(i)]);
                }
            }
            THEN("has alphaMode properties")
            {
                REQUIRE(jsonMaterial.contains("alphaMode"));
                CHECK(testAlphaMode == jsonMaterial["alphaMode"]);
            }
            THEN("has alphaCutoff properties")
            {
                REQUIRE(jsonMaterial.contains("alphaCutoff"));
                CHECK(jsonMaterial["alphaCutoff"] == testAlphaCutoff);
            }
            THEN("has doubleSided properties")
            {
                REQUIRE(jsonMaterial.contains("doubleSided"));
                CHECK(jsonMaterial["doubleSided"] == testDoubleSided);
            }
            THEN("has extras")
            {
                REQUIRE(jsonMaterial.contains("extras"));
                REQUIRE(jsonMaterial["extras"].contains("extraEntry0"));
                REQUIRE(jsonMaterial["extras"].contains("extraEntry1"));

                CHECK(jsonMaterial["extras"]["extraEntry0"] == 5.0);
                CHECK(jsonMaterial["extras"]["extraEntry1"] == "some text");
            }
        }
    }

    GIVEN("Unlit material and relevant values explicitly set")
    {
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createUnlit();

        core::string name = "good name for a material";
        pMaterial->setName(name);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        //Checks:
        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("unlit material has KHR_materials_unlit property")
            {
                REQUIRE(jsonMaterial.contains("extensions"));
                REQUIRE(jsonMaterial["extensions"].contains("KHR_materials_unlit"));
            }
        }
    }

    GIVEN("PbrSpecularGlossiness material and relevant values explicitly set")
    {
        //Setup:
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createPbrSpecularGlossiness();

        core::string name = "good name for a material";
        pMaterial->setName(name);

        const uint32_t texCoordTest = 3;

        //// Begin PbrSpecularGlossiness ////
        std::optional<PbrSpecularGlossiness>& pbrSpecularGlossinessOptional = pMaterial->getPbrSpecularGlossiness();
        REQUIRE(pbrSpecularGlossinessOptional.has_value());
        PbrSpecularGlossiness& pbrSpecularGlossiness = pbrSpecularGlossinessOptional.value();

        REQUIRE(pbrSpecularGlossiness.createDiffuseTextureInfo() == GltfResult::Success);
        std::optional<TextureInfo>& diffuseTextureOptional = pbrSpecularGlossiness.getDiffuseTextureInfo();
        REQUIRE(diffuseTextureOptional.has_value());
        diffuseTextureOptional.value().setTexCoord(texCoordTest);

        REQUIRE(pbrSpecularGlossiness.createSpecularGlossinessTextureInfo() == GltfResult::Success);
        std::optional<TextureInfo>& specularGlossinessTextureOptional = pbrSpecularGlossiness.getSpecularGlossinessTextureInfo();
        REQUIRE(specularGlossinessTextureOptional.has_value());
        specularGlossinessTextureOptional.value().setTexCoord(texCoordTest);

        math::dvec4 testDiffuseFactor(0.1, 0.2, 0.3, 0.5);
        pbrSpecularGlossiness.setDiffuseFactor(testDiffuseFactor);

        double testGlossinessFactor = 0.8;
        pbrSpecularGlossiness.setGlossinessFactor(testGlossinessFactor);

        math::dvec3 testSpecularFactor(0.3, 0.2, 0.3);
        pbrSpecularGlossiness.setSpecularFactor(testSpecularFactor);
        //// End PbrSpecularGlossiness ////

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        //Checks:
        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("has correct KHR_materials_pbrSpecularGlossiness properties ")
            {
                REQUIRE(jsonMaterial.contains("extensions"));
                REQUIRE(jsonMaterial["extensions"].contains("KHR_materials_pbrSpecularGlossiness"));
                REQUIRE(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"].contains("diffuseFactor"));
                REQUIRE(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"].contains("diffuseTexture"));
                REQUIRE(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"]["diffuseTexture"].contains("texCoord"));
                REQUIRE(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"].contains("specularFactor"));
                REQUIRE(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"].contains("glossinessFactor"));
                REQUIRE(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"].contains("specularGlossinessTexture"));
                REQUIRE(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"]["specularGlossinessTexture"].contains("texCoord"));

                for (size_t i = 0; i < 4; i++) {
                    CHECK(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"]["diffuseFactor"][i] == testDiffuseFactor[math::dvec4::length_type(i)]);
                }

                CHECK(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"]["diffuseTexture"]["texCoord"] == texCoordTest);

                for (size_t i = 0; i < 3; i++) {
                    CHECK(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"]["specularFactor"][i] == testSpecularFactor[math::dvec3::length_type(i)]);
                }

                CHECK(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"]["glossinessFactor"] == testGlossinessFactor);

                CHECK(jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"]["specularGlossinessTexture"]["texCoord"] == texCoordTest);
            }
        }
    }

    GIVEN("Clearcoat material and relevant values explicitly set")
    {
        // Test Values:
        const uint32_t texCoordTest = 3;
        const double scaleTest = 0.11;
        const double clearcoatFactorTest = 0.31;
        const double clearcoatRoughnessFactorTest = 0.34;

        // Setup:
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createClearcoat();

        std::optional<Clearcoat>& oClearcoat = pMaterial->getClearcoat();
        REQUIRE(oClearcoat.has_value());

        oClearcoat->setClearcoatFactor(clearcoatFactorTest);

        oClearcoat->createClearcoatTexture();
        std::optional<TextureInfo>& oClearcoatTexture = oClearcoat->getClearcoatTexture();
        REQUIRE(oClearcoatTexture.has_value());
        oClearcoatTexture->setTexCoord(texCoordTest);

        oClearcoat->setClearcoatRoughnessFactor(clearcoatRoughnessFactorTest);

        oClearcoat->createClearcoatRoughnessTexture();
        std::optional<TextureInfo>& oRoughnessTexture = oClearcoat->getClearcoatRoughnessTexture();
        REQUIRE(oRoughnessTexture.has_value());
        oRoughnessTexture->setTexCoord(texCoordTest);

        oClearcoat->createClearcoatNormalTexture();
        std::optional<TextureInfo>& oNormalTexture = oClearcoat->getClearcoatNormalTexture();
        REQUIRE(oNormalTexture.has_value());
        oNormalTexture->setTexCoord(texCoordTest);
        oNormalTexture->setScale(scaleTest);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        // Checks:
        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("has correct KHR_materials_pbrSpecularGlossiness properties ")
            {
                REQUIRE(jsonMaterial.contains("extensions"));
                REQUIRE(jsonMaterial["extensions"].contains("KHR_materials_clearcoat"));

                nlohmann::json jsonClearcoat = jsonMaterial["extensions"]["KHR_materials_clearcoat"];

                REQUIRE(jsonClearcoat.contains("clearcoatFactor"));
                REQUIRE(jsonClearcoat.contains("clearcoatTexture"));
                REQUIRE(jsonClearcoat.contains("clearcoatRoughnessFactor"));
                REQUIRE(jsonClearcoat.contains("clearcoatRoughnessTexture"));
                REQUIRE(jsonClearcoat.contains("clearcoatNormalTexture"));

                CHECK(jsonClearcoat["clearcoatFactor"] == clearcoatFactorTest);
                CHECK(jsonClearcoat["clearcoatTexture"]["texCoord"] == texCoordTest);
                CHECK(jsonClearcoat["clearcoatRoughnessFactor"] == clearcoatRoughnessFactorTest);
                CHECK(jsonClearcoat["clearcoatRoughnessTexture"]["texCoord"] == texCoordTest);
                CHECK(jsonClearcoat["clearcoatNormalTexture"]["texCoord"] == texCoordTest);
                CHECK(jsonClearcoat["clearcoatNormalTexture"]["scale"] == scaleTest);
            }
        }
    }

    GIVEN("TextureInfo and KHR_texture_transform extension with relevant values explicitly set")
    {
        // Test Values:
        const uint32_t texCoordTest = 3;
        const math::dvec2 uvOffsetTest(0.3, 0.5);
        const double rotationTest = 0.8;
        const math::dvec2 uvScaleTest(0.34, 0.54);

        // Setup:
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createPbrMetallicRoughness();

        Texture* pTexture = gltf.create<Texture>();

        std::optional<PbrMetallicRoughness>& pbrMetallicRoughnessOptional = pMaterial->getPbrMetallicRoughness();
        REQUIRE(pbrMetallicRoughnessOptional.has_value());
        PbrMetallicRoughness& pbrMetallicRoughness = pbrMetallicRoughnessOptional.value();

        REQUIRE(pbrMetallicRoughness.createBaseColorTextureInfo() == GltfResult::Success);
        std::optional<TextureInfo>& oBaseColorTextureInfo = pbrMetallicRoughness.getBaseColorTextureInfo();
        REQUIRE(oBaseColorTextureInfo.has_value());

        oBaseColorTextureInfo->setTexCoord(texCoordTest);
        oBaseColorTextureInfo->setTexture(*pTexture);

        oBaseColorTextureInfo->createTextureTransform();
        auto& textureTransform = oBaseColorTextureInfo->getTextureTransform();
        REQUIRE(textureTransform.has_value());

        textureTransform->setOffset(uvOffsetTest);
        textureTransform->setRotation(rotationTest);
        textureTransform->setScale(uvScaleTest);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        //Checks:
        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("texture has correct properties")
            {
                REQUIRE(jsonMaterial.contains("pbrMetallicRoughness"));
                REQUIRE(jsonMaterial["pbrMetallicRoughness"].contains("baseColorTexture"));

                size_t testTextureIndex = indices[oBaseColorTextureInfo->getTexture()];
                CHECK(jsonMaterial["pbrMetallicRoughness"]["baseColorTexture"]["index"] == testTextureIndex);
                CHECK(jsonMaterial["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"] == texCoordTest);
            }

            THEN("extension KHR_texture_transform has correct properties")
            {
                REQUIRE(jsonMaterial.contains("pbrMetallicRoughness"));
                REQUIRE(jsonMaterial["pbrMetallicRoughness"].contains("baseColorTexture"));
                REQUIRE(jsonMaterial["pbrMetallicRoughness"]["baseColorTexture"].contains("extensions"));
                REQUIRE(jsonMaterial["pbrMetallicRoughness"]["baseColorTexture"]["extensions"].contains("KHR_texture_transform"));

                nlohmann::json jsonTextureTransform = jsonMaterial["pbrMetallicRoughness"]["baseColorTexture"]["extensions"]["KHR_texture_transform"];
                CHECK(jsonTextureTransform.contains("offset"));
                CHECK(jsonTextureTransform.contains("rotation"));
                CHECK(jsonTextureTransform.contains("scale"));
                //[TODO] texCoord is currently exclusively stored in jsonTextureInfo
                //CHECK(jsonTextureTransform.contains("texCoord"));

                CHECK(jsonTextureTransform["offset"][0] == uvOffsetTest[0]);
                CHECK(jsonTextureTransform["offset"][1] == uvOffsetTest[1]);

                CHECK(jsonTextureTransform["rotation"] == rotationTest);

                CHECK(jsonTextureTransform["scale"][0] == uvScaleTest[0]);
                CHECK(jsonTextureTransform["scale"][1] == uvScaleTest[1]);
            }
        }
    }

    GIVEN("Specular material and relevant values explicitly set")
    {
        // Test Values:
        const math::vec3 specularColorFactorTest = math::vec3({ 0.0, 0.0, 0.0 });
        const double specularFactorTest = 0.7;
        const uint32_t texCoordTest = 0;

        // Setup:
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createSpecular();

        Texture* pTexture = gltf.create<Texture>();
        REQUIRE(pTexture != nullptr);

        std::optional<gltf::Specular>& oSpecular = pMaterial->getSpecular();
        REQUIRE(oSpecular.has_value());

        oSpecular->setSpecularColorFactor(specularColorFactorTest);
        oSpecular->setSpecularFactor(specularFactorTest);
        oSpecular->createSpecularTextureInfo();
        std::optional<TextureInfo>& oSpecularTextureInfo = oSpecular->getSpecularTextureInfo();

        oSpecularTextureInfo->setTexture(*pTexture);
        oSpecularTextureInfo->setTexCoord(texCoordTest);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        //Checks:
        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("Specular material extension has correct properties")
            {
                REQUIRE(jsonMaterial.contains("extensions"));
                REQUIRE(jsonMaterial["extensions"].contains("KHR_materials_specular"));
                nlohmann::json jsonMSpecular = jsonMaterial["extensions"]["KHR_materials_specular"];

                CHECK(jsonMSpecular["specularFactor"] == specularFactorTest);
                CHECK(jsonMSpecular["specularColorFactor"][0] == 0.0);
                CHECK(jsonMSpecular["specularColorFactor"][1] == 0.0);
                CHECK(jsonMSpecular["specularColorFactor"][2] == 0.0);

                size_t testTextureIndex = indices[oSpecularTextureInfo->getTexture()];
                CHECK(jsonMSpecular["specularTexture"]["index"] == testTextureIndex);
                CHECK(jsonMSpecular["specularTexture"]["texCoord"] == texCoordTest);
            }
        }
    }

    GIVEN("Sheen material and relevant values explicitly set")
    {
        // Test Values:
        const math::vec3 sheenColorFactorTest = math::vec3({ 1.0, 1.0, 1.0 });
        const double sheenRoughnessFactor = 0.7;
        const uint32_t texCoordTest = 0;

        // Setup:
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createSheen();

        Texture* pColorTexture = gltf.create<Texture>();
        REQUIRE(pColorTexture != nullptr);

        Texture* pRoughnessTexture = gltf.create<Texture>();
        REQUIRE(pRoughnessTexture != nullptr);

        std::optional<gltf::Sheen>& oSheen = pMaterial->getSheen();
        REQUIRE(oSheen.has_value());

        oSheen->setSheenColorFactor(sheenColorFactorTest);
        oSheen->setSheenRoughnessFactor(sheenRoughnessFactor);
        oSheen->createSheenColorTextureInfo();
        oSheen->createSheenRoughnessTextureInfo();
        std::optional<TextureInfo>& oSheenColorTextureInfo = oSheen->getSheenColorTextureInfo();
        oSheen->createSheenRoughnessTextureInfo();
        std::optional<TextureInfo>& oSheenRoughnessTextureInfo = oSheen->getSheenRoughnessTextureInfo();

        oSheenColorTextureInfo->setTexture(*pColorTexture);
        oSheenColorTextureInfo->setTexCoord(texCoordTest);

        oSheenRoughnessTextureInfo->setTexture(*pRoughnessTexture);
        oSheenRoughnessTextureInfo->setTexCoord(texCoordTest);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        //Checks:
        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("Sheen material extension has correct properties")
            {
                REQUIRE(jsonMaterial.contains("extensions"));
                REQUIRE(jsonMaterial["extensions"].contains("KHR_materials_sheen"));
                nlohmann::json jsonMSheen = jsonMaterial["extensions"]["KHR_materials_sheen"];

                CHECK(jsonMSheen["sheenRoughnessFactor"] == sheenRoughnessFactor);
                CHECK(jsonMSheen["sheenColorFactor"][0] == sheenColorFactorTest.r);
                CHECK(jsonMSheen["sheenColorFactor"][1] == sheenColorFactorTest.g);
                CHECK(jsonMSheen["sheenColorFactor"][2] == sheenColorFactorTest.b);

                size_t testColorTextureIndex = indices[oSheenColorTextureInfo->getTexture()];
                CHECK(jsonMSheen["sheenColorTexture"]["index"] == testColorTextureIndex);
                CHECK(jsonMSheen["sheenColorTexture"]["texCoord"] == texCoordTest);

                size_t testRoughnessTextureIndex = indices[oSheenRoughnessTextureInfo->getTexture()];
                CHECK(jsonMSheen["sheenRoughnessTexture"]["index"] == testRoughnessTextureIndex);
                CHECK(jsonMSheen["sheenRoughnessTexture"]["texCoord"] == texCoordTest);
            }
        }
    }

    GIVEN("Ior material and relevant values explicitly set")
    {
        // Test Values:
        const double ior = 1.0;

        // Setup:
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createIor();

        std::optional<gltf::Ior>& oIor = pMaterial->getIor();
        REQUIRE(oIor.has_value());

        oIor->setIor(ior);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        //Checks:
        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("Ior material extension has correct properties")
            {
                REQUIRE(jsonMaterial.contains("extensions"));
                REQUIRE(jsonMaterial["extensions"].contains("KHR_materials_ior"));
                nlohmann::json jsonMIor = jsonMaterial["extensions"]["KHR_materials_ior"];

                CHECK(jsonMIor["ior"] == ior);
            }
        }
    }

    GIVEN("Transmission material and relevant values explicitly set")
    {
        // Test Values:
        const double transmissionFactor = 0.7;
        const uint32_t texCoordTest = 0;

        // Setup:
        Gltf gltf;

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pMaterial->createTransmission();

        Texture* pTexture = gltf.create<Texture>();
        REQUIRE(pTexture != nullptr);

        std::optional<gltf::Transmission>& oTransmission = pMaterial->getTransmission();
        REQUIRE(oTransmission.has_value());

        oTransmission->setTransmissionFactor(transmissionFactor);
        oTransmission->createTransmissionTextureInfo();
        std::optional<TextureInfo>& oTransmissionTextureInfo = oTransmission->getTransmissionTextureInfo();

        oTransmissionTextureInfo->setTexture(*pTexture);
        oTransmissionTextureInfo->setTexCoord(texCoordTest);

        IndexLookup indices(gltf);
        ExportOptions options;
        MaterialsToJson converter(indices, options, gltf);

        //Checks:
        WHEN("converted")
        {
            nlohmann::json jsonMaterial;
            converter.convert(*pMaterial, jsonMaterial, {});

            THEN("Transmission material extension has correct properties")
            {
                REQUIRE(jsonMaterial.contains("extensions"));
                REQUIRE(jsonMaterial["extensions"].contains("KHR_materials_transmission"));
                nlohmann::json jsonMTransmission = jsonMaterial["extensions"]["KHR_materials_transmission"];

                CHECK(jsonMTransmission["transmissionFactor"] == transmissionFactor);

                size_t testTextureIndex = indices[oTransmissionTextureInfo->getTexture()];
                CHECK(jsonMTransmission["transmissionTexture"]["index"] == testTextureIndex);
                CHECK(jsonMTransmission["transmissionTexture"]["texCoord"] == texCoordTest);
            }
        }
    }
}
}
