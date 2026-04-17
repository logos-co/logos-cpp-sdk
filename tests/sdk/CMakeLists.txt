add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../cpp ${CMAKE_CURRENT_BINARY_DIR}/logos_sdk)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../cpp-generator ${CMAKE_CURRENT_BINARY_DIR}/logos_generator)

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Qml)

# --- Generate dispatch code for the sample provider fixture ---
set(SAMPLE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sample_provider.h)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(GENERATED_DISPATCH ${GENERATED_DIR}/logos_provider_dispatch.cpp)

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${GENERATED_DISPATCH}
    COMMAND logos-cpp-generator --provider-header ${SAMPLE_HEADER} --output-dir ${GENERATED_DIR}
    DEPENDS logos-cpp-generator ${SAMPLE_HEADER}
    COMMENT "Generating provider dispatch for sample_provider"
)

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
    test_async_calls.cpp
    test_provider_dispatch.cpp
    test_std_string_overloads.cpp
    test_qml_sync_rpc_crash.cpp
    fixtures/sample_provider.cpp
    ${GENERATED_DISPATCH}
)

target_include_directories(sdk_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
    ${CMAKE_CURRENT_SOURCE_DIR}/../../cpp
    ${GENERATED_DIR}
)

target_link_libraries(sdk_tests PRIVATE
    logos_sdk
    GTest::gtest
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::RemoteObjects
    Qt${QT_VERSION_MAJOR}::Test
)

# The QML integration tests (QmlSyncRpcCrashTest) need to locate Qt's
# bundled QML modules (QtQml.Base) at runtime. Under nix the default
# QmlImportsPath isn't always propagated into unwrapped test binaries,
# so we discover the directory at configure time from the qtdeclarative
# install root (derivable from Qt6Qml_DIR) and pass it to the test as a
# compile definition. The test then calls engine.addImportPath() with it.
#
# Qt6Qml_DIR typically points at
#   <qtdeclarative-root>/lib/cmake/Qt6Qml
# On nix/Linux the QML imports are at
#   <qtdeclarative-root>/lib/qt-6/qml
# On macOS (framework builds) the same layout is used — qtdeclarative
# also ships lib/qt-6/qml alongside the frameworks.
set(_qt_qml_import_path "")
if(DEFINED Qt${QT_VERSION_MAJOR}Qml_DIR)
    get_filename_component(_qtdecl_root
        "${Qt${QT_VERSION_MAJOR}Qml_DIR}/../../.." ABSOLUTE)
    foreach(_candidate
            "${_qtdecl_root}/lib/qt-6/qml"
            "${_qtdecl_root}/qml"
            "${_qtdecl_root}/lib/qml")
        if(EXISTS "${_candidate}/QtQml")
            set(_qt_qml_import_path "${_candidate}")
            break()
        endif()
    endforeach()
endif()

# Fallback: derive from the Qt6::Qml library path by walking up.
if(NOT _qt_qml_import_path)
    get_target_property(_qt_qml_lib Qt${QT_VERSION_MAJOR}::Qml LOCATION)
    set(_probe "${_qt_qml_lib}")
    foreach(_i RANGE 10)
        get_filename_component(_probe "${_probe}" DIRECTORY)
        if(EXISTS "${_probe}/lib/qt-6/qml/QtQml")
            set(_qt_qml_import_path "${_probe}/lib/qt-6/qml")
            break()
        endif()
        if(EXISTS "${_probe}/qml/QtQml")
            set(_qt_qml_import_path "${_probe}/qml")
            break()
        endif()
    endforeach()
endif()

message(STATUS "sdk_tests QML import path: ${_qt_qml_import_path}")

target_compile_definitions(sdk_tests PRIVATE
    QT_QML_TEST_IMPORT_PATH="${_qt_qml_import_path}"
)

gtest_discover_tests(sdk_tests)
