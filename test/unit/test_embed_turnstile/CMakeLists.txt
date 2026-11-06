# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

embedded_types_add_suite(test_embed_turnstile)

# the host's threads, its address wait behind embed_word_wait and embed_word_wake, and its clock, in test/support
# where the platform's headers are out of the runner's sight
find_package(Threads REQUIRED)
target_sources(test_embed_turnstile PRIVATE
    "${EMBEDDED_TYPES_TEST_ROOT}/support/host_threads.c"
    "${EMBEDDED_TYPES_TEST_ROOT}/support/host_wait.c")
target_include_directories(test_embed_turnstile PRIVATE "${EMBEDDED_TYPES_TEST_ROOT}/support")
target_link_libraries(test_embed_turnstile PRIVATE Threads::Threads)
if(WIN32)
    target_link_libraries(test_embed_turnstile PRIVATE -lsynchronization)
endif()
