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
    return true;
}

bool MockTransportConnection::reconnect()
{
    return true;
}

QObject* MockTransportConnection::requestObject(const QString& objectName, int /*timeoutMs*/)
{
    qDebug() << "MockTransportConnection: requestObject" << objectName;
    return new MockObject(objectName);
}

void MockTransportConnection::releaseObject(QObject* object)
{
    delete object;
}

QVariant MockTransportConnection::callRemoteMethod(QObject* object,
                                                    const QString& /*authToken*/,
                                                    const QString& methodName,
                                                    const QVariantList& args,
                                                    int /*timeoutMs*/)
{
    MockObject* mockObj = qobject_cast<MockObject*>(object);
    if (!mockObj) {
        qWarning() << "MockTransportConnection: callRemoteMethod called with non-MockObject";
        return QVariant();
    }

    qDebug() << "MockTransportConnection: callRemoteMethod"
             << mockObj->moduleName() << "::" << methodName
             << "args:" << args;

    return MockStore::instance().recordAndReturn(mockObj->moduleName(), methodName, args);
}

bool MockTransportConnection::callInformModuleToken(QObject* /*object*/,
                                                     const QString& /*authToken*/,
                                                     const QString& moduleName,
                                                     const QString& /*token*/,
                                                     int /*timeoutMs*/)
{
    qDebug() << "MockTransportConnection: callInformModuleToken (no-op)" << moduleName;
    return true;
}
