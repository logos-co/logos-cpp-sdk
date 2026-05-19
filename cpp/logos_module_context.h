#ifndef LOGOS_MODULE_CONTEXT_H
#define LOGOS_MODULE_CONTEXT_H

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// `logos_events:` — Qt-`signals:`-style declaration of events the module
// can emit. The macro expands to `public:` so the compiler treats the
// declarations as ordinary public-method signatures (never called from
// outside the impl, but harmless to leave public — mirrors Qt's
// `#define signals public`). The cpp-generator's impl_header_parser
// recognises the raw `logos_events:` token before preprocessing and
// emits typed method bodies for each declaration in a sidecar
// `<name>_events.cpp` that calls `emitEventImpl_()` underneath.
//
//     class MyImpl : public LogosModuleContext {
//     public:
//         void doStuff() { userLoggedIn("alice", 12345); }   // typed emit
//     logos_events:
//         void userLoggedIn(const std::string& userId, int64_t timestamp);
//     };
// ---------------------------------------------------------------------------
#ifndef logos_events
#define logos_events public
#endif

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

// Per-module aggregate of dependency wrappers. Each module's codegen
// emits `struct LogosModules { ... };` at global scope in its own
// `generated_code/logos_sdk.h` (one accessor per `metadata.json#
// dependencies` entry — nothing else; apps that need to manage the
// core itself reach for liblogos' C API instead). Forward-declared
// here so the SDK header stays decoupled from per-module codegen —
// the impl's translation unit makes the type complete via its own
// `#include "logos_sdk.h"`, at which point the inline
// `LogosModuleContext::modules()` body below compiles.
struct LogosModules;

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

    // True once the framework has populated the three getters above.
    // Flipped inside `_logosCoreSetContext_` *before* the
    // `onContextReady()` hook fires, so derived impls can use this as
    // a guard from helper methods that may run earlier in the impl's
    // life (e.g. during construction in tests that bypass the
    // framework). Stays false when the impl is constructed outside a
    // framework-provisioned context, matching the empty-string
    // fallback for the path getters.
    bool isContextReady() const { return m_contextReady; }

    // Typed access to this module's per-build `LogosModules` aggregate,
    // which the codegen emits in `generated_code/logos_sdk.h`. It owns
    // one strongly-typed client wrapper per entry in `metadata.json`'s
    // `dependencies` list — nothing else. An impl can call those
    // declared deps' methods without ever touching the raw `LogosAPI`:
    //
    //     #include "logos_sdk.h"           // generated at build time
    //
    //     void MyModuleImpl::doWork() {
    //         modules().some_dep.someMethod(arg);
    //     }
    //
    // The pointer is set by the codegen-generated provider's `onInit`,
    // which constructs the `LogosModules` from the `LogosAPI`. The
    // return type is forward-declared above so this header stays
    // decoupled from per-module codegen; call sites need to have
    // `logos_sdk.h` included (which defines `LogosModules` as a
    // concrete `struct` in their translation unit) for the inline
    // body below to compile. Calling before the framework has
    // populated the pointer (e.g. from a unit test bypassing the
    // provider) is undefined.
    LogosModules& modules() const {
        return *static_cast<LogosModules*>(m_logosModulesPtr);
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
        // Flip BEFORE invoking the hook so derived impls' onContextReady
        // overrides — and anything they call out to — see a "true"
        // isContextReady() as expected.
        m_contextReady = true;
        onContextReady();
    }

    // Framework-only — sets the typed `LogosModules` pointer that
    // `logos<T>()` dereferences. Untyped (void*) at this layer because
    // the SDK header is shared by every module; the codegen-generated
    // provider does the static_cast once it has the concrete type.
    void _logosCoreSetLogosModulesPtr_(void* ptr) {
        m_logosModulesPtr = ptr;
    }

    // Framework-only — installs the callback that the codegen-emitted
    // bodies of `logos_events:` methods invoke. The `void*` carries a
    // `QVariantList*` constructed inside the .cpp; keeping the
    // signature Qt-free here lets impl headers stay pure C++. The
    // codegen-emitted provider plugs in a lambda that casts the
    // pointer back to QVariantList and forwards through
    // `LogosProviderBase::emitEvent(QString, QVariantList)`.
    void _logosCoreSetEmitEvent_(std::function<void(const std::string&, void*)> cb) {
        m_emitEventCallback = std::move(cb);
    }

protected:
    // Invoked from `<name>_events.cpp` (codegen-emitted method bodies)
    // to dispatch a typed event. `args` is the address of a stack-
    // local `QVariantList` constructed by the generated body; the
    // callback the provider installs casts it back and forwards.
    // No-op when called outside a framework context (the callback
    // stays default-constructed and empty) — same fallback shape as
    // the property getters above.
    void emitEventImpl_(const std::string& eventName, void* args) const {
        if (m_emitEventCallback)
            m_emitEventCallback(eventName, args);
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
    // Tracks whether the framework has called `_logosCoreSetContext_`
    // at least once. Read by `isContextReady()`.
    bool m_contextReady = false;
    // Type-erased so the SDK header doesn't need the per-module
    // LogosModules definition. Reinterpreted via the typed `logos<T>()`
    // accessor above. Stays null when the impl is constructed outside
    // a framework-provisioned context (e.g. lgpd CLI / unit tests),
    // matching the empty-string fallback for the other getters.
    void* m_logosModulesPtr = nullptr;
    // Set by the codegen-generated provider in onInit() via the SFINAE'd
    // _logos_codegen_::maybeSetEmitEvent helper below. Default-empty
    // when the impl is constructed outside a framework-provisioned
    // context, in which case `emitEventImpl_` becomes a no-op.
    std::function<void(const std::string&, void*)> m_emitEventCallback;
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

// Sets the typed-event callback that codegen-emitted `<name>_events.cpp`
// bodies dispatch through. Same tag-dispatch trick as the two above —
// impls that don't inherit LogosModuleContext fall through to the no-op
// overload and compile unchanged.
template<class T>
inline auto maybeSetEmitEvent(T& impl, std::function<void(const std::string&, void*)> cb)
    -> std::enable_if_t<std::is_base_of_v<LogosModuleContext, T>>
{
    static_cast<LogosModuleContext&>(impl)._logosCoreSetEmitEvent_(std::move(cb));
}

template<class T>
inline auto maybeSetEmitEvent(T&, std::function<void(const std::string&, void*)>)
    -> std::enable_if_t<!std::is_base_of_v<LogosModuleContext, T>>
{
    // Module impl didn't opt into LogosModuleContext; nothing to do.
}

} // namespace _logos_codegen_

#endif // LOGOS_MODULE_CONTEXT_H
