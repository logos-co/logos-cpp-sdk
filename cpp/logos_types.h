#ifndef LOGOS_TYPES_H
#define LOGOS_TYPES_H

#include <QVariant>
#include <QDataStream>

struct LogosResult { 
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
    //     QString error = result.value.value<string>();
    // }
    QVariant value;

    template<typename T>
    T getValue() const {
        return value.value<T>();
    }

    template<typename T>
    T getValue(const QString& key, T defaultValue = T()) const {
        const QVariantMap &map = value.value<QVariantMap>();
        if (!map.contains(key)) {
            return defaultValue;
        }
        return qvariant_cast<T>(map.value(key));
    }

    template<typename T>
    T getValue(int index, const QString& key, T defaultValue = T()) const {
        const QVariantList &list = value.value<QVariantList>();

        if (index < 0 || index >= list.size()) {
            return defaultValue;
        }

        return qvariant_cast<T>(list[index].toMap().value(key, defaultValue));
    }

};

// Provide (de)serialisation for being use as Remote Object
QDataStream& operator<<(QDataStream& out, const LogosResult& result);
QDataStream& operator>>(QDataStream& in, LogosResult& result);


#endif
