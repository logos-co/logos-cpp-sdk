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
    mkdir -p $out/include/cpp
    
    # Install core headers
    if [ -f core/interface.h ]; then
      cp core/interface.h $out/include/core/
    fi
    
    # Install cpp headers and sources
    for file in logos_types.cpp logos_types.h logos_api.cpp logos_api.h logos_api_client.cpp logos_api_client.h \
                logos_api_consumer.cpp logos_api_consumer.h logos_api_provider.cpp logos_api_provider.h \
                token_manager.cpp token_manager.h module_proxy.cpp module_proxy.h logos_mode.h; do
      if [ -f cpp/$file ]; then
        cp cpp/$file $out/include/cpp/
      fi
    done
    
    if [ -f cpp/logos_mode.h ]; then
      cp cpp/logos_mode.h $out/include/
    fi
    
    runHook postInstall
  '';
}

