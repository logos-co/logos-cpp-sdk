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
├── experimental/                   # New LIDL + impl-header + c-ffi generator
│   ├── lidl_ast.h                 # AST types (TypeExpr, ModuleDecl, MethodDecl, etc.)
│   ├── lidl_lexer.h/cpp           # LIDL tokenizer
│   ├── lidl_parser.h/cpp          # LIDL recursive descent parser
│   ├── lidl_validator.h/cpp       # Semantic validation
│   ├── lidl_serializer.h/cpp      # AST → LIDL text pretty-printer
│   ├── lidl_gen_client.h/cpp      # Client stub generation + helpers
│   ├── lidl_gen_provider.h/cpp    # Provider glue + dispatch generation (universal + c-ffi)
│   ├── impl_header_parser.h/cpp   # C++ header → ModuleDecl parser (--from-header)
│   └── c_header_parser.h/cpp      # C header → CHeaderParseResult parser (--from-c-header)
└── docs/                          # This documentation
```

## Components

### Entry Point (`main.cpp`)

Checks for `--from-c-header`, `--from-header`, or `--lidl` flags before creating `QCoreApplication`. If none is present, falls through to `legacy_main()`.

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

### Provider Generation (`lidl_gen_provider.h/cpp`)

- `lidlTypeToStd(TypeExpr)` — maps LIDL types to C++ std type strings
- `lidlIsStdConvertible(TypeExpr)` — checks if a type has a pure C++ representation
- `lidlMakeProviderHeader(ModuleDecl, implClass, implHeader)` — generates Qt glue header (universal / `--from-header` mode)
  - Emits `nlohmannToQVariant()` helper when any method has `jsonReturn = true`
  - Wires `m_impl.emitEvent` → `LogosProviderBase::emitEvent` when `hasEmitEvent` or `events` are present
- `lidlMakeProviderDispatch(ModuleDecl)` — generates callMethod/getMethods dispatch
- `lidlMakeProviderHeaderCFFI(CHeaderParseResult)` — generates Qt glue header (c-ffi / `--from-c-header` mode)
  - No `m_impl` member; includes C header via `extern "C" { #include "..." }`
  - Emits direct C function calls with Qt ↔ C type conversions
  - Handles `char*` heap strings with auto-wired free function; `const char*` with no free
- `lidlMakeProviderDispatchCFFI(CHeaderParseResult)` — dispatch for c-ffi (delegates to `lidlMakeProviderDispatch`)
- `lidlGenerateProviderGlue(lidlPath, ...)` — full pipeline from .lidl file

### Impl Header Parser (`impl_header_parser.h/cpp`)

- `parseImplHeader(headerPath, className, metadataPath, err)` — parses C++ header + metadata.json into ModuleDecl
- State machine: `LookingForClass` → `InClass` → `InPublic`/`InPrivate`
- Skips: constructors, destructors, typedefs, using, friend, enum, struct, `std::function` declarations
- Detects `std::function<...> emitEvent` members and sets `ModuleDecl.hasEmitEvent = true`
- Recognizes `LogosMap` and `LogosList` return types (nlohmann::json aliases) and sets `MethodDecl.jsonReturn = true`
- Template-aware parameter splitting (handles `std::vector<std::string>` correctly)

- `parseCHeader(headerPath, prefix, metadataPath, cHeaderInclude, err)` — parses plain C header + metadata.json into `CHeaderParseResult`
  - Strips block comments, skips preprocessor and `extern "C"` lines
  - Matches `rettype prefix_methodname(params);` declarations
  - Maps C types (`int64_t`, `char*`, `const char*`, etc.) to LIDL TypeExpr
  - Detects `{prefix}free_string` and stores it in `freeStringFunc`; excludes it from exposed methods
  - Renames reserved names: `version` → `libVersion`, `name` → `libName`
  - Returns `CHeaderParseResult` carrying `ModuleDecl` + per-method `CHeaderMethod` entries
- `defaultPrefixFromModuleName(moduleName)` — auto-derives prefix: `"rust_example_module"` → `"rust_example_"`

## CLI Usage

### From C header — c-ffi mode (zero hand-written C++)

```bash
logos-cpp-generator --from-c-header rust-lib/include/rust_example.h \
    --metadata metadata.json \
    --backend qt \
    --c-header-include rust_example.h \
    --output-dir ./generated_code \
    [--prefix rust_example_]
```

Flags:
- `--from-c-header <path>` — the C header to parse
- `--metadata <path>` — module name, version, description (required)
- `--backend qt` — only `qt` supported
- `--c-header-include <name>` — the `#include` string embedded in generated code (defaults to header filename)
- `--prefix <prefix>` — override auto-derived prefix (default: `{moduleName_without_module}_`)

Generates: `<name>_qt_glue.h`, `<name>_dispatch.cpp`

### From C++ impl header — universal mode

```bash
logos-cpp-generator --from-header src/my_module_impl.h \
    --backend qt \
    --impl-class MyModuleImpl \
    --impl-header my_module_impl.h \
    --metadata metadata.json \
    --output-dir ./generated_code
```

Generates: `<name>_qt_glue.h`, `<name>_dispatch.cpp`

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
| `test_impl_header_parser.cpp` | C++ header parsing: type mapping, access specifiers, skipping private/protected, error cases |
| `test_lidl_gen_provider.cpp` (c-ffi) | C-FFI provider generation via `lidlMakeProviderHeaderCFFI`: direct C calls, string ownership, free function wiring |

Fixture files in `tests/experimental/fixtures/`:
- `sample_impl.h` — module with all supported type variations
- `sample_metadata.json` — metadata with dependencies
- `complex_impl.h` — module with multiple access specifier sections
- `empty_class_impl.h` — class with no public methods
- `empty_metadata.json` — minimal metadata
- `universal_impl.h` — c-ffi fixture: C header with mixed types, heap/static strings, and free function

## Known Limitations

- The impl header parser (`--from-header`) is lightweight (regex + state machine). It does not handle:
  - Multi-line method declarations
  - Default parameter values
  - Method definitions in the header (only declarations ending with `;`)
  - Nested classes
  - Template methods
  - `std::function` members other than `emitEvent` are silently skipped
- The C header parser (`--from-c-header`) does not handle:
  - Multi-line function declarations
  - Macros that expand to function declarations
  - Function pointer typedefs
  - Variadic functions (`...`)
  - C++ templates (not valid C anyway)
- LIDL does not support generic/parameterized types or inheritance
- Only the `qt` backend is implemented for `--from-header` and `--from-c-header`; future backends (CBOR, Rust) are planned
- Client stub generation (`lidlMakeHeader`/`lidlMakeSource`) is only available from LIDL files, not from `--from-header` or `--from-c-header`
- C-FFI modules do not support events (no `emitEvent` mechanism); use `universal` mode if you need events
