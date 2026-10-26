# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

embedded_types_add_suite(test_embed_spinlock)

# the contention case's host threads, started in test/support where the platform's headers are out of the runner's
# sight, and linked from POSIX's threads library
find_package(Threads REQUIRED)
target_sources(test_embed_spinlock PRIVATE "${EMBEDDED_TYPES_TEST_ROOT}/support/host_threads.c")
target_include_directories(test_embed_spinlock PRIVATE "${EMBEDDED_TYPES_TEST_ROOT}/support")
target_link_libraries(test_embed_spinlock PRIVATE Threads::Threads)
