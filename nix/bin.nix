# Builds the logos-cpp-generator binary
{ pkgs, common, src, logos-protocol }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-generator";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta;
  
  # Skip default configure phase since we do it in buildPhase
  dontUseCmakeConfigure = true;
  
  buildPhase = ''
    runHook preBuild
    
    # Build generator
    mkdir -p build-generator
    cd build-generator
    cmake ../cpp-generator -GNinja -DLOGOS_PROTOCOL_ROOT=${logos-protocol} $cmakeFlags
    ninja
    cd ..
    
    runHook postBuild
  '';
  
  installPhase = ''
    runHook preInstall
    
    # Install generator binary
    mkdir -p $out/bin
    if [ -f build-generator/bin/logos-cpp-generator ]; then
      cp build-generator/bin/logos-cpp-generator $out/bin/
    fi
    
    runHook postInstall
  '';
}

