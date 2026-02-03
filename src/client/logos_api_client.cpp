#include "logos_api_client.h"
#include "logos_api_consumer.h"
#include "../token_manager.h"

LogosAPIClient::LogosAPIClient(const QString& module_to_talk_to, const QString& origin_module, TokenManager* token_manager, QObject *parent)
    : QObject(parent)
    , m_consumer(new LogosAPIConsumer(module_to_talk_to, origin_module, this))
    , m_token_manager(token_manager)
    , m_origin_module(origin_module)
{
}

LogosAPIClient::~LogosAPIClient()
{
    // m_consumer will be deleted automatically as it's a child object
}

QObject* LogosAPIClient::requestObject(const QString& objectName, Timeout timeout)
{
    return m_consumer->requestObject(objectName, timeout);
}

bool LogosAPIClient::isConnected() const
{
    return m_consumer->isConnected();
}

QString LogosAPIClient::registryUrl() const
{
    return m_consumer->registryUrl();
}

bool LogosAPIClient::reconnect()
{
    return m_consumer->reconnect();
}

QVariant LogosAPIClient::invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                                   const QVariantList& args, Timeout timeout)
{
    qDebug() << "LogosAPIClient: invoking remote method" << objectName << methodName << args;

    // Get the token for the module
    QString token = getToken(objectName);

    if (token.isEmpty() && objectName != "capability_module") {
        qDebug() << "LogosAPIClient: calling requestModule for" << objectName;
        LogosAPIConsumer* packageManagerConsumer = new LogosAPIConsumer("capability_module", m_origin_module, this);
        QString capabilityToken = getToken("capability_module");
        QVariant result = packageManagerConsumer->invokeRemoteMethod(capabilityToken, "capability_module", "requestModule", QVariantList() << m_origin_module << objectName, timeout);
        qDebug() << "LogosAPIClient: requestModule result for" << objectName << ":" << result.toString();

        token = result.toString();
    }

    return m_consumer->invokeRemoteMethod(token, objectName, methodName, args, timeout);
}

QVariant LogosAPIClient::invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                                   const QVariant& arg, Timeout timeout)
{
    return invokeRemoteMethod(objectName, methodName, QVariantList() << arg, timeout);
}

QVariant LogosAPIClient::invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                                   const QVariant& arg1, const QVariant& arg2, Timeout timeout)
{
    return invokeRemoteMethod(objectName, methodName, QVariantList() << arg1 << arg2, timeout);
}

QVariant LogosAPIClient::invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                                   const QVariant& arg1, const QVariant& arg2, const QVariant& arg3, Timeout timeout)
{
    return invokeRemoteMethod(objectName, methodName, QVariantList() << arg1 << arg2 << arg3, timeout);
}

QVariant LogosAPIClient::invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                                   const QVariant& arg1, const QVariant& arg2, const QVariant& arg3, 
                                   const QVariant& arg4, Timeout timeout)
{
    return invokeRemoteMethod(objectName, methodName, QVariantList() << arg1 << arg2 << arg3 << arg4, timeout);
}

QVariant LogosAPIClient::invokeRemoteMethod(const QString& objectName, const QString& methodName, 
                                   const QVariant& arg1, const QVariant& arg2, const QVariant& arg3, 
                                   const QVariant& arg4, const QVariant& arg5, Timeout timeout)
{
    return invokeRemoteMethod(objectName, methodName, QVariantList() << arg1 << arg2 << arg3 << arg4 << arg5, timeout);
}

void LogosAPIClient::onEvent(QObject* originObject, QObject* destinationObject, const QString& eventName, std::function<void(const QString&, const QVariantList&)> callback)
{
    m_consumer->onEvent(originObject, destinationObject, eventName, callback);
}

void LogosAPIClient::onEvent(QObject* originObject, QObject* destinationObject, const QString& eventName)
{
    m_consumer->onEvent(originObject, destinationObject, eventName);
}



void LogosAPIClient::invokeCallback(const QString& eventName, const QVariantList& data)
{
    m_consumer->invokeCallback(eventName, data);
}

void LogosAPIClient::onEventResponse(QObject* replica, const QString& eventName, const QVariantList& data)
{
    // qDebug() << "LogosAPIClient: Received event:" << eventName << "with data:" << data;
    qDebug() << "LogosAPIClient: Received event:" << eventName;

    if (eventName.isEmpty()) {
        qWarning() << "LogosAPIClient: Event name cannot be empty";
        return;
    }

    // qDebug() << "LogosAPIClient: Emitting event:" << eventName << "with data:" << data;
    qDebug() << "LogosAPIClient: Emitting event:" << eventName;

    // emit the eventResponse signal of replica
    QMetaObject::invokeMethod(replica, "eventResponse", Qt::QueuedConnection, Q_ARG(QString, eventName), Q_ARG(QVariantList, data));
} 

bool LogosAPIClient::informModuleToken(const QString& authToken, const QString& moduleName, const QString& token)
{
    return m_consumer->informModuleToken(authToken, moduleName, token);
}

bool LogosAPIClient::informModuleToken_module(const QString& authToken, const QString& originModule, const QString& moduleName, const QString& token)
{
    return m_consumer->informModuleToken_module(authToken, originModule, moduleName, token);
}

TokenManager* LogosAPIClient::getTokenManager() const
{
    return m_token_manager;
}

QString LogosAPIClient::getToken(const QString& module_name)
{
    QString token = m_token_manager->getToken(module_name);
    if (!token.isEmpty()) {
        qDebug() << "LogosAPIClient: Found token for module:" << module_name;
        return token;
    }

    qDebug() << "LogosAPIClient: No token found for module:" << module_name;
    return "";
}