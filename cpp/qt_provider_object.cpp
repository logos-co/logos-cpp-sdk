#include "qt_provider_object.h"
#include "../core/interface.h"
#include "logos_api.h"
#include "token_manager.h"
#include "logos_types.h"
#include <QDebug>
#include <QMetaObject>
#include <QMetaMethod>
#include <QMetaType>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>

// ── QMetaObject dispatch helpers (moved from module_proxy.cpp) ──────────────

#define INVOKE_METHOD_WITH_RETURN(returnType, castType) \
    do { \
        castType* result = static_cast<castType*>(returnValue); \
        switch (args.size()) { \
            case 0: \
                return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, Q_RETURN_ARG(returnType, *result)); \
            case 1: \
                return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, Q_RETURN_ARG(returnType, *result), scopedArgs[0].arg); \
            case 2: \
                return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, Q_RETURN_ARG(returnType, *result), scopedArgs[0].arg, scopedArgs[1].arg); \
            case 3: \
                return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, Q_RETURN_ARG(returnType, *result), scopedArgs[0].arg, scopedArgs[1].arg, scopedArgs[2].arg); \
            case 4: \
                return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, Q_RETURN_ARG(returnType, *result), scopedArgs[0].arg, scopedArgs[1].arg, scopedArgs[2].arg, scopedArgs[3].arg); \
            case 5: \
                return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, Q_RETURN_ARG(returnType, *result), scopedArgs[0].arg, scopedArgs[1].arg, scopedArgs[2].arg, scopedArgs[3].arg, scopedArgs[4].arg); \
            default: \
                qWarning() << "QtProviderObject: Currently supports 0-5 arguments. Got:" << args.size(); \
                return false; \
        } \
    } while(0)

namespace {
    class ScopedQArg {
    public:
        ScopedQArg(QMetaMethodArgument a, std::function<void(const void*)> d)
            : arg(a), deleter(std::move(d)) {}

        ~ScopedQArg() {
            if (deleter) {
                deleter(arg.data);
            }
        }

        ScopedQArg(ScopedQArg&& other)
            : arg(std::move(other.arg)), deleter(std::move(other.deleter)) {
                other.deleter = nullptr;
        }
        ScopedQArg& operator=(ScopedQArg&&) = delete;
        ScopedQArg(const ScopedQArg&) = delete;
        ScopedQArg& operator=(const ScopedQArg&) = delete;

        QMetaMethodArgument arg;

    private:
        std::function<void(const void*)> deleter;
    };

    auto toScopedQArgs(const QVariantList& args)
    {
        auto scopedArgs = std::vector<ScopedQArg>{};
        for (const auto& arg : args) {
            switch (arg.typeId()) {
                case QMetaType::Int: {
                    auto value = new int{arg.toInt()};
                    scopedArgs.emplace_back(
                        Q_ARG(int, *value),
                        [](const void* data) { delete static_cast<const int*>(data); }
                    );
                    break;
                }
                case QMetaType::QStringList: {
                    auto value = new QStringList{arg.toStringList()};
                    scopedArgs.emplace_back(
                        Q_ARG(QStringList, *value),
                        [](const void* data) { delete static_cast<const QStringList*>(data); }
                    );
                    break;
                }
                case QMetaType::QByteArray: {
                    auto value = new QByteArray{arg.toByteArray()};
                    scopedArgs.emplace_back(
                        Q_ARG(QByteArray, *value),
                        [](const void* data) { delete static_cast<const QByteArray*>(data); }
                    );
                    break;
                }
                case QMetaType::QUrl: {
                    auto value = new QUrl{arg.toUrl()};
                    scopedArgs.emplace_back(
                        Q_ARG(QUrl, *value),
                        [](const void* data) { delete static_cast<const QUrl*>(data); }
                    );
                    break;
                }
                case QMetaType::Bool: {
                    auto value = new bool{arg.toBool()};
                    scopedArgs.emplace_back(
                        Q_ARG(bool, *value),
                        [](const void* data) { delete static_cast<const bool*>(data); }
                    );
                    break;
                }
                case QMetaType::QString:
                default: {
                    auto value = new QString{arg.toString()};
                    scopedArgs.emplace_back(
                        Q_ARG(QString, *value),
                        [](const void* data) { delete static_cast<const QString*>(data); }
                    );
                    break;
                }
            }
        }
        return scopedArgs;
    }

