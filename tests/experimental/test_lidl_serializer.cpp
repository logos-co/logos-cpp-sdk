#include <gtest/gtest.h>
#include "lidl_serializer.h"
#include "lidl_parser.h"

TEST(LidlSerializer, EmptyModule)
{
    ModuleDecl m;
    m.name = "test";
    QString out = lidlSerialize(m);
    EXPECT_TRUE(out.contains("module test {"));
    EXPECT_TRUE(out.contains("depends []\n"));
    EXPECT_TRUE(out.contains("}\n"));
}

TEST(LidlSerializer, WithMetadata)
{
    ModuleDecl m;
    m.name = "my_mod";
    m.version = "1.0.0";
    m.description = "My module";
    m.category = "testing";
    m.depends << "dep_a" << "dep_b";
    QString out = lidlSerialize(m);
    EXPECT_TRUE(out.contains("version \"1.0.0\""));
    EXPECT_TRUE(out.contains("description \"My module\""));
    EXPECT_TRUE(out.contains("category \"testing\""));
    EXPECT_TRUE(out.contains("depends [dep_a, dep_b]"));
}

TEST(LidlSerializer, WithMethod)
{
    ModuleDecl m;
    m.name = "test";
    MethodDecl md;
    md.name = "greet";
    md.returnType = { TypeExpr::Primitive, "tstr", {} };
    ParamDecl p;
    p.name = "name";
    p.type = { TypeExpr::Primitive, "tstr", {} };
    md.params.append(p);
    m.methods.append(md);
    QString out = lidlSerialize(m);
    EXPECT_TRUE(out.contains("method greet(name: tstr) -> tstr"));
}

TEST(LidlSerializer, WithEvent)
{
    ModuleDecl m;
    m.name = "test";
    EventDecl ed;
    ed.name = "onUpdate";
    ParamDecl p;
    p.name = "data";
    p.type = { TypeExpr::Primitive, "bstr", {} };
    ed.params.append(p);
    m.events.append(ed);
    QString out = lidlSerialize(m);
    EXPECT_TRUE(out.contains("event onUpdate(data: bstr)"));
}

TEST(LidlSerializer, ArrayType)
{
    ModuleDecl m;
    m.name = "test";
    MethodDecl md;
    md.name = "get";
    TypeExpr elem = { TypeExpr::Primitive, "tstr", {} };
    md.returnType = { TypeExpr::Array, "", { elem } };
    m.methods.append(md);
    QString out = lidlSerialize(m);
    EXPECT_TRUE(out.contains("-> [tstr]"));
}

TEST(LidlSerializer, MapType)
{
    ModuleDecl m;
    m.name = "test";
    MethodDecl md;
    md.name = "get";
    TypeExpr key = { TypeExpr::Primitive, "tstr", {} };
    TypeExpr val = { TypeExpr::Primitive, "int", {} };
    md.returnType = { TypeExpr::Map, "", { key, val } };
    m.methods.append(md);
    QString out = lidlSerialize(m);
    EXPECT_TRUE(out.contains("-> {tstr: int}"));
}

TEST(LidlSerializer, OptionalType)
{
    ModuleDecl m;
    m.name = "test";
    MethodDecl md;
    md.name = "get";
    TypeExpr inner = { TypeExpr::Primitive, "tstr", {} };
    md.returnType = { TypeExpr::Optional, "", { inner } };
    m.methods.append(md);
    QString out = lidlSerialize(m);
    EXPECT_TRUE(out.contains("-> ? tstr"));
}

// ---------------------------------------------------------------------------
// Roundtrip: parse → serialize → parse and compare
// ---------------------------------------------------------------------------

TEST(LidlSerializer, Roundtrip)
{
    QString src = R"(module wallet {
  version "2.0.0"
  description "Wallet"
  category "finance"
  depends [crypto]

  type Account {
    addr: tstr
    balance: uint
  }

  method create(pass: tstr) -> tstr
  method list() -> [tstr]
  method transfer(from: tstr, to: tstr, amt: uint) -> result

  event onTx(hash: tstr)
}
)";
    auto r1 = lidlParse(src);
    ASSERT_FALSE(r1.hasError()) << r1.error.toStdString();

    QString serialized = lidlSerialize(r1.module);
    auto r2 = lidlParse(serialized);
    ASSERT_FALSE(r2.hasError()) << r2.error.toStdString();

    EXPECT_EQ(r1.module, r2.module);
}
