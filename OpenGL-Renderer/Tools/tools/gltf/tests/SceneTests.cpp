// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "core/Algorithms.h"
#include "gltf/Gltf.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.Scene]";

TEST_CASE("Scene name", TestNamespace)
{
    Scene scene(0);

    SECTION("initially empty")
    {
        CHECK(scene.hasName() == false);
    }

    SECTION("can be changed")
    {
        REQUIRE(scene.setName("hello").derived == GltfResult::Success);
        CHECK(scene.getName() == "hello");
    }
}

TEST_CASE("Scene nodes", TestNamespace)
{
    Gltf gltf;
    Scene& scene = *gltf.create<Scene>();

    SECTION("initially empty")
    {
        CHECK(scene.getNodes().empty());
    }

    SECTION("can be added")
    {
        Node& node1 = *gltf.create<Node>();
        Node& node2 = *gltf.create<Node>();

        REQUIRE(scene.addNode(node1, gltf));
        REQUIRE(scene.getNodes().size() == 1);
        CHECK(scene.getNodes()[0] == &node1);

        REQUIRE(scene.addNode(node2, gltf));
        REQUIRE(scene.getNodes().size() == 2);
        CHECK(scene.getNodes()[1] == &node2);
    }

    SECTION("duplicate node can not be added")
    {
        Node& node1 = *gltf.create<Node>();

        REQUIRE(scene.addNode(node1, gltf));
        REQUIRE(scene.getNodes().size() == 1);

        CHECK(scene.addNode(node1, gltf).derived == GltfError::ObjectAlreadyExists);
        CHECK(scene.getNodes().size() == 1);
    }

    SECTION("can be removed")
    {
        Node& node1 = *gltf.create<Node>();
        Node& node2 = *gltf.create<Node>();

        REQUIRE(scene.addNode(node1, gltf));
        REQUIRE(scene.addNode(node2, gltf));

        CHECK(scene.removeNode(node1));
        REQUIRE(scene.getNodes().size() == 1);
        CHECK(scene.getNodes()[0] == &node2);
    }

    SECTION("removing preserves order")
    {
        Node& node1 = *gltf.create<Node>();
        Node& node2 = *gltf.create<Node>();
        Node& node3 = *gltf.create<Node>();
        Node& node4 = *gltf.create<Node>();

        REQUIRE(scene.addNode(node1, gltf));
        REQUIRE(scene.addNode(node2, gltf));
        REQUIRE(scene.addNode(node3, gltf));
        REQUIRE(scene.addNode(node4, gltf));

        REQUIRE(scene.removeNode(node2));
        REQUIRE(scene.getNodes().size() == 3);
        CHECK(scene.getNodes()[0] == &node1);
        CHECK(scene.getNodes()[1] == &node3);
        CHECK(scene.getNodes()[2] == &node4);
    }
}

SCENARIO("Scene: move node", TestNamespace)
{
    Gltf gltf;
    Scene scene(0);

    GIVEN("no nodes")
    {
        WHEN("move node")
        {
            GltfResult result = scene.moveNode(0, 0);
            THEN("returns failure")
            {
                CHECK(result.derived == GltfError::ObjectNotFound);
            }
        }
    }

    GIVEN("one node")
    {
        Node node(0);
        scene.addNode(node, gltf);
        REQUIRE_FALSE(scene.getNodes().empty());

        WHEN("same index")
        {
            GltfResult result = scene.moveNode(0, 0);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("nothing happened")
            {
                CHECK(core::safeAt(scene.getNodes(), 0) == &node);
            }
        }
    }

    GIVEN("two nodes")
    {
        Node node1(0);
        Node node2(1);
        scene.addNode(node1, gltf);
        scene.addNode(node2, gltf);
        REQUIRE(scene.getNodes().size() == 2);

        WHEN("move 0 to 1")
        {
            GltfResult result = scene.moveNode(0, 1);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("order inversed")
            {
                CHECK(core::safeAt(scene.getNodes(), 0) == &node2);
                CHECK(core::safeAt(scene.getNodes(), 1) == &node1);
            }
        }

        WHEN("move 1 to 0")
        {
            GltfResult result = scene.moveNode(1, 0);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("order inversed")
            {
                CHECK(core::safeAt(scene.getNodes(), 0) == &node2);
                CHECK(core::safeAt(scene.getNodes(), 1) == &node1);
            }
        }
    }

    GIVEN("many nodes")
    {
        Node node1(0);
        Node node2(1);
        Node node3(2);
        Node node4(3);
        scene.addNode(node1, gltf);
        scene.addNode(node2, gltf);
        scene.addNode(node3, gltf);
        scene.addNode(node4, gltf);
        REQUIRE(scene.getNodes().size() == 4);

        WHEN("move 0 to 3")
        {
            GltfResult result = scene.moveNode(0, 3);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("first node is at the end")
            {
                CHECK(core::safeAt(scene.getNodes(), 3) == &node1);
            }
            THEN("other nodes stay in same order")
            {
                CHECK(core::safeAt(scene.getNodes(), 0) == &node2);
                CHECK(core::safeAt(scene.getNodes(), 1) == &node3);
                CHECK(core::safeAt(scene.getNodes(), 2) == &node4);
            }
        }

        WHEN("move 2 to 0")
        {
            GltfResult result = scene.moveNode(2, 0);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("third node is at beginning")
            {
                CHECK(core::safeAt(scene.getNodes(), 0) == &node3);
            }
            THEN("other nodes stay in same order")
            {
                CHECK(core::safeAt(scene.getNodes(), 1) == &node1);
                CHECK(core::safeAt(scene.getNodes(), 2) == &node2);
                CHECK(core::safeAt(scene.getNodes(), 3) == &node4);
            }
        }
    }
}

