// Records that hold themselves. `? parent: Node` inside Node was spelled
// std::optional<Node> (an incomplete type), `next: Loop` a struct holding
// itself, and a record naming one declared after it an unknown type. A field on
// a by-value cycle is now a std::shared_ptr, and structs are ordered and, when
// needed, declared up front. The round trip runs through the generated codec.

#include <gtest/gtest.h>

#include "generated_code.h"

namespace {

const char* kContract = R"LIDL(
module probe {
  version "1.0.0"
  type Node {
    value: int
    children: [Node]
    byName: {tstr: Node}
    ? parent: Node
  }
  type Loop {
    next: Loop
  }
  type Pair {
    left: Tree
    right: ? Tree
  }
  type Tree {
    pair: ? Pair
    leaf: ? int
  }
  type Early {
    late: Late
    lates: [Late]
  }
  type Late {
    n: int
  }
  method echoNode(n: Node) -> Node
  method echoTree(t: Tree) -> Tree
  method echoLoop(l: Loop) -> Loop
  method echoEarly(e: Early) -> Early
  event grown(node: Node, depth: uint)
}
)LIDL";

const char* kRoundTrip = R"CPP(#include "probe_api.h"
#include <cstdio>
#define CHECK(c) do { if (!(c)) { std::printf("FAILED line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main()
{
    Probe probe("probe_test");
    Probe::Node leaf;
    leaf.value = 3;
    Probe::Node child;
    child.value = 2;
    child.children.push_back(leaf);
    Probe::Node root;
    root.value = 1;
    root.children.push_back(child);
    root.byName["c"] = child;
    root.parent = std::make_shared<Probe::Node>();
    root.parent->parent = std::make_shared<Probe::Node>();
    root.parent->parent->value = -1;
    const Probe::Node n = probe.echoNode(root);
    CHECK(n.value == 1 && n.children.size() == 1 && n.children[0].value == 2);
    CHECK(n.children[0].children.size() == 1 && n.children[0].children[0].value == 3);
    CHECK(n.byName.count("c") == 1 && n.byName.at("c").children.at(0).value == 3);
    CHECK(n.parent && n.parent->value == 0 && n.parent->parent);
    CHECK(n.parent->parent->value == -1 && !n.parent->parent->parent && !n.children[0].parent);

    Probe::Tree t;
    t.pair = std::make_shared<Probe::Pair>();
    t.pair->left = std::make_shared<Probe::Tree>();
    t.pair->left->leaf = 7;
    const Probe::Tree back = probe.echoTree(t);
    CHECK(back.pair && back.pair->left && back.pair->left->leaf == 7);
    CHECK(!back.pair->right && !back.leaf && !back.pair->left->pair);

    // An empty box encodes as absent: no default Loop is built, so no recursion.
    CHECK(!probe.echoLoop(Probe::Loop{}).next);

    Probe::Early e;
    e.late.n = 5;
    e.lates.push_back(Probe::Late{6});
    const Probe::Early early = probe.echoEarly(e);
    CHECK(early.late.n == 5 && early.lates.size() == 1 && early.lates[0].n == 6);
    std::printf("ok\n");
    return 0;
}
)CPP";

} // namespace

TEST(RecursiveRecords, LpWrapperBoxesTheCyclesAndRoundTrips)
{
    const generated::Wrapper w = generated::wrapperFor(kContract, ApiStyle::Lp);
    for (const char* decl : {"struct Node;\n    struct Loop;\n    struct Pair;\n    struct Tree;\n",
                             "std::vector<Node> children{};", "std::map<std::string, Node> byName{};",
                             "std::shared_ptr<Node> parent{};", "std::shared_ptr<Loop> next{};",
                             "std::shared_ptr<Tree> left{};", "std::shared_ptr<Tree> right{};",
                             "std::shared_ptr<Pair> pair{};", "std::optional<int64_t> leaf{};",
                             "#include <memory>\n"})
        EXPECT_TRUE(w.header.contains(decl)) << decl << "\n" << w.header.toStdString();
    // Late is held by value, so it is defined first.
    EXPECT_LT(w.header.indexOf("struct Late {"), w.header.indexOf("struct Early {")) << w.header.toStdString();
    EXPECT_TRUE(w.source.contains("if (v.parent) __j[\"parent\"] = recToWire_Node((*v.parent));"))
        << w.source.toStdString();
    EXPECT_TRUE(w.source.contains(
        "if (w.contains(\"next\") && !w.at(\"next\").is_null()) __out.next = "
        "std::make_shared<Probe::Loop>(recFromWire_Loop(w.at(\"next\")));")) << w.source.toStdString();

    const generated::Result r = generated::runs(
        {{"probe_api.h", w.header}, {"probe_api.cpp", w.source}, {"main.cpp", kRoundTrip},
         {"lp_stub.cpp", generated::lpEchoStub()}},
        {"probe_api.cpp", "main.cpp", "lp_stub.cpp"});
    EXPECT_TRUE(r.ok) << r.log.toStdString();
    EXPECT_TRUE(r.log.contains("ok")) << r.log.toStdString();
}

// On Qt an optional field is a QVariant, so only a required cycle needs a box,
// and Pair/Tree is just an ordering.
TEST(RecursiveRecords, QtWrapperBoxesOnlyRequiredCycles)
{
    const generated::Wrapper w = generated::wrapperFor(kContract, ApiStyle::Qt);
    for (const char* decl : {"QList<Node> children{};", "QMap<QString, Node> byName{};",
                             "QVariant parent{};", "std::shared_ptr<Loop> next{};", "Tree left{};",
                             "QVariant pair{};", "#include <memory>\n"})
        EXPECT_TRUE(w.header.contains(decl)) << decl << "\n" << w.header.toStdString();
    EXPECT_FALSE(w.header.contains("struct Node;")) << w.header.toStdString();
    EXPECT_LT(w.header.indexOf("struct Tree {"), w.header.indexOf("struct Pair {")) << w.header.toStdString();
    EXPECT_TRUE(w.source.contains(
        "if (v.next) __m.insert(QStringLiteral(\"next\"), recToWire_Loop((*v.next)));"))
        << w.source.toStdString();
    EXPECT_TRUE(w.source.contains(
        "if (!__m.value(QStringLiteral(\"next\")).isNull()) __out.next = std::make_shared<Probe::Loop>("))
        << w.source.toStdString();
}

// Records in declaration order that hold nothing recursively keep their exact
// historical shape: no forward declarations, no <memory>, no boxes.
TEST(RecursiveRecords, OtherRecordsAreUnaffected)
{
    const char* plain = R"LIDL(
module probe {
  version "1.0.0"
  type Late {
    n: int
  }
  type Early {
    late: Late
    ? maybe: Late
    lates: [Late]
  }
  method get() -> Early
}
)LIDL";
    for (ApiStyle style : {ApiStyle::Lp, ApiStyle::Qt}) {
        const generated::Wrapper w = generated::wrapperFor(plain, style);
        EXPECT_TRUE(w.header.contains("    // Record types declared by the contract.\n    struct Late {"))
            << w.header.toStdString();
        EXPECT_FALSE(w.header.contains("shared_ptr")) << w.header.toStdString();
        EXPECT_FALSE(w.header.contains("#include <memory>")) << w.header.toStdString();
    }
}