    bool invokeMethodByArgCount(QObject *module, const QString& methodName, const QVariantList& args, void* returnValue, const char* returnTypeName)
    {
        QByteArray methodNameBytes = methodName.toUtf8();
        const char* methodNameCStr = methodNameBytes.constData();

        auto scopedArgs = toScopedQArgs(args);

        if (returnValue == nullptr) {
            switch (args.size()) {
                case 0:  return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection);
                case 1:  return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, scopedArgs[0].arg);
                case 2:  return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, scopedArgs[0].arg, scopedArgs[1].arg);
                case 3:  return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, scopedArgs[0].arg, scopedArgs[1].arg, scopedArgs[2].arg);
                case 4:  return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, scopedArgs[0].arg, scopedArgs[1].arg, scopedArgs[2].arg, scopedArgs[3].arg);
                case 5:  return QMetaObject::invokeMethod(module, methodNameCStr, Qt::DirectConnection, scopedArgs[0].arg, scopedArgs[1].arg, scopedArgs[2].arg, scopedArgs[3].arg, scopedArgs[4].arg);
                default:
                    qWarning() << "QtProviderObject: Currently supports 0-5 arguments. Got:" << args.size();
                    return false;
            }
        } else if (strcmp(returnTypeName, "bool") == 0) {
            INVOKE_METHOD_WITH_RETURN(bool, bool);
        } else if (strcmp(returnTypeName, "int") == 0) {
            INVOKE_METHOD_WITH_RETURN(int, int);
        } else if (strcmp(returnTypeName, "QString") == 0) {
            INVOKE_METHOD_WITH_RETURN(QString, QString);
        } else if (strcmp(returnTypeName, "LogosResult") == 0) {
            INVOKE_METHOD_WITH_RETURN(LogosResult, LogosResult);
        } else if (strcmp(returnTypeName, "QVariant") == 0) {
            INVOKE_METHOD_WITH_RETURN(QVariant, QVariant);
        } else if (strcmp(returnTypeName, "QJsonArray") == 0) {
            INVOKE_METHOD_WITH_RETURN(QJsonArray, QJsonArray);
        } else if (strcmp(returnTypeName, "QStringList") == 0) {
            INVOKE_METHOD_WITH_RETURN(QStringList, QStringList);
        } else {
            qWarning() << "QtProviderObject: Unsupported return type:" << returnTypeName;
            return false;
        }
    }
}

// ── QtProviderObject implementation ─────────────────────────────────────────

QtProviderObject::QtProviderObject(QObject* module, QObject* parent)
    : QObject(parent)
    , m_module(module)
{
    if (m_module) {
        connect(m_module, SIGNAL(eventResponse(QString, QVariantList)),
                this, SLOT(onWrappedEventResponse(QString, QVariantList)));
        qDebug() << "[QT API] QtProviderObject: wrapping Q_INVOKABLE QObject (DEPRECATED — legacy path)";
    }
}

QtProviderObject::~QtProviderObject()
{
}

void QtProviderObject::onWrappedEventResponse(const QString& eventName, const QVariantList& data)
{
    if (m_eventCallback) {
        m_eventCallback(eventName, data);
    }
}

void QtProviderObject::init(void* apiInstance)
{
    if (!m_module) return;

    LogosAPI* api = static_cast<LogosAPI*>(apiInstance);

    int methodIndex = m_module->metaObject()->indexOfMethod("initLogos(LogosAPI*)");
    if (methodIndex != -1) {
        qDebug() << "[QT API] QtProviderObject::init — calling initLogos(LogosAPI*) on wrapped QObject";
        QMetaObject::invokeMethod(m_module, "initLogos",
                                  Qt::DirectConnection,
                                  Q_ARG(LogosAPI*, api));
    } else {
        qDebug() << "[QT API] QtProviderObject::init — wrapped QObject has no initLogos, skipping";
    }
}

QString QtProviderObject::providerName() const
{
    PluginInterface* pi = qobject_cast<PluginInterface*>(m_module);
    return pi ? pi->name() : QString();
}

QString QtProviderObject::providerVersion() const
{
    PluginInterface* pi = qobject_cast<PluginInterface*>(m_module);
    return pi ? pi->version() : QString();
}

void QtProviderObject::setEventListener(EventCallback callback)
{
    m_eventCallback = std::move(callback);
}

