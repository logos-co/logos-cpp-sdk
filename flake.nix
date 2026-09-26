{
  description = "Logos C++ SDK";

  inputs.logos-nix.url = "github:logos-co/logos-nix";
  inputs.nixpkgs.follows = "logos-nix/nixpkgs";
  # The protocol layer (transports, token exchange, lp_* C ABI). Follows our
  # logos-nix so both repos resolve the identical nixpkgs/Qt pin — the QRO
  # wire is Qt-version-sensitive.
  #
  # Master-tracking. This was rev-pinned to feat/per-client-token-store while
  # logos_host_services.h's trust-root surface (lp_token_keys,
  # lp_inform_module_token_to, lp_grant_host_services) lived only on that
  # branch, with protocol master still at LOGOS_PROTOCOL_VERSION_MINOR 2 —
  # the `tests` check could not compile against it. That branch has merged
  # (logos-protocol#59): master is 0.4.0 and carries all three.
  #
  # checks.module-impl-abi additionally consumes
  # packages.<system>.module-impl-abi from here (logos-protocol#66).
  #
  # On protocol 0.14's branch (logos-protocol#99, on #98 and #97) until they
  # merge; back to master then.
  inputs.logos-protocol.url = "github:logos-co/logos-protocol/feat/peering";
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

      # Typed Qt-free clients for an app: `<name>_api.{h,cpp}` per contract, no
      # umbrella. `lidls` maps a module name to its .lidl, or to a directory
      # holding `<name>.lidl` (a module's packages.<sys>.lidl). The app compiles
      # them and links the plain protocol image liblogos links.
      lib.mkClients = { system, lidls, typedCollections ? false }:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          flag = nixpkgs.lib.optionalString typedCollections "--typed-collections";
        in
        pkgs.runCommand "logos-cpp-clients" {
          nativeBuildInputs = [ self.packages.${system}.logos-cpp-bin ];
        } ''
          mkdir -p $out
          ${nixpkgs.lib.concatMapStrings (name: ''
            src=${lidls.${name}}
            if [ -d "$src" ]; then src="$src/${name}.lidl"; fi
            logos-cpp-generator --lidl "$src" --api-style lp ${flag} --output-dir $out
            if [ ! -f $out/${name}_api.h ]; then
              echo "mkClients: $src does not declare module ${name}" >&2
              exit 1
            fi
          '') (builtins.attrNames lidls)}
        '';

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
          # logos_generate_clients() from the installed package, as an app uses it.
          clients-cmake = import ./nix/tests-clients-cmake.nix {
            inherit pkgs common src;
            sdk = self.packages.${pkgs.system}.logos-cpp-sdk;
            protocolPlain = logos-protocol.packages.${pkgs.system}.logos-protocol-plain;
          };
          # lib.mkClients over tests/clients' fixture, from a file and from a
          # directory, with and without typed collections.
          mk-clients =
            let
              probe = ./tests/clients/typed_probe.lidl;
              mk = args: self.lib.mkClients ({ system = pkgs.system; } // args);
            in
            import ./nix/tests-mk-clients.nix {
              inherit pkgs common;
              plain = mk { lidls.typed_probe = probe; };
              typed = mk { lidls.typed_probe = probe; typedCollections = true; };
              fromDir = mk {
                lidls.typed_probe = pkgs.runCommand "typed-probe-lidl" { } ''
                  mkdir -p $out && cp ${probe} $out/typed_probe.lidl
                '';
              };
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
