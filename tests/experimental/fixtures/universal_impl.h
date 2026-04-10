#pragma once
#include <functional>
#include <string>

// Parser tests only read this as text. LogosMap / LogosList match logos_json.h aliases.
class UniversalImpl {
public:
    UniversalImpl() = default;

    LogosMap fetchMap();
    LogosList fetchList();
    QVariantMap asVariantMap();
    QStringList listNames();
    QVariantList anyList();

    std::function<void(const std::string&, const std::string&)> emitEvent;
};
