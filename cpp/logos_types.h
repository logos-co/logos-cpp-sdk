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
};

// Provide (de)serialisation for being use as Remote Object
QDataStream& operator<<(QDataStream& out, const LogosResult& result);
QDataStream& operator>>(QDataStream& in, LogosResult& result);

#endif