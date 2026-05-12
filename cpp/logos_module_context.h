#ifndef LOGOS_MODULE_CONTEXT_H
#define LOGOS_MODULE_CONTEXT_H

#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// LogosModuleContext — opt-in mixin for codegen-generated modules
//
// The Logos runtime stamps three properties onto every module's LogosAPI
// instance before the first method is dispatched (see
// `logos-liblogos/src/runtimes/runtime_qt/host/module_initializer.cpp`):
//
//   - modulePath               — directory the module's plugin file lives in
//   - instanceId               — short ID the host assigns to this instance
//                                (the basename of the persistence directory)
//   - instancePersistencePath  — per-instance, host-owned data directory
//                                (e.g. `<basecamp>/module_data/<name>/<id>/`)
//
// Universal (codegen-generated, "qt_glue") modules used to have no path
// to these values short of being handed the full LogosAPI — too much
// surface for what's almost always a one-line lookup. This mixin
// confines the exposure to the three getters below; the codegen-emitted
// provider copies the values in via `_logosCoreSetContext_`, then fires
// `onContextReady()` so the impl can react in one well-defined place.
//
// Usage from a module impl:
//
//     class MyModuleImpl : public LogosModuleContext {
//     public:
//         // ... LOGOS_METHOD / Q_INVOKABLE methods as before ...
//     protected:
//         void onContextReady() override {
//             // instancePersistencePath() / instanceId() / modulePath()
//             // are now populated; do one-time per-instance setup here.
//         }
//     };
//
// Impls that don't inherit from LogosModuleContext are unaffected — the
// codegen's `if constexpr (std::is_base_of_v<...>)` branch is compiled
// away, so there's no per-module cost to making the override always
// emitted.
// ---------------------------------------------------------------------------

class LogosModuleContext {
public:
    virtual ~LogosModuleContext() = default;

    // Directory the module's plugin file lives in. Useful for loading
    // resources bundled next to the plugin (icons, qml/, schema files…).
    const std::string& modulePath() const { return m_modulePath; }

    // Short ID the host assigns to this instance. Stable across restarts
    // for the same on-disk persistence directory; multiple side-by-side
    // instances of the same module get distinct IDs.
    const std::string& instanceId() const { return m_instanceId; }

    // Per-instance writable data directory the host owns the lifecycle
    // of. Wiped when the module is uninstalled; survives upgrades. The
    // canonical place for module-private state (config files, caches,
    // small databases). Empty when the module is loaded outside a
    // host that provisions persistence (e.g. unit tests using the
    // impl directly), so always null-check before using.
    const std::string& instancePersistencePath() const { return m_instancePersistencePath; }

    // Typed access to this module's per-build `LogosModules` aggregate,
    // which the codegen emits in `generated_code/logos_sdk.h`. It owns
    // one strongly-typed client wrapper per entry in `metadata.json`'s
    // `dependencies` list (plus an always-present `core_manager`), so
    // an impl can call other modules' methods without ever touching
    // the raw `LogosAPI`:
    //
    //     #include "logos_sdk.h"           // generated at build time
    //
    //     void MyModuleImpl::doWork() {
    //         logos<LogosModules>().some_dep.someMethod(arg);
    //     }
    //
    // The pointer is set by the codegen-generated provider's `onInit`,
    // which constructs the `LogosModules` from the `LogosAPI`. The
    // type parameter is required because the SDK header is shared by
    // every module and can't know each module's specific aggregator
    // type (its accessor names come straight from that module's
    // declared dependencies). Returns a reference; calling before the
    // framework has populated the pointer (e.g. from a unit test
    // bypassing the provider) is undefined.
    template<typename Modules>
    Modules& logos() const {
        return *static_cast<Modules*>(m_logosModulesPtr);
    }

