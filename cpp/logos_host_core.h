#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// logos_host_core.h — the C++ veneer over liblogos' core-management C API and
// its runtime control surface, core_service.
//
// This is for HOST programs: the ones that stand up a Logos core and then load
// modules into it (logos-basecamp, logos-logoscore-cli, logos-standalone-app,
// logos-module-viewer). A module never needs it — a module is loaded BY a host
// and reaches its declared dependencies through the generated `LogosModules`
// aggregate instead.
//
// ── Why a wrapper at all ────────────────────────────────────────────────────
// Four hosts used to open-code the same `logos_core_*` calls, and the C API
// has three classes of hazard that a call site cannot see:
//
//   1. OWNERSHIP. `logos_core_process_module` returns a `char*` liblogos
//      allocates with `new char[]`, so `delete[]` is correct and `free()` is
//      undefined behaviour; the binding's strings go back through
//      `logos_consumer_string_free`. Nothing in the signatures says so.
//   2. ORDERING. The setters must precede `logos_core_start()`, and
//      `set_module_transports` must precede the target module's LOAD. Those
//      constraints exist only as comments in logos_core.h.
//   3. SHAPE. core_service's `getModuleStats` takes NO module name — it returns
//      one JSON array covering every loaded module. Every caller that wants one
//      module's stats has to parse and index it.
//
// This header turns (1) into RAII, (2) into constructor arguments (the illegal
// order stops being representable), and (3) into one parse.
//
// ── Why it is a plain object, with no codegen and no injection seam ─────────
// Contrast LogosModuleContext, which needs `_logosCoreSetContext_`, a `void*`
// round-trip in `modules()`, and SFINAE `maybeSet*` helpers. All of that exists
// because a module impl is USER-AUTHORED but FRAMEWORK-INSTANTIATED: the
// generated provider must inject state into an object it did not construct,
// belonging to a class that may or may not inherit the base.
//
// A host has none of those constraints. The host IS main(). It constructs this
// object itself, nothing injects into it, and no generator is involved because
// the host is not generated. So this is an ordinary RAII class.
//
// For the same reason there is deliberately NO `modules()` here. `LogosModules`
// is emitted per-build from the host's own metadata.json#dependencies; the host
// includes its own `logos_sdk.h` and can simply hold one. Only the SDK-side
// module context needs the `void*` indirection, because only it must name a
// type it cannot see.
//
// ── Why the C API is re-declared below rather than included ─────────────────
// logos-liblogos DEPENDS ON logos-cpp-sdk (logos-liblogos/flake.nix:7), so this
// header cannot include liblogos' `logos_core.h` without inverting the
// dependency graph. The declarations below are therefore a hand-maintained
// mirror — which is what every host does today anyway (see
// logos-basecamp/app/CoreModuleManager.cpp), except that it now exists ONCE
// instead of four times. The host links liblogos; this header only declares.
//
// ── The shell binding ───────────────────────────────────────────────────────
// The host is a named consumer, a shell: Config::shellName is required, start()
// takes the shell binding, and every lifecycle call and query goes through
// core_service (core_service.lidl) as that identity; admitConsumer() admits its
// UI plugins. The C API that is left is the configuration before start, start
// and cleanup, the binding itself, and processModule.
//
// Header-only, Qt-free, and it adds no link edge: `cpp/CMakeLists.txt` exports
// an INTERFACE library and this drops straight into it.
// ─────────────────────────────────────────────────────────────────────────────

#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

