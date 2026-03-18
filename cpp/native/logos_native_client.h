#ifndef LOGOS_NATIVE_CLIENT_H
#define LOGOS_NATIVE_CLIENT_H

#include "logos_value.h"
#include "logos_native_types.h"

#include <functional>
#include <string>
#include <vector>

class LogosAPIClient;
class LogosObject;

// ---------------------------------------------------------------------------
// NativeLogosClient — Qt-free wrapper around LogosAPIClient
//
// Module developers use this instead of LogosAPIClient directly.
// The header is Qt-free; conversions happen in the .cpp file.
// ---------------------------------------------------------------------------
class NativeLogosClient {
public:
    explicit NativeLogosClient(LogosAPIClient* qtClient);

    LogosValue invokeMethod(const std::string& objectName, const std::string& methodName,
                            const std::vector<LogosValue>& args = {});

    LogosValue invokeMethod(const std::string& objectName, const std::string& methodName,
                            const LogosValue& arg);

    LogosValue invokeMethod(const std::string& objectName, const std::string& methodName,
                            const LogosValue& arg1, const LogosValue& arg2);

    LogosValue invokeMethod(const std::string& objectName, const std::string& methodName,
                            const LogosValue& arg1, const LogosValue& arg2,
                            const LogosValue& arg3);

    LogosValue invokeMethod(const std::string& objectName, const std::string& methodName,
                            const LogosValue& arg1, const LogosValue& arg2,
                            const LogosValue& arg3, const LogosValue& arg4);

    LogosValue invokeMethod(const std::string& objectName, const std::string& methodName,
                            const LogosValue& arg1, const LogosValue& arg2,
                            const LogosValue& arg3, const LogosValue& arg4,
                            const LogosValue& arg5);

    LogosObject* requestObject(const std::string& objectName);

    void onEvent(LogosObject* origin, const std::string& eventName,
                 std::function<void(const std::string&, const std::vector<LogosValue>&)> callback);

    void onEventResponse(LogosObject* object, const std::string& eventName,
                         const std::vector<LogosValue>& data);

private:
    LogosAPIClient* m_client;
};

#endif // LOGOS_NATIVE_CLIENT_H
