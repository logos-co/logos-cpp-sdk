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
