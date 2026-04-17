// Integration test for the "Object destroyed while one of its QML signal
// handlers is in progress" qFatal that crashed LogosBasecamp every time a
// QML Button.onClicked handler transitively invoked a synchronous
// LogosAPIClient::invokeRemoteMethod while anything in the same event
// loop tick had posted a deleteLater on an ancestor of the Button.
//
// Root cause (recap):
//   1. QML signal handler runs → C++ slot called → sync invokeRemoteMethod
//   2. invokeRemoteMethod → m_transport->requestObject()
//        → m_node->acquireDynamic(name)
//        → replica->waitForSource(timeout)  ← spins a nested QEventLoop
//   3. The nested loop delivers posted events, including any DeferredDelete
//      that was posted before (or during) the sync call
//   4. If a DeferredDelete target is an ancestor of the Button whose
//      onClicked is currently on the call stack, Qt aborts with the
//      qFatal above when the Button's destructor runs via
//      QObjectPrivate::deleteChildren
//
// This test reproduces the exact path end-to-end: real QRemoteObjectHost
// on a local: socket, a real RemoteTransportConnection connecting to it,
// a QQmlEngine hosting a QtObject with a QML signal handler, and a C++
// bridge that the handler calls to drive the sync RPC. Without the
// DeferredDeleteGuard installed inside requestObject, the fixture crashes
// with the qFatal (exercised via gtest's death-test mode). With the
// guard, the deferred delete is held through the nested loop and
// re-posted afterward, so the object is eventually destroyed cleanly
// on the next event-loop iteration — no crash.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QObject>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRemoteObjectDynamicReplica>
#include <QRemoteObjectHost>
#include <QRemoteObjectNode>
#include <QString>
#include <QTest>
#include <QUrl>

#include "implementations/qt_remote/remote_transport.h"
#include "logos_object.h"

// ---------------------------------------------------------------------------
// Q_OBJECT helpers — must live at namespace scope for moc.
// ---------------------------------------------------------------------------

// Minimal source exposed over QRemoteObjects. Any QObject with Q_INVOKABLE
// methods works as a dynamic source; we don't care about the method body —
// we only need the acquireDynamic + waitForSource handshake to run.
class MinimalSource : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE QString ping() { return QStringLiteral("pong"); }
};

// Bridge that goes through the SDK's RemoteTransportConnection — the
// code path the DeferredDeleteGuard fix protects.
class GuardedBridge : public QObject {
    Q_OBJECT
public:
    GuardedBridge(RemoteTransportConnection* transport, QObject* parent = nullptr)
        : QObject(parent), m_transport(transport) {}

    // Called from inside a QML signal handler. Posts deleteLater on
    // `ancestor` to simulate the Repeater-delegate-rebuild race that
    // triggered the production crash, then runs the sync RPC which
    // spins the nested event loop inside requestObject().
    Q_INVOKABLE void syncRpcDuringDelete(QObject* ancestor) {
        if (ancestor) ancestor->deleteLater();
        LogosObject* obj = m_transport->requestObject(
            QStringLiteral("test_source"), 1000);
        if (obj) obj->release();
    }

private:
    RemoteTransportConnection* m_transport;
};

// Bridge that INLINES the crashing pattern (acquireDynamic + waitForSource)
// WITHOUT going through requestObject — so the DeferredDeleteGuard is NOT
// installed. Used by the death test to prove the crash path is real and
// that the SDK-level fix is what protects against it.
class UnguardedBridge : public QObject {
    Q_OBJECT
public:
    UnguardedBridge(QRemoteObjectNode* node, QObject* parent = nullptr)
        : QObject(parent), m_node(node) {}

    Q_INVOKABLE void rawRpcDuringDelete(QObject* ancestor) {
        if (ancestor) ancestor->deleteLater();
        QRemoteObjectReplica* r = m_node->acquireDynamic(
            QStringLiteral("test_source"));
        if (r) {
            // This call spins a nested QEventLoop — the moment where,
            // without a guard, the held deleteLater fires and destroys
            // a QObject whose QML signal handler is still on the stack.
            r->waitForSource(1000);
            r->deleteLater();
        }
    }

private:
    QRemoteObjectNode* m_node;
};

namespace {

// Shared QML payload — a QtObject with a signal + handler. Emitting the
// signal marks the object as "QML signal handler active" (the flag that
// QQmlData::destroyed checks before qFatal'ing), then invokes whatever
// method `bridge` was set to via contextProperty.
constexpr const char* kSignalHandlerQml = R"QML(
    import QtQml 2.0
    QtObject {
        id: root
        signal kick()
        onKick: {
            bridge.triggerCrashScenario(root)
        }
    }
)QML";

// Bring up a QRemoteObjectHost, a client-side QRemoteObjectNode (or a
// RemoteTransportConnection for the guarded case), wait for the socket
// handshake, return the pieces to the caller. Factored out so both the
// guarded and the death-test scenarios can call it identically.
struct TestPeers {
    QRemoteObjectHost* host = nullptr;
    MinimalSource* source = nullptr;
};

TestPeers bringUpHost(const QString& url) {
    TestPeers p;
    p.host = new QRemoteObjectHost(QUrl{url});
    p.source = new MinimalSource();
    p.host->enableRemoting(p.source, QStringLiteral("test_source"));
    return p;
}

