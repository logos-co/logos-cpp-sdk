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

# SDK sources
set(SDK_SOURCES
    logos_types.cpp
    logos_types.h
    logos_api.cpp
    logos_api.h
    logos_api_client.cpp
    logos_api_client.h
    logos_api_consumer.cpp
    logos_api_consumer.h
    logos_api_provider.cpp
    logos_api_provider.h
    module_proxy.cpp
    module_proxy.h
    token_manager.cpp
    token_manager.h
    logos_mode.h
    logos_instance.h
    plugin_registry.h
    logos_object.h
    logos_provider_object.cpp
    logos_provider_object.h
    logos_module_context.h
    qt_provider_object.cpp
    qt_provider_object.h
    logos_transport.h
    logos_transport.cpp
    logos_transport_config.h
    logos_transport_config_json.h
    logos_transport_config_json.cpp
    logos_transport_factory.cpp
    logos_transport_factory.h
    logos_registry.h
    logos_registry_factory.h
    logos_registry_factory.cpp
    implementations/qt_local/local_transport.cpp
    implementations/qt_local/local_transport.h
    implementations/qt_remote/remote_transport.cpp
    implementations/qt_remote/remote_transport.h
    implementations/qt_remote/qt_remote_registry.h
    implementations/qt_remote/qt_remote_registry.cpp
    implementations/mock/mock_store.cpp
    implementations/mock/mock_store.h
    implementations/mock/mock_transport.cpp
    implementations/mock/mock_transport.h
    implementations/mock/mock_registry.h
    implementations/mock/logos_mock.h
    # Plain-C++ wire stack (no Qt)
    implementations/plain/rpc_value.h
    implementations/plain/rpc_message.h
    implementations/plain/rpc_message.cpp
    implementations/plain/wire_codec.h
    implementations/plain/json_mapping.h
    implementations/plain/json_mapping.cpp
    implementations/plain/json_codec.h
    implementations/plain/json_codec.cpp
    implementations/plain/cbor_codec.h
    implementations/plain/cbor_codec.cpp
    implementations/plain/rpc_framing.h
    implementations/plain/rpc_framing.cpp
    implementations/plain/incoming_call_handler.h
    implementations/plain/rpc_connection.h
    implementations/plain/rpc_server.h
    implementations/plain/rpc_server.cpp
    implementations/plain/io_context_pool.h
    implementations/plain/io_context_pool.cpp
    implementations/plain/qvariant_rpc_value.h
    implementations/plain/qvariant_rpc_value.cpp
    implementations/plain/plain_logos_object.h
    implementations/plain/plain_logos_object.cpp
    implementations/plain/plain_transport_host.h
    implementations/plain/plain_transport_host.cpp
    implementations/plain/plain_transport_connection.h
    implementations/plain/plain_transport_connection.cpp
)

# Create the SDK library as STATIC instead of SHARED
add_library(logos_sdk STATIC ${SDK_SOURCES})

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
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/implementations/qt_local>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/implementations/qt_remote>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/implementations/mock>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/implementations/plain>
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

# Install headers
install(FILES
    logos_types.h
    logos_api.h
    logos_api_client.h
    logos_api_consumer.h
    logos_api_provider.h
    module_proxy.h
    token_manager.h
    logos_mode.h
    logos_instance.h
    plugin_registry.h
    logos_object.h
    logos_provider_object.h
    logos_module_context.h
    qt_provider_object.h
    logos_transport.h
    logos_transport_config.h
    logos_transport_config_json.h
    logos_transport_factory.h
    logos_registry.h
    logos_registry_factory.h
    logos_json.h
    DESTINATION include
)

install(FILES
    implementations/plain/rpc_value.h
    implementations/plain/rpc_message.h
    implementations/plain/wire_codec.h
    implementations/plain/json_codec.h
    implementations/plain/rpc_framing.h
    DESTINATION include/implementations/plain
)

install(FILES
    implementations/qt_local/local_transport.h
    DESTINATION include/implementations/qt_local
)

install(FILES
    implementations/qt_remote/remote_transport.h
    implementations/qt_remote/qt_remote_registry.h
    DESTINATION include/implementations/qt_remote
)

install(FILES
    implementations/mock/mock_store.h
    implementations/mock/mock_transport.h
    implementations/mock/mock_registry.h
    implementations/mock/logos_mock.h
    DESTINATION include/implementations/mock
)
