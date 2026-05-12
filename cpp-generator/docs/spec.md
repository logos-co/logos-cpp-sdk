# Logos Code Generator — Experimental

## Overall Description

The experimental code generator extends `logos-cpp-generator` with two new capabilities: a lightweight Interface Definition Language (LIDL) for declaring module contracts, and a C++header parser that can infer module interfaces directly from pure C++ implementation classes. Both paths produce the same output: Qt plugin glue code that bridges pure C++ module implementations to the Logos runtime's Qt Remote Objects transport.

The goal is to decouple module business logic from the Qt framework. Module authors write standard C++ using `std::string`, `int64_t`, `std::vector<T>`, and the build system generates all Qt boilerplate (`QObject`, `Q_PLUGIN_METADATA`, `QString` conversions, method dispatch) automatically.

## Definitions & Acronyms


| Term                 | Definition                                                                                                                               |
| -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- |
| **LIDL**             | Logos Interface Definition Language — a lightweight DSL for declaring module interfaces                                                  |
| **Universal Module** | A module whose implementation is pure C++ (no Qt types) with all Qt glue generated at build time                                         |
| **Provider Glue**    | Generated code that wraps a pure C++ impl class in a `LogosProviderObject` with `callMethod()` dispatch and `getMethods()` introspection |
| **Client Stub**      | Generated type-safe C++ wrapper class that callers use to invoke a module's methods without string-based dispatch                        |
| **Dispatch**         | The generated `callMethod()` function that maps string method names to typed method calls on the provider object                         |
| **Impl Header**      | The pure C++header file (`_impl.h`) that declares a module's public methods using standard C++ types                                     |
| **TypeExpr**         | The AST node representing a type in the LIDL type system                                                                                 |
| **ModuleDecl**       | The AST node representing a complete module declaration (name, version, methods, events, types, `hasEmitEvent` flag)                     |


## Domain Model

### Two Paths to the Same Output

```
Path 1: LIDL file                    Path 2: C++ impl header
    │                                     │
    ▼                                     ▼
 lidlTokenize()                    parseImplHeader()
    │                                     │
    ▼                                     │
 lidlParse()                              │
    │                                     │
    ▼                                     │
 lidlValidate()                           │
    │                                     │
    ▼                                     ▼
 ModuleDecl  ◄────── same AST ──────► ModuleDecl
    │                                     │
    ├──► lidlMakeProviderHeader()  ◄──────┤
    │         → <name>_qt_glue.h          │
    │                                     │
    ├──► lidlMakeProviderDispatch() ◄─────┤
    │         → <name>_dispatch.cpp       │
    │                                     │
    ├──► lidlMakeHeader()                 │
    │         → <name>_api.h              │
    │                                     │
    └──► lidlMakeSource()                 │
              → <name>_api.cpp            │
```

Both paths converge at `ModuleDecl`, the shared AST. From there, the same generation functions produce identical output regardless of the input format.

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

#### Provider Glue (`<name>_qt_glue.h`)

Contains two classes:

1. **ProviderObject** — inherits `LogosProviderBase`, holds an instance of the impl class (`m_impl`). Each public method is wrapped with type conversion:
  - Qt parameters → C++ std parameters (e.g., `QString.toStdString()`)
  - Call `m_impl.method(...)`
  - C++ std return → Qt return (e.g., `QString::fromStdString(result)`)
  - For `jsonReturn` methods (returning `LogosMap`/`LogosList`), the glue calls a generated `nlohmannToQVariant()` recursive helper to convert `nlohmann::json` → `QVariant`/`QVariantMap`/`QVariantList`
  - If the impl declares an `emitEvent` callback (`hasEmitEvent`), the constructor wires it to `LogosProviderBase::emitEvent`
  - Always overrides `onInit(LogosAPI*)` to (a) copy the three runtime-injected properties (`modulePath`, `instanceId`, `instancePersistencePath`) into the impl when it inherits from `LogosModuleContext`, and (b) construct a per-module `LogosModules` (from `generated_code/logos_sdk.h`) owned by the provider, threading its pointer through the same context base. Both wire-ups go through SFINAE'd helpers in `logos_module_context.h` (`_logos_codegen_::maybeSetContext` / `maybeSetLogosModules`), so non-inheriting impls compile unchanged and the `LogosAPI` never escapes the provider.
2. **Plugin** — `QObject` subclass implementing `PluginInterface` and `LogosProviderPlugin`. Carries `Q_PLUGIN_METADATA` and `Q_INTERFACES`. Its `createProviderObject()` factory returns a new ProviderObject instance.

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

### Backwards Compatibility

- All existing generator modes (`--provider-header`, `--metadata`, plugin path) continue to work unchanged via `legacy_main()`
- The new `--from-header` and `--lidl` modes are additive
- Generated plugins implement both `PluginInterface` (for `lm` introspection) and `LogosProviderPlugin` (for new-API provider creation)
- The runtime (`logos-liblogos`) already supports both old and new plugin types via `qobject_cast` detection

### Conversion Helper Generation

Conversion helpers are only emitted when needed:

- **String vector helpers** (`lidlToQStringList`, `lidlToStdStringVector`) — emitted when the module uses `[tstr]` parameters or return types
- **nlohmann→Qt helper** (`nlohmannToQVariant`) — emitted when any method has `jsonReturn = true` (i.e., the impl returns `LogosMap` or `LogosList`). This recursive function converts `nlohmann::json` objects, arrays, strings, numbers, and booleans to their `QVariant` equivalents.

