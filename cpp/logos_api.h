#ifndef LOGOS_API_H
#define LOGOS_API_H

#include "logos_transport_config.h"
#include "logos_types.h"

#include <QObject>
#include <QString>
#include <QHash>
#include <string>

class LogosAPIClient;
class LogosAPIProvider;
class TokenManager;

/**
 * @brief LogosAPI provides a unified interface to the Logos SDK
 * 
 * This class initializes and keeps instances of the client provider and token manager.
 */
class LogosAPI : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new LogosAPI instance
     * @param module_name The name of this module
     * @param parent Parent QObject
     */
    explicit LogosAPI(const QString& module_name, QObject *parent = nullptr);

    /**
     * @brief Construct a new LogosAPI with an explicit transport set.
     *
     * `transports` is empty ⇒ use the process-global default (back-compat).
     * Non-empty ⇒ provider publishes on every configured transport
     * (e.g. a daemon listing both LocalSocket and TCP+SSL so the CLI has
     * a fast in-process path *and* remote clients have a secure path).
     */
    LogosAPI(const QString& module_name,
             LogosTransportSet transports,
             QObject *parent = nullptr);

    /**
     * @brief Construct a new LogosAPI instance (const char* overload — resolves ambiguity)
     */
    explicit LogosAPI(const char* module_name, QObject *parent = nullptr)
        : LogosAPI(QString(module_name), parent) {}

    /**
     * @brief Construct a new LogosAPI instance (std::string overload)
     */
    explicit LogosAPI(const std::string& module_name, QObject *parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~LogosAPI();

    /**
     * @brief Get the client provider instance
     * @return LogosAPIProvider* Pointer to the provider
     */
    LogosAPIProvider* getProvider() const;

    /**
     * @brief Get the client instance for communicating with a module
     * @param target_module The module to communicate with
     * @return LogosAPIClient* Pointer to the client
     */
    LogosAPIClient* getClient(const QString& target_module) const;

    /**
     * @brief Get the client instance — const char* overload (resolves ambiguity)
     */
    LogosAPIClient* getClient(const char* target_module) const
        { return getClient(QString(target_module)); }

    /**
     * @brief Get the client instance for communicating with a module (std::string overload)
     */
    LogosAPIClient* getClient(const std::string& target_module) const;

    /**
     * @brief Get the token manager instance
     * @return TokenManager* Pointer to the token manager
     */
    TokenManager* getTokenManager() const;

    using QObject::setProperty;

    /**
     * @brief Set a dynamic property from a UTF-8 std::string (delegates to QVariant + QString).
     */
    bool setProperty(const char* name, const std::string& value);

private:
    QString m_module_name;
    LogosAPIProvider* m_provider;
    mutable QHash<QString, LogosAPIClient*> m_clients;  // Cache of clients per target module
    TokenManager* m_token_manager;
};

#endif // LOGOS_API_H