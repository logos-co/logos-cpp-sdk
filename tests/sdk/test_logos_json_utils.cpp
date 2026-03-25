#include <gtest/gtest.h>
#include "logos_json_utils.h"
#include <QJsonObject>

// =============================================================================
// jsonParamToVariant tests
// =============================================================================

TEST(LogosJsonUtilsTest, ConvertsStringType)
{
    QJsonObject p;
    p["name"] = "arg0"; p["value"] = "hello"; p["type"] = "string";
    QVariant v = LogosJsonUtils::jsonParamToVariant(p);
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.toString(), "hello");
}

TEST(LogosJsonUtilsTest, ConvertsQStringType)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "qt"; p["type"] = "QString";
    EXPECT_EQ(LogosJsonUtils::jsonParamToVariant(p).toString(), "qt");
}

TEST(LogosJsonUtilsTest, ConvertsIntType)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "42"; p["type"] = "int";
    QVariant v = LogosJsonUtils::jsonParamToVariant(p);
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.toInt(), 42);
}

TEST(LogosJsonUtilsTest, ConvertsIntegerType)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "99"; p["type"] = "integer";
    EXPECT_EQ(LogosJsonUtils::jsonParamToVariant(p).toInt(), 99);
}

TEST(LogosJsonUtilsTest, ConvertsBoolTrue)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "true"; p["type"] = "bool";
    EXPECT_TRUE(LogosJsonUtils::jsonParamToVariant(p).toBool());

    p["value"] = "1";
    EXPECT_TRUE(LogosJsonUtils::jsonParamToVariant(p).toBool());
}

TEST(LogosJsonUtilsTest, ConvertsBoolFalse)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "false"; p["type"] = "bool";
    EXPECT_FALSE(LogosJsonUtils::jsonParamToVariant(p).toBool());

    p["value"] = "0";
    EXPECT_FALSE(LogosJsonUtils::jsonParamToVariant(p).toBool());
}

TEST(LogosJsonUtilsTest, ConvertsBooleanType)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "true"; p["type"] = "boolean";
    EXPECT_TRUE(LogosJsonUtils::jsonParamToVariant(p).toBool());
}

TEST(LogosJsonUtilsTest, ConvertsDoubleType)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "3.14"; p["type"] = "double";
    EXPECT_NEAR(LogosJsonUtils::jsonParamToVariant(p).toDouble(), 3.14, 0.001);
}

TEST(LogosJsonUtilsTest, ConvertsFloatType)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "2.718"; p["type"] = "float";
    EXPECT_NEAR(LogosJsonUtils::jsonParamToVariant(p).toDouble(), 2.718, 0.001);
}

TEST(LogosJsonUtilsTest, InvalidIntReturnsInvalid)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "abc"; p["type"] = "int";
    EXPECT_FALSE(LogosJsonUtils::jsonParamToVariant(p).isValid());
}

TEST(LogosJsonUtilsTest, InvalidBoolReturnsInvalid)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "maybe"; p["type"] = "bool";
    EXPECT_FALSE(LogosJsonUtils::jsonParamToVariant(p).isValid());
}

TEST(LogosJsonUtilsTest, InvalidDoubleReturnsInvalid)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "xyz"; p["type"] = "double";
    EXPECT_FALSE(LogosJsonUtils::jsonParamToVariant(p).isValid());
}

TEST(LogosJsonUtilsTest, UnknownTypeTreatedAsString)
{
    QJsonObject p;
    p["name"] = "a"; p["value"] = "data"; p["type"] = "custom_thing";
    QVariant v = LogosJsonUtils::jsonParamToVariant(p);
    EXPECT_TRUE(v.isValid());
    EXPECT_EQ(v.toString(), "data");
}

// =============================================================================
// parseMethodParams tests
// =============================================================================

