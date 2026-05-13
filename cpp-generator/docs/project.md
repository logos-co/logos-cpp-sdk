# Logos Code Generator — Project Description

## Project Structure

```
cpp-generator/
├── main.cpp                        # Entry point — dispatches to legacy or experimental
├── CMakeLists.txt                  # Build config
├── compile.sh                      # Standalone build script
├── legacy/                         # Original generator (unchanged from master)
│   ├── main.cpp                    # legacy_main() — plugin/metadata/provider-header modes
│   ├── generator_lib.h/cpp         # Shared utilities, type mapping, header parser
│   └── legacy_main.h              # Forward declaration
├── experimental/                   # New LIDL + impl-header generator
│   ├── lidl_ast.h                 # AST types (TypeExpr, ModuleDecl, MethodDecl, etc.)
│   ├── lidl_lexer.h/cpp           # LIDL tokenizer
│   ├── lidl_parser.h/cpp          # LIDL recursive descent parser
│   ├── lidl_validator.h/cpp       # Semantic validation
│   ├── lidl_serializer.h/cpp      # AST → LIDL text pretty-printer
│   ├── lidl_gen_client.h/cpp      # Client stub generation + helpers
│   ├── lidl_gen_provider.h/cpp    # Provider glue + dispatch generation
│   └── impl_header_parser.h/cpp   # C++ header → ModuleDecl parser
└── docs/                          # This documentation
```

## Components

### Entry Point (`main.cpp`)

Checks for `--from-header` or `--lidl` flags before creating `QCoreApplication`. If neither is present, falls through to `legacy_main()`.

### AST (`lidl_ast.h`)

Shared data model used by all pipelines:

- **`TypeExpr`** — type expression with `Kind` (Primitive, Array, Map, Optional, Named), `name`, and `elements`
- **`ParamDecl`** — parameter name + type
- **`MethodDecl`** — method name, params, return type, `jsonReturn` flag (true when impl returns `LogosMap`/`LogosList`)
- **`EventDecl`** — event name + params
- **`FieldDecl`** — struct field name, type, optional flag
- **`TypeDecl`** — named struct type with fields
- **`ModuleDecl`** — complete module: name, version, description, category, depends, types, methods, events, `hasEmitEvent` flag

All types have `operator==` for testing.

### Lexer (`lidl_lexer.h/cpp`)

Tokenizes LIDL source. Token types: `Module`, `TypeKw`, `Method`, `Event`, `Version`, `Description`, `Category`, `Depends`, `Ident`, `StringLit`, symbols (`{`, `}`, `(`, `)`, `[`, `]`, `:`, `,`, `->`, `?`), `Eof`, `Error`. Tracks line/column for error reporting.

### Parser (`lidl_parser.h/cpp`)

Recursive descent parser. Grammar:

```
module     = "module" IDENT "{" body "}"
body       = (metadata | type_def | method_def | event_def)*
metadata   = "version" STRING | "description" STRING | "category" STRING
           | "depends" "[" (IDENT ("," IDENT)*)? "]"
type_def   = "type" IDENT "{" field* "}"
field      = "?"? IDENT ":" type_expr
method_def = "method" IDENT "(" params ")" "->" type_expr
event_def  = "event" IDENT "(" params ")"
params     = (IDENT ":" type_expr ("," IDENT ":" type_expr)*)?
type_expr  = IDENT | "[" type_expr "]" | "{" type_expr ":" type_expr "}"
           | "?" type_expr
```

### Validator (`lidl_validator.h/cpp`)

Checks: empty module name, duplicate type/method/event names, builtin type shadowing, unknown named type references, duplicate parameter names within methods.

### Serializer (`lidl_serializer.h/cpp`)

Converts `ModuleDecl` back to LIDL text. Used for roundtrip testing (parse → serialize → parse → compare).

### Type Mapping (`lidl_gen_client.h/cpp`)

- `lidlTypeToQt(TypeExpr)` — maps LIDL types to Qt type strings
- `lidlToPascalCase(name)` — converts `snake_case` to `PascalCase`
- `lidlMakeHeader(ModuleDecl)` — generates client API header
- `lidlMakeSource(ModuleDecl)` — generates client API source
- `lidlGenerateMetadataJson(ModuleDecl)` — generates metadata.json content

### Per-build API-style choice (`legacy/generator_lib.{h,cpp}`)

The codegen exposes **one** wrapper class per module — `<Module>` — with signatures that match the API style picked at the consumer's build time. The two styles are mutually exclusive (no composite output):

| `--api-style` | Wrapper signatures |
|---|---|
| `qt` (default) | `QString` / `QStringList` / `QVariantList` / `QVariantMap` / `int` / `LogosResult` |
| `std` | `std::string` / `std::vector<std::string>` / `LogosMap` / `LogosList` / `int64_t` / `StdLogosResult` |

Both styles emit:

- A `<Module>` client class with sync method shapes + matching `<method>Async(...)` overloads.
- The std variant additionally inlines Qt↔std conversion in its `.cpp` so the caller's translation unit needs zero Qt headers.

