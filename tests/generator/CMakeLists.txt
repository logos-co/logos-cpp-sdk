# logos-lidl: test_optional_spellings runs the LEGACY path end to end — LIDL
# text -> the JSON surface (lidl_to_json) -> the emitter — because the rule it
# asserts (the two optionality spellings emit identical code) is a property of
# that composition and cannot be checked on either half alone.
find_package(logos-lidl REQUIRED)

add_executable(generator_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator/generator_lib.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator/lidl_to_json.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator/experimental/lidl_emit_common.cpp
    # The provider emitter, so a name rule can be compiled on both sides.
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator/experimental/lidl_gen_cdylib.cpp
    test_to_pascal_case.cpp
    test_normalize_type.cpp
    test_map_param_type.cpp
    test_map_return_type.cpp
    test_to_qvariant_conversion.cpp
    test_make_header.cpp
    test_make_source.cpp
    test_make_umbrella.cpp
    test_records.cpp
    test_async_result.cpp
    # The lp emitter's EVENT surface, which had no coverage at all: every other
    # generator test passes methods and no events. Pins the SubHandle return and
    # the per-module (not per-event) subscription state surface.
    test_lp_event_options.cpp
    test_optional_spellings.cpp
    # Generate from a small contract and compile the result (generated_code.h).
    test_reserved_param_names.cpp
    test_keyword_names.cpp
    test_recursive_records.cpp
)

target_include_directories(generator_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator/experimental
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp
)

# generated_code.h compiles emitted wrappers with this suite's own compiler,
# against the SDK, logos-protocol and nlohmann headers.
find_package(nlohmann_json REQUIRED)
if(EXISTS "${LOGOS_PROTOCOL_ROOT}/cpp/logos_protocol.h")
    set(_protocol_include "${LOGOS_PROTOCOL_ROOT}/cpp")
elseif(EXISTS "${LOGOS_PROTOCOL_ROOT}/include/logos_protocol.h")
    set(_protocol_include "${LOGOS_PROTOCOL_ROOT}/include")
else()
    message(FATAL_ERROR "logos_protocol.h not found under LOGOS_PROTOCOL_ROOT=${LOGOS_PROTOCOL_ROOT}")
endif()
get_target_property(_json_include nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
# nix passes a store path: CI runs ./result/bin/generator_tests, after the build dir is gone.
set(LOGOS_TEST_SDK_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../cpp" CACHE PATH "SDK headers for the compile probes")
string(JOIN "|" _compile_includes
    "${LOGOS_TEST_SDK_INCLUDE}" "${_protocol_include}" ${_json_include})
target_compile_definitions(generator_tests PRIVATE
    LOGOS_TEST_CXX="${CMAKE_CXX_COMPILER}"
    LOGOS_TEST_INCLUDES="${_compile_includes}"
)

target_link_libraries(generator_tests PRIVATE
    GTest::gtest_main
    Qt${QT_VERSION_MAJOR}::Core
    logos-lidl::logos_lidl
)

gtest_discover_tests(generator_tests)
