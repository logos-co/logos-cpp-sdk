# Logos Code Generator — Experimental

## Overall Description

The experimental code generator extends `logos-cpp-generator` with three input modes: a lightweight Interface Definition Language (LIDL) for declaring module contracts, a C++ header parser that infers module interfaces from pure C++ implementation classes, and a C header parser that generates Qt plugin glue directly from plain C function declarations. All three paths produce the same output: Qt plugin glue code that bridges module implementations to the Logos runtime's Qt Remote Objects transport.

The goal is to decouple module business logic from the Qt framework. Module authors write their logic in any language (Rust, Go, Zig, C, C++), and the build system generates all Qt boilerplate (`QObject`, `Q_PLUGIN_METADATA`, `QString` conversions, method dispatch) automatically. The `--from-c-header` mode goes furthest: a module backed by a Rust static library requires zero hand-written C++.

## Definitions & Acronyms


| Term                 | Definition                                                                                                                               |
| -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| **LIDL**             | Logos Interface Definition Language — a lightweight DSL for declaring module interfaces                                                  |
| **Universal Module** | A module whose implementation is pure C++ (no Qt types) with all Qt glue generated at build time                                         |
| **C-FFI Module**     | A module whose implementation is any language (Rust, Go, Zig, C) exposing a C ABI; Qt glue is generated directly from the C header       |
| **Provider Glue**    | Generated code that wraps an implementation in a `LogosProviderObject` with `callMethod()` dispatch and `getMethods()` introspection      |
| **Client Stub**      | Generated type-safe C++ wrapper class that callers use to invoke a module's methods without string-based dispatch                        |
| **Dispatch**         | The generated `callMethod()` function that maps string method names to typed method calls on the provider object                         |
| **Impl Header**      | The pure C++ header file (`_impl.h`) that declares a module's public methods using standard C++ types                                    |
| **C Header**         | A plain C header (`*.h`) that declares exported functions sharing a common prefix; used in `--from-c-header` mode                        |
| **Function Prefix**  | The shared prefix of all exported C functions in a C-FFI module (e.g. `rust_example_`). Auto-derived from module name.                  |
| **TypeExpr**         | The AST node representing a type in the LIDL type system                                                                                 |
| **ModuleDecl**       | The AST node representing a complete module declaration (name, version, methods, events, types, `hasEmitEvent` flag)                     |


## Domain Model

### Three Paths to the Same Output

```
Path 1: LIDL file         Path 2: C++ impl header    Path 3: C header (c-ffi)
    │                            │                           │
    ▼                            ▼                           ▼
 lidlTokenize()          parseImplHeader()           parseCHeader()
    │                            │                           │
    ▼                            │                           │
 lidlParse()                     │                           │
    │                            │                           │
    ▼                            │                           │
 lidlValidate()                  │                           │
    │                            │                           │
    ▼                            ▼                           ▼
 ModuleDecl  ◄──────── same AST ──────────────────► ModuleDecl
    │                                                    │
    │                                                CHeaderParseResult
    │                                                (adds C function names
    │                                                 + string ownership info)
    │                                                    │
    ├──► lidlMakeProviderHeader()  ◄──────────────────────┤ (via lidlMakeProviderHeaderCFFI)
    │         → <name>_qt_glue.h                          │
    │                                                     │
    ├──► lidlMakeProviderDispatch() ◄────────────────────-┤ (via lidlMakeProviderDispatchCFFI)
    │         → <name>_dispatch.cpp                       │
    │
    ├──► lidlMakeHeader()
    │         → <name>_api.h
    │
    └──► lidlMakeSource()
              → <name>_api.cpp
```

All three paths converge at `ModuleDecl`. Path 3 carries additional per-method metadata in `CHeaderParseResult` (original C function name, heap string ownership) so the generator can emit direct C function calls instead of `m_impl.method()` calls.

### Key difference between Path 2 and Path 3

| | Path 2 (`--from-header`) | Path 3 (`--from-c-header`) |
|---|---|---|
| Input | C++ class with public methods | C functions with shared prefix |
| Generated calls | `m_impl.method(args)` (via C++ object) | `c_function(args)` (direct C call) |
| String handling | `std::string` ↔ `QString` | `char*`/`const char*` ↔ `QString` |
| Heap strings | N/A (std::string manages memory) | `char*` return → freed with `{prefix}free_string` |
| Hand-written C++ | ~80 lines (impl .h + .cpp) | Zero |

### LIDL Language

LIDL is a minimal interface definition language. A module declaration contains metadata, type definitions, method signatures, and event signatures:

