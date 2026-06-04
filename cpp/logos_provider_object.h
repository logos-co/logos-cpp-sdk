#ifndef LOGOS_PROVIDER_OBJECT_H
#define LOGOS_PROVIDER_OBJECT_H

#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QJsonArray>
#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <vector>

#include "logos_json_convert.h"

class LogosAPI;

// ---------------------------------------------------------------------------
// LogosProviderObject — abstract provider-side interface (framework internal)
//
// This is the provider-side counterpart of LogosObject (consumer side).
// ModuleProxy wraps a LogosProviderObject* and publishes it via the transport.
// Module authors do NOT implement this directly — they inherit LogosProviderBase.
//
// Two parallel virtual interfaces:
//   Qt interface:        callMethod / getMethods / setEventListener (pure virtual)
//   Universal interface: callMethodStd / getMethodsStd / setEventListenerStd (defaulted)
//
// Providers override ONE set. Those going Qt-free override the Std versions
// and delegate the Qt ones via the provided callMethodStdBridge / getMethodsStdBridge
// helpers (one-line overrides).
// ---------------------------------------------------------------------------
class LogosProviderObject {
public:
    virtual ~LogosProviderObject() = default;

    using EventCallback = std::function<void(const QString&, const QVariantList&)>;
    using UniversalEventCallback = std::function<void(const std::string&, const std::string&)>;

    // --- Qt interface (pure virtual — existing providers override these) ---
    virtual QVariant callMethod(const QString& methodName, const QVariantList& args) = 0;
    virtual bool informModuleToken(const QString& moduleName, const QString& token) = 0;
    virtual QJsonArray getMethods() = 0;
    // Event introspection — parallels getMethods(). Default empty so legacy Qt
    // providers and `provider`-interface modules (which have no logos_events:)
    // compile unchanged; universal modules' generated dispatch overrides it.
    virtual QJsonArray getEvents() { return QJsonArray(); }
    virtual void setEventListener(EventCallback callback) = 0;
    virtual void init(void* apiInstance) = 0;
    virtual QString providerName() const = 0;
    virtual QString providerVersion() const = 0;

    // --- Universal interface (override these to stay Qt-free) ---
    virtual nlohmann::json callMethodStd(const std::string& methodName, const nlohmann::json& args);
    virtual std::vector<LogosMethodMetadata> getMethodsStd();
    virtual void setEventListenerStd(UniversalEventCallback callback);

protected:
    // Bridging helpers for Qt-free providers: override callMethod/getMethods
    // with a one-liner delegating to these.
    QVariant callMethodStdBridge(const QString& methodName, const QVariantList& args);
    QJsonArray getMethodsStdBridge();
    void setEventListenerStdBridge(EventCallback callback);
};

// ---------------------------------------------------------------------------
// LogosProviderBase — convenience base class for new-API modules
//
// Handles framework plumbing so the developer only writes business logic.
// callMethod() and getMethods() are provided by generated code produced
// by logos-cpp-generator --provider-header (analogous to Qt MOC).
// ---------------------------------------------------------------------------
class LogosProviderBase : public LogosProviderObject {
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
// Place at the top of the class body (like Q_OBJECT).
#define LOGOS_PROVIDER(ClassName, Name, Version)            \
public:                                                     \
    QString providerName() const override { return Name; }  \
    QString providerVersion() const override { return Version; } \
    QVariant callMethod(const QString& methodName, const QVariantList& args) override; \
    QJsonArray getMethods() override;                        \
private:                                                    \
    using _LogosProviderThisType = ClassName;

// LOGOS_METHOD: marks a method as callable by the framework.
// Expands to nothing — scanned by logos-cpp-generator to produce
// callMethod() dispatch and getMethods() metadata (like Q_INVOKABLE + MOC).
#define LOGOS_METHOD

#endif // LOGOS_PROVIDER_OBJECT_H
