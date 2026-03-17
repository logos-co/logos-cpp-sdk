#ifndef MOCK_TRANSPORT_H
#define MOCK_TRANSPORT_H

#include "../../logos_transport.h"
#include <QObject>
#include <QString>

/**
 * @brief Lightweight QObject that carries the module name through the transport pipeline.
 *
 * MockTransportConnection::requestObject() returns one of these so that
 * callRemoteMethod() can look up which module is being called.
 */
class MockObject : public QObject {
    Q_OBJECT
public:
    explicit MockObject(const QString& moduleName, QObject* parent = nullptr)
        : QObject(parent), m_moduleName(moduleName) {}

    const QString& moduleName() const { return m_moduleName; }

private:
    QString m_moduleName;
};

/**
 * @brief No-op provider-side transport for mock mode.
 *
 * publishObject / unpublishObject succeed silently; there is no real
 * IPC endpoint and no object needs to be made available.
 */
class MockTransportHost : public LogosTransportHost {
public:
    bool publishObject(const QString& name, QObject* object) override;
    void unpublishObject(const QString& name) override;
};

/**
 * @brief Consumer-side transport for mock mode.
 *
 * requestObject returns a MockObject tagged with the module name.
 * callRemoteMethod records the call in MockStore and returns the
 * configured return value.
 */
class MockTransportConnection : public LogosTransportConnection {
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

#endif // MOCK_TRANSPORT_H
