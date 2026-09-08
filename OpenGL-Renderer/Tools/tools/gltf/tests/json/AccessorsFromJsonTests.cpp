#include "../TestAssets.h"
#include "core/Algorithms.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
TEST_CASE("Asset with sparse accessor", "[tools.gltf.Accessor]")
{
    const Gltf& gltf = SparseAccessor.get();
    const uint32_t accessorIndex = 1;
    REQUIRE(gltf.getAccessors().size() > accessorIndex);
    const Accessor& accessor = gltf.getAccessors().at(accessorIndex);

    SECTION("accessor base")
    {
        SECTION("bufferView = 1")
        {
            const uint32_t bufferViewIndex = 1;
            REQUIRE(gltf.getBufferViews().size() > bufferViewIndex);
            CHECK(accessor.getBufferView() == &gltf.getBufferViews().at(bufferViewIndex));
        }
        SECTION("byteOffset = 0")
        {
            CHECK(accessor.getByteOffset() == 0);
        }
        SECTION("componentType = 5126")
        {
            CHECK(accessor.getComponentType() == static_cast<ComponentType>(5126));
        }
        SECTION("count = 14")
        {
            CHECK(accessor.getCount() == 14);
        }
        SECTION("type = 'VEC3'")
        {
            CHECK(accessor.getType() == AttributeType::Vec3);
        }
        SECTION("max = [ 6.0, 4.0, 0.0 ]")
        {
            CHECK(core::equal(accessor.getMax(), { 6.0, 4.0, 0.0 }));
        }
        SECTION("min = [ 0.0, 0.0, 0.0 ]")
        {
            CHECK(core::equal(accessor.getMin(), { 0.0, 0.0, 0.0 }));
        }
    }

    SECTION("sparse")
    {
        const auto& sparse = accessor.getSparse();
        REQUIRE(sparse.has_value());

        SECTION("count = 3")
        {
            CHECK(sparse->getCount() == 3);
        }
        SECTION("indices")
        {
            const auto& indices = sparse->getIndices();

            SECTION("bufferView = 2")
            {
                const uint32_t bufferViewIndex = 2;
                REQUIRE(gltf.getBufferViews().size() > bufferViewIndex);
                CHECK(indices.getBufferView() == &gltf.getBufferViews().at(bufferViewIndex));
            }
            SECTION("componentType = 5123")
            {
                CHECK(indices.getComponentType() == static_cast<ComponentType>(5123));
            }
            SECTION("byteOffset = 0")
            {
                REQUIRE(indices.getByteOffset().has_value());
                CHECK(indices.getByteOffset().value() == 0);
            }
        }
        SECTION("values")
        {
            const auto& values = sparse->getValues();

            SECTION("bufferView = 3")
            {
                const uint32_t bufferViewIndex = 3;
                REQUIRE(gltf.getBufferViews().size() > bufferViewIndex);
                CHECK(values.getBufferView() == &gltf.getBufferViews().at(bufferViewIndex));
            }
            SECTION("byteOffset = 0")
            {
                REQUIRE(values.getByteOffset().has_value());
                CHECK(values.getByteOffset().value() == 0);
            }
        }
    }
}
}
