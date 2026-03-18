#include "logos_api_provider.h"
#include "logos_object.h"
#include "logos_provider_object.h"
#include "native/logos_native_provider.h"
#include "native/logos_native_adapter.h"
#include "qt_provider_object.h"
#include "module_proxy.h"
#include "logos_api.h"
#include "logos_instance.h"
#include "logos_transport.h"
#include "logos_transport_factory.h"
#include <QDebug>

LogosAPIProvider::LogosAPIProvider(const QString& module_name, QObject *parent)
    : QObject(parent)
    , m_registryUrl(LogosInstance::id(module_name))
    , m_moduleProxy(nullptr)
    , m_qtProviderObject(nullptr)
{
    m_transport = LogosTransportFactory::createHost(m_registryUrl);
}

LogosAPIProvider::~LogosAPIProvider()
{
    if (!m_registeredObjectName.isEmpty()) {
        m_transport->unpublishObject(m_registeredObjectName);
    }
}

// QObject* path: auto-detects LogosProviderPlugin; falls back to QtProviderObject wrapper
bool LogosAPIProvider::registerObject(const QString& name, QObject* object)
{
    if (!object) {
        qWarning() << "LogosAPIProvider: Cannot register null object";
        return false;
    }

    if (name.isEmpty()) {
        qWarning() << "LogosAPIProvider: Cannot register object with empty name";
        return false;
    }

    if (m_moduleProxy) {
        qCritical() << "LogosAPIProvider: Object already registered. Only one registration per provider is allowed";
        return false;
    }

    // Check for NativeProviderPlugin first (new native API)
    NativeProviderPlugin* nativePlugin = qobject_cast<NativeProviderPlugin*>(object);
    if (nativePlugin) {
        qDebug() << "[NATIVE API] Module" << name << "uses NativeProviderPlugin (Qt-free native types)";
        NativeProviderObject* native = nativePlugin->createNativeProviderObject();
        if (native) {
            auto* adapter = new NativeProviderAdapter(native);
            return registerObject(name, adapter);
        }
        qWarning() << "[NATIVE API] Module" << name << ": createNativeProviderObject() returned null";
    }

    // Check if this plugin implements LogosProviderPlugin (Qt-typed new API)
    LogosProviderPlugin* providerPlugin = qobject_cast<LogosProviderPlugin*>(object);
    if (providerPlugin) {
        qDebug() << "[QT API] Module" << name << "uses LogosProviderPlugin (DEPRECATED — Qt-typed provider)";
        LogosProviderObject* provider = providerPlugin->createProviderObject();
        if (provider) {
            return registerObject(name, provider);
        }
        qWarning() << "[QT API] Module" << name << ": createProviderObject() returned null";
    }

    // Legacy path: wrap QObject in QtProviderObject adapter
    qDebug() << "[QT API] Module" << name << "uses Q_INVOKABLE (DEPRECATED — legacy QObject path)";

    m_qtProviderObject = new QtProviderObject(object, this);
    m_qtProviderObject->init(qobject_cast<LogosAPI*>(parent()));

    return publishProvider(name, m_qtProviderObject);
}

// New path: LogosProviderObject* -> ModuleProxy -> transport
bool LogosAPIProvider::registerObject(const QString& name, LogosProviderObject* provider)
{
    if (!provider) {
        qWarning() << "LogosAPIProvider: Cannot register null provider";
        return false;
    }

    if (name.isEmpty()) {
        qWarning() << "LogosAPIProvider: Cannot register provider with empty name";
        return false;
    }

    if (m_moduleProxy) {
        qCritical() << "LogosAPIProvider: Object already registered. Only one registration per provider is allowed";
        return false;
    }

    qDebug() << "LogosAPIProvider: registering LogosProviderObject directly for" << name;

    provider->init(qobject_cast<LogosAPI*>(parent()));

    return publishProvider(name, provider);
}

bool LogosAPIProvider::publishProvider(const QString& name, LogosProviderObject* provider)
{
    m_moduleProxy = new ModuleProxy(provider, this);

    bool success = m_transport->publishObject(name, m_moduleProxy);
    if (success) {
        m_registeredObjectName = name;
        qDebug() << "LogosAPIProvider: successfully published" << name;
    } else {
        qCritical() << "LogosAPIProvider: Failed to publish" << name;
    }

    return success;
}

QString LogosAPIProvider::registryUrl() const
{
    return m_registryUrl;
}

bool LogosAPIProvider::saveToken(const QString& from_module_name, const QString& token)
{
    if (!m_moduleProxy) {
        qWarning() << "LogosAPIProvider: Cannot save token - no module proxy available";
        return false;
    }

    qDebug() << "LogosAPIProvider: Delegating saveToken to module proxy for:" << from_module_name;
    return m_moduleProxy->saveToken(from_module_name, token);
}

void LogosAPIProvider::onEventResponse(LogosObject* object, const QString& eventName, const QVariantList& data)
{
    qDebug() << "[LogosObject] LogosAPIProvider::onEventResponse" << eventName << "-> LogosObject::emitEvent";

    if (eventName.isEmpty()) {
        qWarning() << "LogosAPIProvider: Event name cannot be empty";
        return;
    }
    if (!object) {
        qWarning() << "LogosAPIProvider: Cannot emit event on null object";
        return;
    }

    object->emitEvent(eventName, data);
}
