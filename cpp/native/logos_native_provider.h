#ifndef LOGOS_NATIVE_PROVIDER_H
#define LOGOS_NATIVE_PROVIDER_H

#include "logos_value.h"
#include "logos_native_types.h"
#include "logos_macros.h"

#include <functional>
#include <string>
#include <vector>

class NativeLogosAPI;

// ---------------------------------------------------------------------------
// NativeProviderObject — Qt-free abstract provider interface
//
// Parallel to LogosProviderObject but uses native types exclusively.
// The adapter layer (NativeProviderAdapter) bridges this to the Qt runtime.
// ---------------------------------------------------------------------------
class NativeProviderObject {
public:
    virtual ~NativeProviderObject() = default;

    using EventCallback = std::function<void(const std::string&, const std::vector<LogosValue>&)>;

    virtual LogosValue callMethod(const std::string& methodName,
                                  const std::vector<LogosValue>& args) = 0;
    virtual std::string getMethodsJson() = 0;
    virtual void setEventListener(EventCallback callback) = 0;
    virtual void init(void* apiInstance) = 0;
    virtual bool informModuleToken(const std::string& moduleName, const std::string& token) = 0;
    virtual std::string providerName() const = 0;
    virtual std::string providerVersion() const = 0;
};

// ---------------------------------------------------------------------------
// NativeProviderBase — convenience base class for native modules
//
// Handles framework plumbing. callMethod() and getMethodsJson() are
// provided by generated code from logos-native-generator --provider-dispatch.
// ---------------------------------------------------------------------------
class NativeProviderBase : public NativeProviderObject {
public:
    void setEventListener(EventCallback callback) override;
    bool informModuleToken(const std::string& moduleName, const std::string& token) override;
    void init(void* apiInstance) override;

protected:
    void emitEvent(const std::string& eventName, const std::vector<LogosValue>& data);
    virtual void onInit(NativeLogosAPI* api) {}
    NativeLogosAPI* logosAPI() const { return m_logosAPI; }

private:
    EventCallback m_eventCallback;
    NativeLogosAPI* m_logosAPI = nullptr;
};

// ---------------------------------------------------------------------------
// NATIVE_LOGOS_PROVIDER — macro for native module classes
// ---------------------------------------------------------------------------
#define NATIVE_LOGOS_PROVIDER(ClassName, Name, Version)                        \
public:                                                                        \
    std::string providerName() const override { return Name; }                 \
    std::string providerVersion() const override { return Version; }           \
    LogosValue callMethod(const std::string& methodName,                       \
                          const std::vector<LogosValue>& args) override;       \
    std::string getMethodsJson() override;                                     \
private:                                                                       \
    using _LogosProviderThisType = ClassName;

// ---------------------------------------------------------------------------
// NativeProviderPlugin — interface for Qt plugin detection
//
// The module's thin QObject loader implements this so the runtime can
// detect it via qobject_cast and call createNativeProviderObject().
// The Q_DECLARE_INTERFACE macro goes in the loader header, not here.
// ---------------------------------------------------------------------------
class NativeProviderPlugin {
public:
    virtual ~NativeProviderPlugin() = default;
    virtual NativeProviderObject* createNativeProviderObject() = 0;
};

#define NativeProviderPlugin_iid "org.logos.NativeProviderPlugin"

#endif // LOGOS_NATIVE_PROVIDER_H
