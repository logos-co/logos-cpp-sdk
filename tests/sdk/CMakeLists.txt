add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../cpp ${CMAKE_CURRENT_BINARY_DIR}/logos_sdk)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

add_executable(sdk_tests
    test_main.cpp
    test_logos_result.cpp
    test_token_manager.cpp
    test_mock_store.cpp
    test_logos_mode.cpp
    test_logos_mock_setup.cpp
    test_logos_api_client_mock.cpp
    test_logos_api.cpp
    test_logos_api_client_overloads.cpp
    test_transport_factory.cpp
    test_registry_factory.cpp
    test_plugin_registry.cpp
    test_logos_instance.cpp
    test_logos_provider_base.cpp
    test_module_proxy.cpp
    test_logos_api_provider.cpp
    test_mock_transport.cpp
    test_local_transport_integration.cpp
    test_event_system.cpp
)

target_link_libraries(sdk_tests PRIVATE
    logos_sdk
    GTest::gtest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::RemoteObjects
    Qt${QT_VERSION_MAJOR}::Test
)

gtest_discover_tests(sdk_tests)
