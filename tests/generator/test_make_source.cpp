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
    EXPECT_TRUE(src.contains("m_client->invokeRemoteMethod(\"mod\", \"doStuff\", QVariantList{}, Timeout(), &_err)"));
    EXPECT_TRUE(src.contains("return _result.toInt()"));
}

TEST(MakeSourceTest, OneParam)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "bool", 1));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("m_client->invokeRemoteMethod(\"mod\", \"fn\", QVariantList{QVariant::fromValue(p0)}, Timeout(), &_err)"));
    EXPECT_TRUE(src.contains("return _result.toBool()"));
}

TEST(MakeSourceTest, TwoParams)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "void", 2));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("m_client->invokeRemoteMethod(\"mod\", \"fn\", QVariantList{QVariant::fromValue(p0), QVariant::fromValue(p1)}, Timeout(), &_err)"));
}

TEST(MakeSourceTest, ThreeParams)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "QString", 3));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("QVariant::fromValue(p0), QVariant::fromValue(p1), QVariant::fromValue(p2)"));
    EXPECT_TRUE(src.contains("return _result.toString()"));
}

TEST(MakeSourceTest, FourParams)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "double", 4));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("QVariant::fromValue(p0), QVariant::fromValue(p1), QVariant::fromValue(p2), QVariant::fromValue(p3)"));
    EXPECT_TRUE(src.contains("return _result.toDouble()"));
}

TEST(MakeSourceTest, FiveParams)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "float", 5));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("QVariant::fromValue(p0), QVariant::fromValue(p1), QVariant::fromValue(p2), QVariant::fromValue(p3), QVariant::fromValue(p4)"));
    EXPECT_TRUE(src.contains("return _result.toFloat()"));
}

TEST(MakeSourceTest, MoreThanFiveParamsUsesVariantList)
{
    QJsonArray methods;
    methods.append(makeMethod("fn", "QVariant", 6));
    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("QVariantList{QVariant::fromValue(p0), QVariant::fromValue(p1), QVariant::fromValue(p2), QVariant::fromValue(p3), QVariant::fromValue(p4), QVariant::fromValue(p5)}"));
    EXPECT_TRUE(src.contains("return _result"));
}

// Regression: a QVariantList-typed ([any]/[int]/...) argument must be wrapped as
// ONE element via QVariant::fromValue. A bare `QVariantList{v}` concatenates the
// list's elements into the args list, sending [1,2,3] as three positional args
// — the "typed arrays empty over the Qt path" bug.
TEST(MakeSourceTest, ListArgWrappedAsOneElement)
{
    QJsonObject m;
    m["name"] = "echoList";
    m["returnType"] = "QVariantList";
    m["isInvokable"] = true;
    QJsonObject p;
    p["type"] = "QVariantList";
    p["name"] = "v";
    QJsonArray params;
    params.append(p);
    m["parameters"] = params;
    QJsonArray methods;
    methods.append(m);

    QString src = makeSource("mod", "Mod", "mod.h", methods);
    EXPECT_TRUE(src.contains("invokeRemoteMethod(\"mod\", \"echoList\", QVariantList{QVariant::fromValue(v)}, Timeout(), &_err)"));
    EXPECT_TRUE(src.contains("invokeRemoteMethodAsync(\"mod\", \"echoList\", QVariantList{QVariant::fromValue(v)}"));
    // The bare (spreading) form must not appear.
    EXPECT_FALSE(src.contains("QVariantList{v}"));
}

// Regression: in the Qt-free (lp) wrapper, an `any` (QVariant) return must pass
// the raw json value through, NOT force it to an object. `any` shares the
// LogosMap std type with the `{tstr:any}` map, and forcing `any` to an object
// collapsed every non-object value to `{}` (e.g. a proxy forwarding echoAny
// returned {} for the string "x"). The map keeps its object coercion.
TEST(MakeSourceTest, LpAnyReturnPassesThroughButMapForcesObject)
{
    QJsonObject any;
    any["name"] = "echoAny";
    any["returnType"] = "QVariant";
    any["isInvokable"] = true;
    {
        QJsonObject p; p["type"] = "QVariant"; p["name"] = "v";
        QJsonArray ps; ps.append(p); any["parameters"] = ps;
    }
    QJsonObject mp;
    mp["name"] = "echoMap";
    mp["returnType"] = "QVariantMap";
    mp["isInvokable"] = true;
    {
        QJsonObject p; p["type"] = "QVariantMap"; p["name"] = "v";
        QJsonArray ps; ps.append(p); mp["parameters"] = ps;
    }
    QJsonArray methods;
    methods.append(any);
    methods.append(mp);

    QString src = makeSourceLp("mod", "Mod", "mod.h", methods);
    // `any` return: raw passthrough (return _r;), no is_object coercion.
    EXPECT_TRUE(src.contains("return _r;"));
    // `{tstr:any}` map return: still forced to an object.
    EXPECT_TRUE(src.contains("_r.is_object() ? _r : LogosMap::object()"));
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
