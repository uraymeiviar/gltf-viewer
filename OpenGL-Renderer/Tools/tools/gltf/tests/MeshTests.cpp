// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "gltf/Gltf.h"
#include "gltf/Material.h"
#include "gltf/Mesh.h"

namespace ux3d {
namespace tests {
    constexpr auto TestNamespace = "[tools.gltf.Mesh]";

    TEST_CASE("Mesh name", TestNamespace)
    {
        gltf::Mesh mesh(0);

        SECTION("initially empty")
        {
            CHECK(mesh.hasName() == false);
        }

        SECTION("can be changed")
        {
            REQUIRE(mesh.setName("hello").derived == gltf::GltfResult::Success);
            CHECK(mesh.getName() == "hello");
        }
    }

    TEST_CASE("Add primitive", TestNamespace)
    {
        gltf::Mesh mesh(0);

        SECTION("initially empty")
        {
            CHECK(mesh.getPrimitives().empty());
        }

        SECTION("can be added")
        {
            gltf::Primitive& p1 = *mesh.createPrimitive();
            gltf::Primitive& p2 = *mesh.createPrimitive();

            p2.setPrimitiveMode(gltf::PrimitiveMode(1));

            REQUIRE(mesh.getPrimitives().size() == 2);
            CHECK(&mesh.getPrimitives().front() == &p1);
            CHECK(&mesh.getPrimitives().back() == &p2);
            CHECK(mesh.getPrimitives().front().getPrimitiveMode() == gltf::PrimitiveMode(4));
            CHECK(mesh.getPrimitives().back().getPrimitiveMode() == gltf::PrimitiveMode(1));
            p2.setPrimitiveMode(gltf::PrimitiveMode(20));
            CHECK(p2.getPrimitiveMode() == gltf::PrimitiveMode(20));

            SECTION("can be removed")
            {
                CHECK(mesh.deletePrimitive(p2));
                CHECK(mesh.getPrimitives().size() == 1);
            }

            SECTION("test accessor")
            {
                gltf::Accessor accessor(0);
                SECTION("initially empty")
                {
                    CHECK(p1.getIndicesAccessor() == nullptr);
                }

                SECTION("can be modified")
                {
                    p1.setIndicesAccessor(accessor);
                    CHECK(p1.getIndicesAccessor() == &accessor);
                }

                SECTION("Checking Attributes")
                {
                    SECTION("can be added")
                    {
                        CHECK(p1.getAttributes().size() == 0);
                        gltf::Attribute& a = *p1.addAttribute();
                        REQUIRE(p1.getAttributes().size() == 1);
                        CHECK(&p1.getAttributes().front() == &a);

                        SECTION("Modifying Accessor")
                        {
                            CHECK(a.getAccessor() == nullptr);
                            a.setAccessor(&accessor);
                            CHECK(a.getAccessor() == &accessor);
                        }

                        SECTION("Modifying Name")
                        {
                            CHECK(a.getName() == "");
                            a.setName("New");
                            CHECK(a.getName() == "New");
                        }

                        SECTION("Remove Attribute")
                        {
                            CHECK(p1.removeAttribute(a));
                            CHECK(p1.getAttributes().empty());
                        }
                    }
                }
            }
        }
    }

    TEST_CASE("mesh weights", TestNamespace)
    {
        gltf::Mesh mesh(0);

        SECTION("initially empty")
        {
            CHECK(mesh.getWeights().empty());
        }
    }

    TEST_CASE("Primitive Material Variants", TestNamespace)
    {
        gltf::Gltf gltf;
        gltf.createMaterialVariants();
        gltf::MaterialVariant* pMaterialVariant_1 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_1->setName("first variant");
        gltf::MaterialVariant* pMaterialVariant_2 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_2->setName("second variant");
        gltf::MaterialVariant* pMaterialVariant_3 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_3->setName("third variant");

        gltf::Mesh& mesh = *gltf.create<gltf::Mesh>();
        gltf::Primitive& primitive = *mesh.createPrimitive();
        gltf::Material& material1 = *gltf.create<gltf::Material>();
        gltf::Material& material2 = *gltf.create<gltf::Material>();
        gltf::Material& material3 = *gltf.create<gltf::Material>();

        primitive.createMaterialVariantsMapping();
        CHECK(primitive.hasMaterialVariantsMapping() == true);

        gltf::MaterialVariantsPrimitiveMapping& mapping = primitive.getMaterialVariantsMapping().value();

        SECTION("add material variants to primitive")
        {
            mapping.addVariant(pMaterialVariant_1, &material1);
            mapping.addVariant(pMaterialVariant_2, &material2);
            mapping.addVariant(pMaterialVariant_3, &material3);

            CHECK(mapping.getMappings().size() == 3);
        }

        SECTION("remove material variant from primitive")
        {
            mapping.addVariant(pMaterialVariant_1, &material1);
            mapping.addVariant(pMaterialVariant_2, &material2);
            mapping.addVariant(pMaterialVariant_3, &material3);
            mapping.removeVariant(pMaterialVariant_2);

            CHECK(mapping.getMappings().size() == 2);
            CHECK(mapping.hasMappingForMaterial(&material1));
            CHECK(mapping.hasMappingForMaterial(&material3));
            CHECK(mapping.hasMappingForMaterial(&material2) == false);
            CHECK(mapping.getMappingsForMaterial(&material2).has_value() == false);
        }

        SECTION("add multiple variants to material variant at primitive")
        {
            mapping.addVariant(pMaterialVariant_1, &material1);
            mapping.addVariant(pMaterialVariant_1, &material2);
            mapping.addVariant(pMaterialVariant_2, &material3);

            CHECK(mapping.getMappings().size() == 2);
            CHECK(mapping.hasMappingForMaterial(&material1));
            CHECK(mapping.getMappingsForMaterial(&material1)->size() == 1);
            CHECK(mapping.getMappingsForMaterial(&material1)->at(0) == pMaterialVariant_1);
            CHECK(mapping.hasMappingForMaterial(&material2) == false);
            CHECK(mapping.hasMappingForMaterial(&material3));
            CHECK(mapping.getMappingsForMaterial(&material3)->size() == 1);
        }

        SECTION("add same variant twice")
        {
            mapping.addVariant(pMaterialVariant_1, &material1);
            bool result = mapping.addVariant(pMaterialVariant_1, &material2);

            CHECK(result == false);
            CHECK(mapping.getMappings().size() == 1);
            CHECK(mapping.hasMappingForMaterial(&material1));
            CHECK(mapping.hasMappingForMaterial(&material2) == false);
        }
    }
}
}
