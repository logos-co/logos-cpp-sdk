#include "logos_native_adapter.h"
#include "logos_native_provider.h"
#include "logos_value_qt.h"

#include <QJsonArray>
#include <QJsonDocument>

NativeProviderAdapter::NativeProviderAdapter(NativeProviderObject* native)
    : m_native(native)
{
}

QVariant NativeProviderAdapter::callMethod(const QString& methodName, const QVariantList& args)
{
    qDebug() << "[NATIVE API] NativeProviderAdapter: dispatching" << methodName
             << "with" << args.size() << "args (QVariant→LogosValue→native→LogosValue→QVariant)";
    std::string nativeName = methodName.toStdString();
    std::vector<LogosValue> nativeArgs = logosValueListFromQVariantList(args);
    LogosValue result = m_native->callMethod(nativeName, nativeArgs);
    return logosValueToQVariant(result);
}

QJsonArray NativeProviderAdapter::getMethods()
{
    std::string json = m_native->getMethodsJson();
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (doc.isArray())
        return doc.array();
    return QJsonArray();
}

void NativeProviderAdapter::setEventListener(EventCallback callback)
{
    m_native->setEventListener(
        [callback](const std::string& eventName, const std::vector<LogosValue>& data) {
            callback(QString::fromStdString(eventName), logosValueListToQVariantList(data));
        });
}

void NativeProviderAdapter::init(void* apiInstance)
{
    m_native->init(apiInstance);
}

QString NativeProviderAdapter::providerName() const
{
    return QString::fromStdString(m_native->providerName());
}

QString NativeProviderAdapter::providerVersion() const
{
    return QString::fromStdString(m_native->providerVersion());
}

bool NativeProviderAdapter::informModuleToken(const QString& moduleName, const QString& token)
{
    return m_native->informModuleToken(moduleName.toStdString(), token.toStdString());
}
