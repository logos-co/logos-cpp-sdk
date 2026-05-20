// Coverage for the central peer-liveness guard in LogosAPIConsumer.
//
// Every outbound RPC the SDK exposes funnels through one of four
// methods on LogosAPIConsumer (called via LogosAPIClient):
//
//     invokeRemoteMethod          (sync)
//     invokeRemoteMethodAsync     (async)
//     informModuleToken           (token push to capability_module)
//     informModuleToken_module    (token push to an arbitrary peer)
//
// Each consults m_transport->isConnected() and short-circuits with a
// structured failure (default QVariant / false / async callback with
// default QVariant) before any transport touch. The motivation is
// avoiding a synchronous outbound on a torn-down socket — observed
// once as a Qt-Remote-Objects source-codec race that SIGBUS'd the
// entire logos_host.
//
// These tests drive the guard by flipping MockStore's mocked
// connection state via LogosMockSetup::disconnect().

#include <gtest/gtest.h>
#include <atomic>
#include <QCoreApplication>

#include "logos_mock.h"
#include "mock_store.h"
#include "logos_api.h"
#include "logos_api_client.h"

namespace {

class PeerLivenessGuardTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        m_mock = new LogosMockSetup();
    }
    void TearDown() override
    {
        delete m_api;
        delete m_mock;
    }

    void createApi(const QString& targetModule = "peer")
    {
        m_api = new LogosAPI("origin");
        m_client = m_api->getClient(targetModule);
        ASSERT_NE(m_client, nullptr);
    }

    LogosMockSetup* m_mock = nullptr;
    LogosAPI* m_api = nullptr;
    LogosAPIClient* m_client = nullptr;
};

// ── sync invokeRemoteMethod ─────────────────────────────────────────────────

TEST_F(PeerLivenessGuardTest, SyncInvokeReturnsInvalidQVariantWhenDisconnected)
{
    // Set up an expectation so we can prove the transport is *never*
    // reached. when() also pre-seeds a non-empty token for "peer"
    // (avoids the capability_module requestModule auto-dial path).
    m_mock->when("peer", "getValue").thenReturn(QVariant(42));
    createApi();

    m_mock->disconnect();

    QVariant result = m_client->invokeRemoteMethod("peer", "getValue");

    EXPECT_FALSE(result.isValid())
        << "Disconnected transport must return default-constructed QVariant; "
           "got: " << result.toString().toStdString();
    EXPECT_FALSE(m_mock->wasCalled("peer", "getValue"))
        << "Guard must short-circuit before the transport sees the call.";
}

TEST_F(PeerLivenessGuardTest, SyncInvokeWorksAfterReconnect)
{
    m_mock->when("peer", "getValue").thenReturn(QVariant(42));
    createApi();

    m_mock->disconnect();
    EXPECT_FALSE(m_client->invokeRemoteMethod("peer", "getValue").isValid());
    EXPECT_EQ(m_mock->callCount("peer", "getValue"), 0);

    m_mock->reconnect();
    QVariant result = m_client->invokeRemoteMethod("peer", "getValue");
    EXPECT_EQ(result.toInt(), 42);
    EXPECT_EQ(m_mock->callCount("peer", "getValue"), 1);
}

TEST_F(PeerLivenessGuardTest, SyncInvokeSucceedsWhenConnectedByDefault)
{
    // Sanity: a fresh LogosMockSetup must start in the "connected" state,
    // otherwise every existing test (test_logos_api_client_mock, etc.)
    // would have been silently broken by this refactor.
    m_mock->when("peer", "fn").thenReturn(QVariant("ok"));
    createApi();

    QVariant result = m_client->invokeRemoteMethod("peer", "fn");
    EXPECT_EQ(result.toString(), "ok");
    EXPECT_TRUE(m_mock->wasCalled("peer", "fn"));
}

// ── async invokeRemoteMethodAsync ───────────────────────────────────────────

