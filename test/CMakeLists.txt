# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
# negotiated commercial licensing contract or an educator's license issued to you personally.
#
# Sets up Unity and the harness, then hands off. Every suite declares itself in its own directory,
# so adding one is a new directory plus a line in its parent, and nothing central has to be kept in
# step.
#
#   unit/test_<header>/        one per header in include/, asserting what that header alone states
#   integration/test_<name>/   more than one header together
#
# integration/ does not exist yet. It is named here and in harness.py so the first suite that
# crosses two headers has somewhere to go that is not unit/, where a failure would be reported
# against a header the case only borrowed.
#
# Unity is fetched rather than vendored, and a test framework is a dependency of this directory
# alone. The INTERFACE target a consumer links carries the include path and the C11 requirement and
# nothing else, so nothing below reaches a consumer's build.

include(FetchContent)

# Pinned to a tag, not a branch. The runner generator's output is compiled into every suite, so a
# generator that changed under the tree would change what is built without anything in the tree
# changing.
FetchContent_Declare(
  unity
  GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
  GIT_TAG        v2.6.1
  GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(unity)

set(EMBEDDED_TYPES_UNITY_RB "${unity_SOURCE_DIR}/auto/generate_test_runner.rb")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(EMBEDDED_TYPES_HARNESS "${CMAKE_CURRENT_SOURCE_DIR}/harness.py")
set(EMBEDDED_TYPES_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

include(EmbeddedTypesSuite)

set_property(GLOBAL PROPERTY EMBEDDED_TYPES_SUITES_BUILT "")

add_subdirectory(unit)

get_property(EMBEDDED_TYPES_BUILT GLOBAL PROPERTY EMBEDDED_TYPES_SUITES_BUILT)
list(LENGTH EMBEDDED_TYPES_BUILT EMBEDDED_TYPES_BUILT_COUNT)
if(EMBEDDED_TYPES_BUILT_COUNT EQUAL 0)
  message(WARNING "embedded_types: no test suites were built")
endif()
message(STATUS "embedded_types: ${EMBEDDED_TYPES_BUILT_COUNT} test targets")
