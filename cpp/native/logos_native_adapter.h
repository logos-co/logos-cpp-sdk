#ifndef LOGOS_NATIVE_ADAPTER_H
#define LOGOS_NATIVE_ADAPTER_H

#include "../logos_provider_object.h"
#include "logos_native_provider.h"

Q_DECLARE_INTERFACE(NativeProviderPlugin, NativeProviderPlugin_iid)

// ---------------------------------------------------------------------------
// NativeProviderAdapter — bridges NativeProviderObject to LogosProviderObject
//
// Wraps a NativeProviderObject and presents it as a LogosProviderObject so
// the existing Qt-based runtime (ModuleProxy, transport) can use it unchanged.
// All type conversions (LogosValue <-> QVariant) happen here.
// ---------------------------------------------------------------------------
class NativeProviderAdapter : public LogosProviderObject {
public:
    explicit NativeProviderAdapter(NativeProviderObject* native);

    QVariant callMethod(const QString& methodName, const QVariantList& args) override;
    QJsonArray getMethods() override;
    void setEventListener(EventCallback callback) override;
    void init(void* apiInstance) override;
    QString providerName() const override;
    QString providerVersion() const override;
    bool informModuleToken(const QString& moduleName, const QString& token) override;

private:
    NativeProviderObject* m_native;
};

#endif // LOGOS_NATIVE_ADAPTER_H
