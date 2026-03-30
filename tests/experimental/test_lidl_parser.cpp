#include <gtest/gtest.h>
#include "lidl_parser.h"

// ---------------------------------------------------------------------------
// Minimal valid module
// ---------------------------------------------------------------------------

TEST(LidlParser, MinimalModule)
{
    auto r = lidlParse("module test { depends [] }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    EXPECT_EQ(r.module.name, "test");
    EXPECT_TRUE(r.module.methods.isEmpty());
    EXPECT_TRUE(r.module.events.isEmpty());
    EXPECT_TRUE(r.module.types.isEmpty());
}

// ---------------------------------------------------------------------------
// Metadata
// ---------------------------------------------------------------------------

TEST(LidlParser, AllMetadata)
{
    QString src = R"(module my_mod {
        version "2.0.0"
        description "A test module"
        category "testing"
        depends [foo, bar, baz]
    })";
    auto r = lidlParse(src);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    EXPECT_EQ(r.module.name, "my_mod");
    EXPECT_EQ(r.module.version, "2.0.0");
    EXPECT_EQ(r.module.description, "A test module");
    EXPECT_EQ(r.module.category, "testing");
    ASSERT_EQ(r.module.depends.size(), 3);
    EXPECT_EQ(r.module.depends[0], "foo");
    EXPECT_EQ(r.module.depends[1], "bar");
    EXPECT_EQ(r.module.depends[2], "baz");
}

