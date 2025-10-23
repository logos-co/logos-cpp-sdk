# Common build configuration shared across all packages
{ pkgs }:

{
  pname = "logos-cpp-sdk";
  version = "0.1.0";
  
  # Common native build inputs
  nativeBuildInputs = [ 
    pkgs.cmake 
    pkgs.ninja 
    pkgs.pkg-config
    pkgs.qt6.wrapQtAppsNoGuiHook
  ];
  
  # Common runtime dependencies
  buildInputs = [ 
    pkgs.qt6.qtbase 
    pkgs.qt6.qtremoteobjects 
  ];
  
  # Common CMake flags
  cmakeFlags = [ "-GNinja" ];
  
  # Metadata
  meta = with pkgs.lib; {
    description = "Logos C++ SDK Library and Code Generator";
    platforms = platforms.unix;
  };
}

