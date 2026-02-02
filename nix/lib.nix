# Builds the logos-cpp-sdk library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta;
  
  # Skip default configure phase since we do it in buildPhase
  dontUseCmakeConfigure = true;
  
  buildPhase = ''
    runHook preBuild
    
    # Build SDK library
    mkdir -p build-sdk
    cd build-sdk
    cmake ../src -GNinja $cmakeFlags
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
    
    runHook postInstall
  '';
}

