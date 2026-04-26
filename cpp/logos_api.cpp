#include "logos_api.h"
#include "logos_api_client.h"
#include "logos_api_provider.h"
#include "token_manager.h"
#include <QVariant>
#include <string>

LogosAPI::LogosAPI(const QString& module_name, QObject *parent)
    : LogosAPI(module_name, LogosTransportSet{}, parent)
{
}

LogosAPI::LogosAPI(const QString& module_name,
                   LogosTransportSet transports,
                   QObject *parent)
    : QObject(parent)
    , m_module_name(module_name)
    , m_provider(nullptr)
    , m_token_manager(nullptr)
{
    m_provider = new LogosAPIProvider(m_module_name, std::move(transports), this);
    m_token_manager = &TokenManager::instance();
    qRegisterMetaType<LogosResult>("LogosResult");
}

LogosAPI::LogosAPI(const std::string& module_name, QObject *parent)
    : LogosAPI(QString::fromStdString(module_name), parent)
{
}

LogosAPI::~LogosAPI()
{
    // Provider and client will be automatically deleted as child objects
    // Token manager is a singleton, so we don't delete it


}

LogosAPIProvider* LogosAPI::getProvider() const
{
    return m_provider;
}

LogosAPIClient* LogosAPI::getClient(const QString& target_module) const
{
    // The no-transport overload is just shorthand for "use the
    // process-global default" — the explicit-transport overload below
    // is the single resolution path. Mode-awareness lives in the
    // factory, so this delegation preserves Mock/Local semantics.
    return getClient(target_module, LogosTransportConfigGlobal::getDefault());
}

LogosAPIClient* LogosAPI::getClient(const std::string& target_module) const
{
    return getClient(QString::fromStdString(target_module));
}

LogosAPIClient* LogosAPI::getClient(const QString& target_module,
                                    const LogosTransportConfig& transport) const
{
    // Single cache, single construction path. Key composition mirrors
    // the factory's resolution rule (see LogosAPIClientCacheKey in
    // logos_api.h):
    //   - Mock/Local mode: every cfg collapses to one cache slot per
    //     target — switching cfg returns the same MockTransport-backed
    //     client instead of allocating a duplicate.
    //   - Remote mode: every distinguishing field of cfg matters, so
    //     two callers with different TLS/codec settings get separate
    //     clients (no risk of silently reusing an insecure transport).
    const LogosAPIClientCacheKey key{
        target_module, LogosModeConfig::getMode(), transport};
    auto it = m_clients.constFind(key);
    if (it != m_clients.constEnd()) return it.value();

    LogosAPIClient* client = new LogosAPIClient(
        target_module, m_module_name, m_token_manager, transport,
        const_cast<LogosAPI*>(this));
    m_clients.insert(key, client);
    return client;
}

TokenManager* LogosAPI::getTokenManager() const
{
    return m_token_manager;
}

bool LogosAPI::setProperty(const char* name, const std::string& value)
{
    return QObject::setProperty(name, QVariant(QString::fromStdString(value)));
}
