#include <gtest/gtest.h>
#include <QCoreApplication>
#include "logos_mock.h"
#include "logos_sdk_c.h"
#include <cstring>

static bool s_called = false;
static int s_result = -1;
static std::string s_message;
static void* s_user_data = nullptr;

static void resetState()
{
    s_called = false;
    s_result = -1;
    s_message.clear();
    s_user_data = nullptr;
}

static void testCCallback(int result, const char* message, void* user_data)
{
    s_called = true;
    s_result = result;
    s_message = message ? message : "";
    s_user_data = user_data;
}

class LogosSdkCTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_mock = new LogosMockSetup();
        resetState();
    }
    void TearDown() override
    {
        logos_sdk_shutdown();
        delete m_mock;
    }
    LogosMockSetup* m_mock = nullptr;

    void processEvents() {
        for (int i = 0; i < 10; ++i)
            QCoreApplication::processEvents();
    }
};

TEST_F(LogosSdkCTest, CallMethodAsync_Success)
{
    m_mock->when("mod", "fn").thenReturn(QVariant("ok"));

    logos_sdk_call_method_async("mod", "fn", "[]", testCCallback, nullptr);
    processEvents();

    EXPECT_TRUE(s_called);
    EXPECT_EQ(s_result, 1);
    EXPECT_TRUE(s_message.find("ok") != std::string::npos);
}

TEST_F(LogosSdkCTest, CallMethodAsync_WithParams)
{
    m_mock->when("mod", "fn").thenReturn(QVariant(42));

    const char* params = R"([{"name":"a","value":"hello","type":"string"}])";
    logos_sdk_call_method_async("mod", "fn", params, testCCallback, nullptr);
    processEvents();

    EXPECT_TRUE(s_called);
    EXPECT_EQ(s_result, 1);
}

TEST_F(LogosSdkCTest, CallMethodAsync_NullPluginName)
{
    logos_sdk_call_method_async(nullptr, "fn", "[]", testCCallback, nullptr);

    EXPECT_TRUE(s_called);
    EXPECT_EQ(s_result, 0);
    EXPECT_TRUE(s_message.find("null") != std::string::npos);
}

TEST_F(LogosSdkCTest, CallMethodAsync_NullMethodName)
{
    logos_sdk_call_method_async("mod", nullptr, "[]", testCCallback, nullptr);

    EXPECT_TRUE(s_called);
    EXPECT_EQ(s_result, 0);
}

TEST_F(LogosSdkCTest, CallMethodAsync_NullCallback)
{
    logos_sdk_call_method_async("mod", "fn", "[]", nullptr, nullptr);
    EXPECT_FALSE(s_called);
}

TEST_F(LogosSdkCTest, CallMethodAsync_InvalidJson)
{
    logos_sdk_call_method_async("mod", "fn", "bad json", testCCallback, nullptr);

    EXPECT_TRUE(s_called);
    EXPECT_EQ(s_result, 0);
    EXPECT_TRUE(s_message.find("JSON parse error") != std::string::npos);
}

TEST_F(LogosSdkCTest, CallMethodAsync_NullParamsDefaultsToEmptyArray)
{
    m_mock->when("mod", "fn").thenReturn(QVariant("ok"));

    logos_sdk_call_method_async("mod", "fn", nullptr, testCCallback, nullptr);
    processEvents();

    EXPECT_TRUE(s_called);
    EXPECT_EQ(s_result, 1);
}

TEST_F(LogosSdkCTest, CallMethodAsync_PassesUserData)
{
    m_mock->when("mod", "fn").thenReturn(QVariant("ok"));

    int data = 999;
    logos_sdk_call_method_async("mod", "fn", "[]", testCCallback, &data);
    processEvents();

    EXPECT_TRUE(s_called);
    EXPECT_EQ(s_user_data, &data);
}

TEST_F(LogosSdkCTest, RegisterEvent_NullParams)
{
    logos_sdk_register_event(nullptr, "evt", testCCallback, nullptr);
    EXPECT_FALSE(s_called);

    logos_sdk_register_event("mod", nullptr, testCCallback, nullptr);
    EXPECT_FALSE(s_called);

    logos_sdk_register_event("mod", "evt", nullptr, nullptr);
    EXPECT_FALSE(s_called);
}

TEST_F(LogosSdkCTest, Shutdown_SafeToCallMultipleTimes)
{
    logos_sdk_shutdown();
    logos_sdk_shutdown();
}

TEST_F(LogosSdkCTest, Shutdown_ThenCallRecreatesClient)
{
    m_mock->when("mod", "fn").thenReturn(QVariant("first"));
    logos_sdk_call_method_async("mod", "fn", "[]", testCCallback, nullptr);
    processEvents();
    EXPECT_EQ(s_result, 1);

    logos_sdk_shutdown();
    resetState();

    m_mock->when("mod", "fn").thenReturn(QVariant("second"));
    logos_sdk_call_method_async("mod", "fn", "[]", testCCallback, nullptr);
    processEvents();
    EXPECT_EQ(s_result, 1);
    EXPECT_TRUE(s_message.find("second") != std::string::npos);
}
