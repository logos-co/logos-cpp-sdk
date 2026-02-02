#include "logos_api_consumer.h"
#include "../logos-cpp-sdk/provider/module_proxy.h"
#include "../logos-cpp-sdk/client/logos_api_client.h"
#include "logos_mode.h"
#include "plugin_registry.h"
#include <QRemoteObjectNode>
#include <QRemoteObjectReplica>
#include <QRemoteObjectPendingCall>
#include <QDebug>
#include <QUrl>
#include <QMetaObject>
#include <QTime>
#include <string>

LogosAPIConsumer::LogosAPIConsumer(const QString& module_to_talk_to, const QString& origin_module, QObject *parent)
    : QObject(parent)
    , m_node(nullptr)
    , m_registryUrl(QString("local:logos_%1").arg(module_to_talk_to))
    , m_connected(false)
{
    if (LogosModeConfig::isLocal()) {
        qDebug() << "LogosAPIConsumer: Using Local mode - skipping QRemoteObjectNode";
        m_connected = true;
    } else {
        m_node = new QRemoteObjectNode(this);
        connectToRegistry();
    }
}

LogosAPIConsumer::~LogosAPIConsumer()
{
    // Clean up event callbacks and connections
    for (auto it = m_connections.begin(); it != m_connections.end(); ++it) {
        QObject::disconnect(it.value());
    }
    m_eventCallbacks.clear();
    m_connections.clear();

    // QRemoteObjectNode will be deleted automatically as it's a child object
}

QObject* LogosAPIConsumer::requestObject(const QString& objectName, Timeout timeout)
{
    qDebug() << "LogosAPIConsumer: Requesting object:" << objectName << "at" << QTime::currentTime().toString("hh:mm:ss.zzz");

    if (objectName.isEmpty()) {
        qWarning() << "LogosAPIConsumer: Object name cannot be empty";
        return nullptr;
    }

    if (LogosModeConfig::isLocal()) {
        QObject* plugin = PluginRegistry::getPlugin<QObject>(objectName);
        if (!plugin) {
            qWarning() << "LogosAPIConsumer: Plugin not found in registry:" << objectName;
            return nullptr;
        }
        qDebug() << "LogosAPIConsumer: Successfully found plugin:" << objectName;
        return plugin;
    }

    if (!m_connected) {
        qWarning() << "LogosAPIConsumer: Not connected to registry. Cannot request object:" << objectName;
        return nullptr;
    }

    qDebug() << "LogosAPIConsumer: Requesting object:" << objectName;

    // Acquire the dynamic replica
    QRemoteObjectReplica* replica = m_node->acquireDynamic(objectName);
    if (!replica) {
        qWarning() << "LogosAPIConsumer: Failed to acquire replica for object:" << objectName;
        return nullptr;
    }

    // Wait for the replica to be initialized
    if (!replica->waitForSource(timeout.ms)) {
        qWarning() << "LogosAPIConsumer: Timeout waiting for object replica to be ready:" << objectName;
        delete replica;
        return nullptr;
    }

    qDebug() << "LogosAPIConsumer: Successfully acquired replica for object:" << objectName;
    qDebug() << "LogosAPIConsumer: Replica acquired at" << QTime::currentTime().toString("hh:mm:ss.zzz");
    return replica;
}

bool LogosAPIConsumer::isConnected() const
{
    return m_connected;
}

QString LogosAPIConsumer::registryUrl() const
{
    return m_registryUrl;
}

bool LogosAPIConsumer::reconnect()
{
    if (LogosModeConfig::isLocal()) {
        m_connected = true;
        return true;
    }

    qDebug() << "LogosAPIConsumer: Attempting to reconnect to registry:" << m_registryUrl;

    // Disconnect first if already connected
    if (m_connected) {
        // Note: QRemoteObjectNode doesn't have a direct disconnect method
        // We'll create a new node instead
        m_node->deleteLater();
        m_node = new QRemoteObjectNode(this);
        m_connected = false;
    }

    return connectToRegistry();
}

