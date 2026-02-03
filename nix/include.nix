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
    mkdir -p $out/include
    mkdir -p $out/include/client
    mkdir -p $out/include/provider
    mkdir -p $out/include/provider/core
    
    # Install root headers and sources
    for file in logos_api.cpp logos_api.h token_manager.cpp token_manager.h logos_mode.h plugin_registry.h; do
      if [ -f src/$file ]; then
        cp src/$file $out/include/
      fi
    done
    
    # Install client headers and sources
    for file in logos_api_client.cpp logos_api_client.h logos_api_consumer.cpp logos_api_consumer.h; do
      if [ -f src/client/$file ]; then
        cp src/client/$file $out/include/client/
      fi
    done
    
    # Install provider headers and sources
    for file in logos_api_provider.cpp logos_api_provider.h module_proxy.cpp module_proxy.h; do
      if [ -f src/provider/$file ]; then
        cp src/provider/$file $out/include/provider/
      fi
    done
    
    # Install core headers
    if [ -f src/provider/core/interface.h ]; then
      cp src/provider/core/interface.h $out/include/provider/core/
    fi
    
    runHook postInstall
  '';
}
