#include "logos_api_consumer.h"
#include "module_proxy.h"
#include "logos_api_client.h"
#include "token_manager.h"
#include "logos_mode.h"
#include "logos_instance.h"
#include "logos_transport.h"
#include "logos_transport_factory.h"
#include <QRemoteObjectPendingCall>
#include <QRemoteObjectPendingCallWatcher>
#include <QPointer>
#include <QTimer>
#include <QDebug>
#include <QUrl>
#include <QMetaObject>
#include <QTime>

LogosAPIConsumer::LogosAPIConsumer(const QString& module_to_talk_to, const QString& origin_module, TokenManager* token_manager, QObject *parent)
    : QObject(parent)
    , m_registryUrl(LogosInstance::id(module_to_talk_to))
    , m_token_manager(token_manager)
{
    m_transport = LogosTransportFactory::createConnection(m_registryUrl);
    m_transport->connectToHost();
}

LogosAPIConsumer::~LogosAPIConsumer()
{
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        QObject::disconnect(it.value());
    }
    m_eventCallbacks.clear();
    m_connections.clear();
}

QObject* LogosAPIConsumer::requestObject(const QString& objectName, Timeout timeout)
{
    qDebug() << "LogosAPIConsumer: Requesting object:" << objectName << "at" << QTime::currentTime().toString("hh:mm:ss.zzz");

    if (objectName.isEmpty()) {
        qWarning() << "LogosAPIConsumer: Object name cannot be empty";
        return nullptr;
    }

    if (!m_transport->isConnected()) {
        qWarning() << "LogosAPIConsumer: Not connected to registry. Cannot request object:" << objectName;
        return nullptr;
    }

    QObject* object = m_transport->requestObject(objectName, timeout.ms);
    if (object) {
        qDebug() << "LogosAPIConsumer: Successfully acquired object:" << objectName;
    }
    return object;
}

bool LogosAPIConsumer::isConnected() const
{
    return m_transport->isConnected();
}

QString LogosAPIConsumer::registryUrl() const
{
    return m_registryUrl;
}

bool LogosAPIConsumer::reconnect()
{
    qDebug() << "LogosAPIConsumer: Attempting to reconnect to registry:" << m_registryUrl;
    return m_transport->reconnect();
}

QVariant LogosAPIConsumer::invokeRemoteMethod(const QString& authToken, const QString& objectName, const QString& methodName,
                                   const QVariantList& args, Timeout timeout)
{
    qDebug() << "LogosAPIConsumer: Calling invokeRemoteMethod:" << objectName << methodName << "args_count:" << args.size() << "timeout:" << timeout.ms;

    QObject* plugin = m_transport->requestObject(objectName, timeout.ms);
    if (!plugin) {
        qWarning() << "LogosAPIConsumer: Failed to acquire plugin/replica for object:" << objectName;
        return QVariant();
    }

    QVariant result = m_transport->callRemoteMethod(plugin, authToken, methodName, args, timeout.ms);
    m_transport->releaseObject(plugin);
    return result;
}

void LogosAPIConsumer::invokeRemoteMethodAsync(const QString& authToken, const QString& objectName, const QString& methodName,
                                              const QVariantList& args,
                                              AsyncResultCallback callback,
                                              Timeout timeout)
{
    if (!callback) {
        qWarning() << "LogosAPIConsumer: invokeRemoteMethodAsync called with null callback";
        return;
    }

    QObject* plugin = requestObject(objectName, timeout);
    if (!plugin) {
        qWarning() << "LogosAPIConsumer: Failed to acquire plugin/replica for object:" << objectName;
        QTimer::singleShot(0, this, [callback]() { callback(QVariant()); });
        return;
    }

    ModuleProxy* moduleProxy = qobject_cast<ModuleProxy*>(plugin);
    if (moduleProxy) {
        QVariant result = moduleProxy->callRemoteMethod(authToken, methodName, args);
        if (!LogosModeConfig::isLocal()) {
            delete plugin;
        }
        QTimer::singleShot(0, this, [callback, result]() { callback(result); });
        return;
    }

    if (LogosModeConfig::isLocal()) {
        qWarning() << "LogosAPIConsumer: Local mode requires ModuleProxy-wrapped objects";
        QTimer::singleShot(0, this, [callback]() { callback(QVariant()); });
        return;
    }

    QRemoteObjectPendingCall pendingCall;
    bool success = QMetaObject::invokeMethod(
        plugin,
        "callRemoteMethod",
        Qt::DirectConnection,
        Q_RETURN_ARG(QRemoteObjectPendingCall, pendingCall),
        Q_ARG(QString, authToken),
        Q_ARG(QString, methodName),
        Q_ARG(QVariantList, args)
    );

    if (!success) {
        qWarning() << "LogosAPIConsumer: Failed to invoke callRemoteMethod on replica for object:" << objectName;
        delete plugin;
        QTimer::singleShot(0, this, [callback]() { callback(QVariant()); });
        return;
    }

    QRemoteObjectPendingCallWatcher* watcher = new QRemoteObjectPendingCallWatcher(pendingCall, this);
    QPointer<QRemoteObjectPendingCallWatcher> watcherRef(watcher);
    QObject* pluginToDelete = plugin;
    // Use QueuedConnection so the callback always runs on the consumer's (e.g. main) thread,
    // even if the watcher emits finished() from an IO or worker thread. Otherwise the callback
    // may never run or may run in a context where UI/consumer state is inaccessible.
    connect(watcher, &QRemoteObjectPendingCallWatcher::finished, this, [watcherRef, callback, pluginToDelete]() {
        QVariant result;
        if (!watcherRef.isNull() && watcherRef->error() == QRemoteObjectPendingCall::NoError) {
            result = watcherRef->returnValue();
        }
        if (callback) callback(result);
        delete pluginToDelete;
        if (!watcherRef.isNull()) watcherRef->deleteLater();
    }, Qt::QueuedConnection);
    QTimer::singleShot(timeout.ms, this, [watcherRef, callback, pluginToDelete]() {
        if (!watcherRef.isNull() && !watcherRef->isFinished()) {
            if (callback) callback(QVariant());
            delete pluginToDelete;
            watcherRef->deleteLater();
        }
    });
}

