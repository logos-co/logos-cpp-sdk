// Tests for logos_host_core.h — the host-side veneer over liblogos' C API and
// its runtime control surface, core_service.
//
// The C API and the shell binding are `extern "C"`, so this translation unit
// DEFINES them itself: a stub records what the veneer asked for, in order, and
// answers each core_service method with a canned reply. That is what makes the
// ordering contract ("every setting strictly before start") and the shape of
// each core_service call assertable without a running core.
//
// The one string the C API still returns (logos_core_process_module) is
// allocated EXACTLY as liblogos does (`new char[]`), so a veneer that switched to
// free()/delete would fail under ASan rather than corrupt the heap in production.

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
    bool processReturnsNull = false;

    // The protected setters and the shell binding.
    std::vector<std::string> bundledDirs;
    std::string placement;
    std::string shellName;
    std::string packageConfig;
    std::string peeringConfig;
    int refuseSetters = 0;          // what the protected setters answer
    bool bindingAvailable = true;   // capability_module is the token authority
    int bindingReleases = 0;
    // core_service over the binding: each call, and a canned answer per method.
    std::vector<std::pair<std::string, std::string>> coreServiceCalls; // (method, args)
    std::map<std::string, std::string> answers;

    // The runtime in a process of its own.
    std::vector<std::string> spawnedWith;   // each configuration spawned
    std::string spawnError;                 // non-empty: the spawn fails so
    int stops = 0;
    logos_runtime_exit_cb onExit = nullptr;
    void* onExitData = nullptr;
    std::vector<std::string> processedThere;
};

CoreStub* g = nullptr;

char* dupC(const std::string& s)
{
    char* r = new char[s.size() + 1];   // matches liblogos (logos_core.cpp)
    std::memcpy(r, s.c_str(), s.size() + 1);
    return r;
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
char* logos_core_process_module(const char*)      { return g->processReturnsNull ? nullptr : dupC("processed"); }

int logos_core_set_bundled_modules_dirs(const char* const* dirs)
{
    for (const char* const* d = dirs; *d; ++d) g->bundledDirs.emplace_back(*d);
    g->callOrder.push_back("bundled_dirs");
    return g->refuseSetters;
}
int logos_core_set_placement_policy(const char* p) { g->placement = p; g->callOrder.push_back("placement"); return g->refuseSetters; }
int logos_core_set_shell_identity(const char* n)   { g->shellName = n; g->callOrder.push_back("shell"); return g->refuseSetters; }
int logos_core_set_package_config(const char* c)   { g->packageConfig = c; g->callOrder.push_back("package_config"); return g->refuseSetters; }
int logos_core_set_peering_config(const char* c)   { g->peeringConfig = c; g->callOrder.push_back("peering_config"); return g->refuseSetters; }

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
// Spelled exactly as liblogos' logos_core.h declares it, so a drift in the
// header's mirror is a conflicting-declaration error here.
int logos_consumer_call_async(logos_consumer* consumer, const char* target, const char* method,
                              const char* args_json, int, logos_consumer_result_cb cb,
                              void* user_data)
{
    if (consumer != reinterpret_cast<logos_consumer*>(&gBindingTag) || !cb) return -1;
    g->coreServiceCalls.emplace_back(std::string(target) + "." + method, args_json);
    cb(1, "\"async-answer\"", user_data);
    return 0;
}
logos_consumer_subscription* logos_consumer_subscribe(logos_consumer*, const char*, const char*,
                                                      logos_consumer_event_cb, void*) { return nullptr; }
void logos_consumer_unsubscribe(logos_consumer_subscription*) {}
void logos_consumer_string_free(char* value) { std::free(value); }
void logos_consumer_release(logos_consumer*) { ++g->bindingReleases; g->callOrder.push_back("release_binding"); }

char* mallocC(const std::string& s)
{
    char* r = static_cast<char*>(std::malloc(s.size() + 1));
    std::memcpy(r, s.c_str(), s.size() + 1);
    return r;
}

char gRuntimeTag;
logos_runtime* logos_runtime_spawn(const char* config, char** error)
{
    g->callOrder.push_back("spawn");
    g->spawnedWith.emplace_back(config ? config : "");
    if (!g->spawnError.empty()) {
        *error = mallocC(g->spawnError);
        return nullptr;
    }
    return reinterpret_cast<logos_runtime*>(&gRuntimeTag);
}
logos_consumer* logos_runtime_binding(logos_runtime*)
{
    return reinterpret_cast<logos_consumer*>(&gBindingTag);
}
char* logos_runtime_process_module(logos_runtime*, const char* path)
{
    g->processedThere.emplace_back(path);
    return mallocC("processed-there");
}
void logos_runtime_on_exit(logos_runtime*, logos_runtime_exit_cb cb, void* data)
{
    g->onExit = cb;
    g->onExitData = data;
}
void logos_runtime_stop(logos_runtime*) { ++g->stops; g->callOrder.push_back("stop"); }
}

