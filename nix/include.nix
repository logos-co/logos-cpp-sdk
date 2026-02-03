# Installs the logos-cpp-sdk headers
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-headers";
  version = common.version;
  
  inherit src;
  inherit (common) meta;
  
  # No build phase needed, just install headers
  dontBuild = true;
  dontConfigure = true;
  
  installPhase = ''
    runHook preInstall
    
    # BACKWARD COMPATIBILITY:
    # The installed include structure uses cpp/ and core/ directories to remain
    # compatible with logos-module-builder and other downstream projects that
    # expect the original layout. The internal source is organized differently
    # (src/client/, src/provider/) but the installed output maintains the old paths.
    
    mkdir -p $out/include
    mkdir -p $out/include/cpp
    mkdir -p $out/include/core
    
    # Install all SDK source files into include/cpp/
    # From src/ root
    for file in logos_api.cpp logos_api.h token_manager.cpp token_manager.h logos_mode.h plugin_registry.h; do
      if [ -f src/$file ]; then
        cp src/$file $out/include/cpp/
      fi
    done
    
    # From src/client/
    for file in logos_api_client.cpp logos_api_client.h logos_api_consumer.cpp logos_api_consumer.h; do
      if [ -f src/client/$file ]; then
        cp src/client/$file $out/include/cpp/
      fi
    done
    
    # From src/provider/
    for file in logos_api_provider.cpp logos_api_provider.h module_proxy.cpp module_proxy.h; do
      if [ -f src/provider/$file ]; then
        cp src/provider/$file $out/include/cpp/
      fi
    done
    
    # Install core headers (interface.h from provider/core/ goes to include/core/)
    if [ -f src/provider/core/interface.h ]; then
      cp src/provider/core/interface.h $out/include/core/
    fi
    
    # Also copy logos_mode.h to root include for backward compatibility
    if [ -f src/logos_mode.h ]; then
      cp src/logos_mode.h $out/include/
    fi
    
    runHook postInstall
  '';
}