    // Framework-only entry point — invoked by the generated provider's
    // `onInit` once the LogosAPI properties are readable. The
    // leading-underscore-trailing-underscore name signals "do not call
    // from user code"; a friend declaration would be cleaner but would
    // require the generator to spell out a specific provider class
    // name per module, which we deliberately avoid coupling here.
    void _logosCoreSetContext_(std::string modulePath,
                               std::string instanceId,
                               std::string instancePersistencePath) {
        m_modulePath = std::move(modulePath);
        m_instanceId = std::move(instanceId);
        m_instancePersistencePath = std::move(instancePersistencePath);
        onContextReady();
    }

    // Framework-only — sets the typed `LogosModules` pointer that
    // `logos<T>()` dereferences. Untyped (void*) at this layer because
    // the SDK header is shared by every module; the codegen-generated
    // provider does the static_cast once it has the concrete type.
    void _logosCoreSetLogosModulesPtr_(void* ptr) {
        m_logosModulesPtr = ptr;
    }

protected:
    // Hook for derived impls. Fires exactly once, after the three
    // getters above become readable, before any method dispatch. The
    // default is a no-op; override to wire one-time setup that depends
    // on the persistence path. Do NOT do work in the constructor that
    // needs these values — the constructor runs before the framework
    // hands the context over.
    virtual void onContextReady() {}

private:
    std::string m_modulePath;
    std::string m_instanceId;
    std::string m_instancePersistencePath;
    // Type-erased so the SDK header doesn't need the per-module
    // LogosModules definition. Reinterpreted via the typed `logos<T>()`
    // accessor above. Stays null when the impl is constructed outside
    // a framework-provisioned context (e.g. lgpd CLI / unit tests),
    // matching the empty-string fallback for the other getters.
    void* m_logosModulesPtr = nullptr;
};

// ---------------------------------------------------------------------------
// _logos_codegen_::maybeSetContext — codegen helper, do not call directly.
//
// The generated <Module>ProviderObject::onInit always wants to "set the
// context if the impl inherits from LogosModuleContext, otherwise do
// nothing." Doing this with `if constexpr` inside the override fails to
// compile for non-inheriting impls, because the discarded `static_cast`
// branch is still syntactically parsed and type-checked. Tag-dispatching
// through two function templates side-steps that — overload resolution
// instantiates exactly one of the two, and the unused overload is
// never analysed against the impl type. Backward compatible: existing
// universal modules that don't inherit pay zero runtime cost (the
// no-op overload inlines away) and zero compile cost beyond the
// template instantiation.
// ---------------------------------------------------------------------------
namespace _logos_codegen_ {

template<class T>
inline auto maybeSetContext(T& impl,
                            std::string modulePath,
                            std::string instanceId,
                            std::string instancePersistencePath)
    -> std::enable_if_t<std::is_base_of_v<LogosModuleContext, T>>
{
    static_cast<LogosModuleContext&>(impl)._logosCoreSetContext_(
        std::move(modulePath),
        std::move(instanceId),
        std::move(instancePersistencePath));
}

template<class T>
inline auto maybeSetContext(T&,
                            std::string,
                            std::string,
                            std::string)
    -> std::enable_if_t<!std::is_base_of_v<LogosModuleContext, T>>
{
    // Module impl didn't opt into LogosModuleContext; nothing to do.
}

// Sets the (untyped) `LogosModules` pointer the generated provider
// constructs in onInit. Same tag-dispatch trick as maybeSetContext —
// the static_cast must be invisible to non-inheriting impls or their
// compile would break.
template<class T>
inline auto maybeSetLogosModules(T& impl, void* ptr)
    -> std::enable_if_t<std::is_base_of_v<LogosModuleContext, T>>
{
    static_cast<LogosModuleContext&>(impl)._logosCoreSetLogosModulesPtr_(ptr);
}

template<class T>
inline auto maybeSetLogosModules(T&, void*)
    -> std::enable_if_t<!std::is_base_of_v<LogosModuleContext, T>>
{
    // Module impl didn't opt into LogosModuleContext; nothing to do.
}

} // namespace _logos_codegen_

#endif // LOGOS_MODULE_CONTEXT_H
