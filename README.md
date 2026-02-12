# logos-cpp-sdk

## How to Build

### Using Nix (Recommended)

The project includes a Nix flake for reproducible builds with a modular structure:

#### Build Complete SDK (Library + Headers + Generator)

```bash
# Build everything (default)
nix build

# Or explicitly
nix build '.#logos-cpp-sdk'
nix build '.#default'
```

The result will include:
- `/bin/logos-cpp-generator` - Code generator binary
- `/lib/` - SDK libraries
- `/include/` - Headers (core/ and cpp/)

#### Build Individual Components

```bash
# Build only the generator binary (outputs to /bin)
nix build '.#logos-cpp-bin'

# Build only the SDK library (outputs to /lib)
nix build '.#logos-cpp-lib'

# Build only the headers (outputs to /include)
nix build '.#logos-cpp-include'

# Legacy alias for generator
nix build '.#cpp-generator'
```

#### Development Shell

```bash
# Enter development shell with all dependencies
nix develop
```

**Note:** In zsh, you need to quote the target (e.g., `'.#logos-cpp-sdk'`) to prevent glob expansion.

If you don't have flakes enabled globally, add experimental flags:

```bash
nix build '.#logos-cpp-sdk' --extra-experimental-features 'nix-command flakes'
```

The compiled artifacts can be found at `result/`

#### Modular Architecture

The nix build system is organized into modular files in the `/nix` directory:
- `nix/default.nix` - Common configuration (dependencies, flags, metadata)
- `nix/bin.nix` - Generator binary compilation
- `nix/lib.nix` - SDK library compilation
- `nix/include.nix` - Header installation

### Manual Build

#### Building the C++ SDK

```bash
cd cpp
./compile.sh
```

#### Building the Code Generator

```bash
cd cpp-generator
./compile.sh
```

The generator binary will be available at `build/bin/logos-cpp-generator`.

## Usage

### Code Generator

The `logos-cpp-generator` tool generates C++ wrapper code for Logos plugins.

#### Basic Usage

```bash
# Generate wrapper for a single plugin (uses default output directory)
logos-cpp-generator /path/to/plugin.dylib

# Specify custom output directory
logos-cpp-generator /path/to/plugin.dylib --output-dir /custom/output/path

# Generate only the module files (no core manager or umbrella headers)
logos-cpp-generator /path/to/plugin.dylib --module-only

# Combine options
logos-cpp-generator /path/to/plugin.dylib --output-dir /custom/output --module-only
```

#### Generate from Metadata

```bash
# List dependencies from metadata.json
logos-cpp-generator --metadata /path/to/metadata.json

# Generate wrappers for all dependencies
logos-cpp-generator --metadata /path/to/metadata.json --module-dir /path/to/modules

# Generate with custom output directory
logos-cpp-generator --metadata /path/to/metadata.json --module-dir /path/to/modules --output-dir /custom/output

# Generate only module files (no core manager or umbrella headers)
logos-cpp-generator --metadata /path/to/metadata.json --module-dir /path/to/modules --module-only

# Generate only core manager and umbrella files (assumes module files already exist)
logos-cpp-generator --metadata /path/to/metadata.json --general-only

# Generate general files with custom output directory
logos-cpp-generator --metadata /path/to/metadata.json --general-only --output-dir /custom/output
```

#### Options

**`--output-dir /path/to/output`**
- **Default:** If not specified, generated files are placed in `logos-cpp-sdk/cpp/generated/`
- **Custom:** Specify any directory for the generated files
- The output directory will be created automatically if it doesn't exist

**`--module-only`**
- When specified, generates only the requested module's `.h` and `.cpp` files
- Skips generation of `core_manager_api.*` and umbrella headers (`logos_sdk.*`)
- Useful when you only need wrapper code for specific modules

**`--general-only`**
- When specified with `--metadata`, generates only the core manager and umbrella SDK files
- Assumes module wrapper files already exist in the output directory
- Generates: `core_manager_api.h`, `core_manager_api.cpp`, `logos_sdk.h`, `logos_sdk.cpp`
- The umbrella headers will include references to all modules listed in the metadata's `dependencies` array
- For each dependency (e.g., `"waku_module"`), it will:
  - Include `waku_module_api.h` in the header
  - Include `waku_module_api.cpp` in the source
  - Create a `WakuModule waku_module;` member in the `LogosModules` struct
- Does not require `--module-dir` since it doesn't process plugins

