# Builds and runs the test suite
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-tests";
  version = common.version;

  inherit src;

  nativeBuildInputs = common.nativeBuildInputs;
  buildInputs = common.buildInputs ++ [ pkgs.gtest ];
  cmakeFlags = common.cmakeFlags;

  dontUseCmakeConfigure = true;

  buildPhase = ''
    runHook preBuild

    mkdir -p build-tests
    cd build-tests
    cmake ../tests -GNinja $cmakeFlags
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

    runHook postInstall
  '';

  inherit (common) meta;
}
