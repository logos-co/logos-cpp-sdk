#ifndef LOGOS_API_CLIENT_H
#define LOGOS_API_CLIENT_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QMap>
#include <functional>

#include "logos_mode.h"

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
    explicit LogosAPIClient(const QString& module_to_talk_to, const QString& origin_module, TokenManager* token_manager, QObject *parent = nullptr);
    ~LogosAPIClient();

    /**
     * @brief Request a LogosObject handle by name
     * @return LogosObject* handle, or nullptr if failed
     */
    LogosObject* requestObject(const QString& objectName, Timeout timeout = Timeout());

    bool isConnected() const;
    QString registryUrl() const;
    bool reconnect();

    // DEPRECATED: Use NativeLogosClient::invokeMethod() for native modules.
    // These QVariant-based overloads remain for Q_INVOKABLE and LogosProviderBase modules.
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

    // DEPRECATED: Use NativeLogosClient::onEvent() for native modules.
    void onEvent(LogosObject* originObject, const QString& eventName, 
                std::function<void(const QString&, const QVariantList&)> callback);

    // DEPRECATED: Use NativeLogosClient::onEventResponse() for native modules.
    void onEventResponse(LogosObject* object, const QString& eventName, const QVariantList& data);

    /**
     * @brief Backward-compatible overload for QObject-based plugins.
     *
     * Old-API plugins call onEventResponse(this, ...) where `this` is a QObject*.
     * This overload invokes the eventResponse signal on the QObject via QMetaObject.
     */
    void onEventResponse(QObject* object, const QString& eventName, const QVariantList& data);

    bool informModuleToken(const QString& authToken, const QString& moduleName, const QString& token);
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