// ── liblogos' core-management C ABI ─────────────────────────────────────────
// Mirror of logos-liblogos/src/logos_core/logos_core.h. Kept minimal and in
// declaration order so a diff against that file is easy to eyeball.
extern "C" {
void   logos_core_init(int argc, char* argv[]);
void   logos_core_add_modules_dir(const char* modules_dir);
void   logos_core_start();
void   logos_core_cleanup();
// How far loadModule walks the graph; core_service names them module_only,
// required and required_and_optional.
typedef enum {
    LOGOS_LOAD_MODULE_ONLY = 0,
    LOGOS_LOAD_REQUIRED_DEPS = 1,
    LOGOS_LOAD_REQUIRED_AND_OPTIONAL = 2,
} LogosLoadDeps;
char*  logos_core_process_module(const char* module_path);
void   logos_core_set_persistence_base_path(const char* path);
void   logos_core_set_module_transports(const char* module_name,
                                        const char* transport_set_json);
void   logos_core_set_access_policy(const char* policy_json);
// Protected input, before logos_core_start(); each returns 0, or -1 when refused.
int    logos_core_set_bundled_modules_dirs(const char* const* dirs);
int    logos_core_set_placement_policy(const char* policy_json);
int    logos_core_set_shell_identity(const char* name);
int    logos_core_set_package_config(const char* config_json);
// The shell binding: the host's own identity, admitted by capability_module.
typedef struct logos_consumer logos_consumer;
typedef struct logos_consumer_subscription logos_consumer_subscription;
typedef void (*logos_consumer_event_cb)(const char* event_name, const char* data_json,
                                        void* user_data);
logos_consumer* logos_core_take_shell_binding(void);
const char* logos_consumer_name(const logos_consumer* consumer);
char*  logos_consumer_credential(const logos_consumer* consumer);
int    logos_consumer_call(logos_consumer* consumer, const char* target, const char* method,
                           const char* args_json, int timeout_ms, char** out_result_json,
                           char** out_error_json);
logos_consumer_subscription* logos_consumer_subscribe(logos_consumer* consumer,
                                                      const char* target,
                                                      const char* event_name,
                                                      logos_consumer_event_cb cb,
                                                      void* user_data);
void   logos_consumer_unsubscribe(logos_consumer_subscription* subscription);
void   logos_consumer_string_free(char* value);
void   logos_consumer_release(logos_consumer* consumer);
}

namespace logos {
namespace host {

// NOTE: there is deliberately no "called out of order" exception type here.
// Every pre-start setting is a constructor argument, so applying one after
// start() is not something a caller can express — the ordering constraint is
// enforced by the shape of the type rather than by a runtime check.

// One loaded module's resource usage, indexed out of the single array that
// core_service's getModuleStats returns for ALL modules.
struct ModuleStats {
    std::string name;
    double      cpuPercent = 0.0;
    double      cpuTimeSeconds = 0.0;
    // MEGABYTES, not bytes — that is what the producer emits
    // (process-stats/src/process_stats.h: `double memoryMB`). This member was
    // `long long memoryBytes` and read a key that does not exist, so it was
    // both the wrong unit and always zero.
    double      memoryMb = 0.0;
    // The raw entry, so a host can read fields this struct does not model
    // without waiting for the SDK to grow them.
    nlohmann::json raw;
};

namespace detail {

// liblogos allocates the strings it returns with `new char[]`, so `delete[]` is
// the correct deallocator and `free()` is undefined behaviour. Returns nullopt
// for a NULL return, which the C API uses to mean "no value / error" —
// distinct from an empty string.
inline std::optional<std::string> drainCString(char* s)
{
    if (!s) return std::nullopt;
    std::optional<std::string> out(std::string{s});
    delete[] s;
    return out;
}

// core_service deadlines: a load waits out its modules' bring-up.
constexpr int kLifecycleMs = 120000;
constexpr int kQueryMs = 15000;

// core_service's answer over the shell binding, or null when the call failed or
// there is no binding yet (before start()).
inline nlohmann::json callCoreService(logos_consumer* binding, const char* method,
                                      const nlohmann::json& args, int timeoutMs)
{
    if (!binding) return nlohmann::json();
    char* result = nullptr;
    char* error = nullptr;
    const int status = logos_consumer_call(binding, "core_service", method,
                                           args.dump().c_str(), timeoutMs, &result, &error);
    nlohmann::json value;
    if (status == 0 && result)
        value = nlohmann::json::parse(result, nullptr, /*allow_exceptions=*/false);
    logos_consumer_string_free(result);
    logos_consumer_string_free(error);
    return value.is_discarded() ? nlohmann::json() : value;
}

inline bool answeredOk(const nlohmann::json& answer)
{
    return answer.is_object() && answer.value("status", std::string{}) == "ok";
}

inline const char* depsName(LogosLoadDeps deps)
{
    switch (deps) {
    case LOGOS_LOAD_MODULE_ONLY: return "module_only";
    case LOGOS_LOAD_REQUIRED_DEPS: return "required";
    default: return "required_and_optional";
    }
}

// A JSON array of names, as the dependency queries answer; empty for anything else.
inline std::vector<std::string> names(const nlohmann::json& answer)
{
    std::vector<std::string> out;
    if (!answer.is_array()) return out;
    for (const nlohmann::json& entry : answer)
        if (entry.is_string()) out.push_back(entry.get<std::string>());
    return out;
}

// A JSON answer as text, or nullopt when there was none.
inline std::optional<std::string> text(const nlohmann::json& answer)
{
    if (answer.is_null()) return std::nullopt;
    return answer.dump();
}

// The names in core_service.listModules' answer.
inline std::vector<std::string> listedNames(const nlohmann::json& listed)
{
    std::vector<std::string> out;
    if (!listed.is_array()) return out;
    for (const nlohmann::json& entry : listed)
        if (entry.is_object() && entry.contains("name") && entry["name"].is_string())
            out.push_back(entry["name"].get<std::string>());
    return out;
}

// Key names are process-stats' (src/process_stats.cpp:157-161): name,
// cpu_percent, cpu_time_seconds, memory_mb. They were once read as "cpu" and
// "memory", which nothing emits, so every host reported 0 for both.
inline std::vector<ModuleStats> parseStats(const nlohmann::json& parsed)
{
    std::vector<ModuleStats> out;
    if (!parsed.is_array()) return out;
    for (const nlohmann::json& entry : parsed) {
        if (!entry.is_object()) continue;
        ModuleStats s;
        s.name = entry.value("name", std::string{});
        s.cpuPercent     = entry.value("cpu_percent", 0.0);
        s.cpuTimeSeconds = entry.value("cpu_time_seconds", 0.0);
        s.memoryMb       = entry.value("memory_mb", 0.0);
        s.raw = entry;
        out.push_back(std::move(s));
    }
    return out;
}

} // namespace detail

// ─────────────────────────────────────────────────────────────────────────────
// LogosCore — owns the process-wide Logos core.
//
// Construct exactly ONE, in main(), and keep it alive for the process. The
// underlying C API is process-global, so this type is neither copyable nor
// movable: two instances would mean two owners of one core, and the second
// destructor would call logos_core_cleanup() on an already-cleaned core.
//
//     logos::host::LogosCore::Config cfg;
//     cfg.shellName = "my_app";
//     cfg.bundledModulesDirs = { "/usr/lib/my_app/modules" };
//     cfg.modulesDirs = { "/usr/lib/logos/modules" };
//     cfg.persistenceBasePath = "/var/lib/logos";
//
//     logos::host::LogosCore core(argc, argv, std::move(cfg));
//     core.start();
//     core.loadModule("package_manager");
// ─────────────────────────────────────────────────────────────────────────────
class LogosCore {
public:
    // Everything the C API requires BEFORE logos_core_start(). Passing these
    // through the constructor is the point of the type: it makes the illegal
    // ordering unrepresentable rather than documented.
    struct Config {
        // Applied in order, via logos_core_add_modules_dir.
        std::vector<std::string> modulesDirs;

