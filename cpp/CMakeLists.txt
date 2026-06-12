cmake_minimum_required(VERSION 3.14)
project(LogosSDK)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt packages
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core RemoteObjects)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core RemoteObjects)

# Plain-C++ transport dependencies (no Qt). The `system` component is
# required for boost::system::error_code (Boost.Asio); without it
# Boost::system isn't an exported imported target on Boost 1.87+.
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json REQUIRED)

# ---------------------------------------------------------------------------
# logos-protocol sources
#
# The transport/token/IPC layer (transports, consumer core incl. the
# capability flow, ModuleProxy, token manager, QVariant<->JSON conversion,
# the lp_* C ABI) lives in the logos-protocol repo. For this transition
# phase the SDK still compiles those sources INTO liblogos_sdk.a so the
# installed artifact stays monolithic — every existing link pattern
# (find_package, bare IMPORTED archives, source-layout plugin builds against
# the installed include/cpp tree) keeps working without downstream changes.
# Resolution order: -DLOGOS_PROTOCOL_ROOT / $LOGOS_PROTOCOL_ROOT / sibling
# checkout (workspace layout).
# ---------------------------------------------------------------------------
if(NOT DEFINED LOGOS_PROTOCOL_ROOT)
    if(DEFINED ENV{LOGOS_PROTOCOL_ROOT})
        set(LOGOS_PROTOCOL_ROOT "$ENV{LOGOS_PROTOCOL_ROOT}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../logos-protocol/cpp/logos_protocol.h")
        set(LOGOS_PROTOCOL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../logos-protocol")
    endif()
endif()
if(NOT EXISTS "${LOGOS_PROTOCOL_ROOT}/cpp/logos_protocol.h" AND
   NOT EXISTS "${LOGOS_PROTOCOL_ROOT}/include/cpp/logos_protocol.h")
    message(FATAL_ERROR "logos-protocol sources not found. "
                        "Set LOGOS_PROTOCOL_ROOT (CMake or environment variable) "
                        "to a logos-protocol checkout or its installed source export.")
endif()
# The protocol repo ships sources both as a checkout (<root>/cpp/...) and as
# the installed source export (<root>/include/cpp/...). Accept either.
if(EXISTS "${LOGOS_PROTOCOL_ROOT}/cpp/logos_protocol.cpp")
    set(LP_SRC "${LOGOS_PROTOCOL_ROOT}/cpp")
else()
    set(LP_SRC "${LOGOS_PROTOCOL_ROOT}/include/cpp")
endif()
message(STATUS "Using logos-protocol sources from: ${LP_SRC}")

set(PROTOCOL_SOURCES
    ${LP_SRC}/logos_protocol.h
    ${LP_SRC}/logos_protocol.cpp
    ${LP_SRC}/logos_types.cpp
    ${LP_SRC}/logos_types.h
    ${LP_SRC}/logos_api_client.cpp
    ${LP_SRC}/logos_api_client.h
    ${LP_SRC}/logos_api_consumer.cpp
    ${LP_SRC}/logos_api_consumer.h
    ${LP_SRC}/module_proxy.cpp
    ${LP_SRC}/module_proxy.h
    ${LP_SRC}/token_manager.cpp
    ${LP_SRC}/token_manager.h
    ${LP_SRC}/logos_mode.h
    ${LP_SRC}/logos_instance.h
    ${LP_SRC}/plugin_registry.h
    ${LP_SRC}/logos_object.h
    ${LP_SRC}/logos_thread_marshal.h
    ${LP_SRC}/logos_provider_interface.cpp
    ${LP_SRC}/logos_provider_interface.h
    ${LP_SRC}/logos_json_convert.cpp
    ${LP_SRC}/logos_json_convert.h
    ${LP_SRC}/logos_transport.h
    ${LP_SRC}/logos_transport.cpp
    ${LP_SRC}/logos_transport_config.h
    ${LP_SRC}/logos_transport_config_json.h
    ${LP_SRC}/logos_transport_config_json.cpp
    ${LP_SRC}/logos_transport_factory.cpp
    ${LP_SRC}/logos_transport_factory.h
    ${LP_SRC}/logos_registry.h
    ${LP_SRC}/logos_registry_factory.h
    ${LP_SRC}/logos_registry_factory.cpp
    ${LP_SRC}/implementations/qt_local/local_transport.cpp
    ${LP_SRC}/implementations/qt_local/local_transport.h
    ${LP_SRC}/implementations/qt_remote/remote_transport.cpp
    ${LP_SRC}/implementations/qt_remote/remote_transport.h
    ${LP_SRC}/implementations/qt_remote/qt_remote_registry.h
    ${LP_SRC}/implementations/qt_remote/qt_remote_registry.cpp
    ${LP_SRC}/implementations/mock/mock_store.cpp
    ${LP_SRC}/implementations/mock/mock_store.h
    ${LP_SRC}/implementations/mock/mock_transport.cpp
    ${LP_SRC}/implementations/mock/mock_transport.h
    ${LP_SRC}/implementations/mock/mock_registry.h
    ${LP_SRC}/implementations/mock/logos_mock.h
    # Plain-C++ wire stack (no Qt)
    ${LP_SRC}/implementations/plain/rpc_value.h
    ${LP_SRC}/implementations/plain/rpc_message.h
    ${LP_SRC}/implementations/plain/rpc_message.cpp
    ${LP_SRC}/implementations/plain/wire_codec.h
    ${LP_SRC}/implementations/plain/json_mapping.h
    ${LP_SRC}/implementations/plain/json_mapping.cpp
    ${LP_SRC}/implementations/plain/json_codec.h
    ${LP_SRC}/implementations/plain/json_codec.cpp
    ${LP_SRC}/implementations/plain/cbor_codec.h
    ${LP_SRC}/implementations/plain/cbor_codec.cpp
    ${LP_SRC}/implementations/plain/rpc_framing.h
    ${LP_SRC}/implementations/plain/rpc_framing.cpp
    ${LP_SRC}/implementations/plain/incoming_call_handler.h
    ${LP_SRC}/implementations/plain/rpc_connection.h
    ${LP_SRC}/implementations/plain/rpc_server.h
    ${LP_SRC}/implementations/plain/rpc_server.cpp
    ${LP_SRC}/implementations/plain/io_context_pool.h
    ${LP_SRC}/implementations/plain/io_context_pool.cpp
    ${LP_SRC}/implementations/plain/qvariant_rpc_value.h
    ${LP_SRC}/implementations/plain/qvariant_rpc_value.cpp
    ${LP_SRC}/implementations/plain/plain_logos_object.h
    ${LP_SRC}/implementations/plain/plain_logos_object.cpp
    ${LP_SRC}/implementations/plain/plain_transport_host.h
    ${LP_SRC}/implementations/plain/plain_transport_host.cpp
    ${LP_SRC}/implementations/plain/plain_transport_connection.h
    ${LP_SRC}/implementations/plain/plain_transport_connection.cpp
)

# SDK sources proper — the typed C++ developer layer above logos-protocol.
set(SDK_SOURCES
    logos_api.cpp
    logos_api.h
    logos_api_provider.cpp
    logos_api_provider.h
    logos_provider_object.cpp
    logos_provider_object.h
    logos_module_context.h
    qt_provider_object.cpp
    qt_provider_object.h
)

# Create the SDK library as STATIC instead of SHARED
add_library(logos_sdk STATIC ${SDK_SOURCES} ${PROTOCOL_SOURCES})

# Link Qt + plain-C++ transport deps. Boost.Asio is header-only
# *except* for `boost::system::error_code` (and friends), which lives
# in libboost_system. Without that, downstream consumers linking the
# static archive get "undefined reference to
# boost::system::detail::system_category()" at link time on most
# Linux toolchains. nixpkgs' Boost CMake config exposes Boost::system
# as a separate target; pull it in explicitly.
target_link_libraries(logos_sdk PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::RemoteObjects
    Boost::headers
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# Include directories — use BUILD_INTERFACE / INSTALL_INTERFACE generator
# expressions so the path stored in the exported target points at the
# install tree for downstream consumers and at the source tree only
# while we're building the SDK itself. install(EXPORT ...) refuses
# raw source-tree paths.
target_include_directories(logos_sdk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${LP_SRC}>
    $<BUILD_INTERFACE:${LP_SRC}/implementations/qt_local>
    $<BUILD_INTERFACE:${LP_SRC}/implementations/qt_remote>
    $<BUILD_INTERFACE:${LP_SRC}/implementations/mock>
    $<BUILD_INTERFACE:${LP_SRC}/implementations/plain>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/implementations/qt_local>
    $<INSTALL_INTERFACE:include/implementations/qt_remote>
    $<INSTALL_INTERFACE:include/implementations/mock>
    $<INSTALL_INTERFACE:include/implementations/plain>
)

# Set output directories for static library
set_target_properties(logos_sdk PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Install the library into an EXPORT set so we can ship a CMake Config
# file that downstream consumers `find_package` to get an imported
# target with all the right INTERFACE_LINK_LIBRARIES (OpenSSL, Boost,
# nlohmann_json, Qt). Without this, every consumer that find_library's
# liblogos_sdk.a has to manually link the SDK's transitive deps.
install(TARGETS logos_sdk
    EXPORT logos-cpp-sdkTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(EXPORT logos-cpp-sdkTargets
    FILE logos-cpp-sdkTargets.cmake
    NAMESPACE logos-cpp-sdk::
    DESTINATION lib/cmake/logos-cpp-sdk
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/logos-cpp-sdkConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/logos-cpp-sdkConfig.cmake"
    INSTALL_DESTINATION lib/cmake/logos-cpp-sdk
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/logos-cpp-sdkConfigVersion.cmake"
    VERSION 0.1.0
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/logos-cpp-sdkConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/logos-cpp-sdkConfigVersion.cmake"
    DESTINATION lib/cmake/logos-cpp-sdk
)

# Install headers — the SDK's own plus the logos-protocol headers it
# re-exports, at the exact same relative paths as before the extraction
# (downstream #include lines must keep resolving unchanged).
install(FILES
    logos_api.h
    logos_api_provider.h
    logos_provider_object.h
    logos_module_context.h
    qt_provider_object.h
    logos_json.h
    logos_result.h
    DESTINATION include
)

install(FILES
    ${LP_SRC}/logos_protocol.h
    ${LP_SRC}/logos_types.h
    ${LP_SRC}/logos_api_client.h
    ${LP_SRC}/logos_api_consumer.h
    ${LP_SRC}/module_proxy.h
    ${LP_SRC}/token_manager.h
    ${LP_SRC}/logos_mode.h
    ${LP_SRC}/logos_instance.h
    ${LP_SRC}/plugin_registry.h
    ${LP_SRC}/logos_object.h
    ${LP_SRC}/logos_thread_marshal.h
    ${LP_SRC}/logos_provider_interface.h
    ${LP_SRC}/logos_json_convert.h
    ${LP_SRC}/logos_transport.h
    ${LP_SRC}/logos_transport_config.h
    ${LP_SRC}/logos_transport_config_json.h
    ${LP_SRC}/logos_transport_factory.h
    ${LP_SRC}/logos_registry.h
    ${LP_SRC}/logos_registry_factory.h
    DESTINATION include
)

install(FILES
    ${LP_SRC}/implementations/plain/rpc_value.h
    ${LP_SRC}/implementations/plain/rpc_message.h
    ${LP_SRC}/implementations/plain/wire_codec.h
    ${LP_SRC}/implementations/plain/json_codec.h
    ${LP_SRC}/implementations/plain/rpc_framing.h
    DESTINATION include/implementations/plain
)

install(FILES
    ${LP_SRC}/implementations/qt_local/local_transport.h
    DESTINATION include/implementations/qt_local
)

install(FILES
    ${LP_SRC}/implementations/qt_remote/remote_transport.h
    ${LP_SRC}/implementations/qt_remote/qt_remote_registry.h
    DESTINATION include/implementations/qt_remote
)

install(FILES
    ${LP_SRC}/implementations/mock/mock_store.h
    ${LP_SRC}/implementations/mock/mock_transport.h
    ${LP_SRC}/implementations/mock/mock_registry.h
    ${LP_SRC}/implementations/mock/logos_mock.h
    DESTINATION include/implementations/mock
)
