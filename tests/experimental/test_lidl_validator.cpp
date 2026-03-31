#include <gtest/gtest.h>
#include "lidl_validator.h"

static ModuleDecl makeModule(const QString& name)
{
    ModuleDecl m;
    m.name = name;
    return m;
}

TEST(LidlValidator, ValidEmptyModule)
{
    auto r = lidlValidate(makeModule("test"));
    EXPECT_FALSE(r.hasErrors());
}

TEST(LidlValidator, EmptyModuleName)
{
    auto r = lidlValidate(makeModule(""));
    EXPECT_TRUE(r.hasErrors());
    EXPECT_TRUE(r.errors[0].contains("empty"));
}

TEST(LidlValidator, DuplicateMethodNames)
{
    ModuleDecl m = makeModule("test");
    MethodDecl md;
    md.name = "doThing";
    md.returnType = { TypeExpr::Primitive, "tstr", {} };
    m.methods.append(md);
    m.methods.append(md);

    auto r = lidlValidate(m);
    EXPECT_TRUE(r.hasErrors());
    EXPECT_TRUE(r.errors[0].contains("Duplicate method"));
}

TEST(LidlValidator, DuplicateEventNames)
{
    ModuleDecl m = makeModule("test");
    EventDecl ed;
    ed.name = "onUpdate";
    m.events.append(ed);
    m.events.append(ed);

    auto r = lidlValidate(m);
    EXPECT_TRUE(r.hasErrors());
    EXPECT_TRUE(r.errors[0].contains("Duplicate event"));
}

TEST(LidlValidator, DuplicateTypeNames)
{
    ModuleDecl m = makeModule("test");
    TypeDecl td;
    td.name = "MyType";
    m.types.append(td);
    m.types.append(td);

    auto r = lidlValidate(m);
    EXPECT_TRUE(r.hasErrors());
    EXPECT_TRUE(r.errors[0].contains("Duplicate type"));
}

TEST(LidlValidator, TypeShadowsBuiltin)
{
    ModuleDecl m = makeModule("test");
    TypeDecl td;
    td.name = "tstr";
    m.types.append(td);

    auto r = lidlValidate(m);
    EXPECT_TRUE(r.hasErrors());
    EXPECT_TRUE(r.errors[0].contains("shadows"));
}

TEST(LidlValidator, UnknownNamedType)
{
    ModuleDecl m = makeModule("test");
    MethodDecl md;
    md.name = "foo";
    md.returnType = { TypeExpr::Named, "NonExistentType", {} };
    m.methods.append(md);

    auto r = lidlValidate(m);
    EXPECT_TRUE(r.hasErrors());
    EXPECT_TRUE(r.errors[0].contains("Unknown type"));
}

TEST(LidlValidator, ValidNamedType)
{
    ModuleDecl m = makeModule("test");
    TypeDecl td;
    td.name = "MyStruct";
    m.types.append(td);

    MethodDecl md;
    md.name = "foo";
    md.returnType = { TypeExpr::Named, "MyStruct", {} };
    m.methods.append(md);

    auto r = lidlValidate(m);
    EXPECT_FALSE(r.hasErrors());
}

TEST(LidlValidator, DuplicateParamNames)
{
    ModuleDecl m = makeModule("test");
    MethodDecl md;
    md.name = "foo";
    md.returnType = { TypeExpr::Primitive, "tstr", {} };
    ParamDecl p;
    p.name = "arg";
    p.type = { TypeExpr::Primitive, "tstr", {} };
    md.params.append(p);
    md.params.append(p);
    m.methods.append(md);

    auto r = lidlValidate(m);
    EXPECT_TRUE(r.hasErrors());
    EXPECT_TRUE(r.errors[0].contains("Duplicate parameter"));
}

TEST(LidlValidator, NestedArrayTypeValid)
{
    ModuleDecl m = makeModule("test");
    MethodDecl md;
    md.name = "foo";
    TypeExpr elem = { TypeExpr::Primitive, "tstr", {} };
    md.returnType = { TypeExpr::Array, "", { elem } };
    m.methods.append(md);

    auto r = lidlValidate(m);
    EXPECT_FALSE(r.hasErrors());
}

TEST(LidlValidator, MapWithUnknownValueType)
{
    ModuleDecl m = makeModule("test");
    MethodDecl md;
    md.name = "foo";
    TypeExpr key = { TypeExpr::Primitive, "tstr", {} };
    TypeExpr val = { TypeExpr::Named, "Missing", {} };
    md.returnType = { TypeExpr::Map, "", { key, val } };
    m.methods.append(md);

    auto r = lidlValidate(m);
    EXPECT_TRUE(r.hasErrors());
}
