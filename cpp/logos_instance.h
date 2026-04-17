#ifndef LOGOS_INSTANCE_H
#define LOGOS_INSTANCE_H

#include <QCryptographicHash>
#include <QDebug>
#include <QString>
#include <QUuid>

/**
 * @brief LogosInstance provides a shared identifier for all processes launched
 * by Logos core
 *
 * The root process generates a UUID-based ID and exports it via the
 * LOGOS_INSTANCE_ID environment variable. Child processes inherit the variable
 * and return the same ID, so all processes in the application tree share one
 * ID. Used by both provider and consumer to build matching registry URLs.
 */
namespace LogosInstance {
    inline QString shortSha1Hex(const QString& value, int length)
    {
        return QString::fromLatin1(
            QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha1)
                .toHex()
                .left(length));
    }

    inline QString leftUtf8Bytes(const QString& value, int maxBytes)
    {
        if (maxBytes <= 0)
            return QString();

        const QByteArray utf8 = value.toUtf8();
        if (utf8.size() <= maxBytes)
            return value;

        QByteArray truncated = utf8.left(maxBytes);
        while (!truncated.isEmpty()) {
            const QString decoded =
                QString::fromUtf8(truncated.constData(), truncated.size());
            if (decoded.toUtf8().size() == truncated.size())
                return decoded;
            truncated.chop(1);
        }
        return QString();
    }

    inline const QString id() {
        const QByteArray inherited = qgetenv("LOGOS_INSTANCE_ID");
        if (!inherited.isEmpty())
            return QString::fromUtf8(inherited);
        const QString newId = QUuid::createUuid().toString(QUuid::Id128).left(12);
        qputenv("LOGOS_INSTANCE_ID", newId.toUtf8());
        return newId;
    }

    inline QString id(const QString& moduleName) {
        constexpr int kMaxSocketFilenameBytes = 40;
        constexpr int kModuleHashHexLen = 16;
        constexpr int kMaxInstanceIdBytes = 12;

        const QString instanceId = id();
        const QString baseName =
            QStringLiteral("logos_%1_%2").arg(moduleName, instanceId);

        if (baseName.toUtf8().size() <= kMaxSocketFilenameBytes)
            return QStringLiteral("local:") + baseName;

        const QString instanceIdForSocket =
            (instanceId.toUtf8().size() <= kMaxInstanceIdBytes)
                ? instanceId
                : shortSha1Hex(instanceId, kMaxInstanceIdBytes);
        const QString moduleHash = shortSha1Hex(moduleName, kModuleHashHexLen);

        // Reserve bytes for "logos_" + "_" + moduleHash + "_" + instance ID.
        const int fixedBytes =
            8 + kModuleHashHexLen + instanceIdForSocket.toUtf8().size();
        const int modulePrefixBytes =
            qMax(0, kMaxSocketFilenameBytes - fixedBytes);
        const QString modulePrefix = leftUtf8Bytes(moduleName, modulePrefixBytes);

        const QString socketName =
            QStringLiteral("logos_%1_%2_%3")
                .arg(modulePrefix, moduleHash, instanceIdForSocket);
        return QStringLiteral("local:") + socketName;
    }
}

#endif // LOGOS_INSTANCE_H
