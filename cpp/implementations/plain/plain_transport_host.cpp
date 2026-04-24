#include "plain_transport_host.h"

#include "cbor_codec.h"
#include "io_context_pool.h"
#include "json_codec.h"
#include "qvariant_rpc_value.h"

#include "../../module_proxy.h"

#include <QDebug>
#include <QMetaObject>

#include <boost/asio/ssl/context.hpp>
#include <openssl/ssl.h>

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

boost::asio::ssl::context buildSslCtx(const LogosTransportConfig& cfg, bool server)
{
    boost::asio::ssl::context ctx(server
        ? boost::asio::ssl::context::tls_server
        : boost::asio::ssl::context::tls_client);
    ctx.set_options(boost::asio::ssl::context::default_workarounds
                    | boost::asio::ssl::context::no_sslv2
                    | boost::asio::ssl::context::no_sslv3
                    | boost::asio::ssl::context::single_dh_use);

    // Require TLS 1.2+. Without an explicit floor, certain
    // Boost.Asio × OpenSSL 3.x combinations fall through to a
    // server-side max proto of TLS 1.1, so every modern client (which
    // refuses 1.1) gets a handshake_failure alert before the server
    // sends ServerHello. Symptom: `openssl s_client -tls1_3` returns
    // alert 70 (protocol_version); `-tls1_2` returns alert 40
    // (handshake_failure). Set a floor and a ceiling explicitly.
    SSL_CTX_set_min_proto_version(ctx.native_handle(), TLS1_2_VERSION);
    SSL_CTX_set_max_proto_version(ctx.native_handle(), TLS1_3_VERSION);

    // Enable EC groups for both TLS 1.3 key_share and TLS 1.2 ECDHE.
    // OpenSSL 3.x ships sane defaults here but some
    // minimal / embedded builds ship an empty list, in which case the
    // server has nothing to respond with and aborts the handshake.
    // Being explicit costs nothing and removes a class of mystery
    // bugs.
    SSL_CTX_set1_groups_list(ctx.native_handle(),
                             "X25519:P-256:P-384:P-521");

    // Explicit cipher / cipher-suite lists. TLS 1.3 has a distinct list
    // controlled by `SSL_CTX_set_ciphersuites`; if that's empty, the
    // server can't accept TLS 1.3 ClientHellos at all and reports
    // "unsupported protocol" even though min_proto / max_proto allow
    // it. TLS 1.2 ciphers are set via `SSL_CTX_set_cipher_list` and
    // the symptom of an empty list is "no shared cipher". The bundled
    // OpenSSL 3.x in some nix builds ships empty defaults; spell them
    // out so the daemon works in every build environment.
    SSL_CTX_set_ciphersuites(ctx.native_handle(),
        "TLS_AES_128_GCM_SHA256:"
        "TLS_AES_256_GCM_SHA384:"
        "TLS_CHACHA20_POLY1305_SHA256");
    SSL_CTX_set_cipher_list(ctx.native_handle(),
        "ECDHE+AESGCM:ECDHE+CHACHA20:"
        "DHE+AESGCM:DHE+CHACHA20:"
        "!aNULL:!MD5:!DSS:!RC4:!3DES");

    if (!cfg.certFile.empty())
        ctx.use_certificate_chain_file(cfg.certFile);
    if (!cfg.keyFile.empty())
        ctx.use_private_key_file(cfg.keyFile, boost::asio::ssl::context::pem);
    if (!cfg.caFile.empty())
        ctx.load_verify_file(cfg.caFile);
    if (cfg.verifyPeer && !server) {
        ctx.set_verify_mode(boost::asio::ssl::verify_peer);
    } else if (!server) {
        ctx.set_verify_mode(boost::asio::ssl::verify_none);
    }
    return ctx;
}

} // anonymous namespace

PlainTransportHost::PlainTransportHost(LogosTransportConfig cfg)
    : m_cfg(std::move(cfg))
{
}

PlainTransportHost::~PlainTransportHost()
{
    std::lock_guard<std::mutex> g(m_mu);
    for (auto& [name, pub] : m_published) {
        QObject::disconnect(pub.eventConn);
    }
    if (m_tcp) m_tcp->stop();
    if (m_ssl) m_ssl->stop();
}

