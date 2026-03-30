#include <gtest/gtest.h>
#include "impl_header_parser.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

// Helper: get path to test fixtures relative to this test file
static QString fixturesDir()
{
    // The test binary runs from the build dir; fixtures are in the source tree.
    // We use the FIXTURES_DIR define set by CMake.
#ifdef FIXTURES_DIR
    return QString(FIXTURES_DIR);
#else
    return QDir::currentPath() + "/fixtures";
#endif
}

class ImplHeaderParserTest : public ::testing::Test {
protected:
    QString errOutput;
    QTextStream err{&errOutput};
};

// ---------------------------------------------------------------------------
// Basic parsing
// ---------------------------------------------------------------------------

TEST_F(ImplHeaderParserTest, ParsesSampleImpl)
{
    auto r = parseImplHeader(
        fixturesDir() + "/sample_impl.h",
        "SampleModuleImpl",
        fixturesDir() + "/sample_metadata.json",
        err);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();

    // Metadata from JSON
    EXPECT_EQ(r.module.name, "sample_module");
    EXPECT_EQ(r.module.version, "1.2.3");
    EXPECT_EQ(r.module.description, "A sample module for testing");
    EXPECT_EQ(r.module.category, "testing");
    ASSERT_EQ(r.module.depends.size(), 2);
    EXPECT_EQ(r.module.depends[0], "dep_a");

    // Methods — should find all public methods, skip ctor/dtor/private
    EXPECT_GE(r.module.methods.size(), 10);
}

TEST_F(ImplHeaderParserTest, MethodTypes)
{
    auto r = parseImplHeader(
        fixturesDir() + "/sample_impl.h",
        "SampleModuleImpl",
        fixturesDir() + "/sample_metadata.json",
        err);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();

    // Find specific methods and check their types
    auto findMethod = [&](const QString& name) -> const MethodDecl* {
        for (const auto& m : r.module.methods)
            if (m.name == name) return &m;
        return nullptr;
    };

    // std::string greet(const std::string& name) → tstr
    auto greet = findMethod("greet");
    ASSERT_NE(greet, nullptr);
    EXPECT_EQ(greet->returnType.name, "tstr");
    ASSERT_EQ(greet->params.size(), 1);
    EXPECT_EQ(greet->params[0].name, "name");
    EXPECT_EQ(greet->params[0].type.name, "tstr");

    // bool isValid(const std::string& input) → bool
    auto isValid = findMethod("isValid");
    ASSERT_NE(isValid, nullptr);
    EXPECT_EQ(isValid->returnType.name, "bool");

    // int64_t getCount() → int
    auto getCount = findMethod("getCount");
    ASSERT_NE(getCount, nullptr);
    EXPECT_EQ(getCount->returnType.name, "int");
    EXPECT_TRUE(getCount->params.isEmpty());

    // uint64_t getSize() → uint
    auto getSize = findMethod("getSize");
    ASSERT_NE(getSize, nullptr);
    EXPECT_EQ(getSize->returnType.name, "uint");

    // double getScore() → float64
    auto getScore = findMethod("getScore");
    ASSERT_NE(getScore, nullptr);
    EXPECT_EQ(getScore->returnType.name, "float64");

    // void doNothing() → void
    auto doNothing = findMethod("doNothing");
    ASSERT_NE(doNothing, nullptr);
    EXPECT_EQ(doNothing->returnType.name, "void");

    // std::vector<std::string> getNames() → [tstr]
    auto getNames = findMethod("getNames");
    ASSERT_NE(getNames, nullptr);
    EXPECT_EQ(getNames->returnType.kind, TypeExpr::Array);
    EXPECT_EQ(getNames->returnType.elements[0].name, "tstr");

    // std::vector<uint8_t> getData() → bstr
    auto getData = findMethod("getData");
    ASSERT_NE(getData, nullptr);
    EXPECT_EQ(getData->returnType.name, "bstr");

    // std::vector<int64_t> getIds() → [int]
    auto getIds = findMethod("getIds");
    ASSERT_NE(getIds, nullptr);
    EXPECT_EQ(getIds->returnType.kind, TypeExpr::Array);
    EXPECT_EQ(getIds->returnType.elements[0].name, "int");

    // std::string combine(const std::string& a, const std::string& b, int64_t count)
    auto combine = findMethod("combine");
    ASSERT_NE(combine, nullptr);
    EXPECT_EQ(combine->returnType.name, "tstr");
    ASSERT_EQ(combine->params.size(), 3);
    EXPECT_EQ(combine->params[0].name, "a");
    EXPECT_EQ(combine->params[1].name, "b");
    EXPECT_EQ(combine->params[2].name, "count");
    EXPECT_EQ(combine->params[2].type.name, "int");
}

