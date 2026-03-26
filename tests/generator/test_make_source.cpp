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

TEST(MakeSourceTest, QVariantListReturn)
{
    QJsonArray methods;
    methods.append(makeMethod("getItems", "QVariantList", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("return _result.toList()"));
}

TEST(MakeSourceTest, QVariantMapReturn)
{
    QJsonArray methods;
    methods.append(makeMethod("getData", "QVariantMap", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("return _result.toMap()"));
}

TEST(MakeSourceTest, QVariantListAsync)
{
    QJsonArray methods;
    methods.append(makeMethod("getItems", "QVariantList", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("Mod::getItemsAsync("));
    EXPECT_TRUE(src.contains("std::function<void(QVariantList)> callback"));
    EXPECT_TRUE(src.contains("QVariantList()"));
}

TEST(MakeSourceTest, QVariantMapAsync)
{
    QJsonArray methods;
    methods.append(makeMethod("getData", "QVariantMap", 0));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("Mod::getDataAsync("));
    EXPECT_TRUE(src.contains("std::function<void(QVariantMap)> callback"));
    EXPECT_TRUE(src.contains("QVariantMap()"));
}

TEST(MakeSourceTest, QVariantListConstRefParam)
{
    QJsonArray methods;
    {
        QJsonObject m;
        m["name"] = "process";
        m["returnType"] = "void";
        m["isInvokable"] = true;
        QJsonArray params;
        QJsonObject p;
        p["type"] = "QVariantList";
        p["name"] = "items";
        params.append(p);
        m["parameters"] = params;
        methods.append(m);
    }
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("const QVariantList& items"));
}

TEST(MakeSourceTest, QVariantMapConstRefParam)
{
    QJsonArray methods;
    {
        QJsonObject m;
        m["name"] = "update";
        m["returnType"] = "void";
        m["isInvokable"] = true;
        QJsonArray params;
        QJsonObject p;
        p["type"] = "QVariantMap";
        p["name"] = "data";
        params.append(p);
        m["parameters"] = params;
        methods.append(m);
    }
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("const QVariantMap& data"));
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
