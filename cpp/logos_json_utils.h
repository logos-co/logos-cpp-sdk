#ifndef LOGOS_JSON_UTILS_H
#define LOGOS_JSON_UTILS_H

#include <QVariant>
#include <QVariantList>
#include <QString>
#include <QJsonObject>

namespace LogosJsonUtils {

    /**
     * Convert a single JSON parameter object {name, value, type} to a QVariant.
     * Supports types: string, QString, int, integer, bool, boolean, double, float.
     * Unknown types are treated as strings.
     * Returns invalid QVariant for unparseable values (e.g. "abc" as int).
     */
    QVariant jsonParamToVariant(const QJsonObject& param);

    /**
     * Parse a JSON string containing an array of {name, value, type} objects
     * into a QVariantList. This is the format used by FFI consumers (e.g. logos-js-sdk).
     *
     * On success, sets ok=true and returns the converted arguments.
     * On failure (JSON parse error or invalid parameter), sets ok=false and
     * fills errorMessage with a description.
     */
    QVariantList parseMethodParams(const QString& json, bool* ok = nullptr, QString* errorMessage = nullptr);

    /**
     * Convert a QVariant result into a JSON-formatted string suitable for
     * returning through the C callback API.
     */
    QString variantToJsonString(const QVariant& value);

    /**
     * Format event data (event name + QVariantList payload) as a JSON string
     * matching the format expected by FFI consumers:
     *   {"event":"<name>","data":["<val1>","<val2>",...]}
     */
    QString formatEventJson(const QString& eventName, const QVariantList& data);

}

#endif // LOGOS_JSON_UTILS_H
