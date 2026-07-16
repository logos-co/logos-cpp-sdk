set(EXPERIMENTAL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator/experimental)

# The LIDL frontend (lexer/parser/AST/serializer/validator) now lives in
# logos-lidl and is covered by its own test suite; only the C++/Qt-specific
# backends (type mapping, gen_client, impl-header parsing) are tested here.
find_package(logos-lidl REQUIRED)

add_executable(experimental_tests
    # Type mapping tests
    test_lidl_type_mapping.cpp

    # Code generation tests
    test_lidl_gen_client.cpp
    test_lidl_gen_cdylib.cpp

    # Impl header parser tests
    test_impl_header_parser.cpp

    # Sources under test
    ${EXPERIMENTAL_SRC_DIR}/lidl_emit_common.cpp
    ${EXPERIMENTAL_SRC_DIR}/lidl_gen_client.cpp
    ${EXPERIMENTAL_SRC_DIR}/lidl_gen_cdylib.cpp
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
    logos-lidl::logos_lidl
)

gtest_discover_tests(experimental_tests)