bool PlainTransportHost::start()
{
    std::lock_guard<std::mutex> g(m_mu);
    if (m_started) return true;
    auto codec = makeCodec(m_cfg.codec);
    auto& ioc  = IoContextPool::shared().ioContext();

    if (m_cfg.protocol == LogosProtocol::Tcp) {
        m_tcp = std::make_shared<RpcServerTcp>(ioc, m_cfg.host, m_cfg.port, codec, this);
        if (!m_tcp->start()) {
            qCritical() << "PlainTransportHost: TCP bind failed on"
                        << QString::fromStdString(m_cfg.host) << m_cfg.port;
            m_tcp.reset();
            return false;
        }
        m_boundPort = m_tcp->boundPort();
    } else if (m_cfg.protocol == LogosProtocol::TcpSsl) {
        try {
            auto ctx = buildSslCtx(m_cfg, /*server=*/true);
            m_ssl = std::make_shared<RpcServerSsl>(ioc, m_cfg.host, m_cfg.port,
                                                   std::move(ctx), codec, this);
            if (!m_ssl->start()) {
                qCritical() << "PlainTransportHost: TLS bind failed";
                m_ssl.reset();
                return false;
            }
            m_boundPort = m_ssl->boundPort();
        } catch (const std::exception& e) {
            qCritical() << "PlainTransportHost: SSL context setup failed:" << e.what();
            return false;
        }
    } else {
        qCritical() << "PlainTransportHost: unsupported protocol";
        return false;
    }
    m_started = true;
    return true;
}

QString PlainTransportHost::endpoint() const
{
    std::lock_guard<std::mutex> g(m_mu);
    if (m_boundPort == 0) return QString();
    return QString("tcp://%1:%2")
        .arg(QString::fromStdString(m_cfg.host))
        .arg(m_boundPort);
}

QString PlainTransportHost::bindUrl(const QString& /*instanceId*/,
                                    const QString& /*moduleName*/)
{
    // One PlainTransportHost listens on a single host:port and serves every
    // published module over the same socket; URL is independent of module.
    return endpoint();
}

bool PlainTransportHost::publishObject(const QString& name, QObject* object)
{
    if (!object) return false;
    auto* proxy = qobject_cast<ModuleProxy*>(object);
    if (!proxy) {
        qWarning() << "PlainTransportHost::publishObject: expected ModuleProxy for"
                   << name << "(plain transport only publishes ModuleProxy for now)";
        return false;
    }

    std::lock_guard<std::mutex> g(m_mu);
    Published pub;
    pub.object = object;
    const std::string stdName = name.toStdString();

    // Hook the QObject's eventResponse(QString, QVariantList) signal so every
    // Q_INVOKABLE-style event emission fans out to subscribed connections.
    pub.eventConn = QObject::connect(proxy, &ModuleProxy::eventResponse,
        [this, stdName](const QString& eventName, const QVariantList& data) {
            EventMessage msg;
            msg.object    = stdName;
            msg.eventName = eventName.toStdString();
            msg.data      = qvariantListToRpcList(data);
            fanOutEvent(stdName, std::move(msg));
        });

    m_published[stdName] = std::move(pub);
    return true;
}

void PlainTransportHost::unpublishObject(const QString& name)
{
    std::lock_guard<std::mutex> g(m_mu);
    auto it = m_published.find(name.toStdString());
    if (it == m_published.end()) return;
    QObject::disconnect(it->second.eventConn);
    m_published.erase(it);
}

void PlainTransportHost::fanOutEvent(const std::string& name, EventMessage msg)
{
    std::vector<EventSink> sinks;
    {
        std::lock_guard<std::mutex> g(m_mu);
        auto it = m_published.find(name);
        if (it == m_published.end()) return;
        // Named subscribers + wildcard ("") subscribers get the event.
        for (auto which : {msg.eventName, std::string{}}) {
            auto evtIt = it->second.sinksByEvent.find(which);
            if (evtIt == it->second.sinksByEvent.end()) continue;
            for (auto& [key, sink] : evtIt->second) sinks.push_back(sink);
        }
    }
    for (auto& sink : sinks) {
        try { sink(msg); } catch (...) {}
    }
}