void LogosAPIConsumer::onEvent(QObject* originObject, QObject* destinationObject, const QString& eventName, std::function<void(const QString&, const QVariantList&)> callback)
{
    qDebug() << "LogosAPIConsumer: Registering event listener for event:" << eventName;

    m_eventCallbacks[eventName].append(callback);

    if (!m_connections.contains(originObject)) {
        auto connection = QObject::connect(originObject, SIGNAL(eventResponse(QString, QVariantList)),
                                          this, SLOT(invokeCallback(QString, QVariantList)));

        if (connection) {
            m_connections[originObject] = connection;
            qDebug() << "LogosAPIConsumer: Created new connection for origin object";
        } else {
            qWarning() << "LogosAPIConsumer: Failed to create connection for event:" << eventName;
        }
    } else {
        qDebug() << "LogosAPIConsumer: Reusing existing connection for origin object";
    }

    qDebug() << "LogosAPIConsumer: Registered callback for event:" << eventName;
}

void LogosAPIConsumer::invokeCallback(const QString& eventName, const QVariantList& data)
{
    for (const auto& callback : m_eventCallbacks[eventName]) {
        try {
            callback(eventName, data);
        } catch (...) {
            qWarning() << "LogosAPIConsumer: Exception in callback for event:" << eventName;
        }
    }
}

void LogosAPIConsumer::onEvent(QObject* originObject, QObject* destinationObject, const QString& eventName)
{
    qDebug() << "LogosAPIConsumer: Registering event listener for event:" << eventName << "(connecting to destination slot)";

    QObject::connect(originObject, SIGNAL(eventResponse(QString, QVariantList)),
                    destinationObject, SLOT(onEventResponse(QString, QVariantList)), Qt::AutoConnection);
}

bool LogosAPIConsumer::informModuleToken(const QString& authToken, const QString& moduleName, const QString& token)
{
    qDebug() << "LogosAPIConsumer: Informing module token for module:" << moduleName << "with token:" << token;

    QObject* plugin = m_transport->requestObject("capability_module", 20000);
    if (!plugin) {
        qWarning() << "LogosAPIConsumer: Failed to acquire plugin/replica for object: capability_module";
        return false;
    }

    bool result = m_transport->callInformModuleToken(plugin, authToken, moduleName, token, 20000);
    qDebug() << "LogosAPIConsumer: informModuleToken completed with result:" << result;
    m_transport->releaseObject(plugin);
    return result;
}

bool LogosAPIConsumer::informModuleToken_module(const QString& authToken, const QString& originModule, const QString& moduleName, const QString& token)
{
    qDebug() << "LogosAPIConsumer: Informing module token for module:" << moduleName << "with token:" << token;

    QObject* plugin = m_transport->requestObject(originModule, 20000);
    if (!plugin) {
        qWarning() << "LogosAPIConsumer: Failed to acquire plugin/replica for object:" << originModule;
        return false;
    }

    bool result = m_transport->callInformModuleToken(plugin, authToken, moduleName, token, 20000);
    qDebug() << "LogosAPIConsumer: informModuleToken completed with result:" << result;
    m_transport->releaseObject(plugin);
    return result;
}