// Fire the signal on a fresh QML root and return it so the caller can
// watch its destruction. `bridgeMethodName` is the Q_INVOKABLE method on
// `bridge` that the onKick handler should call.
QObject* emitKickOnFreshRoot(QQmlEngine& engine, QObject* bridge,
                              const QByteArray& bridgeMethodName) {
    // Ensure Qt's bundled QML modules (QtQml in particular) are findable.
    // Under nix the default QmlImportsPath isn't always propagated into
    // the test process; we inject the path discovered at configure time
    // via CMake (see tests/sdk/CMakeLists.txt).
#ifdef QT_QML_TEST_IMPORT_PATH
    engine.addImportPath(QStringLiteral(QT_QML_TEST_IMPORT_PATH));
#endif
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));

    engine.rootContext()->setContextProperty("bridge", bridge);

    QQmlComponent component(&engine);
    // Rewrite the QML snippet on the fly to call the right bridge slot.
    QByteArray qml(kSignalHandlerQml);
    qml.replace("triggerCrashScenario", bridgeMethodName);
    component.setData(qml, QUrl());

    if (!component.isReady()) {
        qCritical() << "QML component not ready:" << component.errorString();
        return nullptr;
    }

    QObject* root = component.create();
    if (!root) {
        qCritical() << "QML create returned null:" << component.errorString();
        return nullptr;
    }

    // Invoke the signal via the metaobject — this is the same pathway
    // Qt uses when a Button emits `clicked`, so the QML engine's bound
    // signal expression machinery marks `root` as having an active
    // signal handler. Anything that destroys `root` while we're still
    // in the lambda invoked here triggers QQmlData::destroyed's qFatal.
    QMetaObject::invokeMethod(root, "kick", Qt::DirectConnection);
    return root;
}

} // namespace

// ---------------------------------------------------------------------------
// Positive: with the fix in RemoteTransportConnection::requestObject, the
// full scenario runs without triggering the qFatal.
//
// What this test proves: a QML signal handler can call a non-async method
// that transitively invokes LogosAPIClient::invokeRemoteMethod (via
// RemoteTransportConnection::requestObject) while a `deleteLater` has been
// posted on an ancestor of the handler's receiver, and the process stays
// alive. Before the fix, the nested event loop inside waitForSource would
// deliver the DeferredDelete mid-handler and QQmlData::destroyed would
// qFatal. After the fix, DeferredDeleteGuard swallows it for the duration
// of the nested loop and re-posts it on scope exit.
//
// We don't assert *when* the re-posted deleteLater eventually fires —
// Qt's loop-level semantics make that non-deterministic in a harness
// that isn't running a real top-level event loop. What matters, and what
// this test guards, is that the qFatal does NOT happen.
// ---------------------------------------------------------------------------
TEST(QmlSyncRpcCrashTest, GuardPreventsQFatalDuringQmlSignalHandler) {
    const QString url = QStringLiteral("local:ddg_test_guarded");
    TestPeers peers = bringUpHost(url);

    RemoteTransportConnection transport(url);
    ASSERT_TRUE(transport.connectToHost());
    QTest::qWait(100);  // let socket negotiation settle

    GuardedBridge bridge(&transport);

    QQmlEngine engine;
    QObject* root = emitKickOnFreshRoot(engine, &bridge,
                                         "syncRpcDuringDelete");
    ASSERT_NE(root, nullptr);

    // Reaching this line at all is the positive result. The handler has
    // returned, the sync RPC completed without aborting the process, and
    // the guard held the DeferredDelete through the nested loop. If the
    // guard were missing, Qt would have qFatal'd inside the handler and
    // the test binary would have crashed before here.
    SUCCEED();

    // Pump a bit so queued events (including the re-posted delete) have
    // a chance to drain before teardown destroys the engine.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    delete peers.source;
    delete peers.host;
}

// ---------------------------------------------------------------------------
// Negative (death test): exercise the exact same QML → sync-RPC path, but
// bypass the SDK's requestObject so the DeferredDeleteGuard is never
// installed. Qt aborts with the production qFatal message — which proves
// the harness can actually see the crash. Without this test, the positive
// test above would look identical to a test that doesn't exercise the bug.
//
// gtest's death-test mode forks a subprocess; the qFatal message on its
// stderr is the signature we're asserting against. Matching a stable
// substring rather than the full string keeps the assertion robust across
// Qt versions.
// ---------------------------------------------------------------------------
TEST(QmlSyncRpcCrashTest, WithoutGuardTheScenarioAborts) {
    auto scenario = []() {
        // Do NOT create a QCoreApplication here. gtest death tests use fork()
        // on Linux/macOS ("fast" style), so the child process inherits the
        // parent's QCoreApplication. Creating a second instance causes Qt to
        // abort with a non-matching message before our scenario can run.
        // In threadsafe (exec) style, the re-invoked main() creates one first.
        // Either way, QCoreApplication::instance() is always valid here.
        const QString url = QStringLiteral("local:ddg_death");
        TestPeers peers = bringUpHost(url);

        QRemoteObjectNode node;
        node.connectToNode(QUrl{url});
        QTest::qWait(100);

        UnguardedBridge bridge(&node);
        QQmlEngine engine;
        (void)emitKickOnFreshRoot(engine, &bridge, "rawRpcDuringDelete");

        // If we reach here without crashing, the test "fails" from the
        // death-test perspective (no crash where one was expected). The
        // real outcome is the qFatal during waitForSource's nested loop.
    };

    // Match a stable substring — Qt includes the offending object's
    // pointer between "Object" and "destroyed" ("Object 0xNNN destroyed
    // while…"), so we can't anchor on the word "Object" alone.
    EXPECT_DEATH({ scenario(); },
        "destroyed while one of its QML signal handlers is in progress");
}

#include "test_qml_sync_rpc_crash.moc"
