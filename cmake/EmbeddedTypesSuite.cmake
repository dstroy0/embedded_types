# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
# negotiated commercial licensing contract or an educator's license issued to you personally.
#
# embedded_types_add_suite() - one CTest target for the suite in the current directory.
#
# A suite is a directory holding exactly one .c with file-scope `void test_<name>(void)` cases.
# harness.py turns that into unity_runner.c through Unity's own generator, and refuses a suite whose
# cases the generator would walk past.
#
#   embedded_types_add_suite(test_embed_types)
#
# The suite declares itself in its own directory, so adding one is a new directory plus a line in
# its parent. Nothing central has to be kept in step, which is the property a hand-maintained list
# of suite names does not have.
#
# Unity's generator reads case names out of the source text and does not see a preprocessor
# conditional, so a case cannot be compiled out of a suite: the runner declares and calls it
# whichever way the conditional went. A case whose body must vary by target puts the #if inside the
# case, and `harness.py suites --strict` reports one that does not.

function(embedded_types_add_suite suite_name)
    set(suite_dir "${CMAKE_CURRENT_SOURCE_DIR}")
    set(suite_src "${suite_dir}/${suite_name}.c")
    set(runner "${suite_dir}/unity_runner.c")

    if(NOT EXISTS "${suite_src}")
        message(FATAL_ERROR
                "embedded_types: ${suite_name} has no ${suite_name}.c - a suite directory holds exactly one")
    endif()

    # The generation step is wrapped in a target of its own rather than left as a bare OUTPUT, so it
    # can be built and read on its own with `cmake --build <tree> --target ${suite_name}_runner`.
    # That is what makes a generator failure a build of one step rather than a suite that fails to
    # link for a reason printed several hundred lines earlier.
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

    # unity.c is compiled into each suite rather than linked from one shared library, because a
    # suite's own compile definitions have to reach the assertion bodies it calls and not only the
    # cases that call them.
    add_executable(${suite_name}
        "${suite_src}"
        "${runner}"
        "${unity_SOURCE_DIR}/src/unity.c")
    add_dependencies(${suite_name} ${suite_name}_runner)
    target_include_directories(${suite_name} PRIVATE "${unity_SOURCE_DIR}/src")

    # EMBEDDED_TYPES_TEST_WARNINGS rather than EMBEDDED_TYPES_WARNINGS. The root CMakeLists documents
    # which two flags come off and why Unity's assertion macros do not survive them.
    target_compile_options(${suite_name} PRIVATE ${EMBEDDED_TYPES_TEST_WARNINGS})
    target_link_libraries(${suite_name} PRIVATE embedded_types::embedded_types)
    set_target_properties(${suite_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test")

    add_test(NAME ${suite_name} COMMAND ${suite_name})
    set_property(GLOBAL APPEND PROPERTY EMBEDDED_TYPES_SUITES_BUILT "${suite_name}")
endfunction()
