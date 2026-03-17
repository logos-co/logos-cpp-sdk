#ifndef LOGOS_API_PROVIDER_H
#define LOGOS_API_PROVIDER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QMap>
#include <memory>

class LogosTransportHost;
class LogosObject;
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
    explicit LogosAPIProvider(const QString& module_name, QObject *parent = nullptr);
    ~LogosAPIProvider();

    /**
     * @brief Register an object to be available for access by consumers.
     *
     * The provider side remains Qt-based: plugins are QObjects loaded via
     * QPluginLoader.  Internally this wraps them in a ModuleProxy.
     */
    bool registerObject(const QString& name, QObject* object);

    QString registryUrl() const;
    bool saveToken(const QString& from_module_name, const QString& token);

public slots:
    /**
     * @brief Handle event responses from objects
     * @param object The LogosObject that should receive the event
     * @param eventName The name of the event
     * @param data The event data
     */
    void onEventResponse(LogosObject* object, const QString& eventName, const QVariantList& data);

private:
    std::unique_ptr<LogosTransportHost> m_transport;
    QString m_registryUrl;
    QMap<QString, QString> m_tokens;
    ModuleProxy* m_moduleProxy;
    QString m_registeredObjectName;
};

#endif // LOGOS_API_PROVIDER_H
