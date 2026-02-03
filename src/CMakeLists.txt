cmake_minimum_required(VERSION 3.14)
project(LogosSDK)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt packages
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core RemoteObjects)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core RemoteObjects)

# Root sources (shared/common)
set(ROOT_SOURCES
    logos_api.cpp
    logos_api.h
    token_manager.cpp
    token_manager.h
    logos_mode.h
    plugin_registry.h
)

# Client sources
set(CLIENT_SOURCES
    client/logos_api_client.cpp
    client/logos_api_client.h
    client/logos_api_consumer.cpp
    client/logos_api_consumer.h
)

# Provider sources
set(PROVIDER_SOURCES
    provider/logos_api_provider.cpp
    provider/logos_api_provider.h
    provider/module_proxy.cpp
    provider/module_proxy.h
)

# Create the SDK library as STATIC instead of SHARED
add_library(logos_sdk STATIC ${ROOT_SOURCES} ${CLIENT_SOURCES} ${PROVIDER_SOURCES})

# Link Qt libraries
target_link_libraries(logos_sdk PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::RemoteObjects)

# Include directories
target_include_directories(logos_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/client
    ${CMAKE_CURRENT_SOURCE_DIR}/provider
    ${CMAKE_CURRENT_SOURCE_DIR}/provider/core
)

# Set output directories for static library
set_target_properties(logos_sdk PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Install the library
install(TARGETS logos_sdk
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install root headers
install(FILES
    logos_api.h
    token_manager.h
    logos_mode.h
    plugin_registry.h
    DESTINATION include
)

# Install client headers
install(FILES
    client/logos_api_client.h
    client/logos_api_consumer.h
    DESTINATION include/client
)

# Install provider headers
install(FILES
    provider/logos_api_provider.h
    provider/module_proxy.h
    DESTINATION include/provider
)

# Install core headers
install(FILES
    provider/core/interface.h
    DESTINATION include/provider/core
)
