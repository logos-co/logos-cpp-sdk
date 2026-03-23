#ifndef LOGOS_TRANSPORT_FACTORY_H
#define LOGOS_TRANSPORT_FACTORY_H

#include <memory>
#include <QString>

class LogosTransportHost;
class LogosTransportConnection;

namespace LogosTransportFactory {

    /**
     * @brief Create the appropriate transport host for the current mode
     * @param registryUrl The URL used by the remote transport (ignored in local mode)
     * @return Owning pointer to the transport host
     */
    std::unique_ptr<LogosTransportHost> createHost(const QString& registryUrl);

    /**
     * @brief Create the appropriate transport connection for the current mode
     * @param registryUrl The URL to connect to (ignored in local mode)
     * @return Owning pointer to the transport connection
     */
    std::unique_ptr<LogosTransportConnection> createConnection(const QString& registryUrl);

}

#endif // LOGOS_TRANSPORT_FACTORY_H
