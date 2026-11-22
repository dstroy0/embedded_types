# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

function(embedded_types_add_suite suite_name)
    set(suite_dir "${CMAKE_CURRENT_SOURCE_DIR}")
    set(suite_src "${suite_dir}/${suite_name}.c")
    set(runner "${suite_dir}/unity_runner.c")

    if(NOT EXISTS "${suite_src}")
        message(FATAL_ERROR
                "embedded_types: ${suite_name} has no ${suite_name}.c - a suite directory holds exactly one")
    endif()

    add_custom_command(
        OUTPUT "${runner}"
        COMMAND ${Python3_EXECUTABLE} "${EMBEDDED_TYPES_HARNESS}" runners gen "${suite_dir}"
                --unity "${EMBEDDED_TYPES_UNITY_RB}"
        DEPENDS "${suite_src}" "${EMBEDDED_TYPES_HARNESS}"
        COMMENT "embedded_types: generating Unity runner for ${suite_name}"
        VERBATIM
    )
    add_custom_target(${suite_name}_runner DEPENDS "${runner}")
    set_source_files_properties("${runner}" PROPERTIES GENERATED TRUE)

    add_executable(${suite_name}
        "${suite_src}"
        "${runner}"
        "${unity_SOURCE_DIR}/src/unity.c")
    add_dependencies(${suite_name} ${suite_name}_runner)
    target_include_directories(${suite_name} PRIVATE "${unity_SOURCE_DIR}/src")
    # Unity turns off its 64-bit assertions on a target with 32-bit pointers. The suites check 64-bit values on
    # every target, so each suite turns them on.
    target_compile_definitions(${suite_name} PRIVATE UNITY_SUPPORT_64)

    target_compile_options(${suite_name} PRIVATE ${EMBEDDED_TYPES_TEST_WARNINGS})
    target_link_libraries(${suite_name} PRIVATE embedded_types::embedded_types)
    set_target_properties(${suite_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")

    add_test(NAME ${suite_name} COMMAND ${suite_name})
    set_property(GLOBAL APPEND PROPERTY EMBEDDED_TYPES_SUITES_BUILT "${suite_name}")
endfunction()

# embedded_types_add_device_suite(<name>) - a suite whose cases, in <name>.c, call kernels in
# <name>.cu. The two link into one program, and the cases check the device arm of a header against
# the host. Where EMBEDDED_TYPES_DEVICE is 0, the function leaves the suite out, says so, and the
# build goes on.
function(embedded_types_add_device_suite suite_name)
    if(NOT EMBEDDED_TYPES_DEVICE)
        message(STATUS "embedded_types: ${suite_name} left out, no device arm in this build")
        return()
    endif()

    set(suite_dir "${CMAKE_CURRENT_SOURCE_DIR}")
    set(suite_src "${suite_dir}/${suite_name}.c")
    set(suite_device "${suite_dir}/${suite_name}.cu")
    set(runner "${suite_dir}/unity_runner.c")

    if(NOT EXISTS "${suite_src}" OR NOT EXISTS "${suite_device}")
        message(FATAL_ERROR
                "embedded_types: ${suite_name} needs ${suite_name}.c for its cases and ${suite_name}.cu for its kernels")
    endif()

    add_custom_command(
        OUTPUT "${runner}"
        COMMAND ${Python3_EXECUTABLE} "${EMBEDDED_TYPES_HARNESS}" runners gen "${suite_dir}"
                --unity "${EMBEDDED_TYPES_UNITY_RB}"
        DEPENDS "${suite_src}" "${EMBEDDED_TYPES_HARNESS}"
        COMMENT "embedded_types: generating Unity runner for ${suite_name}"
        VERBATIM
    )
    add_custom_target(${suite_name}_runner DEPENDS "${runner}")
    set_source_files_properties("${runner}" PROPERTIES GENERATED TRUE)

    add_executable(${suite_name}
        "${suite_src}"
        "${suite_device}"
        "${runner}"
        "${unity_SOURCE_DIR}/src/unity.c")
    add_dependencies(${suite_name} ${suite_name}_runner)
    target_include_directories(${suite_name} PRIVATE "${unity_SOURCE_DIR}/src")
    # The 64-bit assertions of Unity, as in embedded_types_add_suite
    target_compile_definitions(${suite_name} PRIVATE UNITY_SUPPORT_64)

    target_compile_options(${suite_name} PRIVATE $<$<COMPILE_LANGUAGE:C>:${EMBEDDED_TYPES_TEST_WARNINGS}>)
    target_compile_features(${suite_name} PRIVATE cuda_std_17)
    # The host pass of nvcc under MSVC is C++, where cl keeps its old preprocessor unless told otherwise. The
    # dispatch tables count their members through __VA_ARGS__, and only the new preprocessor of cl counts them right
    if(MSVC)
        target_compile_options(${suite_name} PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/Zc:preprocessor>)
    endif()
    set_target_properties(${suite_name} PROPERTIES
        CUDA_ARCHITECTURES "${CMAKE_CUDA_ARCHITECTURES}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")
    target_link_libraries(${suite_name} PRIVATE embedded_types::embedded_types CUDA::cudart)

    add_test(NAME ${suite_name} COMMAND ${suite_name})
    set_property(GLOBAL APPEND PROPERTY EMBEDDED_TYPES_SUITES_BUILT "${suite_name}")
endfunction()
