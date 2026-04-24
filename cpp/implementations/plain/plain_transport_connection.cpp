#include "plain_transport_connection.h"

#include "cbor_codec.h"
#include "io_context_pool.h"
#include "json_codec.h"
#include "plain_logos_object.h"
#include "rpc_server.h"

#include <QDebug>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace logos::plain {

namespace {

std::shared_ptr<IWireCodec> makeCodec(LogosWireCodec kind)
{
    switch (kind) {
    case LogosWireCodec::Cbor: return std::make_shared<CborCodec>();
    case LogosWireCodec::Json:
    default:                   return std::make_shared<JsonCodec>();
    }
}

boost::asio::ssl::context buildClientSslCtx(const LogosTransportConfig& cfg)
{
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tls_client);
    ctx.set_options(boost::asio::ssl::context::default_workarounds
                    | boost::asio::ssl::context::no_sslv2
                    | boost::asio::ssl::context::no_sslv3);
    if (!cfg.caFile.empty())
        ctx.load_verify_file(cfg.caFile);
    ctx.set_verify_mode(cfg.verifyPeer
        ? boost::asio::ssl::verify_peer
        : boost::asio::ssl::verify_none);
    return ctx;
}

} // anonymous namespace

PlainTransportConnection::PlainTransportConnection(LogosTransportConfig cfg)
    : m_cfg(std::move(cfg))
{
}

PlainTransportConnection::~PlainTransportConnection()
{
    if (m_conn) m_conn->stop("connection destroyed");
}

bool PlainTransportConnection::connectToHost()
{
    if (m_connected) return true;

    auto& ioc = IoContextPool::shared().ioContext();
    auto codec = makeCodec(m_cfg.codec);

    try {
        boost::asio::ip::tcp::resolver resolver(ioc);
        auto endpoints = resolver.resolve(m_cfg.host, std::to_string(m_cfg.port));

        if (m_cfg.protocol == LogosProtocol::Tcp) {
            boost::asio::ip::tcp::socket socket(ioc);
            boost::asio::connect(socket, endpoints);
            auto conn = std::make_shared<TcpConnection>(
                std::move(socket), codec, nullptr);
            conn->start();
            m_conn = conn;
            m_connected = true;
            return true;
        }

        if (m_cfg.protocol == LogosProtocol::TcpSsl) {
            auto ctx = buildClientSslCtx(m_cfg);
            SslStream stream(ioc, ctx);
            boost::asio::connect(stream.lowest_layer(), endpoints);
            stream.handshake(boost::asio::ssl::stream_base::client);
            auto conn = std::make_shared<SslConnection>(
                std::move(stream), codec, nullptr);
            conn->start();
            m_conn = conn;
            m_connected = true;
            return true;
        }

        qCritical() << "PlainTransportConnection: unsupported protocol";
        return false;
    } catch (const std::exception& e) {
        qWarning() << "PlainTransportConnection::connectToHost failed:" << e.what();
        m_connected = false;
        return false;
    }
}

bool PlainTransportConnection::isConnected() const
{
    return m_connected && m_conn && m_conn->isOpen();
}

bool PlainTransportConnection::reconnect()
{
    if (m_conn) m_conn->stop("reconnecting");
    m_conn.reset();
    m_connected = false;
    return connectToHost();
}

LogosObject* PlainTransportConnection::requestObject(const QString& objectName, int /*timeoutMs*/)
{
    if (!isConnected()) return nullptr;
    return new PlainLogosObject(objectName.toStdString(), m_conn);
}

QString PlainTransportConnection::endpointUrl(const QString& /*instanceId*/,
                                              const QString& /*moduleName*/)
{
    return QString("tcp://%1:%2")
        .arg(QString::fromStdString(m_cfg.host))
        .arg(m_cfg.port);
}

} // namespace logos::plain
