
#include "gtest/gtest.h"

#include "reactor/GltfEnums.h"

using namespace ux3d;
using namespace ux3d::reactor;
using namespace ux3d::reactor::gltf;

TEST(GltfEnumsTests, indicesAreNotPadded)
{
    EXPECT_EQ(gltf::byteLengthPadded(Type::SCALAR, ComponentType::UnsignedByte), 1);
    EXPECT_EQ(gltf::byteLengthPadded(Type::SCALAR, ComponentType::UnsignedShort), 2);
    EXPECT_EQ(gltf::byteLengthPadded(Type::SCALAR, ComponentType::UnsignedInt), 4);
}

TEST(GltfEnumsTests, floatVecsAreAligned)
{
    EXPECT_EQ(gltf::byteLengthPadded(Type::VEC2, ComponentType::Float), 8);
    EXPECT_EQ(gltf::byteLengthPadded(Type::VEC3, ComponentType::Float), 12);
    EXPECT_EQ(gltf::byteLengthPadded(Type::VEC4, ComponentType::Float), 16);
}

TEST(GltfEnumsTests, smallVecsArePadded)
{
    EXPECT_EQ(gltf::byteLengthPadded(Type::VEC2, ComponentType::Byte), 4);
    EXPECT_EQ(gltf::byteLengthPadded(Type::VEC3, ComponentType::Byte), 4);
    EXPECT_EQ(gltf::byteLengthPadded(Type::VEC2, ComponentType::Short), 4);
}

TEST(GltfEnumsTests, smallMatricesArePadded)
{
    EXPECT_EQ(gltf::byteLengthPadded(Type::MAT2, ComponentType::Byte), 8);
    EXPECT_EQ(gltf::byteLengthPadded(Type::MAT3, ComponentType::Byte), 12);
    EXPECT_EQ(gltf::byteLengthPadded(Type::MAT3, ComponentType::Short), 24);
}
