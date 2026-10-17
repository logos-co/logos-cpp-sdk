// A contract name that is a C++ keyword. LIDL admits any identifier, so a
// record, field, method, parameter or event may be called `class` or `delete`;
// those were emitted verbatim and did not compile. The C++ identifier gains a
// `_`; the wire keeps the name, and so do names derived from it (`deleteAsync`).

#include <gtest/gtest.h>

#include "generated_code.h"

namespace {

const char* kContract = R"LIDL(
module probe {
  version "1.0.0"
  type operator {
    new: tstr
    ? default: int
    namespace: [tstr]
    requires: bool
  }
  type Holder {
    op: operator
    ops: [operator]
    byName: {tstr: operator}
    ? maybe: operator
  }
  method delete(class: tstr, register: int) -> operator
  method delete_(x: int) -> int
  method echo(h: Holder) -> Holder
  method and(or: bool, not: bool) -> bool
  method co_await(concept: int) -> int
  event class(for: tstr, while: int)
}
)LIDL";

} // namespace

TEST(KeywordNames, LpWrapperSpellsThemApartAndCompiles)
{
    const generated::Wrapper w = generated::wrapperFor(kContract, ApiStyle::Lp);
    for (const char* decl : {"struct operator_ {", "std::string new_{};",
                             "std::optional<int64_t> default_{};",
                             "std::vector<std::string> namespace_{};", "bool requires_{};",
                             "operator_ op{};", "std::vector<operator_> ops{};",
                             "std::map<std::string, operator_> byName{};",
                             "std::optional<operator_> maybe{};"})
        EXPECT_TRUE(w.header.contains(decl)) << decl << "\n" << w.header.toStdString();
    // `delete_` is the contract's own method, so `delete` moves past it.
    EXPECT_TRUE(w.header.contains("operator_ delete__(const std::string& class_, int64_t register_, "))
        << w.header.toStdString();
    EXPECT_TRUE(w.header.contains("int64_t delete_(int64_t x, ")) << w.header.toStdString();
    EXPECT_TRUE(w.header.contains("void deleteAsync(const std::string& class_, int64_t register_, "))
        << w.header.toStdString();
    EXPECT_TRUE(w.header.contains("bool and_(bool or_, bool not_, ")) << w.header.toStdString();
    EXPECT_TRUE(w.header.contains("int64_t co_await_(int64_t concept_, ")) << w.header.toStdString();
    EXPECT_TRUE(w.header.contains(
        "logos::SubHandle onClass(std::function<void(const std::string& for_, int64_t while_)> callback);"))
        << w.header.toStdString();

    // The wire keeps every name.
    EXPECT_TRUE(w.source.contains("Probe::operator_ Probe::delete__(")) << w.source.toStdString();
    EXPECT_TRUE(w.source.contains("m_client.invoke(\"delete\", _args, &_err, timeout_ms);"))
        << w.source.toStdString();
    EXPECT_TRUE(w.source.contains("__j[\"new\"] = v.new_;")) << w.source.toStdString();
    EXPECT_TRUE(w.source.contains("if (w.contains(\"requires\")) __out.requires_ = "))
        << w.source.toStdString();
    EXPECT_TRUE(w.source.contains("m_client.subscribe(\"class\", ")) << w.source.toStdString();

    const QMap<QString, QString> files{{"probe_api.h", w.header}, {"probe_api.cpp", w.source}};
    for (const char* std : {"c++17", "c++20"}) {
        const generated::Result r = generated::compiles(files, "probe_api.cpp", std);
        EXPECT_TRUE(r.ok) << std << "\n" << r.log.toStdString();
    }
}

TEST(KeywordNames, QtWrapperSpellsThemApart)
{
    const generated::Wrapper w = generated::wrapperFor(kContract, ApiStyle::Qt);
    EXPECT_TRUE(w.header.contains("struct operator_ {")) << w.header.toStdString();
    EXPECT_TRUE(w.header.contains("QString new_{};")) << w.header.toStdString();
    EXPECT_TRUE(w.header.contains("operator_ delete__(const QString& class_, qlonglong register_, "))
        << w.header.toStdString();
    EXPECT_TRUE(w.source.contains(
        "m_client->invokeRemoteMethod(\"probe\", \"delete\", QVariantList{QVariant::fromValue(class_)"))
        << w.source.toStdString();
    EXPECT_TRUE(w.source.contains("__m.insert(QStringLiteral(\"new\"), QVariant::fromValue(v.new_));"))
        << w.source.toStdString();
    EXPECT_TRUE(w.source.contains("__out.new_ = __m.value(QStringLiteral(\"new\")).toString();"))
        << w.source.toStdString();
}

// A contract-first provider's structs and impl class are the author's, and use
// the same spellings; the generated codec, dispatch and event bodies match them.
TEST(KeywordNames, ProviderCompilesAgainstTheSameSpellings)
{
    const QMap<QString, QString> files = generated::providerFor(kContract, R"CPP(#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "logos_module_context.h"
struct operator_ {
    std::string new_;
    std::optional<int64_t> default_;
    std::vector<std::string> namespace_;
    bool requires_ = false;
};
struct Holder {
    operator_ op;
    std::vector<operator_> ops;
    std::map<std::string, operator_> byName;
    std::optional<operator_> maybe;
};
class ProbeImpl : public LogosModuleContext {
public:
    operator_ delete__(const std::string& class_, int64_t register_);
    int64_t delete_(int64_t x);
    Holder echo(const Holder& h);
    bool and_(bool or_, bool not_);
    int64_t co_await_(int64_t concept_);
logos_events:
    void class_(const std::string& for_, int64_t while_);
};
)CPP");
    EXPECT_TRUE(files.value("probe_types.h").contains("out[\"new\"] = Codec<std::string>::to(v.new_);"))
        << files.value("probe_types.h").toStdString();
    EXPECT_TRUE(files.value("probe_module_impl.cpp").contains("lidlImpl().delete__("))
        << files.value("probe_module_impl.cpp").toStdString();
    EXPECT_TRUE(files.value("probe_events_cdylib.cpp").contains(
        "void ProbeImpl::class_(const std::string& for_, int64_t while_)"))
        << files.value("probe_events_cdylib.cpp").toStdString();

    for (const char* std : {"c++17", "c++20"})
        for (const QString& tu : {QString("probe_events_cdylib.cpp"), QString("probe_module_impl.cpp")}) {
            const generated::Result r = generated::compiles(files, tu, std);
            EXPECT_TRUE(r.ok) << std << " " << tu.toStdString() << "\n" << r.log.toStdString();
        }
}
