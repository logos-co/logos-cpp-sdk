// Tests for logos_host_core.h — the host-side veneer over liblogos'
// logos_core_* C API.
//
// The C API is `extern "C"`, so this translation unit DEFINES it itself. That
// is the whole reason these tests can be meaningful without a running core:
// the interesting behaviour of the veneer is what it does with the memory
// liblogos hands back, and a stub lets us assert that directly — including the
// `delete[]`-not-`free()` rule, which is the single most-copied piece of
// knowledge across the host repos and the one a real core cannot check for us.
//
// The stubs allocate EXACTLY as liblogos does (`new char*[]` for the array,
// `new char[]` for each element and for single-string returns). If the veneer
// ever switched to free()/delete, this suite would fail under ASan rather than
// silently corrupting the heap in production.

#include "logos_host_core.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ── stub state, reset per test ───────────────────────────────────────────────
struct CoreStub {
    int  initCalls = 0;
    int  startCalls = 0;
    int  cleanupCalls = 0;
    std::vector<std::string> modulesDirs;
    std::string persistenceBasePath;
    std::string accessPolicy;
    bool accessPolicySet = false;
    std::vector<std::pair<std::string, std::string>> transports;
    // Records the ORDER in which the C API was touched, so the ordering
    // contract ("all config strictly before start") can be asserted rather
    // than assumed.
    std::vector<std::string> callOrder;

    std::vector<std::string> known{"alpha", "beta", "gamma"};
    std::vector<std::string> loaded{"alpha"};
    // The REAL contract process-stats emits (src/process_stats.cpp:157-161).
    // This previously stubbed {"cpu":..,"memory":..}, keys nothing produces, so
    // it validated the parser's bug instead of the producer's format.
    std::string statsJson =
        R"([{"name":"alpha","cpu_percent":12.5,"cpu_time_seconds":3.5,"memory_mb":4096.0}])";
    bool tokenPresent = true;
    // The LogosLoadDeps value the wrapper passed, not a bool: the point of the
    // enum is that there are three answers, and a bool stub could not tell
    // REQUIRED_DEPS from REQUIRED_AND_OPTIONAL.
    int  lastLoadDeps = -1;
    std::string optionalReport = "[]";
    int  lastUnloadWithDependents = -1;
    bool loadSucceeds = true;
    LogosCoreTokenListener tokenListener = nullptr;
    void* tokenListenerData = nullptr;

    // The protected setters and the shell binding.
    std::vector<std::string> bundledDirs;
    std::string placement;
    std::string shellName;
    std::string packageConfig;
    int refuseSetters = 0;          // what the protected setters answer
    bool bindingAvailable = true;   // capability_module is the token authority
    int bindingReleases = 0;
    // core_service over the binding: each call, and a canned answer per method.
    std::vector<std::pair<std::string, std::string>> coreServiceCalls; // (method, args)
    std::map<std::string, std::string> answers;
};

CoreStub* g = nullptr;

char* dupC(const std::string& s)
{
    char* r = new char[s.size() + 1];   // matches liblogos (logos_core.cpp:85)
    std::memcpy(r, s.c_str(), s.size() + 1);
    return r;
}

char** dupCArray(const std::vector<std::string>& xs)
{
    char** a = new char*[xs.size() + 1]; // matches toNullTerminatedArray
    for (std::size_t i = 0; i < xs.size(); ++i) a[i] = dupC(xs[i]);
    a[xs.size()] = nullptr;
    return a;
}

class HostCoreTest : public ::testing::Test {
protected:
    void SetUp() override { stub = CoreStub{}; g = &stub; }
    void TearDown() override { g = nullptr; }
    CoreStub stub;
};

} // namespace