TEST_F(ImplHeaderParserTest, SkipsPrivateMethods)
{
    auto r = parseImplHeader(
        fixturesDir() + "/sample_impl.h",
        "SampleModuleImpl",
        fixturesDir() + "/sample_metadata.json",
        err);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();

    for (const auto& m : r.module.methods) {
        EXPECT_NE(m.name, "internalHelper") << "Private method should not be parsed";
    }
}

// ---------------------------------------------------------------------------
// Empty class
// ---------------------------------------------------------------------------

TEST_F(ImplHeaderParserTest, EmptyClass)
{
    auto r = parseImplHeader(
        fixturesDir() + "/empty_class_impl.h",
        "EmptyClassImpl",
        fixturesDir() + "/empty_metadata.json",
        err);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();
    EXPECT_TRUE(r.module.methods.isEmpty());
    // Should have a warning in err output
    EXPECT_TRUE(errOutput.contains("Warning"));
}

// ---------------------------------------------------------------------------
// Complex class with access specifier changes
// ---------------------------------------------------------------------------

TEST_F(ImplHeaderParserTest, ComplexAccessSpecifiers)
{
    auto r = parseImplHeader(
        fixturesDir() + "/complex_impl.h",
        "ComplexModuleImpl",
        fixturesDir() + "/empty_metadata.json",
        err);
    ASSERT_FALSE(r.hasError()) << r.error.toStdString();

    auto findMethod = [&](const QString& name) -> const MethodDecl* {
        for (const auto& m : r.module.methods)
            if (m.name == name) return &m;
        return nullptr;
    };

    // First public section
    EXPECT_NE(findMethod("firstMethod"), nullptr);
    // Second public section (after protected)
    EXPECT_NE(findMethod("secondMethod"), nullptr);
    EXPECT_NE(findMethod("thirdMethod"), nullptr);
    // Protected method should be skipped
    EXPECT_EQ(findMethod("protectedHelper"), nullptr);
    // Private method should be skipped
    EXPECT_EQ(findMethod("privateHelper"), nullptr);
}

// ---------------------------------------------------------------------------
// Error cases
// ---------------------------------------------------------------------------

TEST_F(ImplHeaderParserTest, MissingHeaderFile)
{
    auto r = parseImplHeader(
        "/nonexistent/path.h",
        "Foo",
        fixturesDir() + "/sample_metadata.json",
        err);
    EXPECT_TRUE(r.hasError());
    EXPECT_TRUE(r.error.contains("Failed to open header"));
}

TEST_F(ImplHeaderParserTest, MissingMetadataFile)
{
    auto r = parseImplHeader(
        fixturesDir() + "/sample_impl.h",
        "SampleModuleImpl",
        "/nonexistent/metadata.json",
        err);
    EXPECT_TRUE(r.hasError());
    EXPECT_TRUE(r.error.contains("Failed to open metadata"));
}

TEST_F(ImplHeaderParserTest, WrongClassName)
{
    auto r = parseImplHeader(
        fixturesDir() + "/sample_impl.h",
        "NonExistentClass",
        fixturesDir() + "/sample_metadata.json",
        err);
    // Not an error per se, but should find zero methods and warn
    ASSERT_FALSE(r.hasError());
    EXPECT_TRUE(r.module.methods.isEmpty());
    EXPECT_TRUE(errOutput.contains("Warning"));
}
