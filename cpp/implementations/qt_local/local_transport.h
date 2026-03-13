#ifndef LOCAL_TRANSPORT_H
#define LOCAL_TRANSPORT_H

#include "../../logos_transport.h"

class LocalTransportHost : public LogosTransportHost {
public:
    bool publishObject(const QString& name, QObject* object) override;
    void unpublishObject(const QString& name) override;
};

class LocalTransportConnection : public LogosTransportConnection {
public:
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
};

#endif // LOCAL_TRANSPORT_H
