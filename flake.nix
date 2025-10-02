{
  description = "Logos C++ SDK";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      packages = forAllSystems ({ pkgs }: {
        default = pkgs.stdenv.mkDerivation rec {
          pname = "logos-cpp-sdk";
          version = "0.1.0";
          
          src = ./.;
          
          
          nativeBuildInputs = [ 
            pkgs.cmake 
            pkgs.ninja 
            pkgs.pkg-config
            pkgs.qt6.wrapQtAppsNoGuiHook
          ];
          buildInputs = [ 
            pkgs.qt6.qtbase 
            pkgs.qt6.qtremoteobjects 
          ];
          
          
          cmakeFlags = [ "-GNinja" ];
          
          configurePhase = ''
            cd cpp
            cmakeConfigurePhase
          '';
          
          meta = with pkgs.lib; {
            description = "Logos C++ SDK";
            platforms = platforms.unix;
          };
        };
      });

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
          ];
        };
      });
    };
}
