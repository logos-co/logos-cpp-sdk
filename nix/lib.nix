# Builds the logos-cpp-sdk package: the Qt-free, header-only base SDK
# (logos_module_context.h / logos_result.h / logos_json.h) exported as the
# CMake INTERFACE target logos-cpp-sdk::logos_headers.
{ pkgs, common, src, logos-protocol }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;

  inherit src;
  inherit (common) cmakeFlags meta;

  # Header-only: no Qt, no transports. nlohmann_json is the single
  # dependency (the headers use it), propagated so consumers' CMake
  # find_dependency(nlohmann_json) resolves.
  nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
  buildInputs = [ pkgs.nlohmann_json ];
  propagatedBuildInputs = [ pkgs.nlohmann_json ];

  dontUseCmakeConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p build-sdk
    cd build-sdk
    cmake ../cpp -GNinja -DCMAKE_INSTALL_PREFIX=$out $cmakeFlags
    ninja
    cd ..

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    cmake --install build-sdk
    runHook postInstall
  '';
}
