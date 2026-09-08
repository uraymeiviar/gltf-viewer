// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/VariantWrapper.h"
#include <catch2/catch.hpp>

namespace ux3d::core::tests {
SCENARIO("VariantWrapper", "[runtime.core.Variant]")
{
    GIVEN("int/float pointer variant")
    {
        VariantWrapper<int*, float*> wrapper;

        AND_GIVEN("int* value")
        {
            int someInt = 5;
            wrapper = &someInt;

            WHEN("get<int>")
            {
                THEN("return correct pointer")
                {
                    int* result = wrapper.get<int*>();
                    CHECK(result == &someInt);
                }
            }
            WHEN("get<float>")
            {
                THEN("return nullptr")
                {
                    float* result = wrapper.get<float*>();
                    CHECK(result == nullptr);
                }
            }
        }
    }
    GIVEN("const int pointer variant")
    {
        VariantWrapper<const int*> wrapper;

        AND_GIVEN("int* value")
        {
            int someInt = 5;
            wrapper = &someInt;

            WHEN("get<int>")
            {
                THEN("return correct pointer")
                {
                    const int* result = wrapper.get<const int*>();
                    CHECK(result == &someInt);
                }
            }
        }
    }
    GIVEN("monostate variant")
    {
        VariantWrapper<std::monostate> wrapper;

        WHEN("hasType<std::monostate>")
        {
            THEN("returns true")
            {
                CHECK(wrapper.hasType<std::monostate>());
            }
        }
        WHEN("compared to another monostate variant")
        {
            VariantWrapper<std::monostate> other;

            THEN("are equal")
            {
                CHECK(wrapper == other);
                CHECK_FALSE(wrapper != other);
            }
        }
    }
    GIVEN("uint32/float variant with uint32 value")
    {
        VariantWrapper<uint32_t, float> wrapper;

        AND_GIVEN("uint32 value")
        {
            wrapper = 5u;

            WHEN("hasType<uint32>")
            {
                THEN("returns true")
                {
                    REQUIRE(wrapper.hasType<uint32_t>());

                    AND_THEN("unsafe get returns correct value")
                    {
                        CHECK(wrapper.unsafeGet<uint32_t>() == 5u);
                    }
                }
            }
            WHEN("hasType<float>")
            {
                THEN("returns false")
                {
                    CHECK_FALSE(wrapper.hasType<float>());
                }
            }
            WHEN("compared to other wrapper with same value")
            {
                VariantWrapper<uint32_t, float> other = 5u;

                THEN("are equal")
                {
                    CHECK(wrapper == other);
                    CHECK_FALSE(wrapper != other);
                }
            }
            WHEN("compared to wrapper with different value of same type")
            {
                VariantWrapper<uint32_t, float> other = 1u;

                THEN("are not equal")
                {
                    CHECK_FALSE(wrapper == other);
                    CHECK(wrapper != other);
                }
            }
            WHEN("compared to wrapper with value different type")
            {
                VariantWrapper<uint32_t, float> other = 5.f;

                THEN("are not equal")
                {
                    CHECK_FALSE(wrapper == other);
                    CHECK(wrapper != other);
                }
            }
        }
        AND_GIVEN("float value")
        {
            wrapper = 5.f;

            WHEN("hasType<uint32>")
            {
                THEN("returns false")
                {
                    CHECK_FALSE(wrapper.hasType<uint32_t>());
                }
            }
            WHEN("hasType<float>")
            {
                THEN("returns true")
                {
                    REQUIRE(wrapper.hasType<float>());

                    AND_THEN("unsafe get returns correct value")
                    {
                        CHECK(wrapper.unsafeGet<float>() == 5.f);
                    }
                }
            }
            WHEN("compared to other wrapper with same value")
            {
                VariantWrapper<uint32_t, float> other = 5.f;

                THEN("are equal")
                {
                    CHECK(wrapper == other);
                    CHECK_FALSE(wrapper != other);
                }
            }
            WHEN("compared to wrapper with different value of same type")
            {
                VariantWrapper<uint32_t, float> other = 1.f;

                THEN("are not equal")
                {
                    CHECK_FALSE(wrapper == other);
                    CHECK(wrapper != other);
                }
            }
            WHEN("compared to wrapper with value different type")
            {
                VariantWrapper<uint32_t, float> other = 5u;

                THEN("are not equal")
                {
                    CHECK_FALSE(wrapper == other);
                    CHECK(wrapper != other);
                }
            }
        }
    }
}
}