bool LogosAPIConsumer::connectToRegistry()
{
    if (!m_node) {
        qWarning() << "LogosAPIConsumer: Remote object node is null";
        return false;
    }

    if (m_registryUrl.isEmpty()) {
        qWarning() << "LogosAPIConsumer: Registry URL is empty";
        return false;
    }

    qDebug() << "LogosAPIConsumer: Connecting to registry:" << m_registryUrl;
    qDebug() << "LogosAPIConsumer: Connecting to registry at" << QTime::currentTime().toString("hh:mm:ss.zzz");

    // Connect to the registry node
    QUrl url(m_registryUrl);
    bool success = m_node->connectToNode(url);

    if (success) {
        m_connected = true;
        qDebug() << "LogosAPIConsumer: Successfully connected to registry:" << m_registryUrl;
    } else {
        m_connected = false;
        qWarning() << "LogosAPIConsumer: Failed to connect to registry:" << m_registryUrl;
    }
    qDebug() << "LogosAPIConsumer: Connected to registry at" << QTime::currentTime().toString("hh:mm:ss.zzz");

    return m_connected;
}



QVariant LogosAPIConsumer::invokeRemoteMethod(const QString& authToken, const QString& objectName, const QString& methodName, 
                                   const QVariantList& args, Timeout timeout)
{
    qDebug() << "LogosAPIConsumer: Calling invokeRemoteMethod with params:" << authToken << objectName << methodName << args << timeout.ms;

    // This method handles both ModuleProxy-wrapped modules (template_module, package_manager) 
    // and direct remote object calls for other modules
    QObject* plugin = requestObject(objectName, timeout);
    if (!plugin) {
        qWarning() << "LogosAPIConsumer: Failed to acquire plugin/replica for object:" << objectName;
        return QVariant();
    }

    ModuleProxy* moduleProxy = qobject_cast<ModuleProxy*>(plugin);
    if (moduleProxy) {
        QVariant result = moduleProxy->callRemoteMethod(authToken, methodName, args);
        if (!LogosModeConfig::isLocal()) {
            delete plugin;
        }
        return result;
    }

    if (LogosModeConfig::isLocal()) {
        qWarning() << "LogosAPIConsumer: Local mode requires ModuleProxy-wrapped objects";
        return QVariant();
    }

    // Remote mode: callRemoteMethod returns QRemoteObjectPendingCall, not QVariant
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
        return QVariant();
    }

    // Wait for the result
    pendingCall.waitForFinished(timeout.ms);
    delete plugin;

    if (!pendingCall.isFinished() || pendingCall.error() != QRemoteObjectPendingCall::NoError) {
        qWarning() << "LogosAPIConsumer: Remote callRemoteMethod failed or timed out:" << pendingCall.error();
        return QVariant();
    }

    return pendingCall.returnValue();
}