SCENARIO("Scene: get nodes recursively", TestNamespace)
{
    Gltf gltf;
    Scene scene(0);
    Node node(1);
    Node node2(2);
    Node childNode1(3);
    Node grandChildNode1(4);
    Node grandChildNode2(5);

    scene.addNode(node, gltf);
    scene.addNode(node2, gltf);
    node.addChild(childNode1, gltf);
    childNode1.addChild(grandChildNode1, gltf);
    childNode1.addChild(grandChildNode2, gltf);

    GIVEN("scene with deep node hierarchy")
    {
        core::Vector<Node*> recursiveNodes;
        CHECK(scene.getNodesRecursively(recursiveNodes));
        CHECK(recursiveNodes.size() == 5);
    }
}

SCENARIO("Scene: insert node", TestNamespace)
{
    Gltf gltf;
    Scene scene(0);
    Node node(1);
    const auto& nodes = scene.getNodes();

    GIVEN("no nodes")
    {
        WHEN("insert at 0")
        {
            GltfResult result = scene.insertNode(node, 0, gltf);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("node is added")
            {
                CHECK(core::safeAt(nodes, 0) == &node);
            }
        }

        WHEN("insert at 2")
        {
            GltfResult result = scene.insertNode(node, 2, gltf);
            THEN("returns not found")
            {
                CHECK(result.derived == GltfError::ObjectNotFound);
            }
            THEN("node is added")
            {
                CHECK(core::safeAt(nodes, 0) == &node);
            }
        }
    }

    GIVEN("many nodes")
    {
        Node node1(2);
        Node node2(3);
        Node node3(4);
        scene.addNode(node1, gltf);
        scene.addNode(node2, gltf);
        scene.addNode(node3, gltf);

        WHEN("insert at 0")
        {
            GltfResult result = scene.insertNode(node, 0, gltf);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("node is inserted at 0")
            {
                CHECK(core::safeAt(nodes, 0) == &node);
            }
            THEN("other nodes stay in order")
            {
                CHECK(core::safeAt(nodes, 1) == &node1);
                CHECK(core::safeAt(nodes, 2) == &node2);
                CHECK(core::safeAt(nodes, 3) == &node3);
            }
        }

        WHEN("insert at 2 (inside the list)")
        {
            GltfResult result = scene.insertNode(node, 2, gltf);
            THEN("returns success")
            {
                CHECK(result.derived == GltfResult::Success);
            }
            THEN("node is inserted at 2")
            {
                CHECK(core::safeAt(nodes, 2) == &node);
            }
            THEN("other nodes stay in order")
            {
                CHECK(core::safeAt(nodes, 0) == &node1);
                CHECK(core::safeAt(nodes, 1) == &node2);
                CHECK(core::safeAt(nodes, 3) == &node3);
            }
        }

        WHEN("insert out of range")
        {
            GltfResult result = scene.insertNode(node, 7, gltf);
            THEN("returns not found")
            {
                CHECK(result.derived == GltfError::ObjectNotFound);
            }
            THEN("node is at the end")
            {
                REQUIRE_FALSE(nodes.empty());
                CHECK(nodes.back() == &node);
            }
            THEN("other nodes stay in order")
            {
                CHECK(core::safeAt(nodes, 0) == &node1);
                CHECK(core::safeAt(nodes, 1) == &node2);
                CHECK(core::safeAt(nodes, 2) == &node3);
            }
        }
    }
}
}
