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
    #
    # Since the source files use relative includes (e.g., "../logos_mode.h") that
    # work for the internal structure, we use sed to flatten these paths when
    # installing to the flat cpp/ directory.
    
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
    
    # Flatten include paths in cpp/ directory
    # The source uses relative paths like "../logos_mode.h" which need to become "logos_mode.h"
    # in the flat cpp/ directory structure.
    # NOTE: These rewrites are for backward compatibility and may be changed later
    # when downstream projects are updated to use the new include structure.
    for file in $out/include/cpp/*.cpp $out/include/cpp/*.h; do
      if [ -f "$file" ]; then
        sed -i \
          -e 's|#include "../logos_mode.h"|#include "logos_mode.h"|g' \
          -e 's|#include "../logos_api.h"|#include "logos_api.h"|g' \
          -e 's|#include "../token_manager.h"|#include "token_manager.h"|g' \
          -e 's|#include "../plugin_registry.h"|#include "plugin_registry.h"|g' \
          -e 's|#include "../provider/module_proxy.h"|#include "module_proxy.h"|g' \
          -e 's|#include "logos_api_consumer.h"|#include "logos_api_consumer.h"|g' \
          "$file"
          # ^^^ All above rewrites are for backward compatibility, may change later
      fi
    done
    
    # Install core headers (interface.h from provider/core/ goes to include/core/)
    if [ -f src/provider/core/interface.h ]; then
      cp src/provider/core/interface.h $out/include/core/
      # Fix the include path: ../../logos_api.h becomes ../cpp/logos_api.h
      # NOTE: This rewrite is for backward compatibility and may be changed later
      sed -i 's|#include "../../logos_api.h"|#include "../cpp/logos_api.h"|g' $out/include/core/interface.h
    fi
    
    # Also copy logos_mode.h to root include for backward compatibility
    if [ -f src/logos_mode.h ]; then
      cp src/logos_mode.h $out/include/
    fi
    
    runHook postInstall
  '';
}
