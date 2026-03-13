#ifndef LOGOS_TRANSPORT_H
#define LOGOS_TRANSPORT_H

#include <QString>
#include <QVariant>
#include <QVariantList>

class QObject;

/**
 * @brief Abstract interface for the provider/server side of module transport.
 *
 * Implementations handle how a module object is made available to consumers
 * (e.g. via in-process registry, Qt Remote Objects, or other mechanisms).
 */
class LogosTransportHost {
public:
    virtual ~LogosTransportHost() = default;

    /**
     * @brief Publish an object so consumers can discover and invoke it
     * @param name The name to publish the object under
     * @param object The QObject to publish
     * @return true if publishing succeeded
     */
    virtual bool publishObject(const QString& name, QObject* object) = 0;

    /**
     * @brief Remove a previously published object
     * @param name The name the object was published under
     */
    virtual void unpublishObject(const QString& name) = 0;
};

/**
 * @brief Abstract interface for the consumer/client side of module transport.
 *
 * Implementations handle how a consumer connects to a host, acquires object
 * handles, and invokes methods on them.
 */
class LogosTransportConnection {
public:
    virtual ~LogosTransportConnection() = default;

    /**
     * @brief Establish connection to the host/registry
     * @return true if connection succeeded
     */
    virtual bool connectToHost() = 0;

    /**
     * @brief Check if currently connected
     */
    virtual bool isConnected() const = 0;

    /**
     * @brief Tear down and re-establish the connection
     * @return true if reconnection succeeded
     */
    virtual bool reconnect() = 0;

    /**
     * @brief Acquire a handle to a named object from the host
     * @param objectName The published name of the object
     * @param timeoutMs Maximum time to wait for the object to become available
     * @return QObject* handle, or nullptr on failure. Caller must use releaseObject() when done.
     */
    virtual QObject* requestObject(const QString& objectName, int timeoutMs) = 0;

    /**
     * @brief Release a previously acquired object handle
     * @param object The handle returned by requestObject()
     */
    virtual void releaseObject(QObject* object) = 0;

    /**
     * @brief Invoke callRemoteMethod on the given object handle
     * @param object Handle from requestObject()
     * @param authToken Authentication token
     * @param methodName Method to call on the underlying module
     * @param args Arguments for the method
     * @param timeoutMs Maximum time to wait for the result
     * @return The method result, or an invalid QVariant on failure
     */
    virtual QVariant callRemoteMethod(QObject* object, const QString& authToken,
                                       const QString& methodName, const QVariantList& args,
                                       int timeoutMs) = 0;

    /**
     * @brief Invoke informModuleToken on the given object handle
     * @param object Handle from requestObject()
     * @param authToken Authentication token
     * @param moduleName Target module name
     * @param token The token to deliver
     * @param timeoutMs Maximum time to wait for the result
     * @return true if the token was delivered successfully
     */
    virtual bool callInformModuleToken(QObject* object, const QString& authToken,
                                        const QString& moduleName, const QString& token,
                                        int timeoutMs) = 0;
};

#endif // LOGOS_TRANSPORT_H
