#ifndef LOGOS_PLAIN_INCOMING_CALL_HANDLER_H
#define LOGOS_PLAIN_INCOMING_CALL_HANDLER_H

#include "rpc_message.h"

#include <functional>

namespace logos::plain {

// -----------------------------------------------------------------------------
// IncomingCallHandler — provider-side dispatch hook.
//
// rpc_connection hands inbound Call / Methods / Subscribe / Unsubscribe /
// Token messages to a handler that the Qt-boundary layer implements. The
// handler is what talks to the published QObject (ModuleProxy); this
// interface deliberately speaks only plain C++ types so the wire stack
// stays Qt-free.
//
// The reply callbacks can be invoked synchronously (from inside the
// handler) or asynchronously from a different thread — rpc_connection
// serializes the actual frame write internally.
// -----------------------------------------------------------------------------
class IncomingCallHandler {
public:
    virtual ~IncomingCallHandler() = default;

    using CallReply     = std::function<void(ResultMessage)>;
    using MethodsReply  = std::function<void(MethodsResultMessage)>;
    using EventSink     = std::function<void(EventMessage)>;

    virtual void onCall(const CallMessage& req, CallReply reply) = 0;

    virtual void onMethods(const MethodsMessage& req, MethodsReply reply) = 0;

    // `sink` stays alive until onUnsubscribe fires or the connection dies.
    // The handler must call `sink(evt)` on every matching emission.
    virtual void onSubscribe(const SubscribeMessage& req, EventSink sink) = 0;

    virtual void onUnsubscribe(const UnsubscribeMessage& req) = 0;

    virtual void onToken(const TokenMessage& req) = 0;
};

} // namespace logos::plain

#endif // LOGOS_PLAIN_INCOMING_CALL_HANDLER_H
