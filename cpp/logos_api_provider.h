#ifndef LOGOS_API_PROVIDER_H
#define LOGOS_API_PROVIDER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QMap>
#include <memory>

class LogosTransportHost;
class ModuleProxy;

/**
 * @brief LogosAPIProvider handles registering objects for access by consumers
 * 
 * This class is responsible for the provider/server side functionality:
 * - Wrapping module objects with ModuleProxy
 * - Publishing them via the transport layer
 * - Handling event responses
 */
class LogosAPIProvider : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a new LogosAPIProvider
     * @param module_name The name of this module
     * @param parent Parent QObject
     */
    explicit LogosAPIProvider(const QString& module_name, QObject *parent = nullptr);
    
    /**
     * @brief Destructor - unpublishes registered objects
     */
    ~LogosAPIProvider();

    /**
     * @brief Register an object to be available for access by consumers
     * @param name The name to register the object under
     * @param object The object to register
     * @return true if registration successful, false otherwise
     */
    bool registerObject(const QString& name, QObject* object);

    /**
     * @brief Get the registry URL for this provider
     * @return QString containing the registry URL
     */
    QString registryUrl() const;

    /**
     * @brief Save a token from a module via the proxy
     * @param from_module_name The name of the module providing the token
     * @param token The token to save
     * @return bool true if token was saved successfully, false otherwise
     */
    bool saveToken(const QString& from_module_name, const QString& token);

public slots:
    /**
     * @brief Handle event responses from objects
     * @param replica The replica object that should receive the event
     * @param eventName The name of the event
     * @param data The event data
     */
    void onEventResponse(QObject* replica, const QString& eventName, const QVariantList& data);

private:
    std::unique_ptr<LogosTransportHost> m_transport;
    QString m_registryUrl;
    QMap<QString, QString> m_tokens;
    ModuleProxy* m_moduleProxy;
    QString m_registeredObjectName;
};

#endif // LOGOS_API_PROVIDER_H
