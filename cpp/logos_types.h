#ifndef LOGOS_TYPES_H
#define LOGOS_TYPES_H

#include <QDataStream>
#include <QVariant>

struct LogosResult
{
    bool success;
    // Error message if success is false
    // Value can be retrieved like this:
    //
    // LogosResult result = someMethod();
    // if (result.success) {
    //     QString someValue = result.value.value<QString>();
    // }
    //
    // Or:
    //
    // LogosResult result = someMethod();
    // if (!result.success) {
    //     QString error = result.value.value<QString>();
    // }
    QVariant value;

    template<typename T>
    T getValue() const
    {
        return value.value<T>();
    }

    template<typename T>
    T getValue(const QString &key, T defaultValue = T()) const
    {
        const QVariantMap &map = value.value<QVariantMap>();
        if (!map.contains(key)) {
            return defaultValue;
        }
        return qvariant_cast<T>(map.value(key));
    }

    template<typename T>
    T getValue(int index, const QString &key, T defaultValue = T()) const
    {
        const QVariantList &list = value.value<QVariantList>();

        if (index < 0 || index >= list.size()) {
            return defaultValue;
        }

        return qvariant_cast<T>(list[index].toMap().value(key, defaultValue));
    }

    QString error() const
    {
        if (!success) {
            return getString();
        }

        return "";
    }

    QString getString() const { return getValue<QString>(); }

    QString getString(const QString &key, const QString &defaultValue = "") const
    {
        return getValue<QString>(key, defaultValue);
    }

    QString getString(int index, const QString &key, const QString &defaultValue = "") const
    {
        return getValue<QString>(index, key, defaultValue);
    }

    bool getBool() const { return getValue<bool>(); }

    bool getBool(const QString &key) const { return getValue<bool>(key); }

    bool getBool(int index, const QString &key) const { return getValue<bool>(index, key); }

    int getInt() const { return getValue<int>(); }

    int getInt(const QString &key, int defaultValue = 0) const
    {
        return getValue<int>(key, defaultValue);
    }

    int getInt(int index, const QString &key, int defaultValue = 0) const
    {
        return getValue<int>(index, key, defaultValue);
    }

    QVariantList getList() const { return getValue<QVariantList>(); }

    QVariantList getList(const QString &key, const QVariantList &defaultValue = QVariantList()) const
    {
        return getValue<QVariantList>(key, defaultValue);
    }

    QVariantList getList(int index,
                         const QString &key,
                         const QVariantList &defaultValue = QVariantList()) const
    {
        return getValue<QVariantList>(index, key, defaultValue);
    }

    QVariantMap getMap() const { return getValue<QVariantMap>(); }

    QVariantMap getMap(const QString &key, const QVariantMap &defaultValue = QVariantMap()) const
    {
        return getValue<QVariantMap>(key, defaultValue);
    }

    QVariantMap getMap(int index,
                       const QString &key,
                       const QVariantMap &defaultValue = QVariantMap()) const
    {
        return getValue<QVariantMap>(index, key, defaultValue);
    }
};

// Provide (de)serialisation for being use as Remote Object
QDataStream &operator<<(QDataStream &out, const LogosResult &result);
QDataStream &operator>>(QDataStream &in, LogosResult &result);

#endif
