# Running a Real Module Against This C++ SDK

`logos-cpp-sdk` is the foundation every other Logos component is built on: it
pins nixpkgs/Qt and ships the SDK that `logos-liblogos` (`logos_host`,
`liblogos_core`), the module client, and every module compile and link against
— `LogosAPI`, `LogosResult`, the IPC layer, and the code generator. A change
here ripples through the entire stack, so the way to know it is safe is to run
a real module on top of it. This doc-test does exactly that, end-to-end through
the headless `logosctl` runtime:

1. Build the `logosctl` CLI, **overriding `logos-cpp-sdk` with the commit
   under test** — and overriding it in the same way for every consumer in
   the runtime (`logos-liblogos` and the `capability_module`'s
   `logos-module-builder`). Because the published flakes pin the SDK
   independently (there is no single `follows` unifying them), all of these
   must point at the same commit so the whole runtime is built and linked
   against one consistent SDK ABI. Each `logos-qt-sdk` dragged onto the new
   SDK also gets its sibling `logos-lidl` moved with it — see the note in
   the first section.
2. Build the real [`accounts_module`](https://github.com/logos-co/logos-accounts-module)
   as an `.lgx` package straight from its own flake — **also built against the
   SDK commit under test**.
3. Start a `logosctl` daemon and install the `.lgx` into its session.
4. Load `accounts_module`, introspect it, and call its methods — verifying a module compiled against this SDK
   actually loads and round-trips real values over the SDK's IPC.

Because every layer (host, module loader, IPC, and the module itself) is built
against the SDK commit under test, a green run is real evidence that this
change keeps the module runtime working from the bottom of the stack up.

**What you'll build:** The real `accounts_module`, installed into a `logosctl` session and called through its daemon — every layer compiled against this C++ SDK commit.

**What you'll learn:**

- How to build `logosctl` against a specific `logos-cpp-sdk` commit by overriding it across every consumer in the runtime
- Why a foundational input pinned independently by several flakes needs the override applied at each consumer, not just once
- How to build a real module's `.lgx` against the same SDK commit
- How to install an `.lgx` into a `logosctl` session
- How to start the `logosctl` daemon, load a module, and call its methods

## Prerequisites

- **Nix** with flakes enabled. Install from [nixos.org](https://nixos.org/download.html), then enable flakes:

```bash
mkdir -p ~/.config/nix
echo 'experimental-features = nix-command flakes' >> ~/.config/nix/nix.conf
```

Verify: `nix flake --help >/dev/null 2>&1 && echo "Flakes enabled"`

- **git** — to clone the module repository.
- A Linux or macOS machine.

---

## Step 1: Build logosctl against this C++ SDK

Build the `logosctl` CLI from its published flake, but **override
`logos-cpp-sdk` to the commit under test** — and apply the same override to
every consumer in the runtime that pins the SDK. The result is symlinked to
`./logos/`.

> Unlike a leaf input, the SDK is pinned independently by `logos-liblogos`
> and the `capability_module`'s `logos-module-builder` — there is no single
> `follows` tying them together in the published flakes. So we override it
> at each of those paths (e.g.
> `--override-input logos-liblogos/logos-cpp-sdk …`) to keep the whole
> runtime on one consistent SDK ABI. Each override URL carries a ``
> placeholder the doc-test runner expands to a concrete ref: locally that is
> this checkout's `HEAD` (see `run.sh`); in CI it is the commit being
> tested. With no pin it falls back to latest `master`.

> `logosctl` also bundles the package manager, downloader and storage
> modules. Those keep their own SDK pins: like any installed module, each
> runs in its own process and talks to the runtime over IPC.

> **Why the `logos-qt-sdk` and `logos-qt-sdk/logos-lidl` overrides.** This SDK installs
> `share/lidl-frontend/lidl_compat.h`, and `logos-qt-sdk`'s
> `logos-qt-generator` *compiles* that header against **its own**
> `logos-lidl` input. `logos-lidl` is a **sibling** of `logos-cpp-sdk`
> under `logos-qt-sdk`, not a descendant, so overriding the SDK moves the
> header forward while a locked qt-sdk can keep both its older generator
> and older lidl. Each `logos-qt-sdk` node therefore needs to move to the
> compatible generator revision, with its `logos-lidl` moved alongside it.

### 1.1 Build logosctl with the SDK override

```bash
nix build 'github:logos-co/logos-logoscore-cli#ctl' \
  --override-input logos-cpp-sdk 'github:logos-co/logos-cpp-sdk' \
  --override-input logos-liblogos/logos-cpp-sdk 'github:logos-co/logos-cpp-sdk' \
  --override-input logos-liblogos/logos-qt-sdk 'github:logos-co/logos-qt-sdk' \
  --override-input logos-liblogos/logos-qt-sdk/logos-lidl 'github:logos-co/logos-lidl' \
  --override-input logos-capability-module/logos-module-builder/logos-cpp-sdk 'github:logos-co/logos-cpp-sdk' \
  --override-input logos-capability-module/logos-module-builder/logos-qt-sdk 'github:logos-co/logos-qt-sdk' \
  --override-input logos-capability-module/logos-module-builder/logos-qt-sdk/logos-lidl 'github:logos-co/logos-lidl' \
  --override-input logos-capability-module/logos-module-builder/logos-test-framework/logos-qt-sdk 'github:logos-co/logos-qt-sdk' \
  --override-input logos-capability-module/logos-module-builder/logos-test-framework/logos-qt-sdk/logos-lidl 'github:logos-co/logos-lidl' \
  --out-link ./logos
```

The build produces `logos/bin/logosctl` plus bundled runtime libraries
and a `logos/modules/` directory containing the built-in
`capability_module` (required for the auth handshake when loading
modules). Every overridden input resolves to the same SDK commit, so
`logos_host`, `liblogos_core`, the module client, and the capability
module are all rebuilt and linked against this SDK.

---

## Step 2: Build and install the accounts module against this SDK

Clone [`logos-accounts-module`](https://github.com/logos-co/logos-accounts-module),
build its `.lgx` straight from its flake's `#lgx` output **against the SDK
commit under test**, start a daemon, and install the package into its
session. Building the module against the same SDK as the runtime keeps the
plugin ABI-compatible with the host that will load it. Every module built
with [`logos-module-builder`](https://github.com/logos-co/logos-module-builder)
exposes a ready-to-install `#lgx`, and the builder owns the module's SDK
pin — so the override path here is
`logos-module-builder/logos-cpp-sdk`.

### 2.1 Clone the module

We clone over HTTPS so the step works in CI; over SSH the URL is
`git@github.com:logos-co/logos-accounts-module.git`.

```bash
git clone --depth 1 https://github.com/logos-co/logos-accounts-module.git
```

### 2.2 Build the module's .lgx against this SDK

Build the `#lgx` output, overriding the module builder's `logos-cpp-sdk`
to the commit under test, and link it as `./accounts-lgx`. (This
compiles the module and its SDK dependencies through Nix, so the first
build is slow.)

```bash
# From inside the clone this is simply:
#   nix build '.#lgx' --override-input logos-module-builder/logos-cpp-sdk 'github:logos-co/logos-cpp-sdk'
nix build 'path:./logos-accounts-module#lgx' \
  --override-input logos-module-builder/logos-cpp-sdk 'github:logos-co/logos-cpp-sdk' \
  --override-input logos-module-builder/logos-qt-sdk 'github:logos-co/logos-qt-sdk' \
  --override-input logos-module-builder/logos-qt-sdk/logos-lidl 'github:logos-co/logos-lidl' \
  -o accounts-lgx
```

The `.lgx` package is now under `./accounts-lgx/`:

```bash
ls accounts-lgx/*.lgx
```

### 2.3 Start the daemon

Start a `logosctl` daemon. Its session directory, `./session`, holds the
daemon's config, its installed modules and their data. `--detach`
returns once the daemon accepts commands.

```bash
logosctl daemon start --detach
```

### 2.4 Install the .lgx

A path ending in `.lgx` installs from disk, with no catalog involved. A
local build is unsigned, which the session's default policy accepts with
a warning. `-y` confirms, since there is no terminal to ask on.

```bash
logosctl install accounts-lgx/*.lgx -y
```

### 2.5 Confirm the install

List the session's packages and confirm the module landed:

```bash
logosctl package ls
```

---

## Step 3: Call the module

Load `accounts_module`, introspect it, and call its methods with the client
subcommands. The daemon logs to `session/logs/daemon.log`.

### 3.1 Inspect the startup log

Review the daemon's startup output:

```bash
cat session/logs/daemon.log
```

### 3.2 Check daemon status

Verify the daemon is running:

```bash
logosctl daemon status
```

### 3.3 List discovered modules

`accounts_module` should be visible in the scan directory:

```bash
logosctl module ls
```

### 3.4 Load the module

Load `accounts_module` into the running daemon:

```bash
logosctl module load accounts_module
```

### 3.5 Confirm the module is loaded

Re-run `daemon status`; the module that was `not_loaded` before now reports
`loaded`:

```bash
logosctl daemon status
```

### 3.6 Introspect the module with module show

`module show` lists the `Q_INVOKABLE` methods the module exposes — the
same methods you can `call`. Each one is dispatched over the SDK's IPC
layer:

```bash
logosctl module show accounts_module
```

### 3.7 Call a method

Generate a fresh 12-word BIP-39 mnemonic. `createRandomMnemonic` takes
the word count and returns the phrase — a real round-trip dispatched
over the SDK's IPC layer into the module compiled against this SDK:

```bash
logosctl call accounts_module createRandomMnemonic 12
```

### 3.8 Call a second method

`lengthToEntropyStrength` maps a mnemonic word count to its entropy
strength in bits — 12 words is 128 bits. This exercises an `int`
round-trip through `LogosResult` over the SDK's IPC:

```bash
logosctl call accounts_module lengthToEntropyStrength 12
```

### 3.9 Stop the daemon

Shut the daemon down cleanly:

```bash
logosctl daemon stop
```

The daemon removes its state file and exits.

### 3.10 Confirm the daemon has stopped

With no daemon running, the client reports `not_running` and exits
non-zero, so we add `|| true` to let the doc-test assert on the output:

```bash
logosctl daemon status
```
