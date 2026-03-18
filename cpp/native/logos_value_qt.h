#ifndef LOGOS_VALUE_QT_H
#define LOGOS_VALUE_QT_H

#include "logos_value.h"
#include "logos_native_types.h"

#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

struct LogosResult;

LogosValue logosValueFromQVariant(const QVariant& v);
QVariant logosValueToQVariant(const LogosValue& v);

std::vector<LogosValue> logosValueListFromQVariantList(const QVariantList& list);
QVariantList logosValueListToQVariantList(const std::vector<LogosValue>& list);

NativeLogosResult nativeResultFromQt(const LogosResult& qtResult);
LogosResult qtResultFromNative(const NativeLogosResult& result);

#endif // LOGOS_VALUE_QT_H
