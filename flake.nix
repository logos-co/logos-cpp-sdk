{
  description = "Logos C++ SDK";

  inputs.logos-nix.url = "github:logos-co/logos-nix";
  inputs.nixpkgs.follows = "logos-nix/nixpkgs";
  # The protocol layer (transports, token exchange, lp_* C ABI). Follows our
  # logos-nix so both repos resolve the identical nixpkgs/Qt pin — the QRO
  # wire is Qt-version-sensitive.
  #
  # TEMPORARILY REV-PINNED to logos-protocol#66 (feat/module-impl-abi-manifest).
  # That branch publishes packages.<system>.module-impl-abi: the module-impl C
  # ABI export list, the protocol version the list belongs to, and the diff
  # helper. nix/tests-module-impl-abi.nix reads all three, and master does not
  # carry them yet, so unpinned this repo cannot even evaluate that check.
  # TODO: drop the rev, back to plain "github:logos-co/logos-protocol", once
  # logos-protocol#66 merges. Nothing else here depends on the rev — the
  # earlier pin to feat/per-client-token-store was dropped when #59 merged.
  inputs.logos-protocol.url = "github:logos-co/logos-protocol/986813cc661682878c3ecabff2078a6d36cd5c1d";
  inputs.logos-protocol.inputs.logos-nix.follows = "logos-nix";
  # The canonical, language-neutral LIDL frontend (lexer/parser/AST/serializer/
  # validator) the code generator links. Follows our logos-nix so it resolves
  # the identical nixpkgs pin.
  inputs.logos-lidl.url = "github:logos-co/logos-lidl";
  inputs.logos-lidl.inputs.logos-nix.follows = "logos-nix";

  outputs = { self, nixpkgs, logos-nix, logos-protocol, logos-lidl }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      # Adds the "x86_64-windows" pseudo-system; a cross derivation's `system`
      # is its BUILD platform, so it evaluates anywhere and realises on Linux.
      forAllTargets = logos-nix.lib.forAllTargets;

      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      packages = forAllTargets ({ pkgs, ... }: 
        let
          # Common configuration
          common = import ./nix/default.nix { inherit pkgs; };
          src = ./.;
          
          # Individual package components
          bin = import ./nix/bin.nix { inherit pkgs common src logos-protocol; logos-lidl = logos-lidl.packages.${pkgs.system}.logos-lidl; };
          lib = import ./nix/lib.nix { inherit pkgs common src logos-protocol; };
          include = import ./nix/include.nix { inherit pkgs common src logos-protocol; };
          tests = import ./nix/tests.nix { inherit pkgs common src logos-protocol; logos-lidl = logos-lidl.packages.${pkgs.system}.logos-lidl; };
          
          # Combined SDK package. We re-declare propagatedBuildInputs on
          # the join so downstream Nix derivations that depend on the
          # combined `sdk` (rather than the nested `lib`) still inherit
          # OpenSSL / Boost / nlohmann_json — symlinkJoin doesn't
          # forward propagation from its `paths` attribute. Qt is
          # excluded for the same setup-hook ordering reason as in
          # `nix/lib.nix`; consumers must list qt6.qtbase +
          # qt6.wrapQtAppsNoGuiHook themselves.
          sdk = pkgs.symlinkJoin {
            name = "logos-cpp-sdk";
            paths = [ bin lib include ];
            propagatedBuildInputs = common.propagatedBuildInputs;
          };
        in
        {
          # Individual outputs
          logos-cpp-bin = bin;
          logos-cpp-lib = lib;
          logos-cpp-include = include;
          inherit tests;
          
          # Combined outputs (for backward compatibility)
          logos-cpp-sdk = sdk;
          cpp-generator = bin;  # Alias for backward compatibility
          
          # Default package
          default = sdk;
        }
      );

      checks = forAllSystems ({ pkgs }:
        let
          common = import ./nix/default.nix { inherit pkgs; };
          src = ./.;
          tests = import ./nix/tests.nix { inherit pkgs common src logos-protocol; logos-lidl = logos-lidl.packages.${pkgs.system}.logos-lidl; };
          generator = import ./nix/bin.nix { inherit pkgs common src logos-protocol; logos-lidl = logos-lidl.packages.${pkgs.system}.logos-lidl; };
        in
        {
          inherit tests;
          # Runs the BINARY. The gtest suite links the generator's internals and
          # never executes it, so a retired CLI flag can only be asserted here.
          generator-cli = import ./nix/tests-generator-cli.nix {
            inherit pkgs common generator;
          };
          # Diffs what the cdylib backend DEFINES against the module-impl C
          # ABI logos-protocol DECLARES. Nothing else here can catch that gap:
          # a module with a missing export links clean and only dies at
          # dlopen(), on Linux. See nix/tests-module-impl-abi.nix.
          module-impl-abi = import ./nix/tests-module-impl-abi.nix {
            inherit pkgs common src generator;
            module-impl-abi = logos-protocol.packages.${pkgs.system}.module-impl-abi;
          };
        }
      );

      devShells = forAllSystems ({ pkgs }: {
        default = pkgs.mkShell {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtremoteobjects
            pkgs.gtest
            pkgs.boost
            pkgs.openssl
            pkgs.nlohmann_json
          ];
        };
      });
    };
}
