#include "logos_provider_object.h"
#include "logos_api.h"
#include "token_manager.h"
#include <QDebug>

void LogosProviderBase::init(void* apiInstance)
{
    m_logosAPI = static_cast<LogosAPI*>(apiInstance);
    qDebug() << "[QT API] LogosProviderBase::init — module initialized with Qt types (DEPRECATED — migrate to NativeProviderBase)";
    onInit(m_logosAPI);
}

bool LogosProviderBase::informModuleToken(const QString& moduleName, const QString& token)
{
    if (!m_logosAPI) {
        qWarning() << "[QT API] LogosProviderBase::informModuleToken: LogosAPI not available";
        return false;
    }

    TokenManager* tokenManager = m_logosAPI->getTokenManager();
    if (!tokenManager) {
        qWarning() << "[QT API] LogosProviderBase::informModuleToken: TokenManager not available";
        return false;
    }

    qDebug() << "[QT API] LogosProviderBase: saving token for module:" << moduleName;
    tokenManager->saveToken(moduleName, token);
    return true;
}

void LogosProviderBase::emitEvent(const QString& eventName, const QVariantList& data)
{
    if (m_eventCallback) {
        qDebug() << "[QT API] LogosProviderBase::emitEvent:" << eventName;
        m_eventCallback(eventName, data);
    } else {
        qWarning() << "[QT API] LogosProviderBase::emitEvent: no listener set for" << eventName;
    }
}
