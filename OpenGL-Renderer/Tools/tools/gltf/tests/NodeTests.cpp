// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "core/Algorithms.h"
#include "gltf/Gltf.h"
#include "gltf/Node.h"
#include <catch2/catch.hpp>

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.Node]";

TEST_CASE("Node name", TestNamespace)
{
    Node node(0);

    SECTION("initially empty")
    {
        CHECK(node.hasName() == false);
    }

    SECTION("can be changed")
    {
        REQUIRE(node.setName("hello").derived == GltfResult::Success);
        CHECK(node.getName() == "hello");
    }
}

TEST_CASE("Node children", TestNamespace)
{
    Gltf gltf;
    Node& node = *gltf.create<Node>();

    SECTION("initially empty")
    {
        CHECK(node.getChildren().empty());
    }

    SECTION("can be added")
    {
        Node& child1 = *gltf.create<Node>();
        Node& child2 = *gltf.create<Node>();

        REQUIRE(node.addChild(child1, gltf));
        REQUIRE(node.getChildren().size() == 1);
        CHECK(node.getChildren()[0] == &child1);

        REQUIRE(node.addChild(child2, gltf));
        REQUIRE(node.getChildren().size() == 2);
        CHECK(node.getChildren()[1] == &child2);
    }

    SECTION("duplicate cannot be added")
    {
        Node& child = *gltf.create<Node>();

        REQUIRE(core::Succeeded(node.addChild(child, gltf)));
        CHECK(node.addChild(child, gltf).derived == GltfError::ObjectAlreadyExists);
    }

    SECTION("self cannot be added")
    {
        CHECK(node.addChild(node, gltf).derived == GltfError::SelfReference);
    }

    SECTION("parent cannot be added as child")
    {
        Node& child = *gltf.create<Node>();
        REQUIRE(core::Succeeded(node.addChild(child, gltf)));

        CHECK(child.addChild(node, gltf).derived == GltfError::HierarchyRecursion);
    }

    SECTION("can be removed")
    {
        Node& child1 = *gltf.create<Node>();
        Node& child2 = *gltf.create<Node>();

        REQUIRE(node.addChild(child1, gltf));
        REQUIRE(node.addChild(child2, gltf));

        CHECK(node.removeChild(child1));
        REQUIRE(node.getChildren().size() == 1);
        CHECK(node.getChildren()[0] == &child2);
    }

    SECTION("removing preserves order")
    {
        Node& child1 = *gltf.create<Node>();
        Node& child2 = *gltf.create<Node>();
        Node& child3 = *gltf.create<Node>();
        Node& child4 = *gltf.create<Node>();

        REQUIRE(node.addChild(child1, gltf));
        REQUIRE(node.addChild(child2, gltf));
        REQUIRE(node.addChild(child3, gltf));
        REQUIRE(node.addChild(child4, gltf));

        REQUIRE(node.removeChild(child2));
        REQUIRE(node.getChildren().size() == 3);
        CHECK(node.getChildren()[0] == &child1);
        CHECK(node.getChildren()[1] == &child3);
        CHECK(node.getChildren()[2] == &child4);
    }
}

TEST_CASE("Mesh Test", TestNamespace)
{
    Node node(0);
    Mesh mesh(0);

    SECTION("initially empty")
    {
        CHECK(node.getMesh() == nullptr);
    }
    SECTION("can be modified")
    {
        node.setMesh(mesh);
        CHECK(node.getMesh() == &mesh);
    }
}

TEST_CASE("Camera Test", TestNamespace)
{
    Node node(0);

    SECTION("initially empty")
    {
        CHECK(node.getCamera() == nullptr);
    }
}

TEST_CASE("Light Test", TestNamespace)
{
    Node node(0);

    SECTION("initially empty")
    {
        CHECK(node.getLight() == nullptr);
    }
}

TEST_CASE("Skin Test", TestNamespace)
{
    Node node(0);

    SECTION("initially empty")
    {
        CHECK(node.getSkin() == nullptr);
    }
}

TEST_CASE("Initial transform values", TestNamespace)
{
    Node node(0);

    SECTION("rotation")
    {
        CHECK(node.getRotationOrDefault().x == 0.0);
        CHECK(node.getRotationOrDefault().y == 0.0);
        CHECK(node.getRotationOrDefault().z == 0.0);
        CHECK(node.getRotationOrDefault().w == 1.0);
    }

    SECTION("translation")
    {
        CHECK(node.getTranslationOrDefault().x == 0.0);
        CHECK(node.getTranslationOrDefault().y == 0.0);
        CHECK(node.getTranslationOrDefault().z == 0.0);
    }

    SECTION("scale")
    {
        CHECK(node.getScaleOrDefault().x == 1.0);
        CHECK(node.getScaleOrDefault().y == 1.0);
        CHECK(node.getScaleOrDefault().z == 1.0);
    }
}