```
module wallet_module {
    version "1.0.0"
    description "Wallet operations"
    category "finance"
    depends [crypto_module]

    type Account {
        address: tstr
        balance: uint
        ? label: tstr          ; optional field
    }

    method createAccount(passphrase: tstr) -> tstr
    method getBalance(address: tstr) -> uint
    method listAccounts() -> [tstr]
    method transfer(from: tstr, to: tstr, amount: uint) -> result

    event onTransfer(from: tstr, to: tstr, amount: uint)
}
```

Comments start with `;` and run to end of line.

### Type System

Built-in primitive types:


| LIDL type | Meaning                                 | Qt mapping    | C++ std mapping        |
| --------- | --------------------------------------- | ------------- | ---------------------- |
| `tstr`    | Text string                             | `QString`     | `std::string`          |
| `bstr`    | Binary data                             | `QByteArray`  | `std::vector<uint8_t>` |
| `int`     | Signed 64-bit integer                   | `int`         | `int64_t`              |
| `uint`    | Unsigned 64-bit integer                 | `int`         | `uint64_t`             |
| `float64` | Double precision float                  | `double`      | `double`               |
| `bool`    | Boolean                                 | `bool`        | `bool`                 |
| `result`  | Structured result (success/value/error) | `LogosResult` | `LogosResult`          |
| `any`     | Untyped value                           | `QVariant`    | `QVariant`             |
| `void`    | No return value                         | `void`        | `void`                 |


Composite types:

- `[T]` — Array of T (e.g., `[tstr]` → `QStringList` / `std::vector<std::string>`)
- `{K: V}` — Map from K to V (e.g., `{tstr: int}` → `QVariantMap`)
- `?T` — Optional T (→ `QVariant`)

Named types reference `type` definitions within the same module.

### C++ Header Parsing

The `--from-header` mode parses a C++implementation header to extract public method signatures. It maps C++ types to LIDL types:


| C++ type                             | LIDL type                                                          |
| ------------------------------------ | ------------------------------------------------------------------ |
| `std::string` / `const std::string&` | `tstr`                                                             |
| `bool`                               | `bool`                                                             |
| `int64_t`                            | `int`                                                              |
| `uint64_t`                           | `uint`                                                             |
| `double`                             | `float64`                                                          |
| `void`                               | `void`                                                             |
| `std::vector<std::string>`           | `[tstr]`                                                           |
| `std::vector<uint8_t>`               | `bstr`                                                             |
| `std::vector<int64_t>`               | `[int]`                                                            |
| `std::vector<uint64_t>`              | `[uint]`                                                           |
| `std::vector<double>`                | `[float64]`                                                        |
| `std::vector<bool>`                  | `[bool]`                                                           |
| `LogosMap`                           | `{tstr: any}` (Map) — nlohmann::json alias; sets `jsonReturn` flag |
| `LogosList`                          | `[any]` (Array) — nlohmann::json alias; sets `jsonReturn` flag     |
| `QVariantMap`                        | `{tstr: any}` (Map) — legacy Qt type                               |
| `QVariantList`                       | `[any]` (Array) — legacy Qt type                                   |
| `QStringList`                        | `[tstr]` (Array) — legacy Qt type                                  |
| Anything else                        | `any`                                                              |


`LogosMap` and `LogosList` are `using` aliases for `nlohmann::json` defined in `logos_json.h` (part of the SDK). They allow module implementations to remain completely Qt-free while returning rich structured data. The parser maps them to the same LIDL shapes as `QVariantMap`/`QVariantList`, but sets the `jsonReturn` flag on the method so the generator emits an `nlohmannToQVariant()` conversion in the glue layer.

The parser uses a state machine to find the target class, track access specifiers (`public`/`private`/`protected`), and extract method declarations. It skips constructors, destructors, typedefs, using declarations, `std::function` members, and non-method statements.

Module metadata (name, version, description, dependencies) comes from `metadata.json`, not from the header.

### C Header Parsing (`--from-c-header`)

The `--from-c-header` mode parses a plain C header (no class syntax) to extract function declarations that share a common prefix. It maps C types to LIDL types:

| C type | LIDL type | Qt type | Notes |
|--------|-----------|---------|-------|
| `int64_t`, `int32_t`, `int`, `long` | `int` | `int` | Cast via `static_cast<int64_t>` for C call |
| `uint64_t`, `uint32_t`, `unsigned int` | `uint` | `int` | |
| `double`, `float` | `float64` | `double` | |
| `bool`, `_Bool` | `bool` | `bool` | |
| `const char*` | `tstr` | `QString` | Static/borrowed — no free |
| `char*` | `tstr` | `QString` | Heap-allocated — freed with `{prefix}free_string` |
| `void` | `void` | `void` | |
| anything else | `any` | `QVariant` | |