TEST_F(PeerLivenessGuardTest, AsyncInvokeFiresCallbackWithInvalidVariantWhenDisconnected)
{
    m_mock->when("peer", "fetch").thenReturn(QVariant(QStringLiteral("real")));
    createApi();

    m_mock->disconnect();

    bool called = false;
    QVariant received(QStringLiteral("sentinel"));  // overwritten on callback
    m_client->invokeRemoteMethodAsync(
        "peer", "fetch", QVariantList(),
        [&](QVariant v) { called = true; received = v; });

    // Contract from the existing async test suite: the callback never
    // fires synchronously. The guard preserves that — it posts the
    // failure delivery via QTimer::singleShot(0, ...).
    EXPECT_FALSE(called) << "Async callback must not fire synchronously.";
    QCoreApplication::processEvents();

    EXPECT_TRUE(called);
    EXPECT_FALSE(received.isValid())
        << "Disconnected async path must deliver default-constructed QVariant.";
    EXPECT_FALSE(m_mock->wasCalled("peer", "fetch"));
}

TEST_F(PeerLivenessGuardTest, AsyncInvokeWorksAfterReconnect)
{
    m_mock->when("peer", "fetch").thenReturn(QVariant(QStringLiteral("hello")));
    createApi();

    m_mock->disconnect();
    QVariant first(QStringLiteral("sentinel"));
    m_client->invokeRemoteMethodAsync(
        "peer", "fetch", QVariantList(), [&](QVariant v) { first = v; });
    QCoreApplication::processEvents();
    EXPECT_FALSE(first.isValid());
    EXPECT_EQ(m_mock->callCount("peer", "fetch"), 0);

    m_mock->reconnect();
    QVariant second;
    m_client->invokeRemoteMethodAsync(
        "peer", "fetch", QVariantList(), [&](QVariant v) { second = v; });
    QCoreApplication::processEvents();
    EXPECT_EQ(second.toString(), "hello");
    EXPECT_EQ(m_mock->callCount("peer", "fetch"), 1);
}

// ── informModuleToken / informModuleToken_module ───────────────────────────

TEST_F(PeerLivenessGuardTest, InformModuleTokenReturnsFalseWhenDisconnected)
{
    // when() seeds a "capability_module" token, but informModuleToken
    // doesn't itself need one — the call still bottoms out in the
    // consumer's informModuleToken which is the method we're guarding.
    m_mock->when("capability_module", "informModuleToken").thenReturn(QVariant(true));
    createApi("capability_module");

    m_mock->disconnect();

    EXPECT_FALSE(m_client->informModuleToken("auth-token", "target_module", "target-token"));
    EXPECT_FALSE(m_mock->wasCalled("capability_module", "informModuleToken"));
}

TEST_F(PeerLivenessGuardTest, InformModuleTokenModuleReturnsFalseWhenDisconnected)
{
    // The specific call site that triggered the original SIGBUS:
    // capability_module's logos_host calling informModuleToken_module
    // on a ui-host child mid-SIGTERM.
    m_mock->when("origin", "informModuleToken").thenReturn(QVariant(true));
    createApi("origin");

    m_mock->disconnect();

    EXPECT_FALSE(m_client->informModuleToken_module(
        "auth-token", "origin", "target_module", "target-token"));
    EXPECT_FALSE(m_mock->wasCalled("origin", "informModuleToken"));
}

TEST_F(PeerLivenessGuardTest, InformModuleTokenModuleSucceedsAfterReconnect)
{
    m_mock->when("origin", "informModuleToken").thenReturn(QVariant(true));
    createApi("origin");

    m_mock->disconnect();
    EXPECT_FALSE(m_client->informModuleToken_module(
        "auth-token", "origin", "target_module", "target-token"));

    m_mock->reconnect();
    EXPECT_TRUE(m_client->informModuleToken_module(
        "auth-token", "origin", "target_module", "target-token"));
}

// ── Hot-path contract sanity ────────────────────────────────────────────────

TEST_F(PeerLivenessGuardTest, IsConnectedIsLockFreeAndIdempotent)
{
    // Documented contract on LogosTransportConnection::isConnected():
    // implementations MUST return cached state, O(1). The mock backs it
    // with std::atomic<bool>. Verify repeated reads don't drift the
    // underlying state (i.e. the check is pure, never has a side effect).
    createApi();

    for (int i = 0; i < 1000; ++i) {
        // Direct probe through LogosAPIClient::isConnected, which
        // bottoms out in the same transport path the guard uses.
        EXPECT_TRUE(m_client->isConnected());
    }

    m_mock->disconnect();
    for (int i = 0; i < 1000; ++i) {
        EXPECT_FALSE(m_client->isConnected());
    }

    m_mock->reconnect();
    EXPECT_TRUE(m_client->isConnected());
}

} // namespace