namespace {

using logos::host::LogosCore;

// The least a host can pass: its shell name. Most cases run the runtime here.
LogosCore::Config minimalConfig()
{
    LogosCore::Config cfg;
    cfg.shellName = "test_shell";
    cfg.separateProcess = false;
    return cfg;
}

LogosCore::Config shellConfig()
{
    LogosCore::Config cfg;
    cfg.bundledModulesDirs = {"/app/modules", "/app/modules-pkg"};
    cfg.placementPolicyJson = std::string(R"({"default":"subprocess"})");
    cfg.packageConfigJson = std::string(R"({"user_modules_dir":"/u/modules"})");
    cfg.peeringConfigJson = std::string(R"({"name":"desk"})");
    cfg.shellName = "basecamp";
    cfg.separateProcess = false;
    return cfg;
}

// The same, with the runtime in a process of its own.
LogosCore::Config separateConfig()
{
    LogosCore::Config cfg = shellConfig();
    cfg.separateProcess = true;
    return cfg;
}

// ── lifecycle ────────────────────────────────────────────────────────────────

TEST_F(HostCoreTest, ConstructionInitialisesAndDestructionCleansUpExactlyOnce)
{
    {
        LogosCore core(0, nullptr, minimalConfig());
        EXPECT_EQ(stub.initCalls, 1);
        EXPECT_EQ(stub.cleanupCalls, 0);
    }
    EXPECT_EQ(stub.cleanupCalls, 1);
}

TEST_F(HostCoreTest, AShellNameIsRequired)
{
    EXPECT_THROW(LogosCore(0, nullptr, LogosCore::Config{}), std::invalid_argument);
    EXPECT_EQ(stub.initCalls, 0) << "refused before anything was initialised";
    EXPECT_EQ(stub.cleanupCalls, 0);
}

TEST_F(HostCoreTest, EveryPreStartSettingIsAppliedBeforeStart)
{
    LogosCore::Config cfg = minimalConfig();
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

    // The ordering contract, asserted rather than trusted: every configuration
    // call precedes start, and start is followed only by taking the binding.
    ASSERT_GE(stub.callOrder.size(), 2u);
    EXPECT_EQ(stub.callOrder.front(), "init");
    EXPECT_EQ(stub.callOrder[stub.callOrder.size() - 2], "start")
        << "something was configured after start()";
    EXPECT_EQ(stub.callOrder.back(), "take_binding");
}

TEST_F(HostCoreTest, AbsentOptionalSettingsAreNotPushedAtAll)
{
    // nullopt policy must install NO policy — distinct from an empty one,
    // because liblogos treats "no policy" as unrestricted.
    { LogosCore core(0, nullptr, minimalConfig()); }
    EXPECT_FALSE(stub.accessPolicySet);
    EXPECT_TRUE(stub.modulesDirs.empty());
    EXPECT_TRUE(stub.persistenceBasePath.empty());
    EXPECT_EQ(stub.callOrder, (std::vector<std::string>{"init", "shell", "cleanup"}));
}

TEST_F(HostCoreTest, EmptyAccessPolicyStringIsStillInstalled)
{
    LogosCore::Config cfg = minimalConfig();
    cfg.accessPolicyJson = std::string("");
    LogosCore core(0, nullptr, std::move(cfg));
    EXPECT_TRUE(stub.accessPolicySet) << "an explicitly empty policy is a choice, not an absence";
}

TEST_F(HostCoreTest, ProtectedInputIsAppliedBeforeStart)
{
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_EQ(stub.bundledDirs, (std::vector<std::string>{"/app/modules", "/app/modules-pkg"}));
    EXPECT_EQ(stub.placement, R"({"default":"subprocess"})");
    EXPECT_EQ(stub.shellName, "basecamp");
    EXPECT_EQ(stub.packageConfig, R"({"user_modules_dir":"/u/modules"})");
    EXPECT_EQ(stub.peeringConfig, R"({"name":"desk"})");
    EXPECT_EQ(stub.callOrder, (std::vector<std::string>{
        "init", "bundled_dirs", "placement", "package_config", "peering_config", "shell",
        "start", "take_binding"}));
}

TEST_F(HostCoreTest, ARefusedSettingThrowsAfterCleaningUp)
{
    stub.refuseSetters = -1;
    EXPECT_THROW(LogosCore(0, nullptr, shellConfig()), std::invalid_argument);
    EXPECT_EQ(stub.cleanupCalls, 1) << "an initialised core must not be left behind";
}

// Without capability_module in-process there is no binding and nothing loads:
// start() says so instead of handing back a core that silently does nothing.
TEST_F(HostCoreTest, WithoutABindingStartThrows)
{
    stub.bindingAvailable = false;
    {
        LogosCore core(0, nullptr, shellConfig());
        EXPECT_THROW(core.start(), std::runtime_error);
        EXPECT_FALSE(core.shellBound());
        EXPECT_FALSE(core.loadModule("alpha"));
        EXPECT_FALSE(core.shellCredential().has_value());
        EXPECT_TRUE(stub.coreServiceCalls.empty());
    }
    EXPECT_EQ(stub.cleanupCalls, 1);
}

TEST_F(HostCoreTest, BeforeStartNothingReachesCoreService)
{
    stub.answers = {{"loadModule", R"({"status":"ok"})"}, {"listModules", "[]"}};
    LogosCore core(0, nullptr, minimalConfig());
    EXPECT_FALSE(core.loadModule("alpha"));
    EXPECT_TRUE(core.knownModules().empty());
    EXPECT_FALSE(core.admitConsumer("my_ui").has_value());
    EXPECT_TRUE(stub.coreServiceCalls.empty());
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

// ── lifecycle through core_service ──────────────────────────────────────────

TEST_F(HostCoreTest, LifecycleGoesThroughCoreService)
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
    EXPECT_TRUE(core.loadModule("alpha", LOGOS_LOAD_REQUIRED_AND_OPTIONAL));
    EXPECT_TRUE(core.unloadModule("alpha"));
    EXPECT_TRUE(core.unloadModule("alpha", /*withDependents=*/true));
    core.refreshModules();
    EXPECT_EQ(core.loadedModules(), (std::vector<std::string>{"alpha", "beta"}));
    const auto stats = core.stats("alpha");
    ASSERT_TRUE(stats.has_value());
    EXPECT_DOUBLE_EQ(stats->cpuPercent, 1.5);

    // The load default stays the required tree, which is what every host asking
    // loadModule(name) has always got; a cascading unload stays opt-in.
    EXPECT_EQ(stub.coreServiceCalls, (std::vector<std::pair<std::string, std::string>>{
        {"loadModule", R"(["alpha","required"])"},
        {"loadModule", R"(["alpha","module_only"])"},
        {"loadModule", R"(["alpha","required_and_optional"])"},
        {"unloadModule", R"(["alpha",false])"},
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

TEST_F(HostCoreTest, AFailedCallIsAFailedLoad)
{
    // No canned answer: the binding call itself fails.
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_FALSE(core.loadModule("alpha"));
    EXPECT_FALSE(core.unloadModule("alpha"));
}

// ── queries through core_service ────────────────────────────────────────────

TEST_F(HostCoreTest, KnownAndLoadedAreTheListedNames)
{
    stub.answers = {{"listModules", R"([{"name":"alpha"},{"name":"beta"},{"nope":1}])"}};
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_EQ(core.knownModules(), (std::vector<std::string>{"alpha", "beta"}));
    EXPECT_EQ(stub.coreServiceCalls.back().second, R"(["all"])");
}

TEST_F(HostCoreTest, TheGraphQueriesReachCoreService)
{
    stub.answers = {
        {"getModuleDependencies", R"(["d1","d2"])"},
        {"getModuleDependents", "[]"},
        {"getModuleOptionalDependencies", R"(["opt1","opt2"])"},
    };
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_EQ(core.dependencies("alpha", /*recursive=*/true),
              (std::vector<std::string>{"d1", "d2"}));
    EXPECT_TRUE(core.dependents("alpha").empty());
    EXPECT_EQ(core.optionalDependencies("alpha"),
              (std::vector<std::string>{"opt1", "opt2"}));
    EXPECT_EQ(stub.coreServiceCalls, (std::vector<std::pair<std::string, std::string>>{
        {"getModuleDependencies", R"(["alpha",true])"},
        {"getModuleDependents", R"(["alpha",false])"},
        {"getModuleOptionalDependencies", R"(["alpha"])"},
    }));
}

TEST_F(HostCoreTest, ModulesInfoAndTheOptionalLoadReportArePassedThrough)
{
    stub.answers = {
        {"getModulesInfo", R"([{"name":"alpha","loaded":true}])"},
        {"getOptionalLoadReport",
         R"([{"module":"extra","named_by":"alpha","reason":"not_installed"}])"},
    };
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    const auto info = core.modulesInfoJson();
    ASSERT_TRUE(info.has_value());
    EXPECT_NE(info->find("\"name\":\"alpha\""), std::string::npos) << *info;
    const auto report = core.optionalLoadReportJson("alpha");
    ASSERT_TRUE(report.has_value());
    EXPECT_NE(report->find("\"module\":\"extra\""), std::string::npos) << *report;
}

TEST_F(HostCoreTest, AnUnansweredQueryIsNulloptNotAnEmptyString)
{
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    EXPECT_FALSE(core.modulesInfoJson().has_value());
    EXPECT_FALSE(core.optionalLoadReportJson("alpha").has_value());
    EXPECT_TRUE(core.dependencies("alpha").empty());
}

TEST_F(HostCoreTest, ProcessModuleStaysOnTheCApi)
{
    LogosCore core(0, nullptr, minimalConfig());
    EXPECT_EQ(core.processModule("/x.dylib").value_or(""), "processed");
    stub.processReturnsNull = true;
    EXPECT_FALSE(core.processModule("/x.dylib").has_value())
        << "a NULL return means absent, and must not be flattened to \"\"";
}

// ── stats: the one-array parse ──────────────────────────────────────────────

TEST_F(HostCoreTest, StatsAreIndexedOutOfTheSingleArray)
{
    // The REAL contract process-stats emits (src/process_stats.cpp:157-161).
    stub.answers = {{"getModuleStats",
        R"([{"name":"alpha","cpu_percent":12.5,"cpu_time_seconds":3.5,"memory_mb":4096.0}])"}};
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    const auto s = core.stats("alpha");
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->name, "alpha");
    EXPECT_DOUBLE_EQ(s->cpuPercent, 12.5);
    EXPECT_DOUBLE_EQ(s->memoryMb, 4096.0);
    EXPECT_DOUBLE_EQ(s->cpuTimeSeconds, 3.5);
    EXPECT_EQ(s->raw["name"], "alpha") << "the raw entry stays reachable";
    EXPECT_FALSE(core.stats("not-loaded").has_value());
}

TEST_F(HostCoreTest, MalformedOrMissingStatsYieldEmptyRatherThanThrowing)
{
    // A host polls this on a timer; a bad answer must not take the process down.
    LogosCore core(0, nullptr, shellConfig());
    core.start();
    stub.answers = {{"getModuleStats", "{not json"}};
    EXPECT_TRUE(core.allStats().empty());
    stub.answers = {{"getModuleStats", R"({"name":"alpha"})"}};   // object, not array
    EXPECT_TRUE(core.allStats().empty());
    stub.answers.clear();                                          // the call fails
    EXPECT_TRUE(core.allStats().empty());
}

// ── consumers ───────────────────────────────────────────────────────────────

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

// ── the runtime in a process of its own ─────────────────────────────────────

TEST_F(HostCoreTest, TheRuntimeRunsInAProcessOfItsOwnByDefault)
{
    LogosCore::Config cfg;
    EXPECT_TRUE(cfg.separateProcess);
}

TEST_F(HostCoreTest, StartSpawnsTheRuntimeWithEverySetting)
{
    LogosCore::Config cfg = separateConfig();
    cfg.modulesDirs = {"/one"};
    cfg.persistenceBasePath = "/persist";
    cfg.accessPolicyJson = std::string(R"({"mode":"enforce"})");
    cfg.moduleTransports = {{"mod_a", "[]"}};
    stub.answers = {{"loadModule", R"({"status":"ok"})"}};
    {
        LogosCore core(0, nullptr, std::move(cfg));
        EXPECT_TRUE(core.separateProcess());
        EXPECT_TRUE(stub.callOrder.empty()) << "nothing is configured in this process";
        core.start();
        ASSERT_EQ(stub.spawnedWith.size(), 1u);
        EXPECT_EQ(nlohmann::json::parse(stub.spawnedWith.front()), (nlohmann::json{
            {"shell", "basecamp"},
            {"modules_dirs", {"/one"}},
            {"bundled_modules_dirs", {"/app/modules", "/app/modules-pkg"}},
            {"persistence_base_path", "/persist"},
            {"module_transports", {{"mod_a", "[]"}}},
            {"access_policy", R"({"mode":"enforce"})"},
            {"placement_policy", R"({"default":"subprocess"})"},
            {"package_config", R"({"user_modules_dir":"/u/modules"})"},
            {"peering_config", R"({"name":"desk"})"},
        }));
        EXPECT_TRUE(core.shellBound());
        EXPECT_TRUE(core.loadModule("alpha")) << "the same core_service calls, over its binding";
        EXPECT_EQ(core.shellCredential().value_or(""), "shell-cr");
    }
    EXPECT_EQ(stub.stops, 1);
    EXPECT_EQ(stub.cleanupCalls, 0) << "nothing ran here to clean up";
    EXPECT_EQ(stub.bindingReleases, 0) << "the runtime's handle owns its binding";
    EXPECT_EQ(stub.startCalls, 0);
    EXPECT_EQ(stub.initCalls, 0);
}

TEST_F(HostCoreTest, AFailedSpawnThrowsWithItsReason)
{
    stub.spawnError = "there is no token authority";
    {
        LogosCore core(0, nullptr, separateConfig());
        try {
            core.start();
            ADD_FAILURE() << "start() did not throw";
        } catch (const std::runtime_error& e) {
            EXPECT_NE(std::string(e.what()).find("no token authority"), std::string::npos) << e.what();
        }
        EXPECT_FALSE(core.shellBound());
        EXPECT_FALSE(core.loadModule("alpha"));
    }
    EXPECT_EQ(stub.stops, 0);
    EXPECT_EQ(stub.cleanupCalls, 0);
}

TEST_F(HostCoreTest, AModuleFileIsProcessedByTheRuntime)
{
    LogosCore core(0, nullptr, separateConfig());
    EXPECT_FALSE(core.processModule("/x.dylib").has_value()) << "nothing runs before start()";
    core.start();
    EXPECT_EQ(core.processModule("/x.dylib").value_or(""), "processed-there");
    EXPECT_EQ(stub.processedThere, (std::vector<std::string>{"/x.dylib"}));
}

TEST_F(HostCoreTest, TheRuntimesExitReachesTheHost)
{
    std::vector<std::string> reasons;
    LogosCore core(0, nullptr, separateConfig());
    core.onRuntimeExit([&](const std::string& reason) { reasons.push_back(reason); });
    core.start();
    ASSERT_NE(stub.onExit, nullptr);
    stub.onExit("the runtime died on signal 9", stub.onExitData);
    EXPECT_EQ(reasons, (std::vector<std::string>{"the runtime died on signal 9"}));
}

// ── clients acting as the shell ─────────────────────────────────────────────

// The shape of a generated lp client: constructed from its origin, and neither
// copyable nor movable (it owns an lp_client), so client<T>() must build it in
// place.
struct ProbeClient {
    explicit ProbeClient(const std::string& o) : origin(o) {}
    ProbeClient(const ProbeClient&) = delete;
    ProbeClient& operator=(const ProbeClient&) = delete;
    std::string origin;
};

TEST_F(HostCoreTest, AClientCallsAsTheShell)
{
    LogosCore core(0, nullptr, shellConfig());
    EXPECT_EQ(core.shellName(), "basecamp");
    const ProbeClient probe = core.client<ProbeClient>();
    EXPECT_EQ(probe.origin, "basecamp");
}

TEST_F(HostCoreTest, TheShellNameIsKeptWhenTheRuntimeRunsApart)
{
    LogosCore core(0, nullptr, separateConfig());
    core.start();
    EXPECT_EQ(core.shellName(), "basecamp");
    EXPECT_EQ(core.client<ProbeClient>().origin, "basecamp");
}

TEST_F(HostCoreTest, TheBindingAlsoCallsAsynchronously)
{
    LogosCore core(0, nullptr, separateConfig());
    core.start();
    std::string answer;
    const int status = logos_consumer_call_async(
        core.shellBinding(), "core_service", "listModules", "[\"all\"]", 1000,
        [](int ok, const char* json, void* ud) {
            if (ok && json) *static_cast<std::string*>(ud) = json;
        },
        &answer);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(answer, "\"async-answer\"");
    ASSERT_EQ(stub.coreServiceCalls.size(), 1u);
    EXPECT_EQ(stub.coreServiceCalls[0].first, "core_service.listModules");
}

} // namespace
