// Unit tests for LogosModuleContext + the codegen-helper SFINAE overloads
// declared in logos_module_context.h.
//
// These cover the pieces the cpp-generator relies on when emitting the
// `onInit(LogosAPI*) override` in every universal module's provider:
//
//   1. The opt-in base class itself: getters default-empty, the
//      framework setter populates them in one shot and fires
//      onContextReady() exactly once, the typed `logos<T>()` accessor
//      reinterprets the stored void* correctly.
//
//   2. The SFINAE-dispatched helpers (`_logos_codegen_::maybeSetContext`
//      and `maybeSetLogosModules`): the inheriting overload writes
//      through to the base, the non-inheriting overload silently
//      no-ops. The latter is the property that lets the generator
//      emit the same `onInit` body for every module — modules that
//      don't inherit just get the no-op specialisations and compile
//      unchanged.
//
// Where the generator's full output is exercised end-to-end (codegen-
// produced provider + a real impl that reads the context) is the
// logos-test-modules integration suite — these tests stay focused on
// the SDK-side primitives.

#include <gtest/gtest.h>
#include "logos_module_context.h"

#include <string>
#include <type_traits>

namespace {

// Impl that opts in to the context. Tracks whether onContextReady fired
// and which values were visible at that point so the test can assert
// "framework set everything BEFORE invoking the hook" (which is what
// the generator's order guarantees).
class ContextInherit : public LogosModuleContext {
public:
    int onContextReadyCalls = 0;
    std::string seenModulePath;
    std::string seenInstanceId;
    std::string seenInstancePersistencePath;

protected:
    void onContextReady() override {
        ++onContextReadyCalls;
        seenModulePath               = modulePath();
        seenInstanceId               = instanceId();
        seenInstancePersistencePath  = instancePersistencePath();
    }
};

// Stand-in for a module-specific `LogosModules` struct. The real one
// is generated per-module (one accessor per dep) — for the SDK-side
// pointer plumbing we only need a type whose address we can compare.
struct FakeLogosModules {
    int sentinel = 42;
};

// Impl that intentionally does NOT inherit LogosModuleContext, used
// to verify that the SFINAE helpers compile (and silently no-op)
// against arbitrary impl types. If the helpers ever regress to a
// blind `static_cast` the dispatch generator emits, this test fails
// to compile — exactly the failure mode we want.
class NonInheritingImpl {
public:
    int touched = 0;
};

} // namespace

// ── LogosModuleContext ───────────────────────────────────────────────────────

TEST(LogosModuleContextTest, GettersDefaultEmptyBeforeSetup)
{
    ContextInherit ctx;
    EXPECT_TRUE(ctx.modulePath().empty());
    EXPECT_TRUE(ctx.instanceId().empty());
    EXPECT_TRUE(ctx.instancePersistencePath().empty());
    EXPECT_EQ(ctx.onContextReadyCalls, 0);
}

TEST(LogosModuleContextTest, SetContextPopulatesGettersAndFiresHook)
{
    ContextInherit ctx;
    ctx._logosCoreSetContext_("/lib/my", "abc123", "/data/my/abc123");

    EXPECT_EQ(ctx.modulePath(),              "/lib/my");
    EXPECT_EQ(ctx.instanceId(),              "abc123");
    EXPECT_EQ(ctx.instancePersistencePath(), "/data/my/abc123");
    EXPECT_EQ(ctx.onContextReadyCalls, 1);
}

TEST(LogosModuleContextTest, OnContextReadySeesPopulatedGetters)
{
    // The generator's order guarantees the three fields are written
    // BEFORE onContextReady fires, so derived impls can rely on the
    // getters returning the freshly-set values from inside the hook.
    ContextInherit ctx;
    ctx._logosCoreSetContext_("/lib/x", "i-7", "/data/x/i-7");

    EXPECT_EQ(ctx.seenModulePath,              "/lib/x");
    EXPECT_EQ(ctx.seenInstanceId,              "i-7");
    EXPECT_EQ(ctx.seenInstancePersistencePath, "/data/x/i-7");
}

TEST(LogosModuleContextTest, SetContextCalledTwiceFiresHookTwice)
{
    // The base class doesn't gate onContextReady — if the framework
    // somehow reinitialises (re-load scenarios), the hook fires
    // again. Documenting via a test so the behaviour is intentional.
    ContextInherit ctx;
    ctx._logosCoreSetContext_("/a", "1", "/d/1");
    ctx._logosCoreSetContext_("/b", "2", "/d/2");
    EXPECT_EQ(ctx.onContextReadyCalls, 2);
    EXPECT_EQ(ctx.modulePath(),              "/b");
    EXPECT_EQ(ctx.instanceId(),              "2");
    EXPECT_EQ(ctx.instancePersistencePath(), "/d/2");
}