extern "C" {
void logos_core_init(int, char**)                 { ++g->initCalls;    g->callOrder.push_back("init"); }
void logos_core_start()                           { ++g->startCalls;   g->callOrder.push_back("start"); }
void logos_core_cleanup()                         { ++g->cleanupCalls; g->callOrder.push_back("cleanup"); }
void logos_core_add_modules_dir(const char* d)    { g->modulesDirs.emplace_back(d); g->callOrder.push_back("add_dir"); }
void logos_core_set_persistence_base_path(const char* p) { g->persistenceBasePath = p; g->callOrder.push_back("persistence"); }
void logos_core_set_access_policy(const char* p)  { g->accessPolicySet = true; g->accessPolicy = p ? p : ""; g->callOrder.push_back("policy"); }
void logos_core_set_module_transports(const char* m, const char* j) { g->transports.emplace_back(m, j); g->callOrder.push_back("transports"); }
void logos_core_refresh_modules()                 { g->callOrder.push_back("refresh"); }

char** logos_core_get_known_modules()             { return dupCArray(g->known); }
char** logos_core_get_loaded_modules()            { return dupCArray(g->loaded); }
char** logos_core_get_module_dependencies(const char*, bool r) { return dupCArray(r ? std::vector<std::string>{"d1","d2"} : std::vector<std::string>{"d1"}); }
char** logos_core_get_module_dependents(const char*, bool)     { return dupCArray({}); }
char** logos_core_get_module_optional_dependencies(const char*) { return dupCArray({"opt1","opt2"}); }

int logos_core_load_module(const char*, LogosLoadDeps deps) { g->lastLoadDeps = static_cast<int>(deps); return g->loadSucceeds ? 1 : 0; }
char* logos_core_optional_load_report(const char*)          { return dupC(g->optionalReport); }
int logos_core_unload_module(const char*, bool withDepdts) { g->lastUnloadWithDependents = withDepdts ? 1 : 0; return 1; }

char* logos_core_get_modules_info()               { return dupC("[]"); }
char* logos_core_process_module(const char*)      { return dupC("processed"); }
char* logos_core_get_token(const char*)           { return g->tokenPresent ? dupC("tok-123") : nullptr; }
void logos_core_set_token_listener(LogosCoreTokenListener l, void* d)
{
    g->tokenListener = l;
    g->tokenListenerData = d;
    g->callOrder.push_back(l ? "token_listener" : "token_listener_removed");
}
char* logos_core_get_module_stats()               { return g->statsJson.empty() ? nullptr : dupC(g->statsJson); }

int logos_core_set_bundled_modules_dirs(const char* const* dirs)
{
    for (const char* const* d = dirs; *d; ++d) g->bundledDirs.emplace_back(*d);
    g->callOrder.push_back("bundled_dirs");
    return g->refuseSetters;
}
int logos_core_set_placement_policy(const char* p) { g->placement = p; g->callOrder.push_back("placement"); return g->refuseSetters; }
int logos_core_set_shell_identity(const char* n)   { g->shellName = n; g->callOrder.push_back("shell"); return g->refuseSetters; }
int logos_core_set_package_config(const char* c)   { g->packageConfig = c; g->callOrder.push_back("package_config"); return g->refuseSetters; }

// The binding is a tag: nothing here dereferences it.
char gBindingTag;
logos_consumer* logos_core_take_shell_binding(void)
{
    g->callOrder.push_back("take_binding");
    return g->bindingAvailable ? reinterpret_cast<logos_consumer*>(&gBindingTag) : nullptr;
}
const char* logos_consumer_name(const logos_consumer*) { return g->shellName.c_str(); }
char* logos_consumer_credential(const logos_consumer*)
{
    char* value = static_cast<char*>(std::malloc(10));
    std::memcpy(value, "shell-cr", 9);
    return value;
}
int logos_consumer_call(logos_consumer* consumer, const char* target, const char* method,
                        const char* args, int, char** out, char** err)
{
    if (consumer != reinterpret_cast<logos_consumer*>(&gBindingTag) || std::string(target) != "core_service")
        return -1;
    g->coreServiceCalls.emplace_back(method, args);
    const auto it = g->answers.find(method);
    if (it == g->answers.end()) return -1;
    *out = static_cast<char*>(std::malloc(it->second.size() + 1));
    std::memcpy(*out, it->second.c_str(), it->second.size() + 1);
    *err = nullptr;
    return 0;
}
logos_consumer_subscription* logos_consumer_subscribe(logos_consumer*, const char*, const char*,
                                                      logos_consumer_event_cb, void*) { return nullptr; }
void logos_consumer_unsubscribe(logos_consumer_subscription*) {}
void logos_consumer_string_free(char* value) { std::free(value); }
void logos_consumer_release(logos_consumer*) { ++g->bindingReleases; g->callOrder.push_back("release_binding"); }
}

