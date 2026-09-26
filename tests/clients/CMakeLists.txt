# The client mode end to end: logos_generate_clients() runs the generator built
# from this checkout over typed_probe.lidl at build time, and one test source is
# compiled against the plain and the --typed-collections wrapper. Neither target
# links Qt, so building them is the Qt-free assertion.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cpp/LogosGenerateClients.cmake)
set(LOGOS_CPP_GENERATOR logos-cpp-generator)

# logos_lp_client.h is a veneer over the lp_* C ABI; the tests stub the ABI.
if(EXISTS "${LOGOS_PROTOCOL_ROOT}/cpp/logos_protocol.h")
    set(_protocol_include "${LOGOS_PROTOCOL_ROOT}/cpp")
elseif(EXISTS "${LOGOS_PROTOCOL_ROOT}/include/logos_protocol.h")
    set(_protocol_include "${LOGOS_PROTOCOL_ROOT}/include")
else()
    message(FATAL_ERROR "logos_protocol.h not found under LOGOS_PROTOCOL_ROOT=${LOGOS_PROTOCOL_ROOT}")
endif()

add_executable(plain_client_tests test_client_mode.cpp)
logos_generate_clients(TARGET plain_client_tests LIDL typed_probe.lidl)

add_executable(typed_client_tests test_client_mode.cpp)
logos_generate_clients(TARGET typed_client_tests LIDL typed_probe.lidl TYPED_COLLECTIONS)
target_compile_definitions(typed_client_tests PRIVATE LOGOS_TYPED_COLLECTIONS=1)

foreach(_t plain_client_tests typed_client_tests)
    target_include_directories(${_t} PRIVATE "${_protocol_include}")
    target_compile_options(${_t} PRIVATE -Wall -Wextra)
    target_link_libraries(${_t} PRIVATE logos_consumer GTest::gtest_main)
    gtest_discover_tests(${_t})
endforeach()
