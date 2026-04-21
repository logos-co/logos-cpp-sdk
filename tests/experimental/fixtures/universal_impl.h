#pragma once
#include <functional>
#include <string>

// Parser tests only read this as text. LogosMap / LogosList match logos_json.h aliases.
// StdLogosResult matches logos_result.h.
class UniversalImpl {
public:
    UniversalImpl() = default;

    LogosMap fetchMap();
    LogosList fetchList();
    QVariantMap asVariantMap();
    QStringList listNames();
    QVariantList anyList();
    StdLogosResult fetchResult();

    std::function<void(const std::string&, const std::string&)> emitEvent;
};