namespace {

using logos::host::LogosCore;

LogosCore::Config emptyConfig() { return LogosCore::Config{}; }

// ── lifecycle ────────────────────────────────────────────────────────────────

TEST_F(HostCoreTest, ConstructionInitialisesAndDestructionCleansUpExactlyOnce)
{
    {
        LogosCore core(0, nullptr, emptyConfig());
        EXPECT_EQ(stub.initCalls, 1);
        EXPECT_EQ(stub.cleanupCalls, 0);
    }
    EXPECT_EQ(stub.cleanupCalls, 1);
}

TEST_F(HostCoreTest, EveryPreStartSettingIsAppliedBeforeStart)
{
    LogosCore::Config cfg;
    cfg.modulesDirs = {"/one", "/two"};
    cfg.persistenceBasePath = "/persist";
    cfg.accessPolicyJson = std::string(R"({"mode":"enforce"})");
    cfg.moduleTransports = {{"mod_a", "[]"}};

    LogosCore core(0, nullptr, std::move(cfg));
    core.start();

    EXPECT_EQ(stub.modulesDirs, (std::vector<std::string>{"/one", "/two"}));
    EXPECT_EQ(stub.persistenceBasePath, "/persist");
    EXPECT_TRUE(stub.accessPolicySet);
    ASSERT_EQ(stub.transports.size(), 1u);
    EXPECT_EQ(stub.transports[0].first, "mod_a");

    // The ordering contract, asserted rather than trusted: "start" must be the
    // LAST thing, with every configuration call ahead of it. This is the
    // constraint logos_core.h states only in comments.
    const auto startAt = std::find(stub.callOrder.begin(), stub.callOrder.end(), "start");
    ASSERT_NE(startAt, stub.callOrder.end());
    EXPECT_EQ(startAt + 1, stub.callOrder.end())
        << "something was configured after start()";
    EXPECT_EQ(stub.callOrder.front(), "init");
}

TEST_F(HostCoreTest, AbsentOptionalSettingsAreNotPushedAtAll)
{
    // nullopt policy must install NO policy — distinct from an empty one,
    // because liblogos treats "no policy" as unrestricted.
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_FALSE(stub.accessPolicySet);
    EXPECT_TRUE(stub.modulesDirs.empty());
    EXPECT_TRUE(stub.persistenceBasePath.empty());
}

TEST_F(HostCoreTest, TokenListenerIsInstalledBeforeStartAndRemovedBeforeCleanup)
{
    std::vector<std::pair<std::string, std::string>> seen;
    LogosCore::Config cfg;
    cfg.tokenListener = [&](const std::string& key, const std::string& token) {
        seen.emplace_back(key, token);
    };
    {
        LogosCore core(0, nullptr, std::move(cfg));
        ASSERT_NE(stub.tokenListener, nullptr);
        stub.tokenListener("capability_module", "tok-1", stub.tokenListenerData);
        core.start();
    }
    EXPECT_EQ(seen, (std::vector<std::pair<std::string, std::string>>{
        {"capability_module", "tok-1"}}));
    EXPECT_EQ(stub.callOrder, (std::vector<std::string>{
        "init", "token_listener", "start", "token_listener_removed", "cleanup"}));
}

TEST_F(HostCoreTest, NoTokenListenerTouchesNothing)
{
    { LogosCore core(0, nullptr, emptyConfig()); }
    EXPECT_EQ(stub.callOrder, (std::vector<std::string>{"init", "cleanup"}));
}

TEST_F(HostCoreTest, EmptyAccessPolicyStringIsStillInstalled)
{
    LogosCore::Config cfg;
    cfg.accessPolicyJson = std::string("");
    LogosCore core(0, nullptr, std::move(cfg));
    EXPECT_TRUE(stub.accessPolicySet) << "an explicitly empty policy is a choice, not an absence";
}

// ── ownership: the char**/char* draining ────────────────────────────────────

TEST_F(HostCoreTest, StringArraysAreDrainedIntoOwningVectors)
{
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_EQ(core.knownModules(), (std::vector<std::string>{"alpha", "beta", "gamma"}));
    EXPECT_EQ(core.loadedModules(), (std::vector<std::string>{"alpha"}));
}

TEST_F(HostCoreTest, EmptyArrayDrainsToEmptyVectorRatherThanCrashing)
{
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_TRUE(core.dependents("alpha").empty());
}

TEST_F(HostCoreTest, RecursiveFlagReachesTheCApi)
{
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_EQ(core.dependencies("alpha", /*recursive=*/false).size(), 1u);
    EXPECT_EQ(core.dependencies("alpha", /*recursive=*/true).size(), 2u);
}

TEST_F(HostCoreTest, NullCStringBecomesNulloptNotEmptyString)
{
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_EQ(core.token("core").value(), "tok-123");

    stub.tokenPresent = false;
    EXPECT_FALSE(core.token("core").has_value())
        << "a NULL return means absent, and must not be flattened to \"\"";
}

// ── load/unload defaults ────────────────────────────────────────────────────

TEST_F(HostCoreTest, LoadDefaultsToResolvingDependenciesAndUnloadDoesNotCascade)
{
    LogosCore core(0, nullptr, emptyConfig());

    EXPECT_TRUE(core.loadModule("alpha"));
    EXPECT_EQ(g->lastLoadDeps, static_cast<int>(LOGOS_LOAD_REQUIRED_DEPS))
        << "the default must stay the required tree — it is what every host "
           "asking loadModule(name) has always got";
    EXPECT_EQ(stub.lastLoadDeps, static_cast<int>(LOGOS_LOAD_REQUIRED_DEPS))
        << "a host almost always wants the dependency graph";

    EXPECT_TRUE(core.unloadModule("alpha"));
    EXPECT_EQ(stub.lastUnloadWithDependents, 0)
        << "cascading unload must be opt-in; it breaks live dependents";

    core.unloadModule("alpha", /*withDependents=*/true);
    EXPECT_EQ(stub.lastUnloadWithDependents, 1);
}

TEST_F(HostCoreTest, LoadFailureIsReportedAsFalse)
{
    stub.loadSucceeds = false;
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_FALSE(core.loadModule("alpha"))
        << "logos_core_load_module returns int; only ==1 is success";
}

// ── stats: the blob parse ───────────────────────────────────────────────────

TEST_F(HostCoreTest, StatsAreIndexedOutOfTheSingleBlob)
{
    LogosCore core(0, nullptr, emptyConfig());
    const auto s = core.stats("alpha");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->name, "alpha");
    EXPECT_DOUBLE_EQ(s->cpuPercent, 12.5);
    EXPECT_DOUBLE_EQ(s->memoryMb, 4096.0);
    EXPECT_DOUBLE_EQ(s->cpuTimeSeconds, 3.5);
    EXPECT_EQ(s->raw["name"], "alpha") << "the raw entry stays reachable";
}

