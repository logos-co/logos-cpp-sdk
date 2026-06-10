# Installs the Qt-free base SDK headers in the source-export layout
# ($out/include/cpp/...). The transport/protocol sources ship from
# logos-protocol and the Qt developer layer from logos-qt-sdk; build
# systems that need those add the respective include roots.
{ pkgs, common, src, logos-protocol }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-headers";
  version = common.version;

  inherit src;
  inherit (common) meta;

  dontBuild = true;
  dontConfigure = true;

  installPhase = ''
    runHook preInstall

    mkdir -p $out/include/cpp

    for file in logos_module_context.h logos_json.h logos_result.h; do
      cp cpp/$file $out/include/cpp/
      cp cpp/$file $out/include/
    done

    runHook postInstall
  '';
}
