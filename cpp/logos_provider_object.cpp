#include "logos_provider_object.h"
#include "logos_json_convert.h"
#include "logos_api.h"
#include "token_manager.h"
#include <QDebug>
#include <QJsonDocument>

// ---------------------------------------------------------------------------
// LogosProviderObject — universal virtual defaults
// ---------------------------------------------------------------------------

nlohmann::json LogosProviderObject::callMethodStd(const std::string& /*methodName*/,
                                                   const nlohmann::json& /*args*/)
{
    return nullptr;
}

std::vector<LogosMethodMetadata> LogosProviderObject::getMethodsStd()
{
    return {};
}

void LogosProviderObject::setEventListenerStd(UniversalEventCallback /*callback*/)
{
}

// ---------------------------------------------------------------------------
// LogosProviderObject — bridge helpers (Qt-free providers delegate here)
// ---------------------------------------------------------------------------

QVariant LogosProviderObject::callMethodStdBridge(const QString& methodName, const QVariantList& args)
{
    nlohmann::json jArgs = nlohmann::json::array();
    for (const QVariant& a : args)
        jArgs.push_back(logos::qvariantToNlohmann(a));

    nlohmann::json result = callMethodStd(methodName.toStdString(), jArgs);
    return logos::nlohmannToQVariant(result);
}

QJsonArray LogosProviderObject::getMethodsStdBridge()
{
    return logos::methodsToJsonArray(getMethodsStd());
}

void LogosProviderObject::setEventListenerStdBridge(EventCallback callback)
{
    setEventListenerStd([callback](const std::string& eventName, const std::string& data) {
        if (!callback) return;
        QVariantList qData;
        QJsonDocument doc = QJsonDocument::fromJson(
            QByteArray::fromStdString(data));
        if (doc.isArray()) {
            for (const QJsonValue& v : doc.array())
                qData.append(v.toVariant());
        } else {
            qData.append(QString::fromStdString(data));
        }
        callback(QString::fromStdString(eventName), qData);
    });
}

// ---------------------------------------------------------------------------
// LogosProviderBase
// ---------------------------------------------------------------------------

void LogosProviderBase::init(void* apiInstance)
{
    m_logosAPI = static_cast<LogosAPI*>(apiInstance);
    qDebug() << "[LogosProviderObject] LogosProviderBase::init called";
    onInit(m_logosAPI);
}

bool LogosProviderBase::informModuleToken(const QString& moduleName, const QString& token)
{
    if (!m_logosAPI) {
        qWarning() << "[LogosProviderObject] informModuleToken: LogosAPI not available";
        return false;
    }

    TokenManager* tokenManager = m_logosAPI->getTokenManager();
    if (!tokenManager) {
        qWarning() << "[LogosProviderObject] informModuleToken: TokenManager not available";
        return false;
    }

    qDebug() << "[LogosProviderObject] Saving token for module:" << moduleName;
    tokenManager->saveToken(moduleName, token);
    return true;
}

void LogosProviderBase::emitEvent(const QString& eventName, const QVariantList& data)
{
    if (m_eventCallback) {
        qDebug() << "[LogosProviderObject] emitEvent:" << eventName;
        m_eventCallback(eventName, data);
    } else {
        qWarning() << "[LogosProviderObject] emitEvent: no listener set for" << eventName;
    }
}
