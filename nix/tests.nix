# Builds and runs the test suite
{ pkgs, common, src, logos-protocol, logos-lidl }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-tests";
  version = common.version;

  inherit src;

  nativeBuildInputs = common.nativeBuildInputs;
  # logos-lidl: the experimental backend tests link the canonical frontend
  # (find_package(logos-lidl) in tests/experimental/CMakeLists.txt).
  buildInputs = common.buildInputs ++ [ pkgs.gtest logos-lidl ];
  cmakeFlags = common.cmakeFlags;

  dontUseCmakeConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p build-tests
    cd build-tests
    cmake ../tests -GNinja -DLOGOS_PROTOCOL_ROOT=${logos-protocol} $cmakeFlags
    ninja
    cd ..

    runHook postBuild
  '';

  doCheck = true;
  checkPhase = ''
    runHook preCheck
    cd build-tests
    ctest --output-on-failure
    cd ..
    runHook postCheck
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin
    cp build-tests/sdk/sdk_tests $out/bin/
    cp build-tests/generator/generator_tests $out/bin/
    cp build-tests/experimental/experimental_tests $out/bin/

    # Copy test fixtures needed by experimental_tests at runtime
    mkdir -p $out/fixtures
    cp tests/experimental/fixtures/* $out/fixtures/

    runHook postInstall
  '';

  inherit (common) meta;
}
