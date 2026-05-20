#ifndef REMOTE_TRANSPORT_H
#define REMOTE_TRANSPORT_H

#include "../../logos_transport.h"
#include "../../logos_object.h"
#include <QHash>
#include <QPointer>
#include <QString>

class QObject;
class QRemoteObjectRegistryHost;
class QRemoteObjectNode;

class RemoteTransportHost : public LogosTransportHost {
public:
    explicit RemoteTransportHost(const QString& registryUrl);
    ~RemoteTransportHost() override;

    bool publishObject(const QString& name, QObject* object) override;
    void unpublishObject(const QString& name) override;

private:
    QRemoteObjectRegistryHost* m_registryHost;
    QString m_registryUrl;
    // name -> object published via enableRemoting(). QPointer so a
    // caller that deletes the QObject behind our back (rare; the
    // provider owns it for the host's lifetime today) doesn't leave a
    // dangling pointer for unpublishObject to dereference.
    QHash<QString, QPointer<QObject>> m_publishedObjects;
};

class RemoteTransportConnection : public LogosTransportConnection {
public:
    explicit RemoteTransportConnection(const QString& registryUrl);
    ~RemoteTransportConnection() override;

    bool connectToHost() override;
    bool isConnected() const override;
    bool reconnect() override;
    LogosObject* requestObject(const QString& objectName, int timeoutMs) override;

private:
    bool connectToRegistry();

    QRemoteObjectNode* m_node;
    QString m_registryUrl;
    bool m_connected;
};

#endif // REMOTE_TRANSPORT_H
