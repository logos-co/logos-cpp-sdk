#include "remote_transport.h"
#include <QRemoteObjectRegistryHost>
#include <QRemoteObjectNode>
#include <QRemoteObjectReplica>
#include <QRemoteObjectPendingCall>
#include <QDebug>
#include <QUrl>
#include <QMetaObject>
#include <QTime>

// --- RemoteTransportHost ---

RemoteTransportHost::RemoteTransportHost(const QString& registryUrl)
    : m_registryHost(nullptr)
    , m_registryUrl(registryUrl)
{
}

RemoteTransportHost::~RemoteTransportHost()
{
    delete m_registryHost;
}

bool RemoteTransportHost::publishObject(const QString& name, QObject* object)
{
    if (!m_registryHost) {
        m_registryHost = new QRemoteObjectRegistryHost(QUrl(m_registryUrl));
        if (!m_registryHost) {
            qCritical() << "RemoteTransportHost: Failed to create registry host";
            return false;
        }
        qDebug() << "RemoteTransportHost: Created registry host with URL:" << m_registryUrl;
    }

    bool success = m_registryHost->enableRemoting(object, name);
    if (success) {
        qDebug() << "RemoteTransportHost: Published object:" << name;
    } else {
        qCritical() << "RemoteTransportHost: Failed to publish object:" << name;
    }
    return success;
}

void RemoteTransportHost::unpublishObject(const QString& /*name*/)
{
    // Qt Remote Objects doesn't require explicit unregistration;
    // the host cleanup handles it on destruction.
}

// --- RemoteTransportConnection ---

RemoteTransportConnection::RemoteTransportConnection(const QString& registryUrl)
    : m_node(new QRemoteObjectNode())
    , m_registryUrl(registryUrl)
    , m_connected(false)
{
}

RemoteTransportConnection::~RemoteTransportConnection()
{
    delete m_node;
}

bool RemoteTransportConnection::connectToHost()
{
    return connectToRegistry();
}

bool RemoteTransportConnection::isConnected() const
{
    return m_connected;
}

bool RemoteTransportConnection::reconnect()
{
    qDebug() << "RemoteTransportConnection: Attempting to reconnect to registry:" << m_registryUrl;

    if (m_connected) {
        delete m_node;
        m_node = new QRemoteObjectNode();
        m_connected = false;
    }

    return connectToRegistry();
}

bool RemoteTransportConnection::connectToRegistry()
{
    if (!m_node) {
        qWarning() << "RemoteTransportConnection: Remote object node is null";
        return false;
    }

    if (m_registryUrl.isEmpty()) {
        qWarning() << "RemoteTransportConnection: Registry URL is empty";
        return false;
    }

    qDebug() << "RemoteTransportConnection: Connecting to registry:" << m_registryUrl
             << "at" << QTime::currentTime().toString("hh:mm:ss.zzz");

    QUrl url(m_registryUrl);
    bool success = m_node->connectToNode(url);

    if (success) {
        m_connected = true;
        qDebug() << "RemoteTransportConnection: Successfully connected to registry:" << m_registryUrl;
    } else {
        m_connected = false;
        qWarning() << "RemoteTransportConnection: Failed to connect to registry:" << m_registryUrl;
    }
    qDebug() << "RemoteTransportConnection: Connected to registry at"
             << QTime::currentTime().toString("hh:mm:ss.zzz");

    return m_connected;
}

QObject* RemoteTransportConnection::requestObject(const QString& objectName, int timeoutMs)
{
    if (!m_connected) {
        qWarning() << "RemoteTransportConnection: Not connected. Cannot request object:" << objectName;
        return nullptr;
    }

    qDebug() << "RemoteTransportConnection: Requesting object:" << objectName
             << "at" << QTime::currentTime().toString("hh:mm:ss.zzz");

    QRemoteObjectReplica* replica = m_node->acquireDynamic(objectName);
    if (!replica) {
        qWarning() << "RemoteTransportConnection: Failed to acquire replica for:" << objectName;
        return nullptr;
    }

    if (!replica->waitForSource(timeoutMs)) {
        qWarning() << "RemoteTransportConnection: Timeout waiting for replica:" << objectName;
        delete replica;
        return nullptr;
    }

    qDebug() << "RemoteTransportConnection: Acquired replica for:" << objectName
             << "at" << QTime::currentTime().toString("hh:mm:ss.zzz");
    return replica;
}

void RemoteTransportConnection::releaseObject(QObject* object)
{
    delete object;
}

QVariant RemoteTransportConnection::callRemoteMethod(QObject* object, const QString& authToken,
                                                      const QString& methodName, const QVariantList& args,
                                                      int timeoutMs)
{
    if (!object) {
        qWarning() << "RemoteTransportConnection: Cannot call method on null object";
        return QVariant();
    }

    QRemoteObjectPendingCall pendingCall;
    bool success = QMetaObject::invokeMethod(
        object,
        "callRemoteMethod",
        Qt::DirectConnection,
        Q_RETURN_ARG(QRemoteObjectPendingCall, pendingCall),
        Q_ARG(QString, authToken),
        Q_ARG(QString, methodName),
        Q_ARG(QVariantList, args)
    );

    if (!success) {
        qWarning() << "RemoteTransportConnection: Failed to invoke callRemoteMethod on replica";
        return QVariant();
    }

    pendingCall.waitForFinished(timeoutMs);

    if (!pendingCall.isFinished() || pendingCall.error() != QRemoteObjectPendingCall::NoError) {
        qWarning() << "RemoteTransportConnection: callRemoteMethod failed or timed out:" << pendingCall.error();
        return QVariant();
    }

    return pendingCall.returnValue();
}

bool RemoteTransportConnection::callInformModuleToken(QObject* object, const QString& authToken,
                                                       const QString& moduleName, const QString& token,
                                                       int timeoutMs)
{
    if (!object) {
        qWarning() << "RemoteTransportConnection: Cannot call informModuleToken on null object";
        return false;
    }

    QRemoteObjectPendingCall pendingCall;
    bool success = QMetaObject::invokeMethod(
        object,
        "informModuleToken",
        Qt::DirectConnection,
        Q_RETURN_ARG(QRemoteObjectPendingCall, pendingCall),
        Q_ARG(QString, authToken),
        Q_ARG(QString, moduleName),
        Q_ARG(QString, token)
    );

    if (!success) {
        qWarning() << "RemoteTransportConnection: Failed to invoke informModuleToken on replica";
        return false;
    }

    pendingCall.waitForFinished(timeoutMs);

    if (!pendingCall.isFinished() || pendingCall.error() != QRemoteObjectPendingCall::NoError) {
        qWarning() << "RemoteTransportConnection: informModuleToken failed or timed out:" << pendingCall.error();
        return false;
    }

    return pendingCall.returnValue().toBool();
}
