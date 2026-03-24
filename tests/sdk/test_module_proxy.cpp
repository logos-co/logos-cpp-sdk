#include <gtest/gtest.h>
#include <QtTest/QSignalSpy>
#include <QJsonObject>
#include "logos_mock.h"
#include "logos_api.h"
#include "logos_provider_object.h"
#include "module_proxy.h"

// Minimal provider for proxy testing
class ProxyTestProvider : public LogosProviderBase {
public:
    QString providerName() const override { return "proxy_test"; }
    QString providerVersion() const override { return "1.0.0"; }

    QVariant callMethod(const QString& methodName, const QVariantList& args) override
    {
        lastMethodCalled = methodName;
        lastArgs = args;
        return returnValue;
    }

    QJsonArray getMethods() override
    {
        QJsonArray arr;
        QJsonObject m;
        m["name"] = "testMethod";
        arr.append(m);
        return arr;
    }

    // Expose protected emitEvent for testing
    void testEmitEvent(const QString& name, const QVariantList& data) { emitEvent(name, data); }

    QString lastMethodCalled;
    QVariantList lastArgs;
    QVariant returnValue = QVariant(99);
};

class ModuleProxyTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_mock = new LogosMockSetup();
        m_provider = new ProxyTestProvider();
    }
    void TearDown() override
    {
        delete m_provider;
        delete m_mock;
    }
    LogosMockSetup* m_mock = nullptr;
    ProxyTestProvider* m_provider = nullptr;
};

TEST_F(ModuleProxyTest, CallRemoteMethodDispatchesToProvider)
{
    ModuleProxy proxy(m_provider);
    QVariant r = proxy.callRemoteMethod("token", "myMethod", {QVariant(1)});
    EXPECT_EQ(r.toInt(), 99);
    EXPECT_EQ(m_provider->lastMethodCalled, "myMethod");
    EXPECT_EQ(m_provider->lastArgs.size(), 1);
}

TEST_F(ModuleProxyTest, GetPluginMethodsDispatchesToProvider)
{
    ModuleProxy proxy(m_provider);
    QJsonArray methods = proxy.getPluginMethods();
    EXPECT_EQ(methods.size(), 1);
}

TEST_F(ModuleProxyTest, GetPluginMethodsSpecialCaseInCallRemoteMethod)
{
    ModuleProxy proxy(m_provider);
    QVariant r = proxy.callRemoteMethod("token", "getPluginMethods");
    // Should return the methods array as QVariant, not dispatch to provider's callMethod
    EXPECT_TRUE(r.toJsonArray().size() > 0);
    // Provider's callMethod should NOT have been called for "getPluginMethods"
    EXPECT_TRUE(m_provider->lastMethodCalled.isEmpty());
}

TEST_F(ModuleProxyTest, NullProviderHandling)
{
    ModuleProxy proxy(nullptr);
    QVariant r = proxy.callRemoteMethod("token", "fn");
    EXPECT_FALSE(r.isValid());
    EXPECT_EQ(proxy.getPluginMethods().size(), 0);
}

TEST_F(ModuleProxyTest, EmptyMethodNameHandling)
{
    ModuleProxy proxy(m_provider);
    QVariant r = proxy.callRemoteMethod("token", "");
    EXPECT_FALSE(r.isValid());
    EXPECT_TRUE(m_provider->lastMethodCalled.isEmpty());
}

TEST_F(ModuleProxyTest, SaveTokenValidation)
{
    ModuleProxy proxy(m_provider);

    EXPECT_TRUE(proxy.saveToken("mod", "tok"));
    EXPECT_FALSE(proxy.saveToken("", "tok")); // empty module name
    EXPECT_FALSE(proxy.saveToken("mod", "")); // empty token
}

TEST_F(ModuleProxyTest, EventForwarding)
{
    ModuleProxy proxy(m_provider);
    QSignalSpy spy(&proxy, &ModuleProxy::eventResponse);

    // The proxy sets up event listener on construction.
    // When provider emits event, proxy should emit signal.
    m_provider->testEmitEvent("my_event", {QVariant("data")});

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "my_event");
    QVariantList data = spy.at(0).at(1).value<QVariantList>();
    ASSERT_EQ(data.size(), 1);
    EXPECT_EQ(data[0].toString(), "data");
}

TEST_F(ModuleProxyTest, InformModuleTokenDelegatesToProvider)
{
    LogosAPI api("origin");
    m_provider->init(&api);
    ModuleProxy proxy(m_provider);

    bool result = proxy.informModuleToken("auth", "target_mod", "tok123");
    EXPECT_TRUE(result);
    // Provider's informModuleToken saves via TokenManager
    EXPECT_EQ(TokenManager::instance().getToken("target_mod"), "tok123");
}