void PlainTransportHost::onCall(const CallMessage& req, CallReply reply)
{
    QObject* obj = nullptr;
    {
        std::lock_guard<std::mutex> g(m_mu);
        auto it = m_published.find(req.object);
        if (it != m_published.end()) obj = it->second.object;
    }
    if (!obj) {
        ResultMessage res; res.id = req.id; res.ok = false;
        res.err = "object not published: " + req.object;
        res.errCode = "MODULE_NOT_LOADED";
        reply(std::move(res));
        return;
    }

    QString   authToken  = QString::fromStdString(req.authToken);
    QString   methodName = QString::fromStdString(req.method);
    QVariantList args    = rpcListToQVariantList(req.args);
    uint64_t id = req.id;

    QMetaObject::invokeMethod(obj, [obj, authToken, methodName, args, id, reply]() {
        QVariant ret;
        bool ok = QMetaObject::invokeMethod(obj, "callRemoteMethod",
                                            Qt::DirectConnection,
                                            Q_RETURN_ARG(QVariant, ret),
                                            Q_ARG(QString, authToken),
                                            Q_ARG(QString, methodName),
                                            Q_ARG(QVariantList, args));
        ResultMessage res;
        res.id = id;
        if (ok) {
            res.ok = true;
            res.value = qvariantToRpcValue(ret);
        } else {
            res.ok = false;
            res.err = "callRemoteMethod failed";
            res.errCode = "METHOD_FAILED";
        }
        reply(std::move(res));
    }, Qt::QueuedConnection);
}

void PlainTransportHost::onMethods(const MethodsMessage& req, MethodsReply reply)
{
    QObject* obj = nullptr;
    {
        std::lock_guard<std::mutex> g(m_mu);
        auto it = m_published.find(req.object);
        if (it != m_published.end()) obj = it->second.object;
    }
    if (!obj) {
        MethodsResultMessage res; res.id = req.id; res.ok = false;
        res.err = "object not published";
        reply(std::move(res));
        return;
    }
    uint64_t id = req.id;
    QMetaObject::invokeMethod(obj, [obj, id, reply]() {
        QJsonArray arr;
        QMetaObject::invokeMethod(obj, "getPluginMethods",
                                  Qt::DirectConnection,
                                  Q_RETURN_ARG(QJsonArray, arr));
        MethodsResultMessage res;
        res.id = id;
        res.ok = true;
        res.methods = methodsFromJsonArray(arr);
        reply(std::move(res));
    }, Qt::QueuedConnection);
}

void PlainTransportHost::onSubscribe(const SubscribeMessage& req, EventSink sink)
{
    std::lock_guard<std::mutex> g(m_mu);
    auto it = m_published.find(req.object);
    if (it == m_published.end()) return;
    // Sinks are keyed by functor-shared-pointer address. The caller (RpcConnection)
    // moves the sink in; we key by its move-captured lambda-object's address,
    // which is unique per subscription.
    static std::atomic<uint64_t> counter{0};
    it->second.sinksByEvent[req.eventName][
        reinterpret_cast<const void*>(counter.fetch_add(1) + 1)] = std::move(sink);
}

void PlainTransportHost::onUnsubscribe(const UnsubscribeMessage& req)
{
    std::lock_guard<std::mutex> g(m_mu);
    auto it = m_published.find(req.object);
    if (it == m_published.end()) return;
    it->second.sinksByEvent.erase(req.eventName);
}

void PlainTransportHost::onToken(const TokenMessage& req)
{
    QObject* obj = nullptr;
    {
        std::lock_guard<std::mutex> g(m_mu);
        // Route token to the module matching req.moduleName if we host
        // it; otherwise the first published module (matches today's behavior
        // for the single-published-object provider pattern).
        auto it = m_published.find(req.moduleName);
        if (it != m_published.end()) obj = it->second.object;
        else if (!m_published.empty()) obj = m_published.begin()->second.object;
    }
    if (!obj) return;
    QString authToken  = QString::fromStdString(req.authToken);
    QString moduleName = QString::fromStdString(req.moduleName);
    QString token      = QString::fromStdString(req.token);
    QMetaObject::invokeMethod(obj, "informModuleToken",
                              Qt::QueuedConnection,
                              Q_ARG(QString, authToken),
                              Q_ARG(QString, moduleName),
                              Q_ARG(QString, token));
}

} // namespace logos::plain
