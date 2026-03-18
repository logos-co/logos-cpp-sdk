#ifndef LOGOS_NATIVE_API_H
#define LOGOS_NATIVE_API_H

#include <map>
#include <string>

class LogosAPI;
class NativeLogosClient;

// ---------------------------------------------------------------------------
// NativeLogosAPI — Qt-free wrapper around LogosAPI
//
// Provides native module developers with a Qt-free entry point to the SDK.
// The header has no Qt includes; conversions happen in the .cpp file.
// ---------------------------------------------------------------------------
class NativeLogosAPI {
public:
    explicit NativeLogosAPI(LogosAPI* qtApi);
    ~NativeLogosAPI();

    NativeLogosClient* getClient(const std::string& target_module);

    LogosAPI* qtApi() const { return m_qtApi; }

private:
    LogosAPI* m_qtApi;
    std::map<std::string, NativeLogosClient*> m_clients;
};

#endif // LOGOS_NATIVE_API_H