        // Empty ⇒ not set. Each module gets {path}/{module_name}/{instance_id}/.
        std::string persistenceBasePath;

        // nullopt ⇒ install no policy at all, which is NOT the same as an empty
        // policy: liblogos treats "no policy" as unrestricted and only enforces
        // when a policy with mode "enforce" is present.
        std::optional<std::string> accessPolicyJson;

        // module name → JSON array of LogosTransportConfig. Registered before
        // start(), which is what capability_module requires; user modules only
        // need it before their own load, but doing it here covers both.
        std::map<std::string, std::string> moduleTransports;

        // The directories this host ships its own modules in. A reserved name
        // (capability_module, modules_state, ...) then resolves only from them,
        // and capability_module, the token authority, must be among them.
        std::vector<std::string> bundledModulesDirs;

        // Where modules run, as liblogos' placement policy; nullopt keeps its default.
        std::optional<std::string> placementPolicyJson;

        // package_manager's directories and signature policy, which the runtime
        // applies as it loads (its setters answer only the runtime); see
        // logos_core_set_package_config.
        std::optional<std::string> packageConfigJson;

        // REQUIRED: the host's own identity ("basecamp", ...). start() takes the
        // shell binding, and every call below goes through core_service as it.
        std::string shellName;
    };

    LogosCore(int argc, char* argv[], Config config)
    {
        if (config.shellName.empty())
            throw std::invalid_argument("logos::host::LogosCore: a shellName is required");
        logos_core_init(argc, argv);
        // Ordered exactly as liblogos documents: dirs, then persistence, then
        // transports, then policy — all strictly before start().
        for (const std::string& dir : config.modulesDirs)
            logos_core_add_modules_dir(dir.c_str());
        if (!config.persistenceBasePath.empty())
            logos_core_set_persistence_base_path(config.persistenceBasePath.c_str());
        for (const auto& entry : config.moduleTransports)
            logos_core_set_module_transports(entry.first.c_str(), entry.second.c_str());
        if (config.accessPolicyJson.has_value())
            logos_core_set_access_policy(config.accessPolicyJson->c_str());
        if (!config.bundledModulesDirs.empty()) {
            std::vector<const char*> dirs;
            for (const std::string& dir : config.bundledModulesDirs) dirs.push_back(dir.c_str());
            dirs.push_back(nullptr);
            require(logos_core_set_bundled_modules_dirs(dirs.data()), "the bundled directories");
        }
        if (config.placementPolicyJson.has_value())
            require(logos_core_set_placement_policy(config.placementPolicyJson->c_str()),
                    "the placement policy");
        if (config.packageConfigJson.has_value())
            require(logos_core_set_package_config(config.packageConfigJson->c_str()),
                    "the package config");
        require(logos_core_set_shell_identity(config.shellName.c_str()), "the shell name");
    }

