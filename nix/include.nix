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
    
    # Install headers with proper structure
    mkdir -p $out/include/core
    mkdir -p $out/include/logos-cpp-sdk
    mkdir -p $out/include/logos-cpp-sdk/client
    mkdir -p $out/include/logos-cpp-sdk/provider
    mkdir -p $out/include/logos-transport
    
    # Install core headers
    if [ -f src/core/interface.h ]; then
      cp src/core/interface.h $out/include/core/
    fi
    
    # Install logos-cpp-sdk root headers and sources
    for file in logos_api.cpp logos_api.h token_manager.cpp token_manager.h; do
      if [ -f src/logos-cpp-sdk/$file ]; then
        cp src/logos-cpp-sdk/$file $out/include/logos-cpp-sdk/
      fi
    done
    
    # Install logos-cpp-sdk client headers and sources
    for file in logos_api_client.cpp logos_api_client.h; do
      if [ -f src/logos-cpp-sdk/client/$file ]; then
        cp src/logos-cpp-sdk/client/$file $out/include/logos-cpp-sdk/client/
      fi
    done
    
    # Install logos-cpp-sdk provider headers and sources
    for file in module_proxy.cpp module_proxy.h; do
      if [ -f src/logos-cpp-sdk/provider/$file ]; then
        cp src/logos-cpp-sdk/provider/$file $out/include/logos-cpp-sdk/provider/
      fi
    done
    
    # Install logos-transport headers and sources
    for file in logos_api_consumer.cpp logos_api_consumer.h logos_api_provider.cpp logos_api_provider.h \
                logos_mode.h plugin_registry.h; do
      if [ -f src/logos-transport/$file ]; then
        cp src/logos-transport/$file $out/include/logos-transport/
      fi
    done
    
    # Also copy logos_mode.h to root include for backward compatibility
    if [ -f src/logos-transport/logos_mode.h ]; then
      cp src/logos-transport/logos_mode.h $out/include/
    fi
    
    runHook postInstall
  '';
}