**Prefix convention:** All exported C functions must share a prefix. The prefix is auto-derived from the module name: strip `_module` suffix, append `_`. For `"name": "rust_example_module"` → prefix `rust_example_`. Override with `--prefix` CLI flag or `"codegen": {"c_prefix": "..."}` in `metadata.json`.

**String ownership:** The parser distinguishes `char*` (mutable, heap-allocated) from `const char*` (immutable, static/borrowed). For `char*` returns, the generator emits:
```cpp
char* _result = c_function(args);
QString _ret = _result ? QString::fromUtf8(_result) : QString();
prefix_free_string(_result);   // auto-wired
return _ret;
```
For `const char*` returns: no free call. The `{prefix}free_string(char*)` function is detected automatically in the header and **not** exposed as a module method.

**Reserved name renaming:** `{prefix}version` → `libVersion`, `{prefix}name` → `libName`, `{prefix}initLogos` → `libInitLogos`.

The parser skips: `#include`, `#ifdef`/`#endif`, `extern "C"` blocks, block comments (`/* ... */`), blank lines, `typedef`/`struct`/`union`/`enum` declarations, and lines that don't start with the expected prefix.

Module metadata (name, version, description, dependencies) comes from `metadata.json`, same as Path 2.

### Event Emission via Header Detection

Universal modules can emit named events to the host/runtime by declaring a public `emitEvent` callback in their impl header:

```cpp
class MyModuleImpl {
public:
    std::function<void(const std::string& eventName, const std::string& data)> emitEvent;
    // ... methods ...
};
```

The parser detects this `std::function` member by name and sets `ModuleDecl.hasEmitEvent = true`. The generator then wires the callback in the provider constructor:

```cpp
MyModuleProviderObject() {
    m_impl.emitEvent = [this](const std::string& name, const std::string& data) {
        QVariantList args;
        if (!data.empty()) args << QString::fromStdString(data);
        emitEvent(QString::fromStdString(name), args);
    };
}
```

This replaces the previous approach of declaring events in `metadata.json`. The `events` array in metadata.json is still supported for backward compatibility (e.g., LIDL-defined modules), but header detection is the preferred approach for universal modules since it keeps event information co-located with the implementation.

### Generated Output

#### Provider Glue (`<name>_qt_glue.h`) — universal mode (`--from-header`)

Contains two classes:

1. **ProviderObject** — inherits `LogosProviderBase`, holds an instance of the impl class (`m_impl`). Each public method is wrapped with type conversion:
  - Qt parameters → C++ std parameters (e.g., `QString.toStdString()`)
  - Call `m_impl.method(...)`
  - C++ std return → Qt return (e.g., `QString::fromStdString(result)`)
  - For `jsonReturn` methods (returning `LogosMap`/`LogosList`), the glue calls a generated `nlohmannToQVariant()` recursive helper to convert `nlohmann::json` → `QVariant`/`QVariantMap`/`QVariantList`
  - If the impl declares an `emitEvent` callback (`hasEmitEvent`), the constructor wires it to `LogosProviderBase::emitEvent`
2. **Plugin** — `QObject` subclass implementing `PluginInterface` and `LogosProviderPlugin`. Carries `Q_PLUGIN_METADATA` and `Q_INTERFACES`. Its `createProviderObject()` factory returns a new ProviderObject instance.

#### Provider Glue (`<name>_qt_glue.h`) — c-ffi mode (`--from-c-header`)

Same two-class structure, but the ProviderObject:
- Has no `m_impl` member — no C++ class is instantiated
- Includes the C header directly via `extern "C" { #include "..." }`
- Each method calls the C function directly with Qt ↔ C type conversions:
  - `QString` → `QByteArray` (kept alive) → `const char*` for input strings
  - `char*` C return → `QString::fromUtf8()` + `{prefix}free_string()` call
  - `const char*` C return → `QString::fromUtf8()`, no free
  - `int64_t` / `uint64_t` ↔ `int` via `static_cast`

#### Dispatch (`<name>_dispatch.cpp`)

Implements two methods on the ProviderObject:

1. `**callMethod(methodName, args)`** — string-based dispatch table. For each method, extracts args from `QVariantList`, calls the typed wrapper, returns result as `QVariant`. Void methods return `QVariant(true)`.
2. `**getMethods()**` — returns `QJsonArray` of method metadata. Each entry has `name`, `signature`, `returnType`, `isInvokable`, and `parameters[]` (with `type` and `name`).

#### Client Stubs (`<name>_api.h` + `<name>_api.cpp`)

Generated from LIDL (not from `--from-header`). Provides:

- Typed sync methods that call `invokeRemoteMethod()` and convert the `QVariant` result
- Async overloads with callback + timeout
- Event subscription (`on()`) and emission (`trigger()`)
- Umbrella `logos_sdk.h` / `logos_sdk.cpp` aggregating all module wrappers

