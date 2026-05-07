# Builds the logos-cpp-sdk library
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-lib";
  version = common.version;

  inherit src;
  inherit (common) nativeBuildInputs cmakeFlags meta;
  buildInputs = common.buildInputs;

  # Propagate the SDK's transitive non-Qt deps so downstream Nix
  # derivations that depend on the SDK automatically get OpenSSL /
  # Boost / nlohmann_json in their own configure-time
  # `CMAKE_PREFIX_PATH` and link-time search path. Without this, every
  # consumer would have to list pkgs.openssl etc. itself just to
  # satisfy find_dependency(OpenSSL) inside our own Config file.
  #
  # Qt is intentionally excluded — see the `propagatedBuildInputs`
  # comment in default.nix for the setup-hook ordering reason. Every
  # consumer of this SDK must list `pkgs.qt6.qtbase` (+ any other
  # qt6.* it needs) and `pkgs.qt6.wrapQtAppsNoGuiHook` itself.
  propagatedBuildInputs = common.propagatedBuildInputs;

  # Skip default configure phase since we do it in buildPhase
  dontUseCmakeConfigure = true;

  buildPhase = ''
    runHook preBuild

    # Build SDK library
    mkdir -p build-sdk
    cd build-sdk
    cmake ../cpp -GNinja -DCMAKE_INSTALL_PREFIX=$out $cmakeFlags
    ninja
    cd ..

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    # Run cmake's install rules so the EXPORT set + generated
    # logos-cpp-sdkConfig.cmake / Targets.cmake land under
    # $out/lib/cmake/logos-cpp-sdk/. This is what makes
    # `find_package(logos-cpp-sdk)` work in consumers and gives them
    # an imported target carrying all transitive link interface info.
    cmake --install build-sdk

    runHook postInstall
  '';
}

