#ifndef LOGOS_API_CLIENT_H
#define LOGOS_API_CLIENT_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QMap>
#include <functional>
#include <string>

#include "logos_mode.h"
#include "logos_transport_config.h"

class LogosAPIConsumer;
class LogosObject;
class TokenManager;

/**
 * @brief LogosAPIClient provides a high-level interface for remote method calls
 * 
 * This class serves as a facade over LogosAPIConsumer, providing a clean interface
 * for applications that need to call remote methods and handle events.
 */
class LogosAPIClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a client (with its underlying consumer) using
     * `transport`, honoring the process-wide LogosMode.
     *
     * Transport resolution is delegated to LogosAPIConsumer, which in
     * turn goes through the single LogosTransportFactory rule combining
     * LogosMode + LogosTransportConfig. See LogosAPIConsumer's explicit
     * constructor doc-comment for the resolution table.
     */
    LogosAPIClient(const QString& module_to_talk_to,
                   const QString& origin_module,
                   TokenManager* token_manager,
                   const LogosTransportConfig& transport,
                   QObject *parent = nullptr);

    /**
     * @brief Convenience constructor that uses the process-global default
     * LogosTransportConfig. Equivalent to the explicit constructor above
     * with `LogosTransportConfigGlobal::getDefault()`.
     */
    explicit LogosAPIClient(const QString& module_to_talk_to,
                            const QString& origin_module,
                            TokenManager* token_manager,
                            QObject *parent = nullptr);
    ~LogosAPIClient();

    /**
     * @brief Request a LogosObject handle by name
     * @return LogosObject* handle, or nullptr if failed
     */
    LogosObject* requestObject(const QString& objectName, Timeout timeout = Timeout());

    bool isConnected() const;
    QString registryUrl() const;
    bool reconnect();

    QVariant invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                             const QVariantList& args = QVariantList(), Timeout timeout = Timeout());

    QVariant invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                             const QVariant& arg, Timeout timeout = Timeout());

    QVariant invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                             const QVariant& arg1, const QVariant& arg2, Timeout timeout = Timeout());

    QVariant invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                             const QVariant& arg1, const QVariant& arg2, const QVariant& arg3, Timeout timeout = Timeout());

    QVariant invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                             const QVariant& arg1, const QVariant& arg2, const QVariant& arg3, 
                             const QVariant& arg4, Timeout timeout = Timeout());

    QVariant invokeRemoteMethod(const QString& objectName, const QString& methodName,
                             const QVariant& arg1, const QVariant& arg2, const QVariant& arg3,
                             const QVariant& arg4, const QVariant& arg5, Timeout timeout = Timeout());

    using AsyncResultCallback = std::function<void(QVariant)>;

    void invokeRemoteMethodAsync(const QString& objectName, const QString& methodName,
                                 const QVariantList& args, AsyncResultCallback callback,
                                 Timeout timeout = Timeout());

    void invokeRemoteMethodAsync(const QString& objectName, const QString& methodName,
                                 const QVariant& arg, AsyncResultCallback callback,
                                 Timeout timeout = Timeout());

    void invokeRemoteMethodAsync(const QString& objectName, const QString& methodName,
                                 const QVariant& arg1, const QVariant& arg2,
                                 AsyncResultCallback callback, Timeout timeout = Timeout());

    void invokeRemoteMethodAsync(const QString& objectName, const QString& methodName,
                                 const QVariant& arg1, const QVariant& arg2, const QVariant& arg3,
                                 AsyncResultCallback callback, Timeout timeout = Timeout());

    void invokeRemoteMethodAsync(const QString& objectName, const QString& methodName,
                                 const QVariant& arg1, const QVariant& arg2, const QVariant& arg3,
                                 const QVariant& arg4, AsyncResultCallback callback,
                                 Timeout timeout = Timeout());

    void invokeRemoteMethodAsync(const QString& objectName, const QString& methodName,
                                 const QVariant& arg1, const QVariant& arg2, const QVariant& arg3,
                                 const QVariant& arg4, const QVariant& arg5,
                                 AsyncResultCallback callback, Timeout timeout = Timeout());

    /**
     * @brief Register an event listener via LogosObject's callback mechanism
     * @param originObject The LogosObject that will emit the event
     * @param eventName The name of the event to listen for
     * @param callback Function to call when the event is triggered
     */
    void onEvent(LogosObject* originObject, const QString& eventName, 
                std::function<void(const QString&, const QVariantList&)> callback);

    /**
     * @brief Emit an event on a LogosObject (for plugins that act as event sources)
     * @param object The LogosObject to emit the event on
     * @param eventName The name of the event
     * @param data The event data
     */
    void onEventResponse(LogosObject* object, const QString& eventName, const QVariantList& data);

    /**
     * @brief Backward-compatible overload for QObject-based plugins.
     *
     * Old-API plugins call onEventResponse(this, ...) where `this` is a QObject*.
     * This overload invokes the eventResponse signal on the QObject via QMetaObject.
     */
    void onEventResponse(QObject* object, const QString& eventName, const QVariantList& data);

    bool informModuleToken(const QString& authToken, const QString& moduleName, const QString& token);
    bool informModuleToken(const char* authToken, const char* moduleName, const char* token)
        { return informModuleToken(QString(authToken), QString(moduleName), QString(token)); }
    bool informModuleToken(const std::string& authToken, const std::string& moduleName, const std::string& token);
    bool informModuleToken_module(const QString& authToken, const QString& originModule, const QString& moduleName, const QString& token);

    TokenManager* getTokenManager() const;
    QString getToken(const QString& module_name);

private:
    LogosAPIConsumer* m_consumer;
    QMap<QString, QString> m_tokens;
    TokenManager* m_token_manager;
    QString m_origin_module;
};

#endif // LOGOS_API_CLIENT_H
