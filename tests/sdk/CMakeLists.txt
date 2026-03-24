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
)

target_link_libraries(sdk_tests PRIVATE
    logos_sdk
    GTest::gtest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::RemoteObjects
    Qt${QT_VERSION_MAJOR}::Test
)

gtest_discover_tests(sdk_tests)
