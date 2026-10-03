# logos_generate_clients() as an app meets it: from the INSTALLED package, with
# the generator found beside it (not on PATH), the typed client compiled against
# the installed headers and linked to the real plain protocol image.
{ pkgs, common, sdk, protocolPlain, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-clients-cmake-tests";
  version = common.version;
  dontUnpack = true;
  # Keeps the SDK's bin/ off PATH, so the lookup beside the package is tested.
  strictDeps = true;
  nativeBuildInputs = [ pkgs.cmake pkgs.ninja ];
  buildInputs = [ sdk protocolPlain pkgs.boost pkgs.openssl pkgs.nlohmann_json ];

  configurePhase = ''
    runHook preConfigure
    mkdir app && cd app
    cp ${src}/tests/clients/typed_probe.lidl .
    cat > CMakeLists.txt <<'CMAKE'
    cmake_minimum_required(VERSION 3.14)
    project(ClientsApp CXX)
    set(CMAKE_CXX_STANDARD 17)
    find_package(logos-cpp-sdk REQUIRED)
    find_package(logos-protocol REQUIRED)
    add_executable(app main.cpp)
    logos_generate_clients(TARGET app LIDL typed_probe.lidl TYPED_COLLECTIONS)
    target_link_libraries(app PRIVATE logos-protocol::logos_protocol_plain_shared)
    CMAKE
    cat > main.cpp <<'CPP'
    #include "typed_probe_api.h"
    #include <type_traits>
    static_assert(std::is_same_v<decltype(std::declval<TypedProbe&>().echoInts({})),
                                 std::vector<int64_t>>);
    int main() { TypedProbe probe("clients_app"); (void)probe; return 0; }
    CPP
    if command -v logos-cpp-generator >/dev/null; then
      echo "FAIL: the generator is on PATH, so the package lookup is not tested" >&2
      exit 1
    fi
    cmake -S . -B build -GNinja
    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild
    ninja -C build
    grep -q 'LOGOS_CPP_GENERATOR:FILEPATH=${sdk}/bin/logos-cpp-generator' build/CMakeCache.txt \
      || { grep LOGOS_CPP_GENERATOR build/CMakeCache.txt >&2
           echo "FAIL: the generator was not found beside the package" >&2; exit 1; }
    ./build/app
    runHook postBuild
  '';

  installPhase = ''
    mkdir -p $out
    echo "logos_generate_clients() from the installed package: OK" > $out/result.txt
  '';

  meta = common.meta;
}
