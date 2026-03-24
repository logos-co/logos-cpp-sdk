#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonObject>
#include "generator_lib.h"

static QJsonObject makeMethod(const QString& name, const QString& retType, int paramCount)
{
    QJsonObject m;
    m["name"] = name;
    m["returnType"] = retType;
    m["isInvokable"] = true;
    QJsonArray params;
    for (int i = 0; i < paramCount; ++i) {
        QJsonObject p;
        p["type"] = "int";
        p["name"] = QString("p%1").arg(i);
        params.append(p);
    }
    m["parameters"] = params;
    return m;
}

TEST(MakeSourceTest, ContainsInclude)
{
    QString src = makeSource("mod", "Mod", "mod_api.h", QJsonArray());
    EXPECT_TRUE(src.contains("#include \"mod_api.h\""));
}

TEST(MakeSourceTest, ConstructorInitializesClient)
{
    QString src = makeSource("my_mod", "MyMod", "my_mod_api.h", QJsonArray());
    EXPECT_TRUE(src.contains("MyMod::MyMod(LogosAPI* api)"));
    EXPECT_TRUE(src.contains("api->getClient(\"my_mod\")"));
}

TEST(MakeSourceTest, EnsureReplicaMethod)
{
    QString src = makeSource("mod", "Mod", "mod.h", QJsonArray());
    EXPECT_TRUE(src.contains("LogosObject* Mod::ensureReplica()"));
}

TEST(MakeSourceTest, ZeroParams)
{
    QJsonArray methods;
    methods.append(makeMethod("doStuff", "int", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("m_client->invokeRemoteMethod(\"mod\", \"doStuff\")"));
    EXPECT_TRUE(src.contains("return _result.toInt()"));
}

TEST(MakeSourceTest, OneParam)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "bool", 1));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("m_client->invokeRemoteMethod(\"mod\", \"fn\", p0)"));
    EXPECT_TRUE(src.contains("return _result.toBool()"));
}

TEST(MakeSourceTest, TwoParams)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "void", 2));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("m_client->invokeRemoteMethod(\"mod\", \"fn\", p0, p1)"));
}

TEST(MakeSourceTest, ThreeParams)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "QString", 3));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("p0, p1, p2"));
    EXPECT_TRUE(src.contains("return _result.toString()"));
}

TEST(MakeSourceTest, FourParams)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "double", 4));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("p0, p1, p2, p3"));
    EXPECT_TRUE(src.contains("return _result.toDouble()"));
}

TEST(MakeSourceTest, FiveParams)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "float", 5));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("p0, p1, p2, p3, p4"));
    EXPECT_TRUE(src.contains("return _result.toFloat()"));
}

TEST(MakeSourceTest, MoreThanFiveParamsUsesVariantList)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "QVariant", 6));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("QVariantList{p0, p1, p2, p3, p4, p5}"));
    EXPECT_TRUE(src.contains("return _result"));
}

TEST(MakeSourceTest, VoidReturnNoConversion)
{
    QJsonArray methods;
    methods.append(makeMethod("doIt", "void", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_FALSE(src.contains("return _result"));
}

TEST(MakeSourceTest, QStringListReturn)
{
    QJsonArray methods;
    methods.append(makeMethod("getNames", "QStringList", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("return _result.toStringList()"));
}

TEST(MakeSourceTest, QJsonArrayReturn)
{
    QJsonArray methods;
    methods.append(makeMethod("getData", "QJsonArray", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("qvariant_cast<QJsonArray>(_result)"));
}

TEST(MakeSourceTest, LogosResultReturn)
{
    QJsonArray methods;
    methods.append(makeMethod("query", "LogosResult", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("_result.value<LogosResult>()"));
}

TEST(MakeSourceTest, AsyncImplementation)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "int", 1));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("Mod::fnAsync("));
    EXPECT_TRUE(src.contains("invokeRemoteMethodAsync"));
    EXPECT_TRUE(src.contains("callback"));
}

TEST(MakeSourceTest, NonInvokableSkipped)
{
    QJsonArray methods;
    QJsonObject m;
    m["name"] = "hidden";
    m["returnType"] = "void";
    m["isInvokable"] = false;
    m["parameters"] = QJsonArray();
    methods.append(m);

    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_FALSE(src.contains("hidden"));
}
