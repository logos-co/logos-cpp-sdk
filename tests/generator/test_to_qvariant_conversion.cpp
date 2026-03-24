#include <gtest/gtest.h>
#include "generator_lib.h"

TEST(ToQVariantConversionTest, Int)
{
    EXPECT_EQ(toQVariantConversion("int", "v"), "v.toInt()");
}

TEST(ToQVariantConversionTest, Bool)
{
    EXPECT_EQ(toQVariantConversion("bool", "v"), "v.toBool()");
}

TEST(ToQVariantConversionTest, Double)
{
    EXPECT_EQ(toQVariantConversion("double", "v"), "v.toDouble()");
}

TEST(ToQVariantConversionTest, Float)
{
    EXPECT_EQ(toQVariantConversion("float", "v"), "v.toFloat()");
}

TEST(ToQVariantConversionTest, QString)
{
    EXPECT_EQ(toQVariantConversion("QString", "v"), "v.toString()");
}

TEST(ToQVariantConversionTest, QStringList)
{
    EXPECT_EQ(toQVariantConversion("QStringList", "v"), "v.toStringList()");
}

TEST(ToQVariantConversionTest, QJsonArray)
{
    EXPECT_EQ(toQVariantConversion("QJsonArray", "arg"), "qvariant_cast<QJsonArray>(arg)");
}

TEST(ToQVariantConversionTest, QVariantPassthrough)
{
    EXPECT_EQ(toQVariantConversion("QVariant", "v"), "v");
}

TEST(ToQVariantConversionTest, LogosResult)
{
    EXPECT_EQ(toQVariantConversion("LogosResult", "v"), "v.value<LogosResult>()");
}

TEST(ToQVariantConversionTest, UnknownFallsBackToString)
{
    EXPECT_EQ(toQVariantConversion("SomeCustomType", "x"), "x.toString()");
}