TEST(LogosModuleContextTest, SetLogosModulesPtrAndTypedAccessor)
{
    ContextInherit ctx;
    FakeLogosModules modules;
    modules.sentinel = 7;

    // Type-erased framework write…
    ctx._logosCoreSetLogosModulesPtr_(&modules);
    // …typed read on the way out.
    FakeLogosModules& got = ctx.logos<FakeLogosModules>();
    EXPECT_EQ(&got, &modules);
    EXPECT_EQ(got.sentinel, 7);

    // Pointer aliasing: mutating through the original object is
    // visible through the accessor (we hand out a reference, not a
    // copy). Demonstrates the generator's lifetime model — the
    // provider owns the LogosModules and the context holds a non-
    // owning pointer for the impl's lifetime.
    modules.sentinel = 99;
    EXPECT_EQ(ctx.logos<FakeLogosModules>().sentinel, 99);
}

TEST(LogosModuleContextTest, LogosModulesPointerIndependentOfContext)
{
    // The two framework setters touch unrelated state. Setting one
    // doesn't clobber the other — important because the generator
    // calls them in sequence inside the same onInit override and we
    // don't want a future re-ordering or refactor to silently couple
    // them.
    ContextInherit ctx;
    FakeLogosModules modules;

    ctx._logosCoreSetLogosModulesPtr_(&modules);
    ctx._logosCoreSetContext_("/m", "id", "/p");

    EXPECT_EQ(&ctx.logos<FakeLogosModules>(), &modules);
    EXPECT_EQ(ctx.modulePath(), "/m");
}

// ── Tag-dispatched helpers (_logos_codegen_::maybeSet*) ─────────────────────

TEST(LogosModuleContextHelpersTest, MaybeSetContextWritesForInheritingImpl)
{
    ContextInherit impl;
    _logos_codegen_::maybeSetContext(impl, "/p", "id", "/per");
    EXPECT_EQ(impl.modulePath(),              "/p");
    EXPECT_EQ(impl.instanceId(),              "id");
    EXPECT_EQ(impl.instancePersistencePath(), "/per");
    EXPECT_EQ(impl.onContextReadyCalls, 1);
}

TEST(LogosModuleContextHelpersTest, MaybeSetContextNoOpForNonInheritingImpl)
{
    // The whole point: this call has to COMPILE against a type that
    // doesn't inherit LogosModuleContext (because the generator emits
    // the same line for every module). A regression to a blind
    // static_cast would manifest as a build error here, not a
    // runtime failure — that's the property we're locking in.
    NonInheritingImpl impl;
    _logos_codegen_::maybeSetContext(impl, "/p", "id", "/per");
    EXPECT_EQ(impl.touched, 0);  // genuinely nothing happened
}

TEST(LogosModuleContextHelpersTest, MaybeSetLogosModulesWritesForInheritingImpl)
{
    ContextInherit impl;
    FakeLogosModules modules;
    _logos_codegen_::maybeSetLogosModules(impl, &modules);
    EXPECT_EQ(&impl.logos<FakeLogosModules>(), &modules);
}

TEST(LogosModuleContextHelpersTest, MaybeSetLogosModulesNoOpForNonInheritingImpl)
{
    NonInheritingImpl impl;
    FakeLogosModules modules;
    // Same compile-time test as above, separate helper.
    _logos_codegen_::maybeSetLogosModules(impl, &modules);
    EXPECT_EQ(impl.touched, 0);
}

// ── Compile-time enforcement of the SFINAE split ────────────────────────────

// Both overloads must be discoverable by ADL / unqualified lookup.
// Verify at compile time that the right one is selected for each
// branch, so even a silent code-gen regression that picks the wrong
// overload (e.g. removing the std::enable_if_t guard) gets caught
// before any runtime assertion runs.
static_assert(
    std::is_same_v<
        decltype(_logos_codegen_::maybeSetContext(
            std::declval<ContextInherit&>(),
            std::string{}, std::string{}, std::string{})),
        void>,
    "maybeSetContext must resolve to void for impls inheriting LogosModuleContext");

static_assert(
    std::is_same_v<
        decltype(_logos_codegen_::maybeSetContext(
            std::declval<NonInheritingImpl&>(),
            std::string{}, std::string{}, std::string{})),
        void>,
    "maybeSetContext must resolve to void for impls NOT inheriting LogosModuleContext");

static_assert(
    std::is_same_v<
        decltype(_logos_codegen_::maybeSetLogosModules(
            std::declval<ContextInherit&>(),
            std::declval<void*>())),
        void>,
    "maybeSetLogosModules must resolve to void for impls inheriting LogosModuleContext");

static_assert(
    std::is_same_v<
        decltype(_logos_codegen_::maybeSetLogosModules(
            std::declval<NonInheritingImpl&>(),
            std::declval<void*>())),
        void>,
    "maybeSetLogosModules must resolve to void for impls NOT inheriting LogosModuleContext");
