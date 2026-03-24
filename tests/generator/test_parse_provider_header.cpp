#include <gtest/gtest.h>
#include <QTemporaryFile>
#include <QTextStream>
#include "generator_lib.h"

static QString writeTempHeader(const QString& content)
{
    QTemporaryFile* tmp = new QTemporaryFile();
    tmp->setAutoRemove(true);
    tmp->open();
    QTextStream out(tmp);
    out << content;
    out.flush();
    QString path = tmp->fileName();
    // Keep file open so it stays on disk; caller reads it then it auto-removes
    // Actually we need to close so parseProviderHeader can open it
    tmp->close();
    // Keep the object alive by leaking — tests are short-lived
    // Actually QTemporaryFile removes on close by default, so disable auto-remove
    // and manage manually. Let's use a simpler approach:
    return path;
}

class ParseProviderHeaderTest : public ::testing::Test {
protected:
    void writeFile(const QString& content)
    {
        m_file.setAutoRemove(true);
        m_file.open();
        QTextStream out(&m_file);
        out << content;
        out.flush();
        // Don't close — keep the file on disk
    }

    QVector<ParsedMethod> parse()
    {
        QString path = m_file.fileName();
        QString errStr;
        QTextStream err(&errStr);
        return parseProviderHeader(path, err);
    }

    QString parseError()
    {
        QString path = m_file.fileName();
        QString errStr;
        QTextStream err(&errStr);
        parseProviderHeader(path, err);
        return errStr;
    }

    QTemporaryFile m_file;
};

TEST_F(ParseProviderHeaderTest, SimpleMethod)
{
    writeFile("    LOGOS_METHOD void doStuff();\n");
    auto methods = parse();
    ASSERT_EQ(methods.size(), 1);
    EXPECT_EQ(methods[0].returnType, "void");
    EXPECT_EQ(methods[0].name, "doStuff");
    EXPECT_TRUE(methods[0].params.isEmpty());
}

TEST_F(ParseProviderHeaderTest, MethodWithParams)
{
    writeFile("    LOGOS_METHOD int add(int a, int b);\n");
    auto methods = parse();
    ASSERT_EQ(methods.size(), 1);
    EXPECT_EQ(methods[0].returnType, "int");
    EXPECT_EQ(methods[0].name, "add");
    ASSERT_EQ(methods[0].params.size(), 2);
    EXPECT_EQ(methods[0].params[0].first, "int");
    EXPECT_EQ(methods[0].params[0].second, "a");
    EXPECT_EQ(methods[0].params[1].first, "int");
    EXPECT_EQ(methods[0].params[1].second, "b");
}

TEST_F(ParseProviderHeaderTest, ConstRefParam)
{
    writeFile("    LOGOS_METHOD QString greet(const QString& name);\n");
    auto methods = parse();
    ASSERT_EQ(methods.size(), 1);
    ASSERT_EQ(methods[0].params.size(), 1);
    EXPECT_EQ(methods[0].params[0].first, "QString");
    EXPECT_EQ(methods[0].params[0].second, "name");
}

TEST_F(ParseProviderHeaderTest, DefaultArgStripped)
{
    writeFile("    LOGOS_METHOD void setVal(int x = 10);\n");
    auto methods = parse();
    ASSERT_EQ(methods.size(), 1);
    ASSERT_EQ(methods[0].params.size(), 1);
    EXPECT_EQ(methods[0].params[0].first, "int");
    EXPECT_EQ(methods[0].params[0].second, "x");
}

TEST_F(ParseProviderHeaderTest, MultipleMethodsParsed)
{
    writeFile(
        "class Foo : public LogosProviderBase {\n"
        "    LOGOS_METHOD void a();\n"
        "    LOGOS_METHOD int b(int x);\n"
        "    LOGOS_METHOD QString c(const QString& s, bool flag);\n"
        "};\n"
    );
    auto methods = parse();
    EXPECT_EQ(methods.size(), 3);
}

TEST_F(ParseProviderHeaderTest, NonLogosMethods_Ignored)
{
    writeFile(
        "void normalFunction();\n"
        "    LOGOS_METHOD void tracked();\n"
        "int other(int x);\n"
    );
    auto methods = parse();
    EXPECT_EQ(methods.size(), 1);
    EXPECT_EQ(methods[0].name, "tracked");
}

TEST_F(ParseProviderHeaderTest, MissingFileReturnsEmpty)
{
    QString errStr;
    QTextStream err(&errStr);
    auto methods = parseProviderHeader("/nonexistent/path.h", err);
    EXPECT_TRUE(methods.isEmpty());
    EXPECT_TRUE(errStr.contains("Cannot open"));
}

TEST_F(ParseProviderHeaderTest, FallbackParamNames)
{
    writeFile("    LOGOS_METHOD void fn(int, QString);\n");
    auto methods = parse();
    ASSERT_EQ(methods.size(), 1);
    // Single-token params get fallback names
    ASSERT_EQ(methods[0].params.size(), 2);
    EXPECT_EQ(methods[0].params[0].second, "arg0");
    EXPECT_EQ(methods[0].params[1].second, "arg1");
}
