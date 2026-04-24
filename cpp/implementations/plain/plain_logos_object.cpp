#include "plain_logos_object.h"

#include "qvariant_rpc_value.h"

#include <QDebug>

#include <chrono>
#include <future>
#include <utility>

namespace logos::plain {

PlainLogosObject::PlainLogosObject(std::string objectName,
                                   std::shared_ptr<RpcConnectionBase> conn)
    : m_objectName(std::move(objectName))
    , m_conn(std::move(conn))
{
}

PlainLogosObject::~PlainLogosObject()
{
    disconnectEvents();
}

QVariant PlainLogosObject::callMethod(const QString& authToken,
                                      const QString& methodName,
                                      const QVariantList& args,
                                      int timeoutMs)
{
    if (!m_conn || !m_conn->isOpen()) return QVariant();

    CallMessage msg;
    msg.id        = m_conn->nextId();
    msg.authToken = authToken.toStdString();
    msg.object    = m_objectName;
    msg.method    = methodName.toStdString();
    msg.args      = qvariantListToRpcList(args);

    auto fut = m_conn->sendCall(std::move(msg));

    if (fut.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready) {
        qWarning() << "PlainLogosObject::callMethod: timeout for" << methodName;
        return QVariant();
    }
    auto res = fut.get();
    if (!res.ok) {
        qWarning() << "PlainLogosObject::callMethod:" << methodName
                   << "failed:" << QString::fromStdString(res.err);
        return QVariant();
    }
    return rpcValueToQVariant(res.value);
}

void PlainLogosObject::callMethodAsync(const QString& authToken,
                                       const QString& methodName,
                                       const QVariantList& args,
                                       int timeoutMs,
                                       AsyncResultCallback callback)
{
    if (!callback) return;
    if (!m_conn || !m_conn->isOpen()) {
        callback(QVariant());
        return;
    }

    CallMessage msg;
    msg.id        = m_conn->nextId();
    msg.authToken = authToken.toStdString();
    msg.object    = m_objectName;
    msg.method    = methodName.toStdString();
    msg.args      = qvariantListToRpcList(args);

    auto fut = std::make_shared<std::future<ResultMessage>>(
        m_conn->sendCall(std::move(msg)));

    // Simple waiter thread. Acceptable here because callMethodAsync is
    // already an infrequent path and spawns no-ops if the caller never
    // awaits. A future iteration can fold this into the io_context.
    std::thread([fut, timeoutMs, callback = std::move(callback)]() mutable {
        if (fut->wait_for(std::chrono::milliseconds(timeoutMs))
            != std::future_status::ready) {
            callback(QVariant());
            return;
        }
        auto res = fut->get();
        callback(res.ok ? rpcValueToQVariant(res.value) : QVariant());
    }).detach();
}

bool PlainLogosObject::informModuleToken(const QString& authToken,
                                         const QString& moduleName,
                                         const QString& token,
                                         int /*timeoutMs*/)
{
    if (!m_conn || !m_conn->isOpen()) return false;
    TokenMessage msg;
    msg.authToken  = authToken.toStdString();
    msg.moduleName = moduleName.toStdString();
    msg.token      = token.toStdString();
    m_conn->sendToken(std::move(msg));
    return true; // fire-and-forget
}

void PlainLogosObject::onEvent(const QString& eventName, EventCallback callback)
{
    if (!m_conn || !m_conn->isOpen() || !callback) return;

    {
        std::lock_guard<std::mutex> g(m_mu);
        m_subs.emplace_back(eventName, callback);
    }

    SubscribeMessage msg;
    msg.object    = m_objectName;
    msg.eventName = eventName.toStdString();

    // Bridge RPC event → Qt-flavored callback.
    m_conn->sendSubscribe(std::move(msg), [callback](EventMessage evt) {
        callback(QString::fromStdString(evt.eventName),
                 rpcListToQVariantList(evt.data));
    });
}

void PlainLogosObject::disconnectEvents()
{
    std::vector<std::pair<QString, EventCallback>> subs;
    {
        std::lock_guard<std::mutex> g(m_mu);
        subs.swap(m_subs);
    }
    if (!m_conn) return;
    for (const auto& [name, _] : subs) {
        UnsubscribeMessage msg;
        msg.object    = m_objectName;
        msg.eventName = name.toStdString();
        m_conn->sendUnsubscribe(std::move(msg));
    }
}

void PlainLogosObject::emitEvent(const QString& eventName, const QVariantList& data)
{
    if (!m_conn || !m_conn->isOpen()) return;
    EventMessage msg;
    msg.object    = m_objectName;
    msg.eventName = eventName.toStdString();
    msg.data      = qvariantListToRpcList(data);
    m_conn->sendEvent(std::move(msg));
}

QJsonArray PlainLogosObject::getMethods()
{
    if (!m_conn || !m_conn->isOpen()) return QJsonArray();

    MethodsMessage msg;
    msg.id     = m_conn->nextId();
    msg.object = m_objectName;

    auto fut = m_conn->sendMethods(std::move(msg));
    if (fut.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        return QJsonArray();
    }
    auto res = fut.get();
    if (!res.ok) return QJsonArray();
    return methodsToJsonArray(res.methods);
}

void PlainLogosObject::release()
{
    // The RpcConnection is SHARED across every PlainLogosObject a single
    // PlainTransportConnection hands out. Stopping it here would kill
    // the connection for every other holder too, so just unsubscribe our
    // own events and drop our reference — the connection stays alive
    // until PlainTransportConnection itself is destroyed.
    disconnectEvents();
    m_conn.reset();
    delete this;
}

quintptr PlainLogosObject::id() const
{
    return reinterpret_cast<quintptr>(m_conn.get());
}

} // namespace logos::plain
