# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#
# Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
# negotiated commercial licensing contract or an educator's license issued to you personally.

# One executable per header under test, each its own CTest case, so a failure names the header
# rather than the suite. No test framework: the library is widths and macros, and a dependency
# pulled in to prove a width is a dependency the consumer inherits.
#
# Every suite links embedded_types::embedded_types for the include path and the C11 requirement,
# and carries EMBEDDED_TYPES_WARNINGS, which the root CMakeLists documents flag by flag. The
# warnings are on the test targets rather than on the INTERFACE, so a consumer chooses its own.

set(EMBEDDED_TYPES_SUITES test_embed_types test_embed_dispatch_layout)

foreach(suite IN LISTS EMBEDDED_TYPES_SUITES)
  add_executable(${suite} ${suite}.c)
  target_link_libraries(${suite} PRIVATE embedded_types::embedded_types)
  target_compile_options(${suite} PRIVATE ${EMBEDDED_TYPES_WARNINGS})
  add_test(NAME ${suite} COMMAND ${suite})
endforeach()