    ~LogosCore()
    {
        if (m_binding) logos_consumer_release(m_binding);
        logos_core_cleanup();
    }

    LogosCore(const LogosCore&) = delete;
    LogosCore& operator=(const LogosCore&) = delete;
    LogosCore(LogosCore&&) = delete;
    LogosCore& operator=(LogosCore&&) = delete;

    // Boots the core and spawns the modules liblogos starts itself (notably
    // capability_module), then takes the shell binding. After this, the
    // pre-start settings above can no longer be changed. Throws when there is
    // no binding: liblogos then has no token authority and loads nothing.
    void start()
    {
        logos_core_start();
        m_started = true;
        m_binding = logos_core_take_shell_binding();
        if (!m_binding)
            throw std::runtime_error("logos::host::LogosCore: no shell binding; liblogos has no "
                                     "token authority (is capability_module bundled?)");
    }

    bool isStarted() const { return m_started; }

    // ── The shell identity ──────────────────────────────────────────────────

    // Whether start() took the shell binding the calls below go through.
    bool shellBound() const { return m_binding != nullptr; }

    // The shell's credential, for a co-process or a Qt LogosAPI acting as it.
    std::optional<std::string> shellCredential() const
    {
        if (!m_binding) return std::nullopt;
        char* value = logos_consumer_credential(m_binding);
        if (!value) return std::nullopt;
        std::optional<std::string> out(std::string{value});
        logos_consumer_string_free(value);
        return out;
    }

    // For calls and subscriptions this class does not wrap; released here.
    logos_consumer* shellBinding() const { return m_binding; }

    // Admits a presentation consumer (a UI plugin) and returns the credential
    // capability_module minted for it; nullopt before start() or if refused.
    std::optional<std::string> admitConsumer(const std::string& name)
    {
        if (!m_binding) return std::nullopt;
        const nlohmann::json answer = detail::callCoreService(
            m_binding, "admitConsumer", nlohmann::json::array({name, "presentation"}),
            detail::kQueryMs);
        if (!detail::answeredOk(answer) || !answer.contains("credential")
            || !answer["credential"].is_string())
            return std::nullopt;
        return answer["credential"].get<std::string>();
    }

    // Ends a consumer admitted above, revoking its tokens.
    bool retireConsumer(const std::string& name)
    {
        return m_binding
            && detail::answeredOk(detail::callCoreService(
                m_binding, "retireConsumer", nlohmann::json::array({name}), detail::kQueryMs));
    }

    // ── Module lifecycle ────────────────────────────────────────────────────

    // Returns true on success. The default resolves and loads the module's
    // REQUIRED dependency graph first, which is what a host almost always
    // wants. LOGOS_LOAD_REQUIRED_AND_OPTIONAL additionally brings up whichever
    // optional dependencies are installed — none of which can fail this call,
    // so ask optionalLoadReport() below which ones were left out.
    bool loadModule(const std::string& name,
                    LogosLoadDeps deps = LOGOS_LOAD_REQUIRED_DEPS)
    {
        return detail::answeredOk(detail::callCoreService(
            m_binding, "loadModule", nlohmann::json::array({name, detail::depsName(deps)}),
            detail::kLifecycleMs));
    }

