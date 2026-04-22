#pragma once
#include "logos_object.h"
#include <QObject>

// Concrete LogosObject that communicates via Qt Remote Objects.
//
// The constructor takes the replica as QObject* so that this header does not
// pull in QRemoteObjects headers into every translation unit that instantiates
// the class.  At runtime the pointer always points to a QRemoteObjectReplica.
//
// m_helper is also stored as QObject* for the same reason; the implementation
// in remote_transport.cpp casts it to RemoteEventHelper* where needed.
class RemoteLogosObject : public LogosObject {
public:
    explicit RemoteLogosObject(QObject* replica);
    ~RemoteLogosObject() override;

    QVariant callMethod(const QString& authToken,
                        const QString& methodName,
                        const QVariantList& args,
                        int timeoutMs) override;

    void callMethodAsync(const QString& authToken,
                         const QString& methodName,
                         const QVariantList& args,
                         int timeoutMs,
                         AsyncResultCallback callback) override;

    bool informModuleToken(const QString& authToken,
                           const QString& moduleName,
                           const QString& token,
                           int timeoutMs) override;

    void onEvent(const QString& eventName, EventCallback callback) override;
    void disconnectEvents() override;
    void emitEvent(const QString& eventName, const QVariantList& data) override;
    QJsonArray getMethods() override;

    void release() override;
    quintptr id() const override;

private:
    QObject* m_replica;
    QObject* m_helper;
};
