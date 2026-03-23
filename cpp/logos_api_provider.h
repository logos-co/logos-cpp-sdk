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
class LogosProviderObject;
class QtProviderObject;

/**
 * @brief LogosAPIProvider handles registering objects for access by consumers
 * 
 * Supports two registration paths:
 *   1. registerObject(name, QObject*)           — wraps in QtProviderObject, then ModuleProxy
 *   2. registerObject(name, LogosProviderObject*) — wraps directly in ModuleProxy
 * Both paths converge at ModuleProxy -> transport.
 */
class LogosAPIProvider : public QObject
{
    Q_OBJECT

public:
    explicit LogosAPIProvider(const QString& module_name, QObject *parent = nullptr);
    ~LogosAPIProvider();

    /**
     * @brief Register a legacy QObject-based plugin.
     * Wraps in QtProviderObject, then ModuleProxy.
     */
    bool registerObject(const QString& name, QObject* object);

    /**
     * @brief Register a new-API LogosProviderObject plugin.
     * Wraps directly in ModuleProxy.
     */
    bool registerObject(const QString& name, LogosProviderObject* provider);

    QString registryUrl() const;
    bool saveToken(const QString& from_module_name, const QString& token);

public slots:
    void onEventResponse(LogosObject* object, const QString& eventName, const QVariantList& data);

private:
    bool publishProvider(const QString& name, LogosProviderObject* provider);

    std::unique_ptr<LogosTransportHost> m_transport;
    QString m_registryUrl;
    QMap<QString, QString> m_tokens;
    ModuleProxy* m_moduleProxy;
    QtProviderObject* m_qtProviderObject;
    QString m_registeredObjectName;
};

#endif // LOGOS_API_PROVIDER_H
