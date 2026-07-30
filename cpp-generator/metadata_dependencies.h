#ifndef METADATA_DEPENDENCIES_H
#define METADATA_DEPENDENCIES_H

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

/// The module named by one `metadata.json` `dependencies[]` element.
///
/// An element is either a bare name or an object holding that name alongside
/// the constraints an installer resolves it by (version range, signer DID);
/// generation needs the name only. Empty for an element that names nothing.
inline QString dependencyName(const QJsonValue& entry)
{
    if (entry.isString()) {
        return entry.toString();
    }
    if (entry.isObject()) {
        return entry.toObject().value("name").toString();
    }
    return QString();
}

#endif // METADATA_DEPENDENCIES_H
