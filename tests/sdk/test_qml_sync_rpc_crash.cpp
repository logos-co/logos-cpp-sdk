// Integration test for the DeferredDeleteGuard fix in RemoteLogosObject.
//
// Root cause being guarded against:
//   A QML signal handler calls a C++ slot that transitively spins a nested
//   QEventLoop (via QRemoteObjectReplica::waitForSource). If a deleteLater
//   was posted on an ancestor of the handler's QML object before the nested
//   loop started, Qt delivers the DeferredDelete inside the nested loop and
//   destroys the ancestor while the signal handler is still on the call stack.
//   QQmlData::destroyed detects this and aborts with:
//
//     "Object destroyed while one of its QML signal handlers is in progress"
//
// The fix: wrap acquireDynamic + waitForSource in a DeferredDeleteGuard.
// The guard intercepts all DeferredDelete events for the duration of the
// nested loop and re-posts them after it exits, so they fire only once the
// outer signal handler has fully returned.
//
// What this test proves: a QML signal handler can post deleteLater on its
// own root and then call into code that spins a nested event loop (exactly
// as RemoteTransportConnection::requestObject does in production) without
// triggering the qFatal. Removing the DeferredDeleteGuard line from Bridge
// below reproduces the original crash.

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRemoteObjectDynamicReplica>
#include <QRemoteObjectHost>
#include <QRemoteObjectNode>
#include <QString>
#include <QTest>
#include <QUrl>

#include "deferred_delete_guard.h"
#include "implementations/qt_remote/remote_logos_object.h"

// ---------------------------------------------------------------------------
// Q_OBJECT helpers — must live at namespace scope for moc.
// ---------------------------------------------------------------------------

// Minimal source exposed over QRemoteObjects. The method body is irrelevant;
// we only need the acquireDynamic + waitForSource handshake to spin a nested
// event loop.
class MinimalSource : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE QString ping() { return QStringLiteral("pong"); }
};

// Bridge invoked from inside a QML signal handler.
//
// Simulates the production crash path: the handler posts deleteLater on its
// own root object (ancestor), then acquires a RemoteLogosObject — which
// internally calls waitForSource and spins a nested event loop.
//
// DeferredDeleteGuard is the fix. Without it, the DeferredDelete for ancestor
// fires inside the nested loop, destroying a QML-bound object while its
// signal handler is still on the call stack and causing a qFatal. With the
// guard the delete is swallowed for the duration of the nested loop and
// re-posted immediately after, so it fires safely outside the handler.
class Bridge : public QObject {
    Q_OBJECT
    QRemoteObjectNode* m_node;
public:
    explicit Bridge(QRemoteObjectNode* node, QObject* parent = nullptr)
        : QObject(parent), m_node(node) {}

    Q_INVOKABLE void triggerCrashScenario(QObject* ancestor) {
        if (ancestor) ancestor->deleteLater();

        DeferredDeleteGuard guard;   // ← remove this to reproduce the crash
        auto* replica = m_node->acquireDynamic(QStringLiteral("test_source"));
        if (replica) {
            replica->waitForSource(1000);   // spins nested event loop
            RemoteLogosObject wrapped(replica);
            // wrapped.~RemoteLogosObject() calls replica->deleteLater(),
            // also intercepted by the guard still in scope above.
        }
    }
};

// ---------------------------------------------------------------------------
// Test
// ---------------------------------------------------------------------------

TEST(QmlSyncRpcCrashTest, GuardPreventsQFatalDuringQmlSignalHandler) {
    const QString url = QStringLiteral("local:ddg_test");
    QRemoteObjectHost host(QUrl{url});
    MinimalSource source;
    host.enableRemoting(&source, QStringLiteral("test_source"));

    QRemoteObjectNode node;
    node.connectToNode(QUrl{url});
    QTest::qWait(100);  // let socket negotiation settle

    Bridge bridge(&node);
    QQmlEngine engine;

    // Ensure Qt's bundled QML modules are findable. Under nix the default
    // QmlImportsPath isn't always propagated into unwrapped test binaries;
    // we inject the path discovered at configure time via CMake.
#ifdef QT_QML_TEST_IMPORT_PATH
    engine.addImportPath(QStringLiteral(QT_QML_TEST_IMPORT_PATH));
#endif
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QQmlComponent component(&engine);
    component.setData(R"QML(
        import QtQml 2.0
        QtObject {
            id: root
            signal kick()
            onKick: { bridge.triggerCrashScenario(root) }
        }
    )QML", QUrl());
    ASSERT_TRUE(component.isReady()) << component.errorString().toStdString();

    QObject* root = component.create();
    ASSERT_NE(root, nullptr) << component.errorString().toStdString();

    // Emitting kick() marks root as having an active QML signal handler.
    // The handler calls Bridge::triggerCrashScenario, which posts
    // deleteLater on root and then spins a nested event loop. Without
    // DeferredDeleteGuard the process would qFatal here; with it we reach
    // the line below without crashing.
    QMetaObject::invokeMethod(root, "kick", Qt::DirectConnection);

    SUCCEED();  // reaching this line is the positive result

    // Drain re-posted deferred deletes so teardown is clean.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

#include "test_qml_sync_rpc_crash.moc"
