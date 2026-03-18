#include "logos_native_api.h"
#include "logos_native_client.h"
#include "../logos_api.h"

#include <QDebug>
#include <QString>

NativeLogosAPI::NativeLogosAPI(LogosAPI* qtApi)
    : m_qtApi(qtApi)
{
}

NativeLogosAPI::~NativeLogosAPI()
{
    for (auto& [name, client] : m_clients) {
        delete client;
    }
}

NativeLogosClient* NativeLogosAPI::getClient(const std::string& target_module)
{
    auto it = m_clients.find(target_module);
    if (it != m_clients.end())
        return it->second;

    qDebug() << "[NATIVE API] NativeLogosAPI::getClient — creating NativeLogosClient for"
             << QString::fromStdString(target_module);
    LogosAPIClient* qtClient = m_qtApi->getClient(QString::fromStdString(target_module));
    if (!qtClient)
        return nullptr;

    auto* client = new NativeLogosClient(qtClient);
    m_clients[target_module] = client;
    return client;
}
