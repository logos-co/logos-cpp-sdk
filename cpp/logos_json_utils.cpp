#include "logos_json_utils.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

namespace LogosJsonUtils {

QVariant jsonParamToVariant(const QJsonObject& param)
{
    QString name = param.value("name").toString();
    QString value = param.value("value").toString();
    QString type = param.value("type").toString();

    qDebug() << "LogosJsonUtils: Converting param:" << name << "value:" << value << "type:" << type;

    if (type == "string" || type == "QString") {
        return QVariant(value);
    } else if (type == "int" || type == "integer") {
        bool ok;
        int intValue = value.toInt(&ok);
        return ok ? QVariant(intValue) : QVariant();
    } else if (type == "bool" || type == "boolean") {
        if (value.toLower() == "true" || value == "1") {
            return QVariant(true);
        } else if (value.toLower() == "false" || value == "0") {
            return QVariant(false);
        }
        return QVariant();
    } else if (type == "double" || type == "float") {
        bool ok;
        double doubleValue = value.toDouble(&ok);
        return ok ? QVariant(doubleValue) : QVariant();
    } else {
        qWarning() << "LogosJsonUtils: Unknown parameter type:" << type << "- treating as string";
        return QVariant(value);
    }
}

QVariantList parseMethodParams(const QString& json, bool* ok, QString* errorMessage)
{
    QVariantList args;

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(json.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        if (ok) *ok = false;
        if (errorMessage) *errorMessage = QString("JSON parse error: %1").arg(parseError.errorString());
        return args;
    }

    QJsonArray paramsArray = jsonDoc.array();

    for (const QJsonValue& paramValue : paramsArray) {
        if (paramValue.isObject()) {
            QJsonObject paramObj = paramValue.toObject();
            QVariant variant = jsonParamToVariant(paramObj);
            if (variant.isValid()) {
                args.append(variant);
            } else {
                if (ok) *ok = false;
                if (errorMessage) *errorMessage = QString("Invalid parameter: %1").arg(paramObj.value("name").toString());
                return QVariantList();
            }
        }
    }

    if (ok) *ok = true;
    return args;
}

QString variantToJsonString(const QVariant& value)
{
    if (!value.isValid())
        return QStringLiteral("null");

    switch (value.userType()) {
    case QMetaType::Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case QMetaType::Int:
    case QMetaType::LongLong:
        return QString::number(value.toLongLong());
    case QMetaType::Double:
    case QMetaType::Float:
        return QString::number(value.toDouble());
    case QMetaType::QString:
        return value.toString();
    case QMetaType::QVariantMap: {
        QJsonDocument doc(QJsonObject::fromVariantMap(value.toMap()));
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }
    case QMetaType::QVariantList: {
        QJsonDocument doc(QJsonArray::fromVariantList(value.toList()));
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    }
    default:
        break;
    }

    return value.toString();
}

QString formatEventJson(const QString& eventName, const QVariantList& data)
{
    QString result = QString("{\"event\":\"%1\",\"data\":[").arg(eventName);
    for (int i = 0; i < data.size(); ++i) {
        if (i > 0) result += ",";
        result += QString("\"%1\"").arg(data[i].toString());
    }
    result += "]}";
    return result;
}

} // namespace LogosJsonUtils