void LogosAPIConsumer::onEvent(QObject* originObject, QObject* destinationObject, const QString& eventName, std::function<void(const QString&, const QVariantList&)> callback)
{
    qDebug() << "LogosAPIConsumer: Registering event listener for event:" << eventName;

    // Store the callback for this event name
    m_eventCallbacks[eventName].append(callback);

    // Check if we already have a connection for this origin object
    if (!m_connections.contains(originObject)) {
        // Create new connection only if it doesn't exist
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
    // qDebug() << "LogosAPIConsumer: invokeCallback called for event:" << eventName;

    // Call all registered callbacks
    // Note: This will call all callbacks for any event. In a more sophisticated implementation,
    // you might want to store event names with callbacks to filter them.
    for (const auto& callback : m_eventCallbacks[eventName]) {
        try {
            callback(eventName, data);
        } catch (...) {
            qWarning() << "LogosAPIConsumer: Exception in callback for event:" << eventName;
        }
    }

    // qDebug() << "LogosAPIConsumer: Called" << m_eventCallbacks[eventName].size() << "callbacks for event:" << eventName;
}

void LogosAPIConsumer::onEvent(QObject* originObject, QObject* destinationObject, const QString& eventName)
{
    qDebug() << "LogosAPIConsumer: Registering event listener for event:" << eventName << "(connecting to destination slot)";

    // connect to the eventResponse signal of the destinationObject's slot
    QObject::connect(originObject, SIGNAL(eventResponse(QString, QVariantList)), 
                    destinationObject, SLOT(onEventResponse(QString, QVariantList)), Qt::AutoConnection);
}

bool LogosAPIConsumer::informModuleToken(const QString& authToken, const QString& moduleName, const QString& token)
{
    qDebug() << "LogosAPIConsumer: Informing module token for module:" << moduleName << "with token:" << token;

    QObject* plugin = requestObject("capability_module", Timeout(20000));
    if (!plugin) {
        qWarning() << "LogosAPIConsumer: Failed to acquire plugin/replica for object: capability_module";
        return false;
    }

    ModuleProxy* moduleProxy = qobject_cast<ModuleProxy*>(plugin);
    if (moduleProxy) {
        bool result = moduleProxy->informModuleToken(authToken, moduleName, token);
        qDebug() << "LogosAPIConsumer: informModuleToken completed with result:" << result;
        if (!LogosModeConfig::isLocal()) {
            delete plugin;
        }
        return result;
    }

    if (LogosModeConfig::isLocal()) {
        qWarning() << "LogosAPIConsumer: Local mode requires ModuleProxy-wrapped objects";
        return false;
    }

    QRemoteObjectPendingCall pendingCall;
    bool success = QMetaObject::invokeMethod(
        plugin,
        "informModuleToken",
        Qt::DirectConnection,
        Q_RETURN_ARG(QRemoteObjectPendingCall, pendingCall),
        Q_ARG(QString, authToken),
        Q_ARG(QString, moduleName),
        Q_ARG(QString, token)
    );

    if (!success) {
        qWarning() << "LogosAPIConsumer: Failed to invoke informModuleToken on replica";
        delete plugin;
        return false;
    }

    pendingCall.waitForFinished(20000);
    delete plugin;

    if (!pendingCall.isFinished() || pendingCall.error() != QRemoteObjectPendingCall::NoError) {
        qWarning() << "LogosAPIConsumer: Remote informModuleToken failed or timed out:" << pendingCall.error();
        return false;
    }

    QVariant result = pendingCall.returnValue();
    qDebug() << "LogosAPIConsumer: informModuleToken completed with result:" << result;

    return result.toBool();
}

bool LogosAPIConsumer::informModuleToken_module(const QString& authToken, const QString& originModule, const QString& moduleName, const QString& token)
{
    qDebug() << "LogosAPIConsumer: Informing module token for module:" << moduleName << "with token:" << token;

    QObject* plugin = requestObject(originModule, Timeout(20000));
    if (!plugin) {
        qWarning() << "LogosAPIConsumer: Failed to acquire plugin/replica for object:" << originModule;
        return false;
    }

    ModuleProxy* moduleProxy = qobject_cast<ModuleProxy*>(plugin);
    if (moduleProxy) {
        bool result = moduleProxy->informModuleToken(authToken, moduleName, token);
        qDebug() << "LogosAPIConsumer: informModuleToken completed with result:" << result;
        if (!LogosModeConfig::isLocal()) {
            delete plugin;
        }
        return result;
    }

    if (LogosModeConfig::isLocal()) {
        qWarning() << "LogosAPIConsumer: Local mode requires ModuleProxy-wrapped objects";
        return false;
    }
    QRemoteObjectPendingCall pendingCall;
    bool success = QMetaObject::invokeMethod(
        plugin,
        "informModuleToken",
        Qt::DirectConnection,
        Q_RETURN_ARG(QRemoteObjectPendingCall, pendingCall),
        Q_ARG(QString, authToken),
        Q_ARG(QString, moduleName),
        Q_ARG(QString, token)
    );

    if (!success) {
        qWarning() << "LogosAPIConsumer: Failed to invoke informModuleToken on replica";
        delete plugin;
        return false;
    }

    pendingCall.waitForFinished(20000);
    delete plugin;

    if (!pendingCall.isFinished() || pendingCall.error() != QRemoteObjectPendingCall::NoError) {
        qWarning() << "LogosAPIConsumer: Remote informModuleToken failed or timed out:" << pendingCall.error();
        return false;
    }

    QVariant result = pendingCall.returnValue();
    qDebug() << "LogosAPIConsumer: informModuleToken completed with result:" << result;

    return result.toBool();
}
