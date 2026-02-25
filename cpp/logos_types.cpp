#include "logos_types.h"

QDataStream& operator<<(QDataStream& out, const LogosResult& result) {
    out << result.success << result.value;
    return out;
}

QDataStream& operator>>(QDataStream& in, LogosResult& result) {
    in >> result.success >> result.value;
    return in;
}