# logos_generate_clients(TARGET <t> LIDL <a.lidl>... [TYPED_COLLECTIONS] [OUTPUT_DIR <d>])
#
# Qt-free typed clients for a program that is not a module: at build time,
# `logos-cpp-generator --lidl <a.lidl> --api-style lp` writes <module>_api.{h,cpp}
# per contract into OUTPUT_DIR (default <binary dir>/<t>_clients), compiled into
# <t>. TYPED_COLLECTIONS types [T], {tstr: T} and ?T. The generator is
# LOGOS_CPP_GENERATOR (a path or a target), else this package's, else PATH's; it
# needs Qt as a build tool only. <t> must also see logos-protocol's headers and
# link the plain protocol image liblogos links.

include_guard(GLOBAL)

get_filename_component(_logos_cpp_sdk_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set_property(GLOBAL PROPERTY LOGOS_CPP_SDK_GENERATOR_HINT "${_logos_cpp_sdk_prefix}/bin")

function(logos_generate_clients)
    cmake_parse_arguments(ARG "TYPED_COLLECTIONS" "TARGET;OUTPUT_DIR" "LIDL" ${ARGN})
    if(NOT ARG_TARGET OR NOT TARGET "${ARG_TARGET}")
        message(FATAL_ERROR "logos_generate_clients: TARGET must name an existing target")
    endif()
    if(NOT ARG_LIDL)
        message(FATAL_ERROR "logos_generate_clients: no LIDL contract given")
    endif()
    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${ARG_TARGET}_clients")
    endif()

    if(NOT LOGOS_CPP_GENERATOR)
        get_property(_hint GLOBAL PROPERTY LOGOS_CPP_SDK_GENERATOR_HINT)
        find_program(LOGOS_CPP_GENERATOR logos-cpp-generator HINTS "${_hint}")
    endif()
    if(NOT LOGOS_CPP_GENERATOR)
        message(FATAL_ERROR "logos_generate_clients: logos-cpp-generator not found; "
                            "set LOGOS_CPP_GENERATOR or put it on PATH")
    endif()

    set(_typed "")
    if(ARG_TYPED_COLLECTIONS)
        set(_typed --typed-collections)
    endif()

    foreach(_lidl IN LISTS ARG_LIDL)
        get_filename_component(_lidl "${_lidl}" ABSOLUTE)
        # The files are named after the contract's module, so it is read here.
        file(STRINGS "${_lidl}" _decl REGEX "^[ \t]*module[ \t]+[A-Za-z_][A-Za-z0-9_]*"
             LIMIT_COUNT 1)
        string(REGEX REPLACE "^[ \t]*module[ \t]+([A-Za-z_][A-Za-z0-9_]*).*$" "\\1"
               _name "${_decl}")
        if(NOT _name)
            message(FATAL_ERROR "logos_generate_clients: no `module <name>` in ${_lidl}")
        endif()
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_lidl}")

        set(_header "${ARG_OUTPUT_DIR}/${_name}_api.h")
        set(_source "${ARG_OUTPUT_DIR}/${_name}_api.cpp")
        add_custom_command(
            OUTPUT "${_header}" "${_source}"
            COMMAND "${LOGOS_CPP_GENERATOR}" --lidl "${_lidl}" --api-style lp ${_typed}
                    --output-dir "${ARG_OUTPUT_DIR}"
            DEPENDS "${_lidl}" "${LOGOS_CPP_GENERATOR}"
            COMMENT "Generating the ${_name} client"
            VERBATIM)
        target_sources(${ARG_TARGET} PRIVATE "${_source}" "${_header}")
    endforeach()

    target_include_directories(${ARG_TARGET} PRIVATE "${ARG_OUTPUT_DIR}")
    if(TARGET logos-cpp-sdk::logos_consumer)
        target_link_libraries(${ARG_TARGET} PRIVATE logos-cpp-sdk::logos_consumer)
    endif()
endfunction()
