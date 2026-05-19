cmake_minimum_required(VERSION 3.14)
project(LogosCppSdkTests)

# Match the SDK library's C++17 standard. Bumped from 14 because
# logos_module_context.h (publicly included from the SDK) uses C++17
# variable templates — std::is_base_of_v / std::enable_if_t — in its
# tag-dispatched _logos_codegen_::maybeSet* helpers. Any consumer of
# those headers, including the test binaries, has to compile at 17+.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(GTest REQUIRED)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core RemoteObjects)

enable_testing()

add_subdirectory(sdk)
add_subdirectory(generator)
add_subdirectory(experimental)
