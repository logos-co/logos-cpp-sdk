#ifndef MODULE_PROXY_H
#define MODULE_PROXY_H

#include <QObject>
#include <QVariant>
#include <QVariantList>
#include <QHash>
#include <QString>
#include <QJsonArray>

class LogosProviderObject;

/**
 * @brief ModuleProxy wraps a LogosProviderObject and exposes it as a QObject
 *        so that Qt Remote Objects can publish it.
 *
 * All method dispatch, introspection, and event forwarding is delegated
 * to the underlying LogosProviderObject*.  For legacy QObject-based plugins,
 * that provider is a QtProviderObject adapter; for new-API plugins it is
 * the plugin's own LogosProviderObject subclass.
 */
class ModuleProxy : public QObject
{
    Q_OBJECT

public:
    explicit ModuleProxy(LogosProviderObject* provider, QObject* parent = nullptr);
    ~ModuleProxy();

    Q_INVOKABLE QVariant callRemoteMethod(const QString& authToken, const QString& methodName, const QVariantList& args = QVariantList());
    Q_INVOKABLE bool informModuleToken(const QString& authToken, const QString& moduleName, const QString& token);
    bool saveToken(const QString& from_module_name, const QString& token);
    Q_INVOKABLE QJsonArray getPluginMethods();

signals:
    void eventResponse(const QString& eventName, const QVariantList& data);

private:
    LogosProviderObject* m_provider;
    QHash<QString, QString> m_tokens;
};

#endif // MODULE_PROXY_H
