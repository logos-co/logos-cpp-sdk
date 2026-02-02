cmake_minimum_required(VERSION 3.14)
project(LogosSDK)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt packages
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core RemoteObjects)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core RemoteObjects)

# SDK sources (high-level API)
set(SDK_SOURCES
    logos-cpp-sdk/logos_api.cpp
    logos-cpp-sdk/logos_api.h
    logos-cpp-sdk/logos_api_client.cpp
    logos-cpp-sdk/logos_api_client.h
    logos-cpp-sdk/module_proxy.cpp
    logos-cpp-sdk/module_proxy.h
    logos-cpp-sdk/token_manager.cpp
    logos-cpp-sdk/token_manager.h
)

# Transport sources (Qt RemoteObjects layer)
set(TRANSPORT_SOURCES
    logos-transport/logos_api_consumer.cpp
    logos-transport/logos_api_consumer.h
    logos-transport/logos_api_provider.cpp
    logos-transport/logos_api_provider.h
    logos-transport/logos_mode.h
    logos-transport/plugin_registry.h
)

# Create the SDK library as STATIC instead of SHARED
add_library(logos_sdk STATIC ${SDK_SOURCES} ${TRANSPORT_SOURCES})

# Link Qt libraries
target_link_libraries(logos_sdk PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::RemoteObjects)

# Include directories
target_include_directories(logos_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/logos-cpp-sdk
    ${CMAKE_CURRENT_SOURCE_DIR}/logos-transport
    ${CMAKE_CURRENT_SOURCE_DIR}/core
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

# Install SDK headers
install(FILES
    logos-cpp-sdk/logos_api.h
    logos-cpp-sdk/logos_api_client.h
    logos-cpp-sdk/module_proxy.h
    logos-cpp-sdk/token_manager.h
    DESTINATION include/logos-cpp-sdk
)

# Install transport headers
install(FILES
    logos-transport/logos_api_consumer.h
    logos-transport/logos_api_provider.h
    logos-transport/logos_mode.h
    logos-transport/plugin_registry.h
    DESTINATION include/logos-transport
)

# Install core headers
install(FILES
    core/interface.h
    DESTINATION include/core
)
