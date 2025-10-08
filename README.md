# logos-cpp-sdk

## How to Build

### Using Nix (Recommended)

The project includes a Nix flake for reproducible builds:

```bash
# Build the SDK
nix build '.#logos-cpp-sdk'

# Build the code generator
nix build '.#cpp-generator'

# Build both (default)
nix build

# Enter development shell with all dependencies
nix develop
```

**Note:** In zsh, you need to quote the target (e.g., `'.#logos-cpp-sdk'`) to prevent glob expansion.

If you don't have flakes enabled globally, add experimental flags:

```bash
nix build '.#logos-cpp-sdk' --extra-experimental-features 'nix-command flakes'
```

The compiled library can be found at `result/`

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
