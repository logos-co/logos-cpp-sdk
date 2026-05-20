#include "mock_transport.h"
#include "mock_store.h"
#include <QDebug>

// ── MockTransportHost ────────────────────────────────────────────────────────

bool MockTransportHost::publishObject(const QString& name, QObject* /*object*/)
{
    qDebug() << "MockTransportHost: publishObject (no-op)" << name;
    return true;
}

void MockTransportHost::unpublishObject(const QString& name)
{
    qDebug() << "MockTransportHost: unpublishObject (no-op)" << name;
}

// ── MockTransportConnection ──────────────────────────────────────────────────

bool MockTransportConnection::connectToHost()
{
    qDebug() << "MockTransportConnection: connectToHost (no-op, always connected)";
    return true;
}

bool MockTransportConnection::isConnected() const
{
    // Defaults to true; tests can flip via MockStore::setConnected(false)
    // (exposed through LogosMockSetup::disconnect()) to exercise the
    // central peer-liveness guard in LogosAPIConsumer. Implementation
    // is a single std::atomic<bool> relaxed load, satisfying the O(1)
    // hot-path contract on LogosTransportConnection::isConnected().
    return MockStore::instance().isConnected();
}

bool MockTransportConnection::reconnect()
{
    return true;
}

LogosObject* MockTransportConnection::requestObject(const QString& objectName, int /*timeoutMs*/)
{
    qDebug() << "MockTransportConnection: requestObject" << objectName;
    return new MockLogosObject(objectName);
}
