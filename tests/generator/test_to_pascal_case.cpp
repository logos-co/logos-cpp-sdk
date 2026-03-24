#include <gtest/gtest.h>
#include "generator_lib.h"

TEST(ToPascalCaseTest, SnakeCase)
{
    EXPECT_EQ(toPascalCase("my_module"), "MyModule");
}

TEST(ToPascalCaseTest, KebabCase)
{
    EXPECT_EQ(toPascalCase("my-module"), "MyModule");
}

TEST(ToPascalCaseTest, DotSeparated)
{
    EXPECT_EQ(toPascalCase("my.module"), "MyModule");
}

TEST(ToPascalCaseTest, SingleWord)
{
    EXPECT_EQ(toPascalCase("hello"), "Hello");
}

TEST(ToPascalCaseTest, EmptyInput)
{
    EXPECT_EQ(toPascalCase(""), "Module");
}

TEST(ToPascalCaseTest, AllCaps)
{
    EXPECT_EQ(toPascalCase("MY_MODULE"), "MyModule");
}

TEST(ToPascalCaseTest, AlreadyPascal)
{
    EXPECT_EQ(toPascalCase("MyModule"), "Mymodule");
}

TEST(ToPascalCaseTest, MultipleDelimiters)
{
    EXPECT_EQ(toPascalCase("a_b-c.d"), "ABCD");
}

TEST(ToPascalCaseTest, LeadingDelimiters)
{
    EXPECT_EQ(toPascalCase("__test"), "Test");
}

TEST(ToPascalCaseTest, OnlyDelimiters)
{
    EXPECT_EQ(toPascalCase("___"), "Module");
}
