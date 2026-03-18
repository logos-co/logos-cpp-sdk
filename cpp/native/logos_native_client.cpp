#include "logos_native_client.h"
#include "logos_value_qt.h"
#include "../logos_api_client.h"

#include <QString>
#include <QVariant>
#include <QVariantList>

NativeLogosClient::NativeLogosClient(LogosAPIClient* qtClient)
    : m_client(qtClient)
{
}

LogosValue NativeLogosClient::invokeMethod(const std::string& objectName,
                                           const std::string& methodName,
                                           const std::vector<LogosValue>& args)
{
    qDebug() << "[NATIVE API] NativeLogosClient: calling" << QString::fromStdString(objectName)
             << "." << QString::fromStdString(methodName) << "with" << args.size() << "args (native types)";
    QVariantList qArgs = logosValueListToQVariantList(args);
    QVariant result = m_client->invokeRemoteMethod(
        QString::fromStdString(objectName),
        QString::fromStdString(methodName),
        qArgs);
    return logosValueFromQVariant(result);
}

LogosValue NativeLogosClient::invokeMethod(const std::string& objectName,
                                           const std::string& methodName,
                                           const LogosValue& arg)
{
    qDebug() << "[NATIVE API] NativeLogosClient: calling" << QString::fromStdString(objectName)
             << "." << QString::fromStdString(methodName) << "with 1 arg (native types)";
    QVariant result = m_client->invokeRemoteMethod(
        QString::fromStdString(objectName),
        QString::fromStdString(methodName),
        logosValueToQVariant(arg));
    return logosValueFromQVariant(result);
}

LogosValue NativeLogosClient::invokeMethod(const std::string& objectName,
                                           const std::string& methodName,
                                           const LogosValue& arg1,
                                           const LogosValue& arg2)
{
    qDebug() << "[NATIVE API] NativeLogosClient: calling" << QString::fromStdString(objectName)
             << "." << QString::fromStdString(methodName) << "with 2 args (native types)";
    QVariant result = m_client->invokeRemoteMethod(
        QString::fromStdString(objectName),
        QString::fromStdString(methodName),
        logosValueToQVariant(arg1),
        logosValueToQVariant(arg2));
    return logosValueFromQVariant(result);
}

LogosValue NativeLogosClient::invokeMethod(const std::string& objectName,
                                           const std::string& methodName,
                                           const LogosValue& arg1,
                                           const LogosValue& arg2,
                                           const LogosValue& arg3)
{
    qDebug() << "[NATIVE API] NativeLogosClient: calling" << QString::fromStdString(objectName)
             << "." << QString::fromStdString(methodName) << "with 3 args (native types)";
    QVariant result = m_client->invokeRemoteMethod(
        QString::fromStdString(objectName),
        QString::fromStdString(methodName),
        logosValueToQVariant(arg1),
        logosValueToQVariant(arg2),
        logosValueToQVariant(arg3));
    return logosValueFromQVariant(result);
}

LogosValue NativeLogosClient::invokeMethod(const std::string& objectName,
                                           const std::string& methodName,
                                           const LogosValue& arg1,
                                           const LogosValue& arg2,
                                           const LogosValue& arg3,
                                           const LogosValue& arg4)
{
    qDebug() << "[NATIVE API] NativeLogosClient: calling" << QString::fromStdString(objectName)
             << "." << QString::fromStdString(methodName) << "with 4 args (native types)";
    QVariant result = m_client->invokeRemoteMethod(
        QString::fromStdString(objectName),
        QString::fromStdString(methodName),
        logosValueToQVariant(arg1),
        logosValueToQVariant(arg2),
        logosValueToQVariant(arg3),
        logosValueToQVariant(arg4));
    return logosValueFromQVariant(result);
}

LogosValue NativeLogosClient::invokeMethod(const std::string& objectName,
                                           const std::string& methodName,
                                           const LogosValue& arg1,
                                           const LogosValue& arg2,
                                           const LogosValue& arg3,
                                           const LogosValue& arg4,
                                           const LogosValue& arg5)
{
    qDebug() << "[NATIVE API] NativeLogosClient: calling" << QString::fromStdString(objectName)
             << "." << QString::fromStdString(methodName) << "with 5 args (native types)";
    QVariant result = m_client->invokeRemoteMethod(
        QString::fromStdString(objectName),
        QString::fromStdString(methodName),
        logosValueToQVariant(arg1),
        logosValueToQVariant(arg2),
        logosValueToQVariant(arg3),
        logosValueToQVariant(arg4),
        logosValueToQVariant(arg5));
    return logosValueFromQVariant(result);
}

LogosObject* NativeLogosClient::requestObject(const std::string& objectName)
{
    qDebug() << "[NATIVE API] NativeLogosClient: requestObject" << QString::fromStdString(objectName);
    return m_client->requestObject(QString::fromStdString(objectName));
}

void NativeLogosClient::onEvent(LogosObject* origin, const std::string& eventName,
                                std::function<void(const std::string&, const std::vector<LogosValue>&)> callback)
{
    qDebug() << "[NATIVE API] NativeLogosClient: subscribing to event" << QString::fromStdString(eventName)
             << "(native callback)";
    m_client->onEvent(origin, QString::fromStdString(eventName),
        [callback](const QString& name, const QVariantList& data) {
            callback(name.toStdString(), logosValueListFromQVariantList(data));
        });
}

void NativeLogosClient::onEventResponse(LogosObject* object, const std::string& eventName,
                                        const std::vector<LogosValue>& data)
{
    qDebug() << "[NATIVE API] NativeLogosClient: emitting event response" << QString::fromStdString(eventName)
             << "(native types)";
    m_client->onEventResponse(object, QString::fromStdString(eventName),
                              logosValueListToQVariantList(data));
}
