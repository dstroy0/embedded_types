# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
# negotiated commercial licensing contract or an educator's license issued to you personally.

cmake_minimum_required(VERSION 3.16)

project(
  embedded_types
  VERSION 0.1.0
  DESCRIPTION "Fixed widths, a machine word, and the compile-time machinery a C11 embedded library is written in"
  LANGUAGES C)

# ---------------------------------------------------------------------------
# The library
# ---------------------------------------------------------------------------
# Header only, so INTERFACE: there is nothing to compile and nothing to link. A consumer reaches it
# by add_subdirectory or FetchContent and links the alias, which carries the include directory and
# the language requirement with it.

add_library(embedded_types INTERFACE)
add_library(embedded_types::embedded_types ALIAS embedded_types)

target_include_directories(
  embedded_types INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>)

# c_std_11 rather than CMAKE_C_STANDARD, so the requirement travels to whoever links the target
# instead of being a property of this build. embed_compiler_directives.h raises #error below C11,
# which is what this keeps a consumer from meeting.
target_compile_features(embedded_types INTERFACE c_std_11)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(EMBEDDED_TYPES_BUILD_TESTS "Build the test suite" ${PROJECT_IS_TOP_LEVEL})
option(EMBEDDED_TYPES_WERROR "Treat warnings as errors in this project's own targets" OFF)

# ---------------------------------------------------------------------------
# Warning flags
# ---------------------------------------------------------------------------
# Applied to this project's OWN targets only, never to the INTERFACE. A consumer chooses its own
# diagnostics; a library that forces them onto a caller is deciding something that is not its own.
#
# The set is the strictest one a consumer of this library builds under, because a foundation library
# that cannot survive its consumer's diagnostics moves the noise into the consumer instead of
# removing it. Each flag below catches something a widths-and-macros header can plausibly get wrong:
#
#   -Wall -Wextra        the baseline.
#   -Wpedantic           rejects extensions. _Static_assert, compound literals and __VA_ARGS__ are
#                        all C11 features this library leans on, and this is what proves each one is
#                        the standard's rather than the compiler's.
#   -Wshadow             a macro expanding at a call site brings its own names into that scope.
#   -Wconversion         a width that silently narrows is precisely what this library exists to
#                        prevent, so a narrowing it introduces itself must not pass.
#   -Wsign-conversion    the signed word and the unsigned word are the same register, and moving
#                        between them without saying so is the mistake that makes them differ.
#   -Wcast-align         EMBED_RAW lowers alignment deliberately; every other cast that changes it
#                        is unintentional and this reports it.
#   -Wcast-qual          casting const away in a header a consumer includes everywhere.
#   -Wstrict-prototypes  an empty parameter list is not a prototype in C, and EMBED_FUNCTION_POINTER_BYTES
#                        measures void (*)(void), which must be one.
#   -Wmissing-prototypes an external definition with no declaration ahead of it.
#   -Wpointer-arith      arithmetic on void *, whose size is a GNU extension rather than a width.
#   -Wvla                a variable length array is stack the caller did not ask for, which a
#                        library reaching devices with no allocator cannot introduce.

set(EMBEDDED_TYPES_WARNINGS
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wsign-conversion;-Wcast-align;-Wcast-qual;-Wstrict-prototypes;-Wmissing-prototypes;-Wpointer-arith;-Wvla>
    $<$<AND:$<BOOL:${EMBEDDED_TYPES_WERROR}>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:-Werror>
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<AND:$<BOOL:${EMBEDDED_TYPES_WERROR}>,$<C_COMPILER_ID:MSVC>>:/WX>)

# The suites take the same set with three flags removed, each for a reason the framework imposes:
#
#   -Wconversion       Unity's assertion macros widen and compare their operands inside the macro
#   -Wsign-conversion  body, so the cast is attributed to the case's own line. Both fire on every
#                      TEST_ASSERT_EQUAL_* over a type narrower than int, which is most of what this
#                      library declares. A pragma per site would hide a real narrowing among them.
#   -Wmissing-prototypes
#                      a case is `void test_<name>(void)` at file scope with nothing declaring it.
#                      The generated unity_runner.c is what declares and calls it, and the case's
#                      own translation unit never sees that file.
#
# Everything else stays on, -Wpedantic included. The library itself is still compiled under the full
# set by every consumer that carries it, and by the suites that include it.
set(EMBEDDED_TYPES_TEST_WARNINGS
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wcast-align;-Wcast-qual;-Wstrict-prototypes;-Wpointer-arith;-Wvla>
    $<$<AND:$<BOOL:${EMBEDDED_TYPES_WERROR}>,$<C_COMPILER_ID:GNU,Clang,AppleClang>>:-Werror>
    $<$<C_COMPILER_ID:MSVC>:/W4>
    $<$<AND:$<BOOL:${EMBEDDED_TYPES_WERROR}>,$<C_COMPILER_ID:MSVC>>:/WX>)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
# embedded_types_add_suite() lives in cmake/ rather than in test/, so a consumer that vendors this
# tree gets the function with the module path it already sets rather than with a path into test/.

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(EMBEDDED_TYPES_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
