cmake_minimum_required(VERSION 3.14)
project(LogosSDK)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Find Qt packages
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core RemoteObjects)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core RemoteObjects)

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
    logos_transport.h
    logos_transport_factory.cpp
    logos_transport_factory.h
    implementations/qt_local/local_transport.cpp
    implementations/qt_local/local_transport.h
    implementations/qt_remote/remote_transport.cpp
    implementations/qt_remote/remote_transport.h
)

# Create the SDK library as STATIC instead of SHARED
add_library(logos_sdk STATIC ${SDK_SOURCES})

# Link Qt libraries
target_link_libraries(logos_sdk PUBLIC Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::RemoteObjects)

# Include directories
target_include_directories(logos_sdk PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/implementations/qt_local
    ${CMAKE_CURRENT_SOURCE_DIR}/implementations/qt_remote
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
    logos_transport.h
    logos_transport_factory.h
    DESTINATION include
)

install(FILES
    implementations/qt_local/local_transport.h
    DESTINATION include/implementations/qt_local
)

install(FILES
    implementations/qt_remote/remote_transport.h
    DESTINATION include/implementations/qt_remote
)
