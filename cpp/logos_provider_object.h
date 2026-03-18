#ifndef LOGOS_PROVIDER_OBJECT_H
#define LOGOS_PROVIDER_OBJECT_H

#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QJsonArray>
#include <functional>

class LogosAPI;

// ---------------------------------------------------------------------------
// LogosProviderObject — abstract provider-side interface (framework internal)
//
// This is the provider-side counterpart of LogosObject (consumer side).
// ModuleProxy wraps a LogosProviderObject* and publishes it via the transport.
// Module authors do NOT implement this directly — they inherit LogosProviderBase.
// ---------------------------------------------------------------------------
class LogosProviderObject {
public:
    virtual ~LogosProviderObject() = default;

    using EventCallback = std::function<void(const QString&, const QVariantList&)>;

    virtual QVariant callMethod(const QString& methodName, const QVariantList& args) = 0;
    virtual bool informModuleToken(const QString& moduleName, const QString& token) = 0;
    virtual QJsonArray getMethods() = 0;
    virtual void setEventListener(EventCallback callback) = 0;
    virtual void init(void* apiInstance) = 0;
    virtual QString providerName() const = 0;
    virtual QString providerVersion() const = 0;
};

// ---------------------------------------------------------------------------
// LogosProviderBase — convenience base class for Qt-typed LOGOS_METHOD modules
//
// DEPRECATED: Use NativeProviderBase (in native/logos_native_provider.h) for
// new modules. NativeProviderBase provides the same functionality with Qt-free
// types (std::string, LogosValue, NativeLogosResult).
// ---------------------------------------------------------------------------
class [[deprecated("Use NativeProviderBase for new modules — see native/logos_native_provider.h")]]
LogosProviderBase : public LogosProviderObject {
public:
    // These two are implemented by generated code (logos_provider_dispatch.cpp):
    //   QVariant callMethod(const QString& methodName, const QVariantList& args) override;
    //   QJsonArray getMethods() override;

    void setEventListener(EventCallback callback) override { m_eventCallback = callback; }
    bool informModuleToken(const QString& moduleName, const QString& token) override;
    void init(void* apiInstance) override;

protected:
    void emitEvent(const QString& eventName, const QVariantList& data);
    virtual void onInit(LogosAPI* api) {}
    LogosAPI* logosAPI() const { return m_logosAPI; }

private:
    EventCallback m_eventCallback;
    LogosAPI* m_logosAPI = nullptr;
};

// ---------------------------------------------------------------------------
// LogosProviderPlugin — Qt interface for plugin loading
//
// New-API plugins implement this so the runtime can detect them via
// qobject_cast<LogosProviderPlugin*>() and use createProviderObject().
// ---------------------------------------------------------------------------
class LogosProviderPlugin {
public:
    virtual ~LogosProviderPlugin() = default;
    virtual LogosProviderObject* createProviderObject() = 0;
};

#define LogosProviderPlugin_iid "org.logos.LogosProviderPlugin"
Q_DECLARE_INTERFACE(LogosProviderPlugin, LogosProviderPlugin_iid)

// ---------------------------------------------------------------------------
// Macros — the developer-facing API
// ---------------------------------------------------------------------------

// LOGOS_PROVIDER: declares providerName/providerVersion and a private typedef.
// DEPRECATED: Use NATIVE_LOGOS_PROVIDER (in native/logos_native_provider.h) for
// new modules to avoid Qt type dependencies.
#define LOGOS_PROVIDER(ClassName, Name, Version)            \
public:                                                     \
    QString providerName() const override { return Name; }  \
    QString providerVersion() const override { return Version; } \
    QVariant callMethod(const QString& methodName, const QVariantList& args) override; \
    QJsonArray getMethods() override;                        \
private:                                                    \
    using _LogosProviderThisType = ClassName;

// LOGOS_METHOD macro — shared definition in native/logos_macros.h
#include "native/logos_macros.h"

#endif // LOGOS_PROVIDER_OBJECT_H
