#ifndef LOGOS_CORE_CLIENT_H
#define LOGOS_CORE_CLIENT_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QHash>
#include <functional>

class LogosAPI;
class LogosAPIClient;
class LogosObject;

/**
 * @brief LogosCoreClient provides a high-level async interface for calling
 * plugin methods and subscribing to events from the "core" (host) perspective.
 *
 * Unlike creating ephemeral LogosAPI instances per call, this class maintains
 * a single persistent LogosAPI("core") and reuses cached client connections.
 *
 * It uses LogosAPIClient::invokeRemoteMethodAsync for connection-aware calls,
 * eliminating manual QTimer-based connection delays.
 */
class LogosCoreClient : public QObject
{
    Q_OBJECT

public:
    using AsyncCallback = std::function<void(bool success, const QString& message)>;

    explicit LogosCoreClient(QObject* parent = nullptr);
    ~LogosCoreClient();

    /**
     * Call a plugin method asynchronously, with parameters provided as a JSON
     * string in the [{name,value,type},...] format used by FFI consumers.
     * The callback receives (success, resultMessage).
     */
    void callMethodAsync(const QString& pluginName,
                         const QString& methodName,
                         const QString& paramsJson,
                         AsyncCallback callback);

    /**
     * Subscribe to an event from a plugin. The callback fires each time
     * the event is emitted, with a JSON-formatted message.
     */
    void subscribeEvent(const QString& pluginName,
                        const QString& eventName,
                        AsyncCallback callback);

    /**
     * Get (or lazily create) a LogosAPIClient for the named plugin.
     */
    LogosAPIClient* clientFor(const QString& pluginName);

private:
    LogosAPI* m_api;
};

#endif // LOGOS_CORE_CLIENT_H
