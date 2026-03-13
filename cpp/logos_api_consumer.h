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
class TokenManager;

/**
 * @brief LogosAPIConsumer handles connecting to module objects and invoking their methods
 * 
 * This class is responsible for the consumer/client side functionality:
 * - Connecting to module registries via the transport layer
 * - Requesting object handles
 * - Invoking methods on objects
 * - Handling events from objects
 */
class LogosAPIConsumer : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new LogosAPIConsumer
     * @param module_to_talk_to The name of the module to connect to
     * @param origin_module The name of the originating module
     * @param token_manager Pointer to the token manager instance
     * @param parent Parent QObject
     */
    explicit LogosAPIConsumer(const QString& module_to_talk_to, const QString& origin_module, TokenManager* token_manager, QObject *parent = nullptr);
    
    /**
     * @brief Destructor - cleans up connections and resources
     */
    ~LogosAPIConsumer();

    /**
     * @brief Request an object handle by name
     * @param objectName The name of the object to acquire
     * @param timeout Timeout to wait for the object to be ready (default 20000ms)
     * @return QObject* pointer to the object, or nullptr if failed
     */
    QObject* requestObject(const QString& objectName, Timeout timeout = Timeout());

    /**
     * @brief Check if the consumer is connected to the registry
     * @return true if connected, false otherwise
     */
    bool isConnected() const;

    /**
     * @brief Get the registry URL this consumer is connected to
     * @return QString containing the registry URL
     */
    QString registryUrl() const;

    /**
     * @brief Reconnect to the registry
     * @return true if reconnection successful, false otherwise
     */
    bool reconnect();

    /**
     * @brief Invoke a method on a module object
     * @param authToken Authentication token for the operation
     * @param objectName The name of the object
     * @param methodName The name of the method to call
     * @param args Arguments to pass to the method
     * @param timeout Timeout to wait for the result (default 20000ms)
     * @return QVariant containing the result, or invalid QVariant if failed
     */
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
     * @brief Register an event listener for the specified event name
     * @param originObject The object that will emit the event
     * @param destinationObject The object that will receive the event
     * @param eventName The name of the event to listen for
     * @param callback Function to call when the event is triggered
     */
    void onEvent(QObject* originObject, QObject* destinationObject, const QString& eventName, 
                std::function<void(const QString&, const QVariantList&)> callback);
    
    /**
     * @brief Register an event listener without callback (connects to destinationObject's slot)
     * @param originObject The object that will emit the event
     * @param destinationObject The object that will receive the event
     * @param eventName The name of the event to listen for
     */
    void onEvent(QObject* originObject, QObject* destinationObject, const QString& eventName);

public slots:
    /**
     * @brief Helper slot to invoke stored callbacks
     * @param eventName The name of the event that was triggered
     * @param data The event data to pass to the callback
     */
    void invokeCallback(const QString& eventName, const QVariantList& data);

    /**
     * @brief Inform a module about a token
     * @param authToken Authentication token for the operation
     * @param moduleName The name of the module
     * @param token The token to inform the module about
     * @return bool true if successful, false otherwise
     */
    bool informModuleToken(const QString& authToken, const QString& moduleName, const QString& token);

    bool informModuleToken_module(const QString& authToken, const QString& originModule, const QString& moduleName, const QString& token);

private:
    std::unique_ptr<LogosTransportConnection> m_transport;
    QString m_registryUrl;
    QMap<QString, QString> m_tokens;
    TokenManager* m_token_manager;

    QHash<QString, QList<std::function<void(const QString&, const QVariantList&)>>> m_eventCallbacks;
    QHash<QObject*, QMetaObject::Connection> m_connections;
};

#endif // LOGOS_API_CONSUMER_H