TEST_F(HostCoreTest, StatsForAnUnloadedModuleIsNullopt)
{
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_FALSE(core.stats("not-loaded").has_value());
}

TEST_F(HostCoreTest, MalformedStatsJsonYieldsEmptyRatherThanThrowing)
{
    // A host polls this on a timer; a parse failure must not take the process
    // down. nlohmann is invoked with allow_exceptions=false for this reason.
    stub.statsJson = "{not json";
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_TRUE(core.allStats().empty());
    EXPECT_FALSE(core.stats("alpha").has_value());
}

TEST_F(HostCoreTest, NullStatsYieldsEmpty)
{
    stub.statsJson.clear();   // stub returns nullptr
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_TRUE(core.allStats().empty());
}

TEST_F(HostCoreTest, NonArrayStatsIsRejected)
{
    stub.statsJson = R"({"name":"alpha"})";   // object, not array
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_TRUE(core.allStats().empty());
}

} // namespace

// The third answer the enum exists for. A bool could not express it, which is
// why this parameter stopped being one.
TEST_F(HostCoreTest, BestEffortOptionalReachesTheCApi)
{
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_TRUE(core.loadModule("alpha", LOGOS_LOAD_REQUIRED_AND_OPTIONAL));
    EXPECT_EQ(stub.lastLoadDeps, static_cast<int>(LOGOS_LOAD_REQUIRED_AND_OPTIONAL));
}

