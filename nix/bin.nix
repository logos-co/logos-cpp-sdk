# Builds the logos-cpp-generator and logos-native-generator binaries
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-generator";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta;
  
  # Skip default configure phase since we do it in buildPhase
  dontUseCmakeConfigure = true;
  
  buildPhase = ''
    runHook preBuild
    
    # Build Qt-based generator
    mkdir -p build-generator
    cd build-generator
    cmake ../cpp-generator -GNinja $cmakeFlags
    ninja
    cd ..

    # Build native generator (no Qt dependency)
    mkdir -p build-native-generator
    cd build-native-generator
    cmake ../native-generator -GNinja
    ninja
    cd ..
    
    runHook postBuild
  '';
  
  installPhase = ''
    runHook preInstall
    
    # Install generator binaries
    mkdir -p $out/bin
    if [ -f build-generator/bin/logos-cpp-generator ]; then
      cp build-generator/bin/logos-cpp-generator $out/bin/
    fi
    if [ -f build-native-generator/logos-native-generator ]; then
      cp build-native-generator/logos-native-generator $out/bin/
    fi
    
    runHook postInstall
  '';
}

