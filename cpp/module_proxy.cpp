#include "module_proxy.h"
#include "logos_provider_object.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonValue>

ModuleProxy::ModuleProxy(LogosProviderObject* provider, QObject* parent)
    : QObject(parent)
    , m_provider(provider)
{
    if (m_provider) {
        m_provider->setEventListener([this](const QString& eventName, const QVariantList& data) {
            qDebug() << "[LogosProviderObject] ModuleProxy: forwarding event" << eventName << "as Qt signal";
            // Events may be fired from any thread (e.g. a module's worker/FFI
            // thread), but this object is the QtRemoteObjects source and must be
            // driven from its own thread. Emitting directly from a foreign
            // thread runs QtRO's source serialization there, racing the source
            // socket against a reply being sent from the source thread, which
            // can silently drop the reply. AutoConnection keeps same-thread
            // callers synchronous (the common case) and only queues the
            // emission when it arrives from another thread, so events and
            // replies stay serialized on the thread QtRO expects to own the
            // source. Passing `this` as the context also cancels a queued
            // emission if this object is destroyed first.
            QMetaObject::invokeMethod(this, [this, eventName, data]() {
                emit eventResponse(eventName, data);
            }, Qt::AutoConnection);
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

    if (methodName == "getPluginEvents" && args.isEmpty()) {
        return QVariant(getPluginEvents());
    }

    if (methodName == "getPluginInterface" && args.isEmpty()) {
        return QVariant(getPluginInterface());
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

namespace {
// getMethods() returns the module's full interface — both methods and events,
// each tagged with a "type" ("method"/"event"). Split it back out. An entry
// with no "type" counts as a method, so modules built against the pre-events
// SDK (whose getMethods() contains no events) report zero events, not a crash.
QJsonArray filterInterface(const QJsonArray& interface, bool keepEvents)
{
    QJsonArray out;
    for (const QJsonValue& v : interface) {
        const bool isEvent =
            v.toObject().value(QStringLiteral("type")).toString() == QStringLiteral("event");
        if (isEvent == keepEvents) out.append(v);
    }
    return out;
}
} // namespace

QJsonArray ModuleProxy::getPluginInterface()
{
    if (!m_provider) return QJsonArray();

    qDebug() << "[LogosProviderObject] ModuleProxy: calling LogosProviderObject::getMethods()";
    return m_provider->getMethods();
}

QJsonArray ModuleProxy::getPluginMethods()
{
    return filterInterface(getPluginInterface(), /*keepEvents=*/false);
}

QJsonArray ModuleProxy::getPluginEvents()
{
    return filterInterface(getPluginInterface(), /*keepEvents=*/true);
}

#include "moc_module_proxy.cpp"
