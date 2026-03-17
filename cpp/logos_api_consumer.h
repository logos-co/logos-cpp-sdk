#ifndef LOGOS_API_CONSUMER_H
#define LOGOS_API_CONSUMER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QHash>
#include <QMap>
#include <functional>
#include <memory>

#include "logos_mode.h"

class LogosTransportConnection;
class LogosObject;
class TokenManager;

/**
 * @brief LogosAPIConsumer handles connecting to module objects and invoking their methods
 * 
 * This class is responsible for the consumer/client side functionality:
 * - Connecting to module registries via the transport layer
 * - Requesting LogosObject handles
 * - Invoking methods on objects
 * - Handling events from objects
 */
class LogosAPIConsumer : public QObject
{
    Q_OBJECT

public:
    explicit LogosAPIConsumer(const QString& module_to_talk_to, const QString& origin_module, TokenManager* token_manager, QObject *parent = nullptr);
    ~LogosAPIConsumer();

    /**
     * @brief Request a LogosObject handle by name
     * @return LogosObject* handle, or nullptr if failed. Caller must call release() when done.
     */
    LogosObject* requestObject(const QString& objectName, Timeout timeout = Timeout());

    bool isConnected() const;
    QString registryUrl() const;
    bool reconnect();

    QVariant invokeRemoteMethod(const QString& authToken, const QString& objectName, const QString& methodName, 
                             const QVariantList& args = QVariantList(), Timeout timeout = Timeout());

    /** Callback type for async remote method results. Receives the result QVariant (invalid on error/timeout). */
    using AsyncResultCallback = std::function<void(QVariant)>;

    /**
     * @brief Invoke a remote method asynchronously; result or error is delivered via callback
     * @param authToken Authentication token for the operation
     * @param objectName The name of the remote object
     * @param methodName The name of the method to call
     * @param args Arguments to pass to the method
     * @param callback Called when the call completes (on the same thread as this consumer, typically main)
     * @param timeout Timeout for replica acquisition and for the remote call (default 20000ms)
     */
    void invokeRemoteMethodAsync(const QString& authToken, const QString& objectName, const QString& methodName,
                                 const QVariantList& args,
                                 AsyncResultCallback callback,
                                 Timeout timeout = Timeout());

    /**
     * @brief Register an event listener via LogosObject's callback mechanism
     * @param originObject The LogosObject that will emit the event
     * @param eventName The name of the event to listen for
     * @param callback Function to call when the event is triggered
     */
    void onEvent(LogosObject* originObject, const QString& eventName, 
                std::function<void(const QString&, const QVariantList&)> callback);

public slots:
    bool informModuleToken(const QString& authToken, const QString& moduleName, const QString& token);
    bool informModuleToken_module(const QString& authToken, const QString& originModule, const QString& moduleName, const QString& token);

private:
    std::unique_ptr<LogosTransportConnection> m_transport;
    QString m_registryUrl;
    QMap<QString, QString> m_tokens;
    TokenManager* m_token_manager;
};

#endif // LOGOS_API_CONSUMER_H
