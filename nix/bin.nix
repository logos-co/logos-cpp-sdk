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

    # LIDL frontend sources for external generators (logos-qt-sdk's
    # logos-qt-generator compiles these in — source-level sharing, no
    # binary ABI between the two generators).
    mkdir -p $out/share/lidl-frontend
    cp cpp-generator/experimental/lidl_ast.h \
       cpp-generator/experimental/lidl_lexer.h cpp-generator/experimental/lidl_lexer.cpp \
       cpp-generator/experimental/lidl_parser.h cpp-generator/experimental/lidl_parser.cpp \
       cpp-generator/experimental/lidl_serializer.h cpp-generator/experimental/lidl_serializer.cpp \
       cpp-generator/experimental/lidl_validator.h cpp-generator/experimental/lidl_validator.cpp \
       cpp-generator/experimental/impl_header_parser.h cpp-generator/experimental/impl_header_parser.cpp \
       cpp-generator/experimental/lidl_emit_common.h cpp-generator/experimental/lidl_emit_common.cpp \
       $out/share/lidl-frontend/
    
    runHook postInstall
  '';
}

