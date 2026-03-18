#include "logos_native_provider.h"
#include "logos_native_api.h"
#include "../logos_api.h"
#include "../token_manager.h"

#include <QDebug>
#include <QString>

void NativeProviderBase::setEventListener(EventCallback callback)
{
    m_eventCallback = callback;
}

void NativeProviderBase::init(void* apiInstance)
{
    auto* qtApi = static_cast<LogosAPI*>(apiInstance);
    m_logosAPI = new NativeLogosAPI(qtApi);
    qDebug() << "[NATIVE API] NativeProviderBase::init — module initialized with native types";
    onInit(m_logosAPI);
}

bool NativeProviderBase::informModuleToken(const std::string& moduleName, const std::string& token)
{
    if (!m_logosAPI) {
        qWarning() << "[NATIVE API] NativeProviderBase::informModuleToken: LogosAPI not available";
        return false;
    }

    auto* qtApi = m_logosAPI->qtApi();
    if (!qtApi) {
        qWarning() << "[NATIVE API] NativeProviderBase::informModuleToken: underlying LogosAPI not available";
        return false;
    }

    TokenManager* tokenManager = qtApi->getTokenManager();
    if (!tokenManager) {
        qWarning() << "[NATIVE API] NativeProviderBase::informModuleToken: TokenManager not available";
        return false;
    }

    qDebug() << "[NATIVE API] NativeProviderBase: saving token for module:" << QString::fromStdString(moduleName);
    tokenManager->saveToken(QString::fromStdString(moduleName), QString::fromStdString(token));
    return true;
}

void NativeProviderBase::emitEvent(const std::string& eventName, const std::vector<LogosValue>& data)
{
    if (m_eventCallback) {
        qDebug() << "[NATIVE API] NativeProviderBase::emitEvent:" << QString::fromStdString(eventName);
        m_eventCallback(eventName, data);
    } else {
        qWarning() << "[NATIVE API] NativeProviderBase::emitEvent: no listener set for"
                    << QString::fromStdString(eventName);
    }
}
