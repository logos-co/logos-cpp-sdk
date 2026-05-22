#include "module_proxy.h"
#include "logos_provider_object.h"
#include <QDebug>

ModuleProxy::ModuleProxy(LogosProviderObject* provider, QObject* parent)
    : QObject(parent)
    , m_provider(provider)
{
    if (m_provider) {
        m_provider->setEventListener([this](const QString& eventName, const QVariantList& data) {
            qDebug() << "[LogosProviderObject] ModuleProxy: forwarding event" << eventName << "as Qt signal";
            // Module events fire on the module's worker/FFI thread, not on this
            // QObject's (the remoting source's) thread. Emitting eventResponse
            // directly here runs QtRemoteObjects' source serialization on that
            // foreign thread, racing the source socket against a method reply
            // being sent from the source thread — which silently drops the
            // reply. That is why start(), which emits connectionStateChanged
            // mid-call, never returns to the caller while createNode (no event)
            // does. Marshal the emission onto this object's thread so events
            // and replies are serialized on the single thread QtRemoteObjects
            // expects to own the source.
            QMetaObject::invokeMethod(this, [this, eventName, data]() {
                emit eventResponse(eventName, data);
            }, Qt::QueuedConnection);
        });
        qDebug() << "[LogosProviderObject] ModuleProxy: created, wrapping LogosProviderObject"
                 << m_provider->providerName();
    }
}

ModuleProxy::~ModuleProxy()
{
    qDebug() << "ModuleProxy: destroyed";
}

bool ModuleProxy::saveToken(const QString& from_module_name, const QString& token)
{
    if (from_module_name.isEmpty()) {
        qWarning() << "ModuleProxy: Cannot save token with empty module name";
        return false;
    }
    if (token.isEmpty()) {
        qWarning() << "ModuleProxy: Cannot save empty token for module:" << from_module_name;
        return false;
    }

    m_tokens[from_module_name] = token;
    qDebug() << "ModuleProxy: Token saved for module:" << from_module_name;
    return true;
}

QVariant ModuleProxy::callRemoteMethod(const QString& authToken, const QString& methodName, const QVariantList& args)
{
    if (!m_provider) {
        qWarning() << "ModuleProxy: Cannot call method on null provider:" << methodName;
        return QVariant();
    }

    if (methodName.isEmpty()) {
        qWarning() << "ModuleProxy: Method name cannot be empty";
        return QVariant();
    }

    if (methodName == "getPluginMethods" && args.isEmpty()) {
        return QVariant(getPluginMethods());
    }

    qDebug() << "ModuleProxy: callRemoteMethod" << methodName << "args:" << args;
    return m_provider->callMethod(methodName, args);
}

bool ModuleProxy::informModuleToken(const QString& authToken, const QString& moduleName, const QString& token)
{
    Q_UNUSED(authToken)

    if (!m_provider) {
        qWarning() << "ModuleProxy: Cannot inform token on null provider";
        return false;
    }

    return m_provider->informModuleToken(moduleName, token);
}

QJsonArray ModuleProxy::getPluginMethods()
{
    if (!m_provider) return QJsonArray();

    qDebug() << "[LogosProviderObject] ModuleProxy: calling LogosProviderObject::getMethods()";
    return m_provider->getMethods();
}

#include "moc_module_proxy.cpp"
