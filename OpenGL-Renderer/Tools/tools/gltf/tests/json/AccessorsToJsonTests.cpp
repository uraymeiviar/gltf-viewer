// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/ResourceCollector.h"
#include "gltf/json/AccessorsToJson.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
SCENARIO("AccessorsToJson", "[tools.gltf.AccessorsToJson]")
{
    Gltf gltf;

    Accessor* pAccessor = gltf.create<Accessor>();
    REQUIRE(pAccessor != nullptr);

    BufferView* pBufferView = gltf.create<BufferView>();
    REQUIRE(pBufferView != nullptr);
    BufferView* sparseIndicesView = gltf.create<BufferView>();
    REQUIRE(pBufferView != nullptr);
    BufferView* sparseValuesView = gltf.create<BufferView>();
    REQUIRE(pBufferView != nullptr);

    IndexLookup indices(gltf);
    ResourceCollector resourceCollector;
    ExportOptions options;
    AccessorsToJson converter(indices, resourceCollector, options);

    GIVEN("no explicit values")
    {
        WHEN("converted")
        {
            nlohmann::json jsonAccessor;
            converter.convert(*pAccessor, jsonAccessor, {});

            THEN("has no name")
            {
                CHECK_FALSE(jsonAccessor.contains("name"));
            }
            THEN("has no bufferView")
            {
                CHECK_FALSE(jsonAccessor.contains("bufferView"));
            }
            THEN("has no byteOffset")
            {
                CHECK_FALSE(jsonAccessor.contains("byteOffset"));
            }
            THEN("has no componentType")
            {
                CHECK_FALSE(jsonAccessor.contains("componentType"));
            }
            THEN("has no 'normalized'")
            {
                CHECK_FALSE(jsonAccessor.contains("normalized"));
            }
            THEN("has no count")
            {
                CHECK_FALSE(jsonAccessor.contains("count"));
            }
            THEN("has no type")
            {
                CHECK_FALSE(jsonAccessor.contains("type"));
            }
            THEN("has no max")
            {
                CHECK_FALSE(jsonAccessor.contains("max"));
            }
            THEN("has no min")
            {
                CHECK_FALSE(jsonAccessor.contains("min"));
            }
            THEN("has no sparse")
            {
                CHECK_FALSE(jsonAccessor.contains("sparse"));
            }
        }
    }

    GIVEN("all explicit values")
    {
        core::string name = "good name for an accessor";
        uint32_t byteOffset = 22u;
        ComponentType componentType = ComponentType::Short;
        bool normalized = true;
        uint32_t count = 123u;
        AttributeType type = AttributeType::Vec2;
        core::string typeString = "VEC2";
        core::Vector<double> max = { 1.2, 2.3 };
        core::Vector<double> min = { 4.3, 3.2 };
        pAccessor->setName(name);
        pAccessor->setBufferView(*pBufferView);
        pAccessor->setByteOffset(byteOffset);
        pAccessor->setComponentType(componentType);
        pAccessor->setNormalized(normalized);
        pAccessor->setCount(count);
        pAccessor->setType(type);
        pAccessor->createSparse();
        REQUIRE(pAccessor->getMax().assign(max));
        REQUIRE(pAccessor->getMin().assign(min));
        auto& sparse = pAccessor->getSparse();
        REQUIRE(sparse.has_value());
        sparse->setCount(5);
        sparse->getIndices().setByteOffset(2);
        sparse->getIndices().setComponentType(ComponentType::Float);
        sparse->getIndices().setBufferView(*sparseIndicesView);
        sparse->getValues().setByteOffset(5);
        sparse->getValues().setBufferView(*sparseValuesView);

        WHEN("converted")
        {
            nlohmann::json jsonAccessor;
            converter.convert(*pAccessor, jsonAccessor, {});

            THEN("has correct name")
            {
                REQUIRE(jsonAccessor.contains("name"));
                CHECK(jsonAccessor["name"].get<core::string>() == name);
            }
            THEN("has correct bufferView index")
            {
                REQUIRE(jsonAccessor.contains("bufferView"));
                size_t index = indices[pBufferView];
                CHECK(jsonAccessor["bufferView"].get<uint32_t>() == index);
            }
            THEN("has correct byteOffset")
            {
                REQUIRE(jsonAccessor.contains("byteOffset"));
                CHECK(jsonAccessor["byteOffset"].get<uint32_t>() == byteOffset);
            }
            THEN("has correct componentType")
            {
                REQUIRE(jsonAccessor.contains("componentType"));
                CHECK(jsonAccessor["componentType"].get<uint32_t>() == static_cast<uint32_t>(componentType));
            }
            THEN("has correct 'normalized'")
            {
                REQUIRE(jsonAccessor.contains("normalized"));
                CHECK(jsonAccessor["normalized"].get<bool>() == normalized);
            }
            THEN("has correct count")
            {
                REQUIRE(jsonAccessor.contains("count"));
                CHECK(jsonAccessor["count"].get<uint32_t>() == count);
            }
            THEN("has correct type")
            {
                REQUIRE(jsonAccessor.contains("type"));
                CHECK(jsonAccessor["type"].get<core::string>() == typeString);
            }
            THEN("has correct max")
            {
                REQUIRE(jsonAccessor.contains("max"));
                nlohmann::json jsonMax = jsonAccessor["max"];
                REQUIRE(jsonMax.size() == 2);
                REQUIRE(max.size() == 2);
                CHECK(jsonMax[0].get<double>() == max[0]);
                CHECK(jsonMax[1].get<double>() == max[1]);
            }
            THEN("has correct min")
            {
                REQUIRE(jsonAccessor.contains("min"));
                nlohmann::json jsonMin = jsonAccessor["min"];
                REQUIRE(jsonMin.size() == 2);
                REQUIRE(min.size() == 2);
                CHECK(jsonMin[0].get<double>() == min[0]);
                CHECK(jsonMin[1].get<double>() == min[1]);
            }
            THEN("has sparse")
            {
                REQUIRE(jsonAccessor.contains("sparse"));
                nlohmann::json jsonSparse = jsonAccessor["sparse"];

                AND_THEN("sparse has correct count")
                {
                    REQUIRE(jsonSparse.contains("count"));
                    CHECK(jsonSparse["count"].get<uint32_t>() == 5);
                }
                AND_THEN("sparse has indices")
                {
                    REQUIRE(jsonSparse.contains("indices"));
                    nlohmann::json jsonIndices = jsonSparse["indices"];

                    AND_THEN("indices has correct byteOffset")
                    {
                        REQUIRE(jsonIndices.contains("byteOffset"));
                        CHECK(jsonIndices["byteOffset"].get<uint32_t>() == 2);
                    }
                    AND_THEN("indices has correct componentType")
                    {
                        REQUIRE(jsonIndices.contains("componentType"));
                        CHECK(jsonIndices["componentType"].get<uint32_t>() == static_cast<uint32_t>(ComponentType::Float));
                    }
                    AND_THEN("indices has correct bufferView")
                    {
                        REQUIRE(jsonIndices.contains("bufferView"));
                        CHECK(jsonIndices["bufferView"].get<size_t>() == indices[sparseIndicesView]);
                    }
                }
                AND_THEN("sparse has values")
                {
                    REQUIRE(jsonSparse.contains("values"));
                    nlohmann::json jsonValues = jsonSparse["values"];

                    AND_THEN("values has correct byteOffset")
                    {
                        REQUIRE(jsonValues.contains("byteOffset"));
                        CHECK(jsonValues["byteOffset"].get<uint32_t>() == 5);
                    }
                    AND_THEN("values has correct bufferView")
                    {
                        REQUIRE(jsonValues.contains("bufferView"));
                        CHECK(jsonValues["bufferView"].get<size_t>() == indices[sparseValuesView]);
                    }
                }
            }
        }
    }
}
}
