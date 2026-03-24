#include <gtest/gtest.h>
#include "generator_lib.h"

TEST(NormalizeTypeTest, PlainType)
{
    EXPECT_EQ(normalizeType("int"), "int");
}

TEST(NormalizeTypeTest, ConstRef)
{
    EXPECT_EQ(normalizeType("const QString&"), "QString");
}

TEST(NormalizeTypeTest, ConstValue)
{
    EXPECT_EQ(normalizeType("const int"), "int");
}

TEST(NormalizeTypeTest, Pointer)
{
    EXPECT_EQ(normalizeType("QObject*"), "QObject");
}

TEST(NormalizeTypeTest, Reference)
{
    EXPECT_EQ(normalizeType("QString&"), "QString");
}

TEST(NormalizeTypeTest, WithWhitespace)
{
    EXPECT_EQ(normalizeType("  const  QString &  "), "QString");
}

TEST(NormalizeTypeTest, EmptyString)
{
    EXPECT_EQ(normalizeType(""), "");
}

TEST(NormalizeTypeTest, NoQualifiers)
{
    EXPECT_EQ(normalizeType("QVariant"), "QVariant");
}