The umbrella `logos_sdk.h` is also generated per-build and aggregates every dep into a flat `LogosModules` struct — no nested view:

```cpp
struct LogosModules {
    LogosAPI*       api;
    SomeDep         some_dep;              // one accessor per `metadata.json#dependencies` entry
    // ...
};
```

Only the modules explicitly listed as dependencies are exposed. The runtime's `core_manager` is intentionally NOT in `LogosModules` — apps that need to manage the core do so via liblogos' C API, not via a typed RPC wrapper.

`ApiStyle` enum + new helpers in `generator_lib`:

- `enum class ApiStyle { Qt, Std }` — passed to every wrapper-emitting function.
- File-local `mapParamTypeStd` / `mapReturnTypeStd` / `stdParamToQVariant` / `qVariantToStdReturn` — std-side type-mapping + Qt↔std conversion expressions. Hidden from `generator_lib.h` (not part of the public surface).
- `makeHeader(moduleName, className, methods, apiStyle, events)` / `makeSource(moduleName, className, headerBaseName, methods, apiStyle, events)` — single entry points that branch on `apiStyle` internally to emit the right include block, signature shape, and conversion bridges. `events` is loaded from a `<name>.lidl` sidecar via `--events-from`; when non-empty, the wrapper also gets one typed `on<EventName>(callback)` adapter per declared event (callback arg types follow `apiStyle`). The std-style wrapper grows the necessary `ensureReplica()` plumbing on demand.

Flag plumbing:

1. `metadata.json#interface == "universal"` → `mkLogosModule.nix` adds `-DLOGOS_API_STYLE=std` to `extraCmakeFlags`. Anything else (`"legacy"`, `"provider"`, absent) leaves the default `qt`.
2. `LogosModule.cmake` reads `${LOGOS_API_STYLE}` (default `qt`) and forwards `--api-style=${LOGOS_API_STYLE}` to the `logos-cpp-generator --general-only` invocation that writes the umbrella. Each module's Nix build emits **two** header derivations (`<name>.headers-qt` and `<name>.headers-std`) via `buildHeaders.nix` — one `logos-cpp-generator --api-style=…` run per style, at the dep's build time. A consumer's `buildPlugin.nix` picks `dep.headers-${apiStyle}` and copies its `include/` straight into the build sandbox; no codegen runs at consume time. Nix's laziness means only the variant a downstream actually depends on is realised.
3. `legacy/main.cpp` parses `--api-style` once and threads the resulting `ApiStyle` through `generateFromPlugin`, `writeUmbrellaHeader{,FromDeps}`. No `_api_std.{h,cpp}` files are ever emitted; each module gets a single `<name>_api.h` + `<name>_api.cpp` pair regardless of style.

### Provider Generation (`lidl_gen_provider.h/cpp`)