    // Which optional dependencies LOGOS_LOAD_REQUIRED_AND_OPTIONAL would leave
    // out for `name`, and why, as liblogos' JSON. "[]" when it would leave out
    // none. Worth asking after such a load: a skipped module keeps whatever
    // state it had, so nothing else distinguishes it from one nobody wanted.
    std::optional<std::string> optionalLoadReportJson(const std::string& name) const
    {
        return detail::text(detail::callCoreService(
            m_binding, "getOptionalLoadReport", nlohmann::json::array({name}), detail::kQueryMs));
    }

    // Returns true on success. `withDependents` cascades to modules that depend
    // on this one; without it, unloading a module something else is using
    // fails rather than breaking the dependent.
    bool unloadModule(const std::string& name, bool withDependents = false)
    {
        return detail::answeredOk(detail::callCoreService(
            m_binding, "unloadModule", nlohmann::json::array({name, withDependents}),
            detail::kLifecycleMs));
    }

    // Re-scans the modules directories for changes on disk.
    void refreshModules()
    {
        detail::callCoreService(m_binding, "refreshModules", nlohmann::json::array(),
                                detail::kLifecycleMs);
    }

    // Registers a module file with the core, returning whatever liblogos
    // reports about it (nullopt on error). The one call left on the C API: it
    // is the embedder's alone, and refuses names the runtime already knows.
    std::optional<std::string> processModule(const std::string& modulePath)
    {
        return detail::drainCString(logos_core_process_module(modulePath.c_str()));
    }

    // ── Introspection ───────────────────────────────────────────────────────

    std::vector<std::string> knownModules() const
    {
        return detail::listedNames(detail::callCoreService(
            m_binding, "listModules", nlohmann::json::array({"all"}), detail::kQueryMs));
    }

    std::vector<std::string> loadedModules() const
    {
        return detail::listedNames(detail::callCoreService(
            m_binding, "listModules", nlohmann::json::array({"loaded"}), detail::kQueryMs));
    }

    std::vector<std::string> dependencies(const std::string& name, bool recursive = false) const
    {
        return detail::names(detail::callCoreService(
            m_binding, "getModuleDependencies", nlohmann::json::array({name, recursive}),
            detail::kQueryMs));
    }

    std::vector<std::string> dependents(const std::string& name, bool recursive = false) const
    {
        return detail::names(detail::callCoreService(
            m_binding, "getModuleDependents", nlohmann::json::array({name, recursive}),
            detail::kQueryMs));
    }

    // The module's optional dependencies: concrete names it may call and does
    // not require. Direct only — there is no recursive form, because the set
    // a caller can act on is the one this module declares, and an optional
    // dependency's own optional dependencies are its business.
    //
    // Worth pairing with loadModule(name, LOGOS_LOAD_REQUIRED_AND_OPTIONAL):
    // this says what could come up, and optionalLoadReportJson says what will
    // not.
    std::vector<std::string> optionalDependencies(const std::string& name) const
    {
        return detail::names(detail::callCoreService(
            m_binding, "getModuleOptionalDependencies", nlohmann::json::array({name}),
            detail::kQueryMs));
    }

    // Full metadata for every known module, as liblogos' JSON.
    std::optional<std::string> modulesInfoJson() const
    {
        return detail::text(detail::callCoreService(
            m_binding, "getModulesInfo", nlohmann::json::array(), detail::kQueryMs));
    }

    // ── Stats ───────────────────────────────────────────────────────────────
    //
    // getModuleStats takes no module name: it returns ONE JSON array covering
    // every loaded module. Both accessors below share that single call, so
    // asking for one module's stats costs the same as asking for all of them —
    // do not loop over `stats(name)` for a whole list, call `allStats()` once.

    std::vector<ModuleStats> allStats() const
    {
        return detail::parseStats(detail::callCoreService(
            m_binding, "getModuleStats", nlohmann::json::array(), detail::kQueryMs));
    }

    // nullopt when the module is not loaded (and therefore has no entry).
    std::optional<ModuleStats> stats(const std::string& moduleName) const
    {
        std::vector<ModuleStats> all = allStats();
        for (ModuleStats& s : all) {
            if (s.name == moduleName) return std::move(s);
        }
        return std::nullopt;
    }

private:
    // A refused protected input is a configuration error: nothing has started.
    static void require(int status, const char* what)
    {
        if (status == 0) return;
        logos_core_cleanup();
        throw std::invalid_argument(std::string("logos::host::LogosCore: liblogos refused ") + what);
    }

    logos_consumer* m_binding = nullptr;
    bool m_started = false;
};

} // namespace host
} // namespace logos
