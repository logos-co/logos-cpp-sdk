#include "local_transport.h"
#include "../../plugin_registry.h"
#include "../../module_proxy.h"
#include <QDebug>

// --- LocalTransportHost ---

bool LocalTransportHost::publishObject(const QString& name, QObject* object)
{
    PluginRegistry::registerPlugin(object, name);
    qDebug() << "LocalTransportHost: Published object:" << name;
    return true;
}

void LocalTransportHost::unpublishObject(const QString& name)
{
    if (!name.isEmpty()) {
        PluginRegistry::unregisterPlugin(name);
        qDebug() << "LocalTransportHost: Unpublished object:" << name;
    }
}

// --- LocalTransportConnection ---

bool LocalTransportConnection::connectToHost()
{
    qDebug() << "LocalTransportConnection: Local mode - no connection needed";
    return true;
}

bool LocalTransportConnection::isConnected() const
{
    return true;
}

bool LocalTransportConnection::reconnect()
{
    return true;
}

QObject* LocalTransportConnection::requestObject(const QString& objectName, int /*timeoutMs*/)
{
    QObject* plugin = PluginRegistry::getPlugin<QObject>(objectName);
    if (!plugin) {
        qWarning() << "LocalTransportConnection: Plugin not found in registry:" << objectName;
        return nullptr;
    }
    qDebug() << "LocalTransportConnection: Found plugin:" << objectName;
    return plugin;
}

void LocalTransportConnection::releaseObject(QObject* /*object*/)
{
    // Local mode: we don't own the object, so nothing to do
}

QVariant LocalTransportConnection::callRemoteMethod(QObject* object, const QString& authToken,
                                                     const QString& methodName, const QVariantList& args,
                                                     int /*timeoutMs*/)
{
    if (!object) {
        qWarning() << "LocalTransportConnection: Cannot call method on null object";
        return QVariant();
    }

    ModuleProxy* proxy = qobject_cast<ModuleProxy*>(object);
    if (!proxy) {
        qWarning() << "LocalTransportConnection: Object is not a ModuleProxy";
        return QVariant();
    }

    return proxy->callRemoteMethod(authToken, methodName, args);
}

bool LocalTransportConnection::callInformModuleToken(QObject* object, const QString& authToken,
                                                      const QString& moduleName, const QString& token,
                                                      int /*timeoutMs*/)
{
    if (!object) {
        qWarning() << "LocalTransportConnection: Cannot call informModuleToken on null object";
        return false;
    }

    ModuleProxy* proxy = qobject_cast<ModuleProxy*>(object);
    if (!proxy) {
        qWarning() << "LocalTransportConnection: Object is not a ModuleProxy";
        return false;
    }

    return proxy->informModuleToken(authToken, moduleName, token);
}