SCENARIO("Node: move child", TestNamespace)
{
    Gltf gltf;
    Node node(0);

    GIVEN("no children")
    {
        WHEN("move child")
        {
            GltfResult result = node.moveChild(0, 0);
            THEN("returns failure")
            {
                CHECK(result.derived == GltfError::ObjectNotFound);
            }
        }
    }

    GIVEN("one child")
    {
        Node child(0);
        node.addChild(child, gltf);
        REQUIRE_FALSE(node.getChildren().empty());

        WHEN("same index")
        {
            GltfResult result = node.moveChild(0, 0);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("nothing happened")
            {
                CHECK(core::safeAt(node.getChildren(), 0) == &child);
            }
        }
    }

    GIVEN("two children")
    {
        Node child1(0);
        Node child2(1);
        node.addChild(child1, gltf);
        node.addChild(child2, gltf);
        REQUIRE(node.getChildren().size() == 2);

        WHEN("move 0 to 1")
        {
            GltfResult result = node.moveChild(0, 1);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("order inversed")
            {
                CHECK(core::safeAt(node.getChildren(), 0) == &child2);
                CHECK(core::safeAt(node.getChildren(), 1) == &child1);
            }
        }

        WHEN("move 1 to 0")
        {
            GltfResult result = node.moveChild(1, 0);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("order inversed")
            {
                CHECK(core::safeAt(node.getChildren(), 0) == &child2);
                CHECK(core::safeAt(node.getChildren(), 1) == &child1);
            }
        }
    }

    GIVEN("many children")
    {
        Node child1(0);
        Node child2(1);
        Node child3(2);
        Node child4(3);
        node.addChild(child1, gltf);
        node.addChild(child2, gltf);
        node.addChild(child3, gltf);
        node.addChild(child4, gltf);
        REQUIRE(node.getChildren().size() == 4);

        WHEN("move 0 to 3")
        {
            GltfResult result = node.moveChild(0, 3);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("first child is at the end")
            {
                CHECK(core::safeAt(node.getChildren(), 3) == &child1);
            }
            THEN("other children stay in same order")
            {
                CHECK(core::safeAt(node.getChildren(), 0) == &child2);
                CHECK(core::safeAt(node.getChildren(), 1) == &child3);
                CHECK(core::safeAt(node.getChildren(), 2) == &child4);
            }
        }

        WHEN("move 2 to 0")
        {
            GltfResult result = node.moveChild(2, 0);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("third child is at beginning")
            {
                CHECK(core::safeAt(node.getChildren(), 0) == &child3);
            }
            THEN("other children stay in same order")
            {
                CHECK(core::safeAt(node.getChildren(), 1) == &child1);
                CHECK(core::safeAt(node.getChildren(), 2) == &child2);
                CHECK(core::safeAt(node.getChildren(), 3) == &child4);
            }
        }
    }
}

SCENARIO("Node: insert child", TestNamespace)
{
    Gltf gltf;
    Node node(0);
    Node child(1);
    const auto& children = node.getChildren();

    GIVEN("no children")
    {
        WHEN("insert at 0")
        {
            GltfResult result = node.insertChild(child, 0, gltf);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("child is added")
            {
                CHECK(core::safeAt(children, 0) == &child);
            }
        }

        WHEN("insert at 2")
        {
            GltfResult result = node.insertChild(child, 2, gltf);
            THEN("returns not found")
            {
                CHECK(result.derived == GltfError::ObjectNotFound);
            }
            THEN("child is added")
            {
                CHECK(core::safeAt(children, 0) == &child);
            }
        }
    }

    GIVEN("many children")
    {
        Node child1(2);
        Node child2(3);
        Node child3(4);
        node.addChild(child1, gltf);
        node.addChild(child2, gltf);
        node.addChild(child3, gltf);

        WHEN("insert at 0")
        {
            GltfResult result = node.insertChild(child, 0, gltf);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("child is inserted at 0")
            {
                CHECK(core::safeAt(children, 0) == &child);
            }
            THEN("other children stay in order")
            {
                CHECK(core::safeAt(children, 1) == &child1);
                CHECK(core::safeAt(children, 2) == &child2);
                CHECK(core::safeAt(children, 3) == &child3);
            }
        }

        WHEN("insert at 2 (inside the list)")
        {
            GltfResult result = node.insertChild(child, 2, gltf);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("child is inserted at 2")
            {
                CHECK(core::safeAt(children, 2) == &child);
            }
            THEN("other children stay in order")
            {
                CHECK(core::safeAt(children, 0) == &child1);
                CHECK(core::safeAt(children, 1) == &child2);
                CHECK(core::safeAt(children, 3) == &child3);
            }
        }

        WHEN("insert out of range")
        {
            GltfResult result = node.insertChild(child, 7, gltf);
            THEN("returns not found")
            {
                CHECK(result.derived == GltfError::ObjectNotFound);
            }
            THEN("child is at the end")
            {
                REQUIRE_FALSE(children.empty());
                CHECK(children.back() == &child);
            }
            THEN("other children stay in order")
            {
                CHECK(core::safeAt(children, 0) == &child1);
                CHECK(core::safeAt(children, 1) == &child2);
                CHECK(core::safeAt(children, 2) == &child3);
            }
        }
    }
}

SCENARIO("Node: getRecursiveChildren", TestNamespace)
{
    Gltf gltf;
    Node node(0);
    Node child1(1);
    Node child2(2);
    Node grandChild1(3);
    Node grandChild2(4);

    node.addChild(child1, gltf);
    node.addChild(child2, gltf);
    child1.addChild(grandChild1, gltf);
    child2.addChild(grandChild2, gltf);

    GIVEN("node with grandchildren")
    {
        core::Vector<Node*> descendents;
        CHECK(node.getChildrenRecursively(descendents));
        CHECK(descendents.size() == 4);
    }
}

SCENARIO("Node: hasParent", TestNamespace)
{
    Gltf gltf;
    Node& node = *gltf.create<Node>();

    GIVEN("node without parent")
    {
        CHECK_FALSE(node.hasParent(gltf));
    }
    GIVEN("node with scene parent")
    {
        Scene& scene = *gltf.create<Scene>();
        REQUIRE(core::Succeeded(scene.addNode(node, gltf)));

        CHECK(node.hasParent(gltf));
    }
    GIVEN("Node with node parent")
    {
        Node& parent = *gltf.create<Node>();
        REQUIRE(core::Succeeded(parent.addChild(node, gltf)));

        CHECK(node.hasParent(gltf));
    }
}
}