#### Generated Files

**By default (without `--module-only` or `--general-only`):**
- `<module>_api.h` and `<module>_api.cpp` - Wrapper code for each module
- `core_manager_api.h` and `core_manager_api.cpp` - Core manager wrapper
- `logos_sdk.h` and `logos_sdk.cpp` - Umbrella headers including all modules

**With `--module-only`:**
- Only `<module>_api.h` and `<module>_api.cpp` - Wrapper code for the requested module(s)

**With `--general-only`:**
- Only `core_manager_api.h` and `core_manager_api.cpp` - Core manager wrapper
- Only `logos_sdk.h` and `logos_sdk.cpp` - Umbrella headers that reference existing module files

#### Typical Workflow

A common workflow is to generate module wrappers separately, then generate the umbrella SDK:

```bash
# Step 1: Generate individual module wrappers
logos-cpp-generator /path/to/plugin1.dylib --module-only --output-dir ./generated
logos-cpp-generator /path/to/plugin2.dylib --module-only --output-dir ./generated

# Step 2: Generate core manager and umbrella SDK (references modules from step 1)
logos-cpp-generator --metadata metadata.json --general-only --output-dir ./generated
```

This approach gives you fine-grained control over which modules to include and allows rebuilding just the umbrella headers without regenerating all module wrappers.

### API

#### LogosResult

`LogosResult` provides a structured way to return either a value or an error from synchronous method calls.

If the `success` attribute is `true`, you can retrieve the value using a cast. Otherwise, retrieve the error which should be a string (though not enforced).

### Example

```cpp
LogosResult result = m_logos->my_module.someMethod();
if (result.success) {
    // Use shorthand
    QString value = result.getString();
    // Or
    QString value = result.getValue<QString>();
    // Or
    QString value = result.value.value<QString>();
} else {
    // Use shorthand
    QString error = result.error();
    // Or
    QString error = result.getString();
    // Or
    QString error = result.getValue<QString>();
    // Or
    QString error = result.value.value<QString>();
}
```

#### Complex objects

Let's say you need to return a complex object. In the SDK, you have to build your type with primitive like QVariantMap:

```cpp
// Received JSON: {"cid": "QmXyz...", "filename": "photo.jpg", "size": 2048576, "mimetype": "image/jpeg"}

QVariantMap manifest;
manifest["cid"] = "QmXyz...";
manifest["filename"] = "photo.jpg";
manifest["size"] = 2048576;
manifest["mimetype"] = "image/jpeg";
return {true, manifest};
```

And then to consume by using the shorthand function:

```cpp
LogosResult result = m_logos->my_plugin.someMethod(cid);
if (result.success) {
  QString cid = result.getString("cid");
  // You can define a default value as well
  QString cid = result.getString("cid", "unknown");
}
```

Or you can use the value directly:

```cpp
LogosResult result = m_logos->my_plugin.someMethod(cid);
if (result.success) {
    QVariantMap manifest = result.getMap();
    QString cid = manifest["cid"].toString();
}
```

Same thing for a list, you can use `QVariantList`:

```cpp
QVariantList manifests;

QVariantMap m1;
m1["cid"] = "QmAbc...";
m1["filename"] = "document.pdf";
m1["size"] = 1024000;
manifests.append(m1);

QVariantMap m2;
m2["cid"] = "QmDef...";
m2["filename"] = "image.png";
m2["size"] = 512000;
manifests.append(m2);

return {true, manifests};
```

To consume it using the shorthand function:

```cpp
LogosResult result = m_logos->my_plugin.someMethod();
if (result.success) {
    for (int i = 0; i < list.size(); ++i) {
        QString cid = result.getString(i, "cid");
        // You can define a default value as well
        QString cid = result.getString(0, "cid", "unknown");
    }
}
```

Or you can use the value directly:

```cpp
LogosResult result = m_logos->my_plugin.someMethod();
if (result.success) {
    QVariantList list = result.getList();
    for (const QVariant& item : list) {
        QVariantMap manifest = item.toMap();
        QString cid = manifest["cid"].toString();
    }
}
```

### Requirements

#### Build Tools
- CMake (3.x or later)
- Ninja build system
- pkg-config

#### Dependencies
- Qt6 (qtbase)
- Qt6 Remote Objects (qtremoteobjects)

## Supported Platforms

- macOS (aarch64-darwin, x86_64-darwin)
- Linux (aarch64-linux, x86_64-linux)
