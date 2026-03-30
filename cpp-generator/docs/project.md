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
- **`MethodDecl`** — method name, params, return type
- **`EventDecl`** — event name + params
- **`FieldDecl`** — struct field name, type, optional flag
- **`TypeDecl`** — named struct type with fields
- **`ModuleDecl`** — complete module: name, version, description, category, depends, types, methods, events

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
- `lidlMakeProviderHeader(ModuleDecl, implClass, implHeader)` — generates Qt glue header
- `lidlMakeProviderDispatch(ModuleDecl)` — generates callMethod/getMethods dispatch
- `lidlGenerateProviderGlue(lidlPath, ...)` — full pipeline from .lidl file

### Impl Header Parser (`impl_header_parser.h/cpp`)

- `parseImplHeader(headerPath, className, metadataPath, err)` — parses C++ header + metadata.json into ModuleDecl
- State machine: `LookingForClass` → `InClass` → `InPublic`/`InPrivate`
- Skips: constructors, destructors, typedefs, using, friend, enum, struct declarations
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
- LIDL does not support generic/parameterized types or inheritance
- Only the `qt` backend is implemented for `--from-header`; future backends (CBOR, Rust) are planned
- Client stub generation (`lidlMakeHeader`/`lidlMakeSource`) is only available from LIDL files, not from `--from-header`
