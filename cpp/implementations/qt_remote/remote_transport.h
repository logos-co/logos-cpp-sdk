#ifndef REMOTE_TRANSPORT_H
#define REMOTE_TRANSPORT_H

#include "../../logos_transport.h"
#include <QString>

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
};

class RemoteTransportConnection : public LogosTransportConnection {
public:
    explicit RemoteTransportConnection(const QString& registryUrl);
    ~RemoteTransportConnection() override;

    bool connectToHost() override;
    bool isConnected() const override;
    bool reconnect() override;
    QObject* requestObject(const QString& objectName, int timeoutMs) override;
    void releaseObject(QObject* object) override;
    QVariant callRemoteMethod(QObject* object, const QString& authToken,
                               const QString& methodName, const QVariantList& args,
                               int timeoutMs) override;
    bool callInformModuleToken(QObject* object, const QString& authToken,
                                const QString& moduleName, const QString& token,
                                int timeoutMs) override;

private:
    bool connectToRegistry();

    QRemoteObjectNode* m_node;
    QString m_registryUrl;
    bool m_connected;
};

#endif // REMOTE_TRANSPORT_H
