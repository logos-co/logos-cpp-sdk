// Compiling what the emitters produce. The rest of this suite asserts on text;
// these helpers hand generated code to the compiler that builds the suite, with
// the SDK, logos-protocol and nlohmann include dirs CMake passes in.
#pragma once

#include <QDir>
#include <QFile>
#include <QMap>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

#include "generator_lib.h"
#include "lidl_gen_cdylib.h"
#include "lidl_to_json.h"

namespace generated {

struct Result {
    bool ok = false;
    QString log;
};

struct Wrapper {
    QString header;
    QString source;
};

// `<module>_api.{h,cpp}` for `lidl`, as the umbrella's `--dep` path emits them.
inline Wrapper wrapperFor(const char* lidl, ApiStyle style, const QString& module = "probe",
                          const QString& className = "Probe")
{
    const LidlParseResult pr = lidlParse(QString::fromUtf8(lidl));
    if (pr.hasError()) return {"#error " + qs(pr.error), QString()};
    ModuleDecl mod = pr.module;
    QString idErr;
    lidlInjectIdentity(mod, &idErr);
    const QJsonArray methods = moduleMethodsToJson(mod);
    const QJsonArray events = moduleEventsToJson(mod);
    const QJsonArray records = moduleRecordsToJson(mod);
    return {makeHeader(module, className, methods, style, events, BindMode::Static, records),
            makeSource(module, className, module + "_api.h", methods, style, events,
                       BindMode::Static, records)};
}

// A contract-first cdylib provider around the hand-written `implHeader` (class
// ProbeImpl), plus the empty lp umbrella its exports include.
inline QMap<QString, QString> providerFor(const char* lidl, const QString& implHeader)
{
    ModuleDecl mod = lidlParse(QString::fromUtf8(lidl)).module;
    const QString document = lidlSerialize(mod);
    QString idErr;
    lidlInjectIdentity(mod, &idErr);
    const QString name = qs(mod.name);
    QMap<QString, QString> files{
        {name + "_impl.h", implHeader},
        {name + "_types.h", lidlMakeTypesHeaderCdylib(mod)},
        {name + "_module_impl.cpp",
         lidlMakeModuleImplExports(mod, "ProbeImpl", name + "_impl.h", document)},
        {"logos_sdk.h", makeUmbrellaHeaderFromDeps({}, {}, ApiStyle::Lp, name)},
    };
    if (!mod.events.empty())
        files.insert(name + "_events_cdylib.cpp",
                     lidlMakeEventsSourceCdylib(mod, "ProbeImpl", name + "_impl.h"));
    return files;
}

inline Result runTool(const QString& program, const QStringList& args, const QString& dir)
{
    QProcess p;
    p.setWorkingDirectory(dir);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(program, args);
    Result r;
    if (!p.waitForFinished(300000)) {
        r.log = program + " did not finish: " + p.errorString();
        return r;
    }
    r.log = QString::fromUtf8(p.readAll());
    r.ok = p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
    return r;
}

inline QStringList compilerFlags(const QString& std)
{
    QStringList flags{"-std=" + std, "-Wall", "-Wextra", "-I."};
    for (const QString& dir : QString(LOGOS_TEST_INCLUDES).split('|', Qt::SkipEmptyParts))
        flags << "-I" + dir;
    return flags;
}

inline bool writeFiles(const QTemporaryDir& dir, const QMap<QString, QString>& files)
{
    for (auto it = files.cbegin(); it != files.cend(); ++it) {
        QFile f(QDir(dir.path()).filePath(it.key()));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(it.value().toUtf8());
    }
    return true;
}

// Whether `source`, one of `files`, compiles as `std`.
inline Result compiles(const QMap<QString, QString>& files, const QString& source,
                       const QString& std = "c++17")
{
    QTemporaryDir dir;
    if (!dir.isValid() || !writeFiles(dir, files)) return {false, "cannot write the sources"};
    return runTool(LOGOS_TEST_CXX, compilerFlags(std) << "-fsyntax-only" << source, dir.path());
}

// The lp_* C ABI an lp wrapper calls, stubbed so that every call answers its
// first argument: a value goes out through the generated encode and comes back
// through the generated decode.
inline QString lpEchoStub()
{
    return QStringLiteral(R"CPP(#include "logos_protocol.h"
#include <cstdlib>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>
namespace {
char* heapCopy(const std::string& s)
{
    char* out = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(out, s.c_str(), s.size() + 1);
    return out;
}
std::string firstArg(const char* args)
{
    const nlohmann::json a = nlohmann::json::parse(args ? args : "[]");
    return a.empty() ? std::string("null") : a.at(0).dump();
}
}
extern "C" {
lp_client* lp_client_create(const char*, const char*, const char*, const char*) { return reinterpret_cast<lp_client*>(new int(0)); }
void lp_client_destroy(lp_client* c) { delete reinterpret_cast<int*>(c); }
int lp_invoke(lp_client*, const char*, const char* args, int, char** out, char** err)
{
    if (out) *out = heapCopy(firstArg(args));
    if (err) *err = nullptr;
    return LP_OK;
}
int lp_invoke_async(lp_client*, const char*, const char* args, int, lp_result_cb cb, void* ud)
{
    cb(1, firstArg(args).c_str(), ud);
    return LP_OK;
}
lp_subscription* lp_subscribe(lp_client*, const char*, lp_event_cb, void*) { return reinterpret_cast<lp_subscription*>(new int(0)); }
void lp_unsubscribe(lp_subscription* s) { delete reinterpret_cast<int*>(s); }
void lp_string_free(char* s) { std::free(s); }
char* lp_get_methods(lp_client*) { return heapCopy("[]"); }
int lp_client_set_subscription_status_cb(lp_client*, lp_subscription_status_cb, void*) { return 1; }
unsigned long long lp_client_subscription_generation(lp_client*) { return 0; }
int lp_client_set_subscription_options(lp_client*, const char*) { return 1; }
int lp_client_rearm_subscriptions(lp_client*) { return 0; }
}
)CPP");
}

// Builds `sources` into one program and runs it: ok when it exits 0.
inline Result runs(const QMap<QString, QString>& files, const QStringList& sources)
{
    QTemporaryDir dir;
    if (!dir.isValid() || !writeFiles(dir, files)) return {false, "cannot write the sources"};
    const Result built = runTool(LOGOS_TEST_CXX,
                                 compilerFlags("c++17") << sources << "-o" << "program",
                                 dir.path());
    if (!built.ok) return {false, "build failed:\n" + built.log};
    return runTool(QDir(dir.path()).filePath("program"), {}, dir.path());
}

} // namespace generated
