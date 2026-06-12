# Installs the logos-cpp-sdk headers
#
# The transport/protocol sources moved to logos-protocol, but this output
# keeps shipping them at the exact same include/cpp/... paths by copying
# them back in from the logos-protocol flake input. That keeps the
# installed SDK artifact layout byte-compatible for every existing
# consumer (logos-plugin-qt's LogosModule.cmake include dirs and source
# layout, hand-rolled IMPORTED archives, etc.) while the source of truth
# lives in the protocol repo.
{ pkgs, common, src, logos-protocol }:

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
    mkdir -p $out/include/cpp/implementations/plain

    # Install core headers
    if [ -f core/interface.h ]; then
      cp core/interface.h $out/include/core/
    fi

    # Re-export the logos-protocol sources at their historical paths.
    # The protocol's own include output already uses this exact layout
    # ($out/include/cpp/...), so this is a straight overlay.
    for file in ${logos-protocol}/cpp/*.h ${logos-protocol}/cpp/*.cpp; do
      cp "$file" $out/include/cpp/
    done
    for dir in qt_local qt_remote mock plain; do
      for file in ${logos-protocol}/cpp/implementations/$dir/*; do
        cp "$file" $out/include/cpp/implementations/$dir/
      done
    done

    # Install the SDK's own cpp headers and sources on top
    for file in logos_api.cpp logos_api.h logos_api_provider.cpp logos_api_provider.h \
                logos_provider_object.h logos_provider_object.cpp \
                logos_module_context.h \
                qt_provider_object.h qt_provider_object.cpp \
                logos_json.h logos_result.h; do
      if [ -f cpp/$file ]; then
        cp cpp/$file $out/include/cpp/
      fi
    done

    if [ -f ${logos-protocol}/cpp/logos_mode.h ]; then
      cp ${logos-protocol}/cpp/logos_mode.h $out/include/
    fi
    cp ${logos-protocol}/cpp/logos_protocol.h $out/include/

    runHook postInstall
  '';
}
