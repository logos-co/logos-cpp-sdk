#include "logos_transport_factory.h"
#include "logos_transport.h"
#include "logos_mode.h"
#include "logos_transport_config.h"
#include "implementations/qt_local/local_transport.h"
#include "implementations/qt_remote/remote_transport.h"
#include "implementations/mock/mock_transport.h"
#include "implementations/plain/plain_transport_connection.h"
#include "implementations/plain/plain_transport_host.h"

namespace LogosTransportFactory {

namespace {

// Pick the remote-transport backend based on the process-global
// LogosTransportConfig. A future iteration (full multi-transport publish)
// threads a config per LogosAPI instance through createHost/createConnection.
std::unique_ptr<LogosTransportHost> createRemoteHost(const QString& registryUrl)
{
    const auto& cfg = LogosTransportConfigGlobal::getDefault();
    switch (cfg.protocol) {
    case LogosProtocol::Tcp:
    case LogosProtocol::TcpSsl: {
        auto host = std::make_unique<logos::plain::PlainTransportHost>(cfg);
        host->start();
        return host;
    }
    case LogosProtocol::LocalSocket:
    default:
        return std::make_unique<RemoteTransportHost>(registryUrl);
    }
}

std::unique_ptr<LogosTransportConnection> createRemoteConnection(const QString& registryUrl)
{
    const auto& cfg = LogosTransportConfigGlobal::getDefault();
    switch (cfg.protocol) {
    case LogosProtocol::Tcp:
    case LogosProtocol::TcpSsl:
        return std::make_unique<logos::plain::PlainTransportConnection>(cfg);
    case LogosProtocol::LocalSocket:
    default:
        return std::make_unique<RemoteTransportConnection>(registryUrl);
    }
}

} // anonymous namespace

std::unique_ptr<LogosTransportHost> createHost(const QString& registryUrl)
{
    if (LogosModeConfig::isLocal()) {
        return std::make_unique<LocalTransportHost>();
    }
    if (LogosModeConfig::isMock()) {
        return std::make_unique<MockTransportHost>();
    }
    return createRemoteHost(registryUrl);
}

std::unique_ptr<LogosTransportHost>
createHost(const LogosTransportConfig& cfg, const QString& registryUrl)
{
    switch (cfg.protocol) {
    case LogosProtocol::Tcp:
    case LogosProtocol::TcpSsl: {
        auto host = std::make_unique<logos::plain::PlainTransportHost>(cfg);
        host->start();
        return host;
    }
    case LogosProtocol::LocalSocket:
    default:
        return std::make_unique<RemoteTransportHost>(registryUrl);
    }
}

std::unique_ptr<LogosTransportConnection> createConnection(const QString& registryUrl)
{
    if (LogosModeConfig::isLocal()) {
        return std::make_unique<LocalTransportConnection>();
    }
    if (LogosModeConfig::isMock()) {
        return std::make_unique<MockTransportConnection>();
    }
    return createRemoteConnection(registryUrl);
}

std::unique_ptr<LogosTransportConnection>
createConnection(const LogosTransportConfig& cfg, const QString& registryUrl)
{
    switch (cfg.protocol) {
    case LogosProtocol::Tcp:
    case LogosProtocol::TcpSsl:
        return std::make_unique<logos::plain::PlainTransportConnection>(cfg);
    case LogosProtocol::LocalSocket:
    default:
        return std::make_unique<RemoteTransportConnection>(registryUrl);
    }
}

}
