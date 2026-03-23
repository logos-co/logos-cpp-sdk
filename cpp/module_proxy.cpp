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
            emit eventResponse(eventName, data);
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
