// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/ResourceCollector.h"
#include "gltf/json/MeshesToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("MeshesToJson", "[tools.gltf.MeshesToJson]")
{

    GIVEN("no explicit values")
    {
        Gltf gltf;

        Mesh* pMesh = gltf.create<Mesh>();
        REQUIRE(pMesh != nullptr);

        IndexLookup indices(gltf);
        ResourceCollector resourceCollector;
        MeshesToJson converter(indices, resourceCollector, {}, gltf);

        WHEN("converted")
        {
            nlohmann::json jsonFile;
            converter.convert(*pMesh, jsonFile, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonFile.contains("name"));
            }
            THEN("has no primitives")
            {
                CHECK_FALSE(jsonFile.contains("primitives"));
            }
            THEN("has no weights")
            {
                CHECK_FALSE(jsonFile.contains("weights"));
            }
        }
    }

    GIVEN("all explicit values")
    {
        Gltf gltf;

        Mesh* pMesh = gltf.create<Mesh>();
        REQUIRE(pMesh != nullptr);

        Primitive* pPrimitive0 = pMesh->createPrimitive();
        REQUIRE(pPrimitive0 != nullptr);

        Primitive* pPrimitive1 = pMesh->createPrimitive();
        REQUIRE(pPrimitive1 != nullptr);

        Attribute* pAttribute0 = pPrimitive0->addAttribute();
        REQUIRE(pAttribute0 != nullptr);
        Attribute* pAttribute1 = pPrimitive0->addAttribute();
        REQUIRE(pAttribute1 != nullptr);

        Attribute* pAttribute2 = pPrimitive1->addAttribute();
        REQUIRE(pAttribute2 != nullptr);

        Material* pMaterial = gltf.create<Material>();
        REQUIRE(pMaterial != nullptr);

        pAttribute0->setAccessor(gltf.create<Accessor>());
        pAttribute0->setName("att0");

        pAttribute1->setAccessor(gltf.create<Accessor>());
        pAttribute1->setName("att1");

        //Primitive0:
        pPrimitive0->setIndicesAccessor(*gltf.create<Accessor>());
        pPrimitive0->setMaterial(*pMaterial);
        pPrimitive0->setPrimitiveMode(PrimitiveMode::LineStrip);
        MorphTarget* pMorphTarget0 = pPrimitive0->addTarget();
        REQUIRE(pMorphTarget0 != nullptr);
        pMorphTarget0->setNormalAccessor(*gltf.create<Accessor>());
        pMorphTarget0->setPositionAccessor(*gltf.create<Accessor>());
        pMorphTarget0->setTangentAccessor(*gltf.create<Accessor>());
        MorphTarget* pMorphTarget1 = pPrimitive0->addTarget();
        REQUIRE(pMorphTarget1 != nullptr);
        pMorphTarget1->setPositionAccessor(*gltf.create<Accessor>());

        //Primitive1:
        pPrimitive1->setIndicesAccessor(*gltf.create<Accessor>());
        pPrimitive1->setMaterial(*pMaterial);
        pPrimitive1->setPrimitiveMode(PrimitiveMode::Points);
        MorphTarget* pMorphTarget2 = pPrimitive1->addTarget(); //empty morph target
        REQUIRE(pMorphTarget2 != nullptr);

        IndexLookup indices(gltf);
        ResourceCollector resourceCollector;
        ExportOptions options;
        MeshesToJson converter(indices, resourceCollector, options, gltf);

        core::string name = "good name";
        pMesh->setName(name);

        pMesh->getWeights();
        core::Vector<double>& weights = pMesh->getWeights();
        REQUIRE(weights.resize(5));
        for (size_t i = 0; i < weights.size(); i++) {
            weights[i] = double(i);
        }

        WHEN("converted")
        {
            nlohmann::json jsonFile;
            converter.convert(*pMesh, jsonFile, {});

            THEN("has correct name")
            {
                REQUIRE(jsonFile.contains("name"));
                CHECK(jsonFile["name"].get<core::string>() == name);
            }
            THEN("has correct weights")
            {
                REQUIRE(jsonFile.contains("weights"));
                nlohmann::json jsonArray = jsonFile["weights"];

                AND_THEN("weights are correct")
                {
                    REQUIRE(jsonArray.size() == weights.size());

                    for (size_t i = 0; i < jsonArray.size(); i++) {

                        CHECK(jsonArray[i].get<double>() == weights[i]);
                    }
                }
            }
            THEN("has correct primitives")
            {
                REQUIRE(jsonFile.contains("primitives"));
                nlohmann::json jsonPrimitives = jsonFile["primitives"];

                core::LinkedList<Primitive>& primitives = pMesh->getPrimitives();

                AND_THEN("primitives are correct")
                {
                    REQUIRE(jsonPrimitives.size() == primitives.size());

                    for (size_t i = 0; i < primitives.size(); i++) {
                        nlohmann::json jsonPrimitive = jsonPrimitives[i];
                        Primitive& primitive = primitives.at(i);

                        REQUIRE(jsonPrimitive.contains("attributes"));
                        REQUIRE(jsonPrimitive.contains("indices"));
                        REQUIRE(jsonPrimitive.contains("material"));
                        REQUIRE(jsonPrimitive.contains("mode"));
                        REQUIRE(jsonPrimitive.contains("targets"));

                        AND_THEN("attributes are correct")
                        {
                            nlohmann::json jsonAttributes = jsonPrimitive["attributes"];
                            core::LinkedList<Attribute>& attributes = primitive.getAttributes();

                            REQUIRE(attributes.size() == jsonAttributes.size());

                            for (const auto& attribute : attributes) {
                                const core::string key = attribute.getName();
                                size_t textAttributeValue = indices[attribute.getAccessor()];

                                REQUIRE(jsonAttributes.contains(key));
                                CHECK(jsonAttributes[key] == textAttributeValue);
                            }
                        }

                        AND_THEN("indices (accessor index) are correct")
                        {
                            size_t jsonIndexToIndicesAccessor = jsonPrimitive["indices"].get<size_t>();
                            size_t testIndexToIndicesAccessor = indices[primitive.getIndicesAccessor()];

                            CHECK(jsonIndexToIndicesAccessor == testIndexToIndicesAccessor);
                        }

                        AND_THEN("material is correct")
                        {
                            size_t jsonMaterial = jsonPrimitive["material"].get<size_t>();
                            size_t testMaterial = indices[primitive.getMaterial()];

                            CHECK(jsonMaterial == testMaterial);
                        }

                        AND_THEN("mode is correct")
                        {
                            PrimitiveMode jsonMode = PrimitiveMode(jsonPrimitive["mode"]);
                            PrimitiveMode testMode = primitive.getPrimitiveMode();

                            CHECK(jsonMode == testMode);
                        }

                        AND_THEN("targets correct")
                        {
                            nlohmann::json jsonMorphTargets = jsonPrimitive["targets"];
                            core::LinkedList<MorphTarget>& morphTargetList = primitive.getTargets();

                            REQUIRE(jsonMorphTargets.size() == morphTargetList.size());

                            for (size_t j = 0; j < morphTargetList.size(); j++) {
                                nlohmann::json jsonMorphTargetObject = jsonMorphTargets[j];
                                MorphTarget& testMorphTarget = morphTargetList.at(j);

                                if (testMorphTarget.getNormalAccessor() != nullptr) {
                                    size_t jsonMorphTargetIndex = jsonMorphTargetObject["NORMAL"].get<size_t>();
                                    size_t testMorphTargetIndex = indices[testMorphTarget.getNormalAccessor()];
                                    CHECK(jsonMorphTargetIndex == testMorphTargetIndex);
                                }

                                if (testMorphTarget.getTangentAccessor() != nullptr) {
                                    size_t jsonMorphTargetIndex = jsonMorphTargetObject["TANGENT"].get<size_t>();
                                    size_t testMorphTargetIndex = indices[testMorphTarget.getTangentAccessor()];
                                    CHECK(jsonMorphTargetIndex == testMorphTargetIndex);
                                }

                                if (testMorphTarget.getPositionAccessor() != nullptr) {
                                    size_t jsonMorphTargetIndex = jsonMorphTargetObject["POSITION"].get<size_t>();
                                    size_t testMorphTargetIndex = indices[testMorphTarget.getPositionAccessor()];
                                    CHECK(jsonMorphTargetIndex == testMorphTargetIndex);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
}
