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
      packages = forAllSystems ({ pkgs }: 
        let
          sdk = pkgs.stdenv.mkDerivation rec {
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
              description = "Logos C++ SDK Library";
              platforms = platforms.unix;
            };
          };

          generator = pkgs.stdenv.mkDerivation rec {
            pname = "logos-cpp-generator";
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
            ];
            
            cmakeFlags = [ "-GNinja" ];
            
            configurePhase = ''
              cd cpp-generator
              cmakeConfigurePhase
            '';
            
            installPhase = ''
              mkdir -p $out/bin
              cp bin/logos-cpp-generator $out/bin/
            '';
            
            meta = with pkgs.lib; {
              description = "Logos C++ Code Generator";
              platforms = platforms.unix;
            };
          };
        in
        {
          logos-cpp-sdk = sdk;
          cpp-generator = generator;
          default = pkgs.symlinkJoin {
            name = "logos-cpp-sdk-with-generator";
            paths = [ sdk generator ];
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
          ];
        };
      });
    };
}
