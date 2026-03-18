#include "logos_value_qt.h"
#include "../logos_types.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>
#include <cstring>
#include <limits>

LogosValue logosValueFromQVariant(const QVariant& v)
{
    if (!v.isValid() || v.isNull())
        return LogosValue();

    switch (v.typeId()) {
    case QMetaType::Bool:
        return LogosValue(v.toBool());
    case QMetaType::Int:
    case QMetaType::LongLong:
        return LogosValue(static_cast<int64_t>(v.toLongLong()));
    case QMetaType::UInt:
    case QMetaType::ULongLong:
        return LogosValue(static_cast<int64_t>(v.toLongLong()));
    case QMetaType::Double:
    case QMetaType::Float:
        return LogosValue(v.toDouble());
    case QMetaType::QString:
        return LogosValue(v.toString().toStdString());
    case QMetaType::QStringList: {
        LogosValue::List list;
        for (const auto& s : v.toStringList())
            list.emplace_back(s.toStdString());
        return LogosValue(list);
    }
    case QMetaType::QVariantList: {
        return LogosValue(logosValueListFromQVariantList(v.toList()));
    }
    case QMetaType::QVariantMap: {
        LogosValue::Map map;
        QVariantMap qm = v.toMap();
        for (auto it = qm.constBegin(); it != qm.constEnd(); ++it) {
            map[it.key().toStdString()] = logosValueFromQVariant(it.value());
        }
        return LogosValue(map);
    }
    case QMetaType::QJsonArray: {
        LogosValue::List list;
        QJsonArray arr = v.toJsonArray();
        for (const auto& item : arr) {
            list.push_back(logosValueFromQVariant(item.toVariant()));
        }
        return LogosValue(list);
    }
    case QMetaType::QJsonObject: {
        LogosValue::Map map;
        QJsonObject obj = v.toJsonObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            map[it.key().toStdString()] = logosValueFromQVariant(it.value().toVariant());
        }
        return LogosValue(map);
    }
    default: {
        const char* tn = v.typeName();
        if (tn && strcmp(tn, "LogosResult") == 0) {
            const LogosResult* lr = static_cast<const LogosResult*>(v.constData());
            LogosValue::Map m;
            m["success"] = LogosValue(lr->success);
            m["value"] = logosValueFromQVariant(lr->value);
            m["error"] = logosValueFromQVariant(lr->error);
            return LogosValue(m);
        }
        if (v.canConvert<QString>())
            return LogosValue(v.toString().toStdString());
        return LogosValue();
    }
    }
}

QVariant logosValueToQVariant(const LogosValue& v)
{
    if (v.isNull()) return QVariant();
    if (v.isBool()) return QVariant(v.toBool());
    if (v.isInt()) {
        int64_t val = v.toInt();
        if (val >= std::numeric_limits<int>::min() && val <= std::numeric_limits<int>::max())
            return QVariant(static_cast<int>(val));
        return QVariant(static_cast<qlonglong>(val));
    }
    if (v.isDouble()) return QVariant(v.toDouble());
    if (v.isString()) return QVariant(QString::fromStdString(v.toString()));
    if (v.isList()) return QVariant(logosValueListToQVariantList(v.toList()));
    if (v.isMap()) {
        QVariantMap qm;
        for (const auto& [k, val] : v.toMap()) {
            qm[QString::fromStdString(k)] = logosValueToQVariant(val);
        }
        return QVariant(qm);
    }
    return QVariant();
}

std::vector<LogosValue> logosValueListFromQVariantList(const QVariantList& list)
{
    std::vector<LogosValue> result;
    result.reserve(static_cast<size_t>(list.size()));
    for (const auto& item : list) {
        result.push_back(logosValueFromQVariant(item));
    }
    return result;
}

QVariantList logosValueListToQVariantList(const std::vector<LogosValue>& list)
{
    QVariantList result;
    result.reserve(static_cast<int>(list.size()));
    for (const auto& item : list) {
        result.append(logosValueToQVariant(item));
    }
    return result;
}

NativeLogosResult nativeResultFromQt(const LogosResult& qtResult)
{
    NativeLogosResult result;
    result.success = qtResult.success;
    result.value = logosValueFromQVariant(qtResult.value);
    result.error = qtResult.error.toString().toStdString();
    return result;
}

LogosResult qtResultFromNative(const NativeLogosResult& result)
{
    LogosResult qtResult;
    qtResult.success = result.success;
    qtResult.value = logosValueToQVariant(result.value);
    qtResult.error = QVariant(QString::fromStdString(result.error));
    return qtResult;
}
