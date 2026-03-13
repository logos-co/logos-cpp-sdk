#include "logos_api_provider.h"
#include "module_proxy.h"
#include "logos_api.h"
#include "logos_transport.h"
#include "logos_transport_factory.h"
#include <QDebug>
#include <QUrl>
#include <QMetaObject>

LogosAPIProvider::LogosAPIProvider(const QString& module_name, QObject *parent)
    : QObject(parent)
    , m_registryUrl(QString("local:logos_%1").arg(module_name))
    , m_moduleProxy(nullptr)
{
    m_transport = LogosTransportFactory::createHost(m_registryUrl);
}

LogosAPIProvider::~LogosAPIProvider()
{
    if (!m_registeredObjectName.isEmpty()) {
        m_transport->unpublishObject(m_registeredObjectName);
    }
}

bool LogosAPIProvider::registerObject(const QString& name, QObject* object)
{
    if (!object) {
        qWarning() << "LogosAPIProvider: Cannot register null object";
        return false;
    }

    if (name.isEmpty()) {
        qWarning() << "LogosAPIProvider: Cannot register object with empty name";
        return false;
    }

    if (m_moduleProxy) {
        qCritical() << "LogosAPIProvider: Object already registered. Only one registration per provider is allowed";
        return false;
    }

    qDebug() << "LogosAPIProvider: Creating ModuleProxy for" << name << "wrapping the provided object";

    int methodIndex = object->metaObject()->indexOfMethod("initLogos(LogosAPI*)");
    if (methodIndex != -1) {
        qDebug() << "LogosAPIProvider: Calling initLogos on object before wrapping";
        bool methodSuccess = QMetaObject::invokeMethod(object, "initLogos", 
                                                     Qt::DirectConnection,
                                                     Q_ARG(LogosAPI*, qobject_cast<LogosAPI*>(parent())));
        if (methodSuccess) {
            qDebug() << "LogosAPIProvider: Successfully called initLogos on object";
        } else {
            qWarning() << "LogosAPIProvider: Failed to call initLogos on object";
        }
    } else {
        qDebug() << "LogosAPIProvider: Object does not have initLogos method, skipping";
    }

    m_moduleProxy = new ModuleProxy(object, this);

    bool success = m_transport->publishObject(name, m_moduleProxy);
    if (success) {
        m_registeredObjectName = name;
        qDebug() << "LogosAPIProvider: Successfully registered object with name:" << name;
    } else {
        qCritical() << "LogosAPIProvider: Failed to register object with name:" << name;
    }

    return success;
}

QString LogosAPIProvider::registryUrl() const
{
    return m_registryUrl;
}

bool LogosAPIProvider::saveToken(const QString& from_module_name, const QString& token)
{
    if (!m_moduleProxy) {
        qWarning() << "LogosAPIProvider: Cannot save token - no module proxy available";
        return false;
    }

    qDebug() << "LogosAPIProvider: Delegating saveToken call to module proxy for module:" << from_module_name;
    return m_moduleProxy->saveToken(from_module_name, token);
}

void LogosAPIProvider::onEventResponse(QObject* replica, const QString& eventName, const QVariantList& data)
{
    qDebug() << "LogosAPIProvider: Received event:" << eventName;

    if (eventName.isEmpty()) {
        qWarning() << "LogosAPIProvider: Event name cannot be empty";
        return;
    }

    qDebug() << "LogosAPIProvider: Emitting event:" << eventName;

    QMetaObject::invokeMethod(replica, "eventResponse", Qt::QueuedConnection, Q_ARG(QString, eventName), Q_ARG(QVariantList, data));
}