// Worth asking after such a load: a skipped optional dependency keeps whatever
// state it had, so nothing else tells it apart from one nobody wanted.
TEST_F(HostCoreTest, OptionalLoadReportIsPassedThrough)
{
    stub.optionalReport =
        R"([{"module":"extra","named_by":"alpha","reason":"not_installed"}])";
    LogosCore core(0, nullptr, emptyConfig());
    const auto report = core.optionalLoadReportJson("alpha");
    ASSERT_TRUE(report.has_value());
    EXPECT_NE(report->find("\"module\":\"extra\""), std::string::npos) << *report;
}

// The one entry point the mirror used to omit, in the release that made
// optional dependencies loadable.
TEST_F(HostCoreTest, OptionalDependenciesAreReachable)
{
    LogosCore core(0, nullptr, emptyConfig());
    EXPECT_EQ(core.optionalDependencies("alpha"),
              (std::vector<std::string>{"opt1", "opt2"}));
}

// ── the shell binding ───────────────────────────────────────────────────────

namespace {

LogosCore::Config shellConfig()
{
    LogosCore::Config cfg;
    cfg.bundledModulesDirs = {"/app/modules", "/app/modules-pkg"};
    cfg.placementPolicyJson = std::string(R"({"default":"subprocess"})");
    cfg.packageConfigJson = std::string(R"({"user_modules_dir":"/u/modules"})");
    cfg.shellName = "basecamp";
    return cfg;
}

} // namespace

TEST_F(HostCoreTest, ProtectedInputIsAppliedBeforeStart)
{
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_EQ(stub.bundledDirs, (std::vector<std::string>{"/app/modules", "/app/modules-pkg"}));
    EXPECT_EQ(stub.placement, R"({"default":"subprocess"})");
    EXPECT_EQ(stub.shellName, "basecamp");
    EXPECT_EQ(stub.packageConfig, R"({"user_modules_dir":"/u/modules"})");
    EXPECT_EQ(stub.callOrder, (std::vector<std::string>{
        "init", "bundled_dirs", "placement", "package_config", "shell", "start",
        "take_binding"}));
}

TEST_F(HostCoreTest, ARefusedSettingThrowsAfterCleaningUp)
{
    stub.refuseSetters = -1;
    EXPECT_THROW(LogosCore(0, nullptr, shellConfig()), std::invalid_argument);
    EXPECT_EQ(stub.cleanupCalls, 1) << "an initialised core must not be left behind";
}

