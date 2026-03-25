#include <gtest/gtest.h>
#include <QCoreApplication>
#include "logos_mock.h"
#include "logos_core_client.h"

class LogosCoreClientTest : public ::testing::Test {
protected:
    void SetUp() override { m_mock = new LogosMockSetup(); }
    void TearDown() override { delete m_mock; }
    LogosMockSetup* m_mock = nullptr;

    void processEvents() {
        for (int i = 0; i < 10; ++i)
            QCoreApplication::processEvents();
    }
};

TEST_F(LogosCoreClientTest, CallMethodAsync_SuccessfulCall)
{
    m_mock->when("test_module", "greet").thenReturn(QVariant("hello world"));

    LogosCoreClient client;
    bool called = false;
    bool success = false;
    QString message;

    client.callMethodAsync("test_module", "greet", "[]",
        [&](bool s, const QString& m) { called = true; success = s; message = m; });

    processEvents();

    EXPECT_TRUE(called);
    EXPECT_TRUE(success);
    EXPECT_TRUE(message.contains("hello world"));
}

TEST_F(LogosCoreClientTest, CallMethodAsync_WithJsonParams)
{
    m_mock->when("math", "add").thenReturn(QVariant(30));

    LogosCoreClient client;
    bool called = false;
    bool success = false;

    QString params = R"([{"name":"a","value":"10","type":"int"},{"name":"b","value":"20","type":"int"}])";
    client.callMethodAsync("math", "add", params,
        [&](bool s, const QString&) { called = true; success = s; });

    processEvents();

    EXPECT_TRUE(called);
    EXPECT_TRUE(success);
    EXPECT_TRUE(m_mock->wasCalled("math", "add"));
}

TEST_F(LogosCoreClientTest, CallMethodAsync_InvalidJsonReportsError)
{
    LogosCoreClient client;
    bool called = false;
    bool success = true;
    QString message;

    client.callMethodAsync("mod", "fn", "not valid json",
        [&](bool s, const QString& m) { called = true; success = s; message = m; });

    EXPECT_TRUE(called);
    EXPECT_FALSE(success);
    EXPECT_TRUE(message.contains("JSON parse error"));
}

TEST_F(LogosCoreClientTest, CallMethodAsync_InvalidParamReportsError)
{
    LogosCoreClient client;
    bool called = false;
    bool success = true;
    QString message;

    QString params = R"([{"name":"a","value":"abc","type":"int"}])";
    client.callMethodAsync("mod", "fn", params,
        [&](bool s, const QString& m) { called = true; success = s; message = m; });

    EXPECT_TRUE(called);
    EXPECT_FALSE(success);
    EXPECT_TRUE(message.contains("Invalid parameter"));
}

TEST_F(LogosCoreClientTest, CallMethodAsync_NullCallbackIsNoOp)
{
    LogosCoreClient client;
    client.callMethodAsync("mod", "fn", "[]", nullptr);
}

TEST_F(LogosCoreClientTest, CallMethodAsync_InvalidResultReportsError)
{
    // No expectation set -> mock returns invalid QVariant
    m_mock->when("mod", "other").thenReturn(QVariant(1));

    LogosCoreClient client;
    bool called = false;
    bool success = true;
    QString message;

    client.callMethodAsync("mod", "missing_fn", "[]",
        [&](bool s, const QString& m) { called = true; success = s; message = m; });

    processEvents();

    EXPECT_TRUE(called);
    EXPECT_FALSE(success);
    EXPECT_TRUE(message.contains("invalid result"));
}

TEST_F(LogosCoreClientTest, ClientFor_ReturnsSameClientForSamePlugin)
{
    LogosCoreClient client;
    auto* c1 = client.clientFor("mod");
    auto* c2 = client.clientFor("mod");
    EXPECT_EQ(c1, c2);
}

TEST_F(LogosCoreClientTest, ClientFor_ReturnsDifferentForDifferentPlugins)
{
    LogosCoreClient client;
    auto* c1 = client.clientFor("mod_a");
    auto* c2 = client.clientFor("mod_b");
    EXPECT_NE(c1, c2);
}
