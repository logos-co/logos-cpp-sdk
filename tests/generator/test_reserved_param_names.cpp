// A contract parameter named like one of the wrapper's own. Every method appends
// `err` and a deadline (`timeout_ms` on lp, `timeout` on Qt), the async ones a
// `callback`, and the bodies declare locals; logos-rust-sdk's
// sdk_test_caller_module has `timed_call(sleep_ms, timeout_ms)`, which emitted
// "redefinition of parameter 'timeout_ms'". The C++ name moves; the wire, which
// is positional, does not.

#include <gtest/gtest.h>

#include "generated_code.h"

namespace {

const char* kContract = R"LIDL(
module probe {
  version "1.0.0"
  method timed(sleep_ms: int, timeout_ms: int) -> int
  method every(err: tstr, callback: tstr, timeout: bool, _args: int, _err: int, _r: int, _result: int, m_client: tstr, m_state: tstr, m_moduleName: tstr) -> tstr
  method taken(timeout_ms: int, timeout_ms_: int) -> int
  method plain(timeoutMs: int, error: tstr) -> int
  event fired(args: tstr, emitEventImpl_: int)
}
)LIDL";

} // namespace

TEST(ReservedParamNames, LpWrapperSpellsThemApartAndCompiles)
{
    const generated::Wrapper w = generated::wrapperFor(kContract, ApiStyle::Lp);
    EXPECT_TRUE(w.header.contains(
        "int64_t timed(int64_t sleep_ms, int64_t timeout_ms_, logos::CallError* err = nullptr, "
        "int timeout_ms = 0);")) << w.header.toStdString();
    // The argument is the contract's; the deadline is still the caller's.
    EXPECT_TRUE(w.source.contains("_args.push_back(timeout_ms_);")) << w.source.toStdString();
    EXPECT_TRUE(w.source.contains("m_client.invoke(\"timed\", _args, &_err, timeout_ms);"))
        << w.source.toStdString();
    EXPECT_TRUE(w.header.contains(
        "std::string every(const std::string& err_, const std::string& callback_, bool timeout, "
        "int64_t _args_, int64_t _err_, int64_t _r_, int64_t _result, "
        "const std::string& m_client_, const std::string& m_state_, "
        "const std::string& m_moduleName, ")) << w.header.toStdString();
    // Never onto a name the contract already spells.
    EXPECT_TRUE(w.header.contains("int64_t taken(int64_t timeout_ms__, int64_t timeout_ms_, "))
        << w.header.toStdString();
    EXPECT_TRUE(w.header.contains("int64_t plain(int64_t timeoutMs, const std::string& error, "))
        << w.header.toStdString();

    const generated::Result r = generated::compiles(
        {{"probe_api.h", w.header}, {"probe_api.cpp", w.source}}, "probe_api.cpp");
    EXPECT_TRUE(r.ok) << r.log.toStdString();
}

// The Qt surface has its own names (`timeout`, `_result`, `m_moduleName`); its
// headers live in logos-plugin-qt, so only the spelling is checked here.
TEST(ReservedParamNames, QtWrapperSpellsThemApart)
{
    const generated::Wrapper w = generated::wrapperFor(kContract, ApiStyle::Qt);
    EXPECT_TRUE(w.header.contains(
        "QString every(const QString& err_, const QString& callback_, bool timeout_, "
        "qlonglong _args, qlonglong _err_, qlonglong _r, qlonglong _result_, "
        "const QString& m_client_, const QString& m_state, const QString& m_moduleName_, "
        "logos::CallError* err = nullptr, Timeout timeout = Timeout());")) << w.header.toStdString();
    EXPECT_TRUE(w.source.contains("QVariant::fromValue(timeout_)")) << w.source.toStdString();
    EXPECT_TRUE(w.header.contains("qlonglong timed(qlonglong sleep_ms, qlonglong timeout_ms, "))
        << w.header.toStdString();
}

// The provider's event bodies declare `args` and call `emitEventImpl_`.
TEST(ReservedParamNames, ProviderEventBodiesCompile)
{
    const QMap<QString, QString> files = generated::providerFor(kContract, R"CPP(#pragma once
#include <cstdint>
#include <string>
#include "logos_module_context.h"
class ProbeImpl : public LogosModuleContext {
public:
    int64_t timed(int64_t sleep_ms, int64_t timeout_ms) { return sleep_ms + timeout_ms; }
    std::string every(const std::string&, const std::string&, bool, int64_t, int64_t, int64_t,
                      int64_t, const std::string&, const std::string&, const std::string&)
    { return {}; }
    int64_t taken(int64_t a, int64_t b) { return a + b; }
    int64_t plain(int64_t a, const std::string&) { return a; }
logos_events:
    void fired(const std::string& args, int64_t emitEventImpl_);
};
)CPP");
    const QString events = files.value("probe_events_cdylib.cpp");
    EXPECT_TRUE(events.contains(
        "void ProbeImpl::fired(const std::string& args_, int64_t emitEventImpl__)"))
        << events.toStdString();
    EXPECT_TRUE(events.contains("args.push_back(args_);")) << events.toStdString();
    EXPECT_TRUE(events.contains("emitEventImpl_(\"fired\", &args);")) << events.toStdString();

    for (const QString& tu : {QString("probe_events_cdylib.cpp"), QString("probe_module_impl.cpp")}) {
        const generated::Result r = generated::compiles(files, tu);
        EXPECT_TRUE(r.ok) << tu.toStdString() << "\n" << r.log.toStdString();
    }
}