TEST_F(HostCoreTest, WithTheBindingLifecycleGoesThroughCoreService)
{
    stub.answers = {
        {"loadModule", R"({"status":"ok","module":"alpha"})"},
        {"unloadModule", R"({"status":"ok","module":"alpha"})"},
        {"refreshModules", R"({"status":"ok"})"},
        {"listModules", R"([{"name":"alpha","status":"loaded"},{"name":"beta","status":"loaded"}])"},
        {"getModuleStats", R"([{"name":"alpha","cpu_percent":1.5,"memory_mb":2.0}])"},
    };
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    ASSERT_TRUE(core.shellBound());

    EXPECT_TRUE(core.loadModule("alpha"));
    EXPECT_TRUE(core.loadModule("alpha", LOGOS_LOAD_MODULE_ONLY));
    EXPECT_TRUE(core.unloadModule("alpha", /*withDependents=*/true));
    core.refreshModules();
    EXPECT_EQ(core.loadedModules(), (std::vector<std::string>{"alpha", "beta"}));
    const auto stats = core.stats("alpha");
    ASSERT_TRUE(stats.has_value());
    EXPECT_DOUBLE_EQ(stats->cpuPercent, 1.5);

    EXPECT_EQ(stub.lastLoadDeps, -1) << "the C API was bypassed";
    EXPECT_EQ(stub.coreServiceCalls, (std::vector<std::pair<std::string, std::string>>{
        {"loadModule", R"(["alpha","required"])"},
        {"loadModule", R"(["alpha","module_only"])"},
        {"unloadModule", R"(["alpha",true])"},
        {"refreshModules", "[]"},
        {"listModules", R"(["loaded"])"},
        {"getModuleStats", "[]"},
    }));
}

TEST_F(HostCoreTest, AnErrorAnswerIsAFailedLoad)
{
    stub.answers = {{"loadModule", R"({"status":"error","code":"MODULE_LOAD_FAILED"})"}};
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_FALSE(core.loadModule("alpha"));
}

// Without capability_module in-process there is no binding: the C API serves.
TEST_F(HostCoreTest, WithoutTheBindingTheCApiStillServes)
{
    stub.bindingAvailable = false;
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_FALSE(core.shellBound());
    EXPECT_TRUE(core.loadModule("alpha"));
    EXPECT_EQ(stub.lastLoadDeps, static_cast<int>(LOGOS_LOAD_REQUIRED_DEPS));
    EXPECT_FALSE(core.admitConsumer("my_ui").has_value());
    EXPECT_FALSE(core.shellCredential().has_value());
    EXPECT_TRUE(stub.coreServiceCalls.empty());
}

TEST_F(HostCoreTest, ConsumersAreAdmittedThroughCoreService)
{
    stub.answers = {
        {"admitConsumer", R"({"status":"ok","name":"my_ui","credential":"cred-1"})"},
        {"retireConsumer", R"({"status":"ok","name":"my_ui"})"},
    };
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_EQ(core.shellCredential().value_or(""), "shell-cr");
    EXPECT_EQ(core.admitConsumer("my_ui").value_or(""), "cred-1");
    EXPECT_TRUE(core.retireConsumer("my_ui"));
    ASSERT_EQ(stub.coreServiceCalls.size(), 2u);
    EXPECT_EQ(stub.coreServiceCalls[0].second, R"(["my_ui","presentation"])");
}

TEST_F(HostCoreTest, TheBindingIsReleasedBeforeCleanup)
{
    {
        LogosCore core(0, nullptr, shellConfig());
        core.start();
    }
    EXPECT_EQ(stub.bindingReleases, 1);
    const auto release = std::find(stub.callOrder.begin(), stub.callOrder.end(), "release_binding");
    const auto cleanup = std::find(stub.callOrder.begin(), stub.callOrder.end(), "cleanup");
    ASSERT_NE(release, stub.callOrder.end());
    EXPECT_LT(release, cleanup);
}
