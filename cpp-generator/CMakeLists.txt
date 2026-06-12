cmake_minimum_required(VERSION 3.14)
project(LogosCppGenerator)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt Core (QCoreApplication, QPluginLoader, QJson*)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(logos-cpp-generator
    main.cpp
    legacy/main.cpp
    legacy/generator_lib.cpp
    experimental/lidl_emit_common.cpp
    experimental/lidl_lexer.cpp
    experimental/lidl_parser.cpp
    experimental/lidl_serializer.cpp
    experimental/lidl_validator.cpp
    experimental/lidl_gen_client.cpp
    experimental/lidl_gen_provider.cpp
    experimental/lidl_gen_cdylib.cpp
    experimental/lidl_gen_ui.cpp
    experimental/impl_header_parser.cpp
)

target_link_libraries(logos-cpp-generator PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# nlohmann_json: pulled in transitively by logos_provider_object.h →
# logos_provider_interface.h / logos_json_convert.h (header-only use).
find_package(nlohmann_json REQUIRED)
target_link_libraries(logos-cpp-generator PRIVATE nlohmann_json::nlohmann_json)

# logos-protocol headers (logos_provider_interface.h, logos_json_convert.h —
# included transitively via ../cpp/logos_provider_object.h). Header-only:
# the generator does not link the protocol library. Resolution mirrors
# cpp/CMakeLists.txt: -DLOGOS_PROTOCOL_ROOT / env / sibling checkout.
if(NOT DEFINED LOGOS_PROTOCOL_ROOT)
    if(DEFINED ENV{LOGOS_PROTOCOL_ROOT})
        set(LOGOS_PROTOCOL_ROOT "$ENV{LOGOS_PROTOCOL_ROOT}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../logos-protocol/cpp/logos_protocol.h")
        set(LOGOS_PROTOCOL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../logos-protocol")
    endif()
endif()
if(EXISTS "${LOGOS_PROTOCOL_ROOT}/cpp/logos_protocol.h")
    set(LP_INCLUDE "${LOGOS_PROTOCOL_ROOT}/cpp")
elseif(EXISTS "${LOGOS_PROTOCOL_ROOT}/include/cpp/logos_protocol.h")
    set(LP_INCLUDE "${LOGOS_PROTOCOL_ROOT}/include/cpp")
else()
    message(FATAL_ERROR "logos-protocol headers not found. Set LOGOS_PROTOCOL_ROOT "
                        "(CMake or environment variable).")
endif()

target_include_directories(logos-cpp-generator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/legacy
    ${CMAKE_CURRENT_SOURCE_DIR}/experimental
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp
    ${LP_INCLUDE}
)

# Output directories
set_target_properties(logos-cpp-generator PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ── LIDL frontend distribution ───────────────────────────────────────────────
# logos-qt-sdk's logos-qt-generator (which emits ALL Qt glue: universal,
# cdylib, ui) compiles these sources directly — source-level sharing keeps the
# two generators on one frontend without a binary-ABI coupling.
install(FILES
    experimental/lidl_ast.h
    experimental/lidl_lexer.h
    experimental/lidl_lexer.cpp
    experimental/lidl_parser.h
    experimental/lidl_parser.cpp
    experimental/lidl_serializer.h
    experimental/lidl_serializer.cpp
    experimental/lidl_validator.h
    experimental/lidl_validator.cpp
    experimental/impl_header_parser.h
    experimental/impl_header_parser.cpp
    experimental/lidl_emit_common.h
    experimental/lidl_emit_common.cpp
    DESTINATION share/lidl-frontend
)
