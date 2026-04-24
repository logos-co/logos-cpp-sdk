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
    // Check if we already have a client for this target module
    if (m_clients.contains(target_module)) {
        return m_clients.value(target_module);
    }
    
    // Create a new client for this target module
    LogosAPIClient* client = new LogosAPIClient(target_module, m_module_name, m_token_manager, const_cast<LogosAPI*>(this));
    
    // Cache it for future use
    m_clients.insert(target_module, client);
    
    return client;
}

LogosAPIClient* LogosAPI::getClient(const std::string& target_module) const
{
    return getClient(QString::fromStdString(target_module));
}

LogosAPIClient* LogosAPI::getClient(const QString& target_module,
                                    const LogosTransportConfig& transport) const
{
    // Separate cache from the default-transport path. Caching by
    // (target, protocol) keeps `getClient(x, tcp_ssl)` and
    // `getClient(x, local)` from aliasing onto the same object, which
    // would double-open connections / confuse reuse.
    const QString key = target_module + "#" +
        QString::number(static_cast<int>(transport.protocol)) + ":" +
        QString::fromStdString(transport.host) + ":" +
        QString::number(transport.port);
    if (m_clientsByTransport.contains(key))
        return m_clientsByTransport.value(key);

    LogosAPIClient* client = new LogosAPIClient(
        target_module, m_module_name, m_token_manager, transport,
        const_cast<LogosAPI*>(this));
    m_clientsByTransport.insert(key, client);
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