QVariant QtProviderObject::callMethod(const QString& methodName, const QVariantList& args)
{
    if (!m_module) {
        qWarning() << "[QT API] QtProviderObject::callMethod: null module";
        return QVariant();
    }

    if (methodName.isEmpty()) {
        qWarning() << "[QT API] QtProviderObject::callMethod: empty method name";
        return QVariant();
    }

    qDebug() << "[QT API] QtProviderObject: dispatching" << methodName
             << "with" << args.size() << "args via QMetaObject::invokeMethod (Q_INVOKABLE path)";

    // Special-case getPluginMethods (framework-level, not on the wrapped plugin)
    if (methodName == "getPluginMethods" && args.isEmpty()) {
        return QVariant(getMethods());
    }

    // Auth-token validation (mirrors the old ModuleProxy logic)
    PluginInterface* pluginInterface = qobject_cast<PluginInterface*>(m_module);
    if (!pluginInterface) {
        qWarning() << "[QT API] QtProviderObject::callMethod: module is not a PluginInterface";
        return QVariant();
    }

    LogosAPI* api = pluginInterface->logosAPI;
    if (!api) {
        qWarning() << "[QT API] QtProviderObject::callMethod: LogosAPI not available";
        return QVariant();
    }

    // Find method via QMetaObject
    const QMetaObject* metaObject = m_module->metaObject();
    int methodIndex = -1;
    for (int i = 0; i < metaObject->methodCount(); ++i) {
        QMetaMethod method = metaObject->method(i);
        if (method.name() == methodName && method.parameterCount() == args.size()) {
            methodIndex = i;
            break;
        }
    }

    if (methodIndex == -1) {
        qWarning() << "[QT API] QtProviderObject: method not found:" << methodName
                    << "with" << args.size() << "arguments";
        return QVariant();
    }

    QMetaMethod method = metaObject->method(methodIndex);
    QMetaType returnType = method.returnMetaType();

    bool success = false;
    QVariant result;

    if (returnType == QMetaType::fromType<void>()) {
        success = invokeMethodByArgCount(m_module, methodName, args, nullptr, nullptr);
        if (success) result = QVariant(true);
    } else if (returnType == QMetaType::fromType<bool>()) {
        bool v = false;
        success = invokeMethodByArgCount(m_module, methodName, args, &v, "bool");
        if (success) result = QVariant(v);
    } else if (returnType == QMetaType::fromType<int>()) {
        int v = 0;
        success = invokeMethodByArgCount(m_module, methodName, args, &v, "int");
        if (success) result = QVariant(v);
    } else if (returnType == QMetaType::fromType<QString>()) {
        QString v;
        success = invokeMethodByArgCount(m_module, methodName, args, &v, "QString");
        if (success) result = QVariant(v);
    } else if (returnType == QMetaType::fromType<LogosResult>()) {
        LogosResult v;
        success = invokeMethodByArgCount(m_module, methodName, args, &v, "LogosResult");
        if (success) result = QVariant::fromValue(v);
    } else if (returnType == QMetaType::fromType<QVariant>()) {
        QVariant v;
        success = invokeMethodByArgCount(m_module, methodName, args, &v, "QVariant");
        if (success) result = v;
    } else if (returnType == QMetaType::fromType<QJsonArray>()) {
        QJsonArray v;
        success = invokeMethodByArgCount(m_module, methodName, args, &v, "QJsonArray");
        if (success) result = QVariant(v);
    } else if (returnType == QMetaType::fromType<QStringList>()) {
        QStringList v;
        success = invokeMethodByArgCount(m_module, methodName, args, &v, "QStringList");
        if (success) result = QVariant(v);
    } else {
        qWarning() << "[QT API] QtProviderObject: unsupported return type:"
                    << returnType.name() << "for method:" << methodName;
        return QVariant();
    }

    if (!success) {
        qWarning() << "[QT API] QtProviderObject: failed to invoke" << methodName;
    }
    return result;
}

bool QtProviderObject::informModuleToken(const QString& moduleName, const QString& token)
{
    PluginInterface* pluginInterface = qobject_cast<PluginInterface*>(m_module);
    if (!pluginInterface) {
        qWarning() << "[QT API] QtProviderObject::informModuleToken: not a PluginInterface";
        return false;
    }

    LogosAPI* api = pluginInterface->logosAPI;
    if (!api) {
        qWarning() << "[QT API] QtProviderObject::informModuleToken: LogosAPI not available";
        return false;
    }

    TokenManager* tokenManager = api->getTokenManager();
    if (!tokenManager) {
        qWarning() << "[QT API] QtProviderObject::informModuleToken: TokenManager not available";
        return false;
    }

    qDebug() << "[QT API] QtProviderObject: saving token for module:" << moduleName;
    tokenManager->saveToken(moduleName, token);
    return true;
}

QJsonArray QtProviderObject::getMethods()
{
    if (!m_module) return QJsonArray();

    QJsonArray methodsArray;
    const QMetaObject* metaObject = m_module->metaObject();

    for (int i = 0; i < metaObject->methodCount(); ++i) {
        QMetaMethod method = metaObject->method(i);

        if (method.enclosingMetaObject() != metaObject) {
            continue;
        }

        QJsonObject methodObj;
        methodObj["signature"] = QString::fromUtf8(method.methodSignature());
        methodObj["name"] = QString::fromUtf8(method.name());
        methodObj["returnType"] = QString::fromUtf8(method.typeName());
        methodObj["isInvokable"] = method.isValid() &&
            (method.methodType() == QMetaMethod::Method || method.methodType() == QMetaMethod::Slot);

        if (method.parameterCount() > 0) {
            QJsonArray params;
            for (int p = 0; p < method.parameterCount(); ++p) {
                QJsonObject paramObj;
                paramObj["type"] = QString::fromUtf8(method.parameterTypeName(p));
                QByteArrayList paramNames = method.parameterNames();
                if (p < paramNames.size() && !paramNames.at(p).isEmpty()) {
                    paramObj["name"] = QString::fromUtf8(paramNames.at(p));
                } else {
                    paramObj["name"] = QString("param%1").arg(p);
                }
                params.append(paramObj);
            }
            methodObj["parameters"] = params;
        }

        methodsArray.append(methodObj);
    }

    return methodsArray;
}

#include "moc_qt_provider_object.cpp"
