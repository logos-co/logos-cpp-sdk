set(EXPERIMENTAL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator/experimental)

add_executable(experimental_tests
    # LIDL language tests
    test_lidl_lexer.cpp
    test_lidl_parser.cpp
    test_lidl_validator.cpp
    test_lidl_serializer.cpp

    # Type mapping tests
    test_lidl_type_mapping.cpp

    # Code generation tests
    test_lidl_gen_provider.cpp
    test_lidl_gen_client.cpp

    # Impl header parser tests
    test_impl_header_parser.cpp

    # Sources under test
    ${EXPERIMENTAL_SRC_DIR}/lidl_lexer.cpp
    ${EXPERIMENTAL_SRC_DIR}/lidl_parser.cpp
    ${EXPERIMENTAL_SRC_DIR}/lidl_validator.cpp
    ${EXPERIMENTAL_SRC_DIR}/lidl_serializer.cpp
    ${EXPERIMENTAL_SRC_DIR}/lidl_gen_client.cpp
    ${EXPERIMENTAL_SRC_DIR}/lidl_gen_provider.cpp
    ${EXPERIMENTAL_SRC_DIR}/impl_header_parser.cpp
)

target_include_directories(experimental_tests PRIVATE
    ${EXPERIMENTAL_SRC_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp
)

# Pass fixtures directory as a compile definition
target_compile_definitions(experimental_tests PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(experimental_tests PRIVATE
    GTest::gtest_main
    Qt${QT_VERSION_MAJOR}::Core
)

gtest_discover_tests(experimental_tests)
