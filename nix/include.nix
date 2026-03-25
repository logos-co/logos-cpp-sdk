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
    mkdir -p $out/include/cpp/implementations/qt_local
    mkdir -p $out/include/cpp/implementations/qt_remote
    mkdir -p $out/include/cpp/implementations/mock
    
    # Install core headers
    if [ -f core/interface.h ]; then
      cp core/interface.h $out/include/core/
    fi
    
    # Install cpp headers and sources
    for file in logos_types.cpp logos_types.h logos_api.cpp logos_api.h logos_api_client.cpp logos_api_client.h \
                logos_api_consumer.cpp logos_api_consumer.h logos_api_provider.cpp logos_api_provider.h \
                token_manager.cpp token_manager.h module_proxy.cpp module_proxy.h logos_mode.h \
                logos_instance.h logos_object.h \
                logos_provider_object.h logos_provider_object.cpp \
                qt_provider_object.h qt_provider_object.cpp \
                logos_transport.h logos_transport_factory.h logos_transport_factory.cpp \
                logos_registry.h logos_registry_factory.h logos_registry_factory.cpp \
                plugin_registry.h; do
      if [ -f cpp/$file ]; then
        cp cpp/$file $out/include/cpp/
      fi
    done
    
    # Install transport implementation headers and sources
    for file in local_transport.h local_transport.cpp; do
      if [ -f cpp/implementations/qt_local/$file ]; then
        cp cpp/implementations/qt_local/$file $out/include/cpp/implementations/qt_local/
      fi
    done

    for file in remote_transport.h remote_transport.cpp qt_remote_registry.h qt_remote_registry.cpp; do
      if [ -f cpp/implementations/qt_remote/$file ]; then
        cp cpp/implementations/qt_remote/$file $out/include/cpp/implementations/qt_remote/
      fi
    done

    for file in mock_store.h mock_store.cpp mock_transport.h mock_transport.cpp mock_registry.h logos_mock.h; do
      if [ -f cpp/implementations/mock/$file ]; then
        cp cpp/implementations/mock/$file $out/include/cpp/implementations/mock/
      fi
    done
    
    if [ -f cpp/logos_mode.h ]; then
      cp cpp/logos_mode.h $out/include/
    fi
    
    runHook postInstall
  '';
}
