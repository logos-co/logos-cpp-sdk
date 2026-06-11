# Builds the logos-cpp-generator binary
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-generator";
  version = common.version;
  
  inherit src;
  inherit (common) nativeBuildInputs buildInputs cmakeFlags meta;
  
  # Skip default configure phase since we do it in buildPhase
  dontUseCmakeConfigure = true;

  # The generator is a build-time CLI tool, not a runtime Qt app — it doesn't
  # need QT_PLUGIN_PATH or other env vars injected by wrapQtApp. Skipping the
  # wrap step avoids a segfault in wrapQtAppsNoGuiHook on some macOS versions
  # where the Darwin binary wrapper tooling crashes (exit code 139).
  dontWrapQtApps = true;
  
  buildPhase = ''
    runHook preBuild
    
    # Build generator
    mkdir -p build-generator
    cd build-generator
    cmake ../cpp-generator -GNinja $cmakeFlags
    ninja
    cd ..
    
    runHook postBuild
  '';
  
  installPhase = ''
    runHook preInstall
    
    # Install generator binary
    mkdir -p $out/bin
    if [ -f build-generator/bin/logos-cpp-generator ]; then
      cp build-generator/bin/logos-cpp-generator $out/bin/
    fi
    
    runHook postInstall
  '';
}