- `lidlTypeToStd(TypeExpr)` — maps LIDL types to C++ std type strings
- `lidlIsStdConvertible(TypeExpr)` — checks if a type has a pure C++ representation
- `lidlMakeProviderHeader(ModuleDecl, implClass, implHeader)` — generates Qt glue header
  - Emits `nlohmannToQVariant()` helper when any method has `jsonReturn = true`
  - Legacy path: wires `m_impl.emitEvent` → `LogosProviderBase::emitEvent` in the constructor when `hasEmitEvent` is set (un-migrated modules using the old `std::function emitEvent` member)
  - Always emits an `onInit(LogosAPI*) override` that, via SFINAE'd helpers in `logos_module_context.h`, (a) copies the three runtime-injected properties (`modulePath`, `instanceId`, `instancePersistencePath`) into the impl, (b) constructs a per-module `LogosModules` aggregate and threads its pointer through the same base, and (c) installs the typed-event callback (`maybeSetEmitEvent`) consumed by `<name>_events.cpp` method bodies. Impls that don't inherit `LogosModuleContext` compile unchanged — the helper overloads collapse to no-ops. The full `LogosAPI` is never exposed past the provider boundary.
  - Always emits `#include "logos_sdk.h"` and a `std::unique_ptr<LogosModules> m_logosModules` member; ownership lives on the provider, the context base sees only a non-owning `void*` reinterpreted in `LogosModuleContext::modules()` (which depends on the impl's TU having included `logos_sdk.h`).
- `lidlMakeProviderDispatch(ModuleDecl)` — generates callMethod/getMethods dispatch
- `lidlMakeEventsSource(ModuleDecl, implClass, implHeader)` — generates `<name>_events.cpp`: Qt-MOC-style method bodies for prototypes declared in the impl's `logos_events:` block. Each body marshals typed args into a `QVariantList` and calls `this->emitEventImpl_("<name>", &args)` on the LogosModuleContext base.
- `lidlGenerateProviderGlue(lidlPath, ...)` — full pipeline from .lidl file. Also emits `<name>_events.cpp` and a `<name>.lidl` sidecar (via `lidlSerialize`) when the module has any events; both ride the dep's `headers-*` outputs to power consumer-side typed `on<X>()` accessors.

### Impl Header Parser (`impl_header_parser.h/cpp`)

- `parseImplHeader(headerPath, className, metadataPath, err)` — parses C++ header + metadata.json into ModuleDecl
- State machine: `LookingForClass` → `InClass` → `InPublic`/`InPrivate`/`InLogosEvents`
- The literal `logos_events:` token (defined in `logos_module_context.h` as `#define logos_events public`) opens an events section; bare prototypes inside become `EventDecl{name, params}` entries appended to `ModuleDecl.events`
- Skips: constructors, destructors, typedefs, using, friend, enum, struct, `std::function` declarations
- Legacy: still detects `std::function<...> emitEvent` members and sets `ModuleDecl.hasEmitEvent = true` so un-migrated modules keep working through the provider constructor's lambda wiring
- Recognizes `LogosMap` and `LogosList` return types (nlohmann::json aliases) and sets `MethodDecl.jsonReturn = true`
- Template-aware parameter splitting (handles `std::vector<std::string>` correctly)

## CLI Usage

### From C++ impl header (primary use case for universal modules)

```bash
logos-cpp-generator --from-header src/my_module_impl.h \
    --backend qt \
    --impl-class MyModuleImpl \
    --impl-header my_module_impl.h \
    --metadata metadata.json \
    --output-dir ./generated_code
```

Generates: `my_module_qt_glue.h`, `my_module_dispatch.cpp`

### From LIDL file — provider glue

```bash
logos-cpp-generator --lidl my_module.lidl \
    --backend qt \
    --impl-class MyModuleImpl \
    --impl-header my_module_impl.h \
    --output-dir ./generated_code
```

### From LIDL file — client stubs

```bash
logos-cpp-generator --lidl my_module.lidl \
    --output-dir ./generated_code \
    --module-only
```

### Legacy modes (unchanged)

```bash
logos-cpp-generator /path/to/plugin.so --output-dir ./generated
logos-cpp-generator --metadata metadata.json --general-only --output-dir ./generated
logos-cpp-generator --provider-header src/provider.h --output-dir ./generated
```

### Consumer wrapper with typed event accessors

The `--events-from <path>` flag points the legacy `<plugin>.dylib --module-only` codegen at a LIDL sidecar shipped alongside the dep's pre-built headers. When set, the generated `<name>_api.{h,cpp}` gains one typed `on<EventName>(callback)` accessor per declared event (callback arg types match `--api-style`):

```bash
logos-cpp-generator /path/to/plugin.dylib \
    --module-only --api-style std \
    --events-from /path/to/dep/share/logos/my_module.lidl \
    --output-dir ./generated
```

In Nix builds this is wired automatically: `buildHeaders.nix` looks for `<pluginLib>/share/logos/<name>.lidl` (which `buildPlugin.nix`'s installPhase placed there) and threads it through.

## Building

The generator is built as part of logos-cpp-sdk:

```bash
ws build logos-cpp-sdk     # builds everything including the generator
```

The generator binary is available as `logos-cpp-generator` in module build environments (provided by logos-module-builder's `nativeBuildInputs`).

## Testing

Tests are in `tests/experimental/`:

```bash
ws test logos-cpp-sdk      # runs all tests including experimental
```

Test coverage:

| Test file | What it tests |
|-----------|---------------|
| `test_lidl_lexer.cpp` | Tokenization: keywords, identifiers, symbols, strings, escapes, comments, errors, line/column tracking |
| `test_lidl_parser.cpp` | Parsing: metadata, methods, events, types, type expressions (array, map, optional, all primitives), error cases |
| `test_lidl_validator.cpp` | Validation: duplicates, shadowing, unknown types, duplicate params |
| `test_lidl_serializer.cpp` | Serialization: all constructs, roundtrip (parse → serialize → parse → compare) |
| `test_lidl_type_mapping.cpp` | `lidlTypeToQt`, `lidlTypeToStd`, `lidlIsStdConvertible`, `lidlToPascalCase` |
| `test_lidl_gen_provider.cpp` | Provider header + dispatch generation: class names, includes, macros, wrapper methods, conversions, events |
| `test_lidl_gen_client.cpp` | Client stub generation: sync/async methods, events, metadata JSON, edge cases |
| `test_impl_header_parser.cpp` | Header parsing: type mapping, access specifiers, skipping private/protected, error cases |

Fixture files in `tests/experimental/fixtures/`:
- `sample_impl.h` — module with all supported type variations
- `sample_metadata.json` — metadata with dependencies
- `complex_impl.h` — module with multiple access specifier sections
- `empty_class_impl.h` — class with no public methods
- `empty_metadata.json` — minimal metadata

## Known Limitations

- The impl header parser is lightweight (regex + state machine). It does not handle:
  - Multi-line method declarations
  - Default parameter values
  - Method definitions in the header (only declarations ending with `;`)
  - Nested classes
  - Template methods
  - `std::function` members other than `emitEvent` are silently skipped
- LIDL does not support generic/parameterized types or inheritance
- Only the `qt` backend is implemented for `--from-header`; future backends (CBOR, Rust) are planned
- Client stub generation (`lidlMakeHeader`/`lidlMakeSource`) is only available from LIDL files, not from `--from-header`
