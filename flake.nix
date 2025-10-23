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
            
            # Skip default configure phase since we do it in buildPhase
            dontUseCmakeConfigure = true;
            
            # Build both SDK library and generator
            buildPhase = ''
              runHook preBuild
              
              # Build SDK
              mkdir -p build-sdk
              cd build-sdk
              cmake ../cpp -GNinja $cmakeFlags
              ninja
              cd ..
              
              # Build generator
              mkdir -p build-generator
              cd build-generator
              cmake ../cpp-generator -GNinja $cmakeFlags
              ninja
              cd ..
              
              runHook postBuild
            '';
            
            installPhase = ''
              runHook preInstall
              
              # Install SDK library
              mkdir -p $out/lib
              if [ -d build-sdk/lib ]; then
                cp -r build-sdk/lib/* $out/lib/ || true
              fi
              
              # Install headers with proper structure
              mkdir -p $out/include/core
              mkdir -p $out/include/cpp
              
              # Install core headers
              if [ -f core/interface.h ]; then
                cp core/interface.h $out/include/core/
              fi
              
              # Install cpp headers and sources
              for file in logos_api.cpp logos_api.h logos_api_client.cpp logos_api_client.h \
                          logos_api_consumer.cpp logos_api_consumer.h logos_api_provider.cpp logos_api_provider.h \
                          token_manager.cpp token_manager.h module_proxy.cpp module_proxy.h; do
                if [ -f cpp/$file ]; then
                  cp cpp/$file $out/include/cpp/
                fi
              done
              
              # Install generator binary
              mkdir -p $out/bin
              if [ -f build-generator/bin/logos-cpp-generator ]; then
                cp build-generator/bin/logos-cpp-generator $out/bin/
              fi
              
              runHook postInstall
            '';
            
            meta = with pkgs.lib; {
              description = "Logos C++ SDK Library and Code Generator";
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