TEST(LidlParser, EmptyDepends)
{
    auto r = lidlParse("module m { depends [] }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    EXPECT_TRUE(r.module.depends.isEmpty());
}

// ---------------------------------------------------------------------------
// Methods
// ---------------------------------------------------------------------------

TEST(LidlParser, SimpleMethod)
{
    auto r = lidlParse("module m { method greet(name: tstr) -> tstr }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    ASSERT_EQ(r.module.methods.size(), 1);
    auto& md = r.module.methods[0];
    EXPECT_EQ(md.name, "greet");
    EXPECT_EQ(md.returnType.kind, TypeExpr::Primitive);
    EXPECT_EQ(md.returnType.name, "tstr");
    ASSERT_EQ(md.params.size(), 1);
    EXPECT_EQ(md.params[0].name, "name");
    EXPECT_EQ(md.params[0].type.name, "tstr");
}

TEST(LidlParser, MethodNoParams)
{
    auto r = lidlParse("module m { method getCount() -> int }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    ASSERT_EQ(r.module.methods.size(), 1);
    EXPECT_TRUE(r.module.methods[0].params.isEmpty());
    EXPECT_EQ(r.module.methods[0].returnType.name, "int");
}

TEST(LidlParser, MethodMultipleParams)
{
    auto r = lidlParse("module m { method combine(a: tstr, b: tstr, n: int) -> tstr }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    ASSERT_EQ(r.module.methods[0].params.size(), 3);
    EXPECT_EQ(r.module.methods[0].params[0].name, "a");
    EXPECT_EQ(r.module.methods[0].params[1].name, "b");
    EXPECT_EQ(r.module.methods[0].params[2].name, "n");
    EXPECT_EQ(r.module.methods[0].params[2].type.name, "int");
}

TEST(LidlParser, MultipleMethods)
{
    QString src = R"(module m {
        method a() -> bool
        method b(x: int) -> tstr
        method c(y: float64, z: uint) -> bstr
    })";
    auto r = lidlParse(src);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    ASSERT_EQ(r.module.methods.size(), 3);
    EXPECT_EQ(r.module.methods[0].name, "a");
    EXPECT_EQ(r.module.methods[1].name, "b");
    EXPECT_EQ(r.module.methods[2].name, "c");
}

// ---------------------------------------------------------------------------
// Type expressions
// ---------------------------------------------------------------------------

TEST(LidlParser, ArrayType)
{
    auto r = lidlParse("module m { method get() -> [tstr] }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    auto& ret = r.module.methods[0].returnType;
    EXPECT_EQ(ret.kind, TypeExpr::Array);
    ASSERT_EQ(ret.elements.size(), 1);
    EXPECT_EQ(ret.elements[0].name, "tstr");
}

TEST(LidlParser, MapType)
{
    auto r = lidlParse("module m { method get() -> {tstr: int} }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    auto& ret = r.module.methods[0].returnType;
    EXPECT_EQ(ret.kind, TypeExpr::Map);
    ASSERT_EQ(ret.elements.size(), 2);
    EXPECT_EQ(ret.elements[0].name, "tstr");
    EXPECT_EQ(ret.elements[1].name, "int");
}

TEST(LidlParser, OptionalType)
{
    auto r = lidlParse("module m { method get() -> ?tstr }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    auto& ret = r.module.methods[0].returnType;
    EXPECT_EQ(ret.kind, TypeExpr::Optional);
    ASSERT_EQ(ret.elements.size(), 1);
    EXPECT_EQ(ret.elements[0].name, "tstr");
}

TEST(LidlParser, AllPrimitiveTypes)
{
    QString src = R"(module m {
        method a() -> tstr
        method b() -> bstr
        method c() -> int
        method d() -> uint
        method e() -> float64
        method f() -> bool
        method g() -> result
        method h() -> any
    })";
    auto r = lidlParse(src);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    ASSERT_EQ(r.module.methods.size(), 8);
    EXPECT_EQ(r.module.methods[0].returnType.name, "tstr");
    EXPECT_EQ(r.module.methods[1].returnType.name, "bstr");
    EXPECT_EQ(r.module.methods[2].returnType.name, "int");
    EXPECT_EQ(r.module.methods[3].returnType.name, "uint");
    EXPECT_EQ(r.module.methods[4].returnType.name, "float64");
    EXPECT_EQ(r.module.methods[5].returnType.name, "bool");
    EXPECT_EQ(r.module.methods[6].returnType.name, "result");
    EXPECT_EQ(r.module.methods[7].returnType.name, "any");
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

TEST(LidlParser, SimpleEvent)
{
    auto r = lidlParse("module m { event onData(payload: tstr) }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    ASSERT_EQ(r.module.events.size(), 1);
    EXPECT_EQ(r.module.events[0].name, "onData");
    ASSERT_EQ(r.module.events[0].params.size(), 1);
    EXPECT_EQ(r.module.events[0].params[0].name, "payload");
}

TEST(LidlParser, EventNoParams)
{
    auto r = lidlParse("module m { event ping() }");
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    EXPECT_TRUE(r.module.events[0].params.isEmpty());
}

// ---------------------------------------------------------------------------
// Type definitions
// ---------------------------------------------------------------------------

TEST(LidlParser, TypeDefinition)
{
    QString src = R"(module m {
        type Person {
            name: tstr
            age: int
            ? nickname: tstr
        }
    })";
    auto r = lidlParse(src);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    ASSERT_EQ(r.module.types.size(), 1);
    auto& td = r.module.types[0];
    EXPECT_EQ(td.name, "Person");
    ASSERT_EQ(td.fields.size(), 3);
    EXPECT_EQ(td.fields[0].name, "name");
    EXPECT_FALSE(td.fields[0].optional);
    EXPECT_EQ(td.fields[2].name, "nickname");
    EXPECT_TRUE(td.fields[2].optional);
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

TEST(LidlParser, MissingModuleKeyword)
{
    auto r = lidlParse("test { }");
    ASSERT_TRUE(r.hasError());
}

TEST(LidlParser, MissingModuleName)
{
    auto r = lidlParse("module { }");
    ASSERT_TRUE(r.hasError());
}

TEST(LidlParser, MissingOpenBrace)
{
    auto r = lidlParse("module test }");
    ASSERT_TRUE(r.hasError());
}

TEST(LidlParser, MissingCloseBrace)
{
    auto r = lidlParse("module test {");
    ASSERT_TRUE(r.hasError());
}

TEST(LidlParser, MissingArrowInMethod)
{
    auto r = lidlParse("module m { method foo() tstr }");
    ASSERT_TRUE(r.hasError());
}

TEST(LidlParser, TrailingContent)
{
    auto r = lidlParse("module m { } extra_stuff");
    ASSERT_TRUE(r.hasError());
}

TEST(LidlParser, ErrorReportsLineColumn)
{
    auto r = lidlParse("module m {\n  method foo() tstr\n}");
    ASSERT_TRUE(r.hasError());
    EXPECT_EQ(r.errorLine, 2);
    EXPECT_GT(r.errorColumn, 0);
}

// ---------------------------------------------------------------------------
// Full module
// ---------------------------------------------------------------------------

TEST(LidlParser, CompleteModule)
{
    QString src = R"(module wallet {
        version "1.0.0"
        description "Wallet module"
        category "finance"
        depends [crypto, storage]

        type Account {
            address: tstr
            balance: uint
            ? label: tstr
        }

        method createAccount(passphrase: tstr) -> tstr
        method getBalance(address: tstr) -> uint
        method listAccounts() -> [tstr]
        method transfer(from: tstr, to: tstr, amount: uint) -> result

        event onTransfer(from: tstr, to: tstr, amount: uint)
    })";
    auto r = lidlParse(src);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    EXPECT_EQ(r.module.name, "wallet");
    EXPECT_EQ(r.module.version, "1.0.0");
    EXPECT_EQ(r.module.depends.size(), 2);
    EXPECT_EQ(r.module.types.size(), 1);
    EXPECT_EQ(r.module.methods.size(), 4);
    EXPECT_EQ(r.module.events.size(), 1);
}