## Features & Requirements

### LIDL Pipeline

1. **Lexer** (`lidlTokenize`) — tokenizes source into keywords, identifiers, string literals, symbols
2. **Parser** (`lidlParse`) — recursive descent parser producing a `ModuleDecl` AST
3. **Validator** (`lidlValidate`) — checks for duplicate names, unknown type references, builtin shadowing, duplicate parameters
4. **Serializer** (`lidlSerialize`) — pretty-prints a `ModuleDecl` back to LIDL text (useful for roundtrip testing)

### Impl Header Pipeline

1. **parseImplHeader** — reads `metadata.json` + C++ header, produces a `ModuleDecl`
2. Same generation functions as LIDL path

### C Header Pipeline

1. **parseCHeader** — reads `metadata.json` + C header, produces a `CHeaderParseResult` containing:
   - `ModuleDecl` (same AST as the other paths)
   - Per-method `CHeaderMethod` entries: original C function name + heap-string ownership flag
   - `freeStringFunc` — the detected `{prefix}free_string` function name (empty if none)
2. **lidlMakeProviderHeaderCFFI** — generates Qt glue with direct C function calls
3. **lidlMakeProviderDispatchCFFI** — generates callMethod/getMethods dispatch (delegates to `lidlMakeProviderDispatch` since the dispatch shape is identical)

### Backwards Compatibility

- All existing generator modes (`--provider-header`, `--metadata`, plugin path) continue to work unchanged via `legacy_main()`
- The `--from-header`, `--from-c-header`, and `--lidl` modes are additive
- Generated plugins implement both `PluginInterface` (for `lm` introspection) and `LogosProviderPlugin` (for new-API provider creation)
- The runtime (`logos-liblogos`) already supports both old and new plugin types via `qobject_cast` detection

### Conversion Helper Generation

Conversion helpers are only emitted when needed:

- **String vector helpers** (`lidlToQStringList`, `lidlToStdStringVector`) — emitted when the module uses `[tstr]` parameters or return types
- **nlohmann→Qt helper** (`nlohmannToQVariant`) — emitted when any method has `jsonReturn = true` (i.e., the impl returns `LogosMap` or `LogosList`). This recursive function converts `nlohmann::json` objects, arrays, strings, numbers, and booleans to their `QVariant` equivalents.

## Choosing an Interface Mode

| Situation | Recommended mode |
|-----------|-----------------|
| Logic in Rust/Go/Zig/C with synchronous C API | `c-ffi` (`--from-c-header`) |
| Logic in C++ without Qt dependency | `universal` (`--from-header`) |
| Complex async/callback-driven C library | Hand-written plugin (or `universal` with manual impl class) |
| Formal cross-team contract needed | `lidl` (`.lidl` file) |

### When `c-ffi` is the right choice

Use `c-ffi` when the C library is **synchronous and the return value is immediate**:

```
Qt call → C function → immediate return value → Qt return
```

This covers pure computation (math, crypto primitives, string processing), simple config/storage APIs, and most wrappers around Rust or Zig libraries. Zero hand-written C++ required.

### When `c-ffi` is NOT the right choice

`c-ffi` generates a direct call-and-return for every method. It cannot express:

1. **Async / callback-driven APIs.** If the C library takes a `void (*callback)(int code, const char* msg, void* userData)` and calls it later, there is nowhere in the generated code to wait for it. A real example is `logos-storage-module`, which wraps `libstorage` — every operation (`init`, `start`, `upload`, `download`) is asynchronous. The hand-written plugin uses a Qt mutex + `QWaitCondition` to turn callbacks into synchronous `LogosResult` returns, and a Qt signal system to propagate async events (`storageConnect`, `uploadProgress`, etc.) to the host.

2. **Per-instance state.** `c-ffi` generates no class instance — all state must live inside the C library itself (global or thread-local). If you need a context pointer (`void* ctx`) that is created on init and passed to every subsequent call, you need an impl class (or a hand-written plugin) to hold it as a member variable.

3. **Asynchronous events.** If the C library fires callbacks on its own schedule (connection events, progress notifications), those need to be converted into Logos events (`emitEvent()`). This requires a custom callback registration step and a way to route the callback back to the Qt object — not expressible in generated straight-line code.

4. **Non-trivial Qt types as parameters.** `c-ffi` supports `int64_t`, `bool`, `double`, `char*`, `const char*`, and `void`. Parameters like `QUrl`, `QByteArray`, `QStringList`, default argument values, or overloaded methods require hand-written conversion logic.

The rule of thumb: if wrapping the library requires more than type conversions in the generated glue, use `universal` or write the plugin by hand.

