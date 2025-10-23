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
```

#### Generate from Metadata

```bash
# List dependencies from metadata.json
logos-cpp-generator --metadata /path/to/metadata.json

# Generate wrappers for all dependencies
logos-cpp-generator --metadata /path/to/metadata.json --module-dir /path/to/modules

# Generate with custom output directory
logos-cpp-generator --metadata /path/to/metadata.json --module-dir /path/to/modules --output-dir /custom/output
```

#### Output Directory

- **Default:** If `--output-dir` is not specified, generated files are placed in `logos-cpp-sdk/cpp/generated/`
- **Custom:** Use `--output-dir` to specify any directory for the generated files
- The output directory will be created automatically if it doesn't exist

Generated files include:
- `<module>_api.h` and `<module>_api.cpp` - Wrapper code for each module
- `core_manager_api.h` and `core_manager_api.cpp` - Core manager wrapper
- `logos_sdk.h` and `logos_sdk.cpp` - Umbrella headers including all modules

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