TEST(LogosJsonUtilsTest, ParseMixedParams)
{
    QString json = R"([
        {"name":"arg0","value":"hello","type":"string"},
        {"name":"arg1","value":"42","type":"int"},
        {"name":"arg2","value":"true","type":"bool"},
        {"name":"arg3","value":"3.14","type":"double"}
    ])";
    bool ok = false;
    QString err;
    QVariantList args = LogosJsonUtils::parseMethodParams(json, &ok, &err);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(err.isEmpty());
    ASSERT_EQ(args.size(), 4);
    EXPECT_EQ(args[0].toString(), "hello");
    EXPECT_EQ(args[1].toInt(), 42);
    EXPECT_TRUE(args[2].toBool());
    EXPECT_NEAR(args[3].toDouble(), 3.14, 0.001);
}

TEST(LogosJsonUtilsTest, ParseEmptyArray)
{
    bool ok = false;
    QVariantList args = LogosJsonUtils::parseMethodParams("[]", &ok);
    EXPECT_TRUE(ok);
    EXPECT_EQ(args.size(), 0);
}

TEST(LogosJsonUtilsTest, ParseInvalidJson)
{
    bool ok = true;
    QString err;
    QVariantList args = LogosJsonUtils::parseMethodParams("not json", &ok, &err);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(err.contains("JSON parse error"));
    EXPECT_EQ(args.size(), 0);
}

TEST(LogosJsonUtilsTest, ParseInvalidParamValue)
{
    QString json = R"([{"name":"a","value":"abc","type":"int"}])";
    bool ok = true;
    QString err;
    QVariantList args = LogosJsonUtils::parseMethodParams(json, &ok, &err);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(err.contains("Invalid parameter"));
}

TEST(LogosJsonUtilsTest, ParseSingleString)
{
    QString json = R"([{"name":"arg0","value":"test","type":"string"}])";
    bool ok = false;
    QVariantList args = LogosJsonUtils::parseMethodParams(json, &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(args.size(), 1);
    EXPECT_EQ(args[0].toString(), "test");
}

TEST(LogosJsonUtilsTest, ParseNullOkPointer)
{
    QVariantList args = LogosJsonUtils::parseMethodParams("[]");
    EXPECT_EQ(args.size(), 0);
}

// =============================================================================
// variantToJsonString tests
// =============================================================================

TEST(LogosJsonUtilsTest, VariantToJson_Invalid)
{
    EXPECT_EQ(LogosJsonUtils::variantToJsonString(QVariant()), "null");
}

TEST(LogosJsonUtilsTest, VariantToJson_String)
{
    EXPECT_EQ(LogosJsonUtils::variantToJsonString(QVariant("hello")), "hello");
}

TEST(LogosJsonUtilsTest, VariantToJson_Int)
{
    EXPECT_EQ(LogosJsonUtils::variantToJsonString(QVariant(42)), "42");
}

TEST(LogosJsonUtilsTest, VariantToJson_Bool)
{
    EXPECT_EQ(LogosJsonUtils::variantToJsonString(QVariant(true)), "true");
    EXPECT_EQ(LogosJsonUtils::variantToJsonString(QVariant(false)), "false");
}

TEST(LogosJsonUtilsTest, VariantToJson_Double)
{
    QString result = LogosJsonUtils::variantToJsonString(QVariant(3.14));
    EXPECT_TRUE(result.startsWith("3.14"));
}

TEST(LogosJsonUtilsTest, VariantToJson_Map)
{
    QVariantMap map;
    map["key"] = "value";
    QString result = LogosJsonUtils::variantToJsonString(QVariant(map));
    EXPECT_TRUE(result.contains("key"));
    EXPECT_TRUE(result.contains("value"));
}

// =============================================================================
// formatEventJson tests
// =============================================================================

TEST(LogosJsonUtilsTest, FormatEventJson_Basic)
{
    QVariantList data;
    data << QVariant("hello") << QVariant(42);
    QString result = LogosJsonUtils::formatEventJson("myEvent", data);
    EXPECT_EQ(result, R"({"event":"myEvent","data":["hello","42"]})");
}

TEST(LogosJsonUtilsTest, FormatEventJson_Empty)
{
    QString result = LogosJsonUtils::formatEventJson("evt", {});
    EXPECT_EQ(result, R"({"event":"evt","data":[]})");
}
