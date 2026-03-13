#include "logos_transport_factory.h"
#include "logos_transport.h"
#include "logos_mode.h"
#include "implementations/qt_local/local_transport.h"
#include "implementations/qt_remote/remote_transport.h"

namespace LogosTransportFactory {

std::unique_ptr<LogosTransportHost> createHost(const QString& registryUrl)
{
    if (LogosModeConfig::isLocal()) {
        return std::make_unique<LocalTransportHost>();
    }
    return std::make_unique<RemoteTransportHost>(registryUrl);
}

std::unique_ptr<LogosTransportConnection> createConnection(const QString& registryUrl)
{
    if (LogosModeConfig::isLocal()) {
        return std::make_unique<LocalTransportConnection>();
    }
    return std::make_unique<RemoteTransportConnection>(registryUrl);
}

}
