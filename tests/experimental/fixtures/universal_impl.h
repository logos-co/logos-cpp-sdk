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
    [[nodiscard]] StdLogosResult fetchResultNodiscard();
    static StdLogosResult fetchResultStatic();
    [[nodiscard]] static StdLogosResult fetchResultNodiscardStatic();
    static [[nodiscard]] StdLogosResult fetchResultStaticNodiscard();
    [[nodiscard]] [[deprecated]] StdLogosResult fetchResultMultiAttr();
    inline static StdLogosResult fetchResultInlineStatic();
    [[nodiscard]] consteval StdLogosResult fetchResultConsteval();

    // A std::function member: the parser must skip it (not treat it as a
    // callable method). Used to predate the typed `logos_events:` mechanism
    // as an event hook; that special handling is gone.
    std::function<void(const std::string&, const std::string&)> emitEvent;
};
