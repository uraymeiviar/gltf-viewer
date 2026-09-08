// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "catch2/catch.hpp"
#include "gltf/Gltf.h"

namespace ux3d::gltf::tests {
constexpr auto TestNamespace = "[tools.gltf.Gltf]";

TEST_CASE("Get default values", TestNamespace)
{
    Gltf gltf;
    CHECK(gltf.getDefaultScene() == nullptr);
}

TEST_CASE("Add node", TestNamespace)
{
    Gltf gltf;
    auto& node = *gltf.create<Node>();
    CHECK(&*gltf.getNodes().begin() == &node);
}

TEST_CASE("Check copyright", TestNamespace)
{
    Gltf gltf;
    CHECK(gltf.getAsset().getCopyrightOrDefault() == "");

    gltf.getAsset().setCopyright("Copyright (c) 2019 UX3D GmbH. All rights reserved.");
    CHECK(gltf.getAsset().getCopyrightOrDefault() == "Copyright (c) 2019 UX3D GmbH. All rights reserved.");
}

TEST_CASE("Add scene", TestNamespace)
{
    Gltf gltf;
    auto& scene = *gltf.create<Scene>();
    CHECK(&*gltf.getScenes().begin() == &scene);
}

TEST_CASE("Deleting Scenes", TestNamespace)
{
    Gltf gltf;
    Scene& scene1 = *gltf.create<Scene>();
    Scene& scene2 = *gltf.create<Scene>();
    Scene& scene3 = *gltf.create<Scene>();
    Node& node = *gltf.create<Node>();
    REQUIRE(scene2.addNode(node, gltf));
    gltf.setDefaultScene(scene1);

    REQUIRE(gltf.deleteScene(scene2));
    REQUIRE(gltf.getScenes().size() == 2);
    REQUIRE((&gltf.getScenes().front() == &scene1 && &gltf.getScenes().back() == &scene3));

    REQUIRE(gltf.deleteScene(scene3));
    REQUIRE(gltf.getScenes().size() == 1);
    REQUIRE(&gltf.getScenes().front() == &scene1);

    REQUIRE(gltf.deleteScene(scene1));
    REQUIRE(gltf.getScenes().empty());
    REQUIRE(gltf.getScenes().begin() == nullptr);
    REQUIRE(gltf.getNodes().size() == 1);
    CHECK(gltf.getDefaultScene() == nullptr);
}

SCENARIO("Delete node", TestNamespace)
{
    Gltf gltf;
    Node& node = *gltf.create<Node>();

    auto check = [&gltf, &node] {
        size_t nodesCountBefore = gltf.getNodes().size();
        auto result = gltf.deleteNode(node);
        size_t nodesCountAfter = gltf.getNodes().size();

        CHECK(core::Succeeded(result));
        CHECK(nodesCountAfter == nodesCountBefore - 1);
    };
    auto setupSceneParent = [&gltf, &node] {
        Scene& scene = *gltf.create<Scene>();
        REQUIRE(core::Succeeded(scene.addNode(node, gltf)));
    };
    auto setupNodeParent = [&gltf, &node] {
        Node& parent = *gltf.create<Node>();
        REQUIRE(core::Succeeded(parent.addChild(node, gltf)));
    };
    auto setupNodeChild = [&gltf, &node] {
        Node& child = *gltf.create<Node>();
        REQUIRE(core::Succeeded(node.addChild(child, gltf)));
    };

    GIVEN("node without parent")
    {
        AND_GIVEN("without children")
        {
            check();
        }
        AND_GIVEN("with child node")
        {
            setupNodeChild();
            check();
        }
    }
    GIVEN("node with parent scene")
    {
        setupSceneParent();

        AND_GIVEN("without children")
        {
            check();
        }
        AND_GIVEN("with child node")
        {
            setupNodeChild();
            check();
        }
    }
    GIVEN("node with parent node")
    {
        setupNodeParent();

        AND_GIVEN("without children")
        {
            check();
        }
        AND_GIVEN("with child node")
        {
            setupNodeChild();
            check();
        }
    }
}

TEST_CASE("glTF version", TestNamespace)
{
    Gltf gltf;
    CHECK(gltf.getAsset().getVersionOrDefault() == "2.0");
}

TEST_CASE("glTF generator", TestNamespace)
{
    Gltf gltf;
    CHECK(gltf.getAsset().getGeneratorOrDefault() == "");
}

TEST_CASE("Gltf: IDs", TestNamespace)
{
    Gltf gltf;
    Node& node1 = *gltf.create<Node>();
    Node& node2 = *gltf.create<Node>();
    Node& node3 = *gltf.import(node1);

    SECTION("regular nodes")
    {
        CHECK(node1.m_ID == 0);
        CHECK(node2.m_ID == 1);
    }

    SECTION("copied node")
    {
        CHECK(node3.m_ID == 2);
    }

    SECTION("node deletion doesn't change other nodes IDs")
    {
        gltf.deleteNode(node2);
        CHECK(node1.m_ID == 0);
        CHECK(node3.m_ID == 2);
    }

    SECTION("nodes can be found by their IDs")
    {
        CHECK(&node1 == gltf.getByID<Node>(0));
        CHECK(&node2 == gltf.getByID<Node>(1));
        CHECK(&node3 == gltf.getByID<Node>(2));
        CHECK(nullptr == gltf.getByID<Node>(3));
    }
}

TEST_CASE("Gltf: import a Node from another Gltf")
{
    Gltf gltf1;
    Gltf gltf2;
    Node& nodeInGltf1 = *gltf1.create<Node>();
    Node& nodeInGltf2 = *gltf2.create<Node>();
    Mesh& meshInGltf1 = *gltf1.create<Mesh>();
    nodeInGltf1.setMesh(meshInGltf1);

    Node* copy = gltf2.import(nodeInGltf1);
    REQUIRE(copy != nullptr);

    SECTION("has new ID")
    {
        CHECK(nodeInGltf1.m_ID == nodeInGltf2.m_ID);
        CHECK(copy->m_ID != nodeInGltf2.m_ID);
    }

    SECTION("does not keep Mesh reference")
    {
        CHECK(nodeInGltf1.getMesh() == &meshInGltf1);
        CHECK(nodeInGltf2.getMesh() == nullptr);
    }
}

TEST_CASE("Gltf: duplicate a Node")
{
    Gltf gltf;
    Node& nodeInGltf1 = *gltf.create<Node>();
    Mesh& meshInGltf1 = *gltf.create<Mesh>();
    nodeInGltf1.setMesh(meshInGltf1);

    SECTION("Node belongs to Gltf instance")
    {
        SECTION("has no children")
        {
            Node* duplicate = gltf.duplicate(nodeInGltf1);
            REQUIRE(duplicate != nullptr);

            SECTION("has new ID")
            {
                CHECK(duplicate->m_ID != nodeInGltf1.m_ID);
            }

            SECTION("keeps Mesh reference")
            {
                CHECK(duplicate->getMesh() == &meshInGltf1);
            }
        }
        SECTION("has children")
        {
            Node& child1 = *gltf.create<Node>();
            Node& child2 = *gltf.create<Node>();
            Node& childOfChild1 = *gltf.create<Node>();

            nodeInGltf1.addChild(child1, gltf);
            nodeInGltf1.addChild(child2, gltf);
            child1.addChild(childOfChild1, gltf);

            const size_t nodesCountBefore = gltf.getNodes().size();

            const Node* duplicate = gltf.duplicate(nodeInGltf1);
            REQUIRE(duplicate != nullptr);

            const size_t nodesCountAfter = gltf.getNodes().size();

            SECTION("gltf has correct number of Nodes (all children should be duplicated too)")
            {
                CHECK(nodesCountAfter == nodesCountBefore * 2);
            }

            SECTION("children are duplicated too")
            {
                CHECK(duplicate->getChildren().size() == nodeInGltf1.getChildren().size());
                REQUIRE(duplicate->getChildren().size() == 2);

                const Node* pDupliChild1 = duplicate->getChildren().at(0);
                const Node* pDupliChild2 = duplicate->getChildren().at(1);

                SECTION("children duplicates have new IDs")
                {
                    CHECK(pDupliChild1->m_ID != child1.m_ID);
                    CHECK(pDupliChild1->m_ID != child2.m_ID);

                    CHECK(pDupliChild2->m_ID != child1.m_ID);
                    CHECK(pDupliChild2->m_ID != child2.m_ID);
                }
                SECTION("child of child is duplicated as well")
                {
                    CHECK(pDupliChild1->getChildren().size() == child1.getChildren().size());
                    REQUIRE(pDupliChild1->getChildren().size() == 1);

                    const Node* pChildOfDupliChild1 = pDupliChild1->getChildren().front();

                    SECTION("child of child duplicate has new ID")
                    {
                        CHECK(pChildOfDupliChild1->m_ID != childOfChild1.m_ID);
                    }
                }
            }
        }
    }
}

TEST_CASE("glTF reject node removal that are part of a skin")
{
    Gltf gltf;
    Node& node1 = *gltf.create<Node>();
    Node& node2 = *gltf.create<Node>();
    Skin& skin1 = *gltf.create<Skin>();
    skin1.addJoint(node1);

    CHECK(gltf.deleteNode(node1) == GltfError::NodeIsPartOfSkin);
    CHECK(gltf.getNodes().size() == 2);
    CHECK(gltf.deleteNode(node2));
}

TEST_CASE("Gltf: duplicate a Scene")
{
    Gltf gltf;
    Scene& scene = *gltf.create<Scene>();

    SECTION("has no nodes")
    {
        const Scene* duplicate = gltf.duplicate(scene);
        REQUIRE(duplicate != nullptr);

        SECTION("has new ID")
        {
            CHECK(duplicate->m_ID != scene.m_ID);
        }
    }
    SECTION("has nodes")
    {
        Node& node1 = *gltf.create<Node>();
        Node& node2 = *gltf.create<Node>();
        Node& childOfNode1 = *gltf.create<Node>();

        scene.addNode(node1, gltf);
        scene.addNode(node2, gltf);
        node1.addChild(childOfNode1, gltf);

        const size_t nodesCountBefore = gltf.getNodes().size();

        const Scene* duplicate = gltf.duplicate(scene);
        REQUIRE(duplicate != nullptr);

        const size_t nodesCountAfter = gltf.getNodes().size();

        SECTION("gltf has correct number of Nodes (all nodes should be duplicated too)")
        {
            CHECK(nodesCountAfter == nodesCountBefore * 2);
        }

        SECTION("nodes are duplicated too")
        {
            CHECK(duplicate->getNodes().size() == scene.getNodes().size());
            REQUIRE(duplicate->getNodes().size() == 2);

            const Node* pDupliNode1 = duplicate->getNodes().at(0);
            const Node* pDupliNode2 = duplicate->getNodes().at(1);

            SECTION("node duplicates have new IDs")
            {
                CHECK(pDupliNode1->m_ID != node1.m_ID);
                CHECK(pDupliNode1->m_ID != node2.m_ID);

                CHECK(pDupliNode2->m_ID != node1.m_ID);
                CHECK(pDupliNode2->m_ID != node2.m_ID);
            }
            SECTION("child of node is duplicated as well")
            {
                CHECK(pDupliNode1->getChildren().size() == node1.getChildren().size());
                REQUIRE(pDupliNode1->getChildren().size() == 1);

                const Node* pChildOfDupliNode1 = pDupliNode1->getChildren().front();

                SECTION("child of child duplicate has new ID")
                {
                    CHECK(pChildOfDupliNode1->m_ID != childOfNode1.m_ID);
                }
            }
        }
    }
}

TEST_CASE("Material Variants", TestNamespace)
{
    Gltf gltf;
    CHECK(gltf.hasMaterialVariants() == false);
    gltf.createMaterialVariants();
    CHECK(gltf.hasMaterialVariants() == true);
    CHECK(gltf.getMaterialVariants().empty());

    SECTION("add material variants to global array")
    {
        gltf::MaterialVariant* pMaterialVariant_1 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_1->setName("first variant");
        CHECK(gltf.getMaterialVariants().size() == 1);
        CHECK(gltf.getMaterialVariants().at(0).getName() == "first variant");
        gltf::MaterialVariant* pMaterialVariant_2 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_2->setName("second variant");
        gltf::MaterialVariant* pMaterialVariant_3 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_3->setName("third variant");
        gltf::MaterialVariant* pMaterialVariant_4 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_4->setName("forth variant");
        CHECK(gltf.getMaterialVariants().size() == 4);
        CHECK(gltf.getMaterialVariants().at(3).getName() == "forth variant");
    }

    SECTION("delete material variants from global array")
    {
        gltf::MaterialVariant* pMaterialVariant_1 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_1->setName("first variant");
        gltf::MaterialVariant* pMaterialVariant_2 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_2->setName("second variant");
        gltf::MaterialVariant* pMaterialVariant_3 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_3->setName("third variant");
        gltf.deleteMaterialVariant(0);
        CHECK(gltf.getMaterialVariants().size() == 2);
        CHECK(gltf.getMaterialVariants().at(0).getName() == "second variant");
        CHECK(gltf.getMaterialVariants().at(1).getName() == "third variant");
    }

    SECTION("update material variants in global array")
    {

        gltf::MaterialVariant* pMaterialVariant_1 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_1->setName("first variant");
        gltf::MaterialVariant* pMaterialVariant_2 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_2->setName("hallo variant");
        gltf::MaterialVariant* pMaterialVariant_3 = gltf.create<gltf::MaterialVariant>();
        pMaterialVariant_3->setName("muenchen variant");
        gltf.getMaterialVariants().at(1).setName("second variant");
        gltf.getMaterialVariants().at(2).setName("third variant");
        CHECK(gltf.getMaterialVariants().at(1).getName() == "second variant");
        CHECK(gltf.getMaterialVariants().at(2).getName() == "third variant");
    }

    SECTION("remove material variants from global gltf")
    {
        gltf.destroyMaterialVariants();
        CHECK(gltf.hasMaterialVariants() == false);
    }
}
}
