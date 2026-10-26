/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file host_threads.c
 * @brief host_threads_run on the host's threads: CreateThread on Windows, pthreads elsewhere.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-23
 */
#include "host_threads.h"

#include <stddef.h>

#if defined(_WIN32)
#include <windows.h>

/** @brief The work the threads of the current run do, read by each as it starts. */
static void (*s_each)(void);

/** @brief A Windows thread's entry, doing the run's work once. */
static DWORD WINAPI host_threads_entry(LPVOID unused)
{
    (void)unused;
    s_each();
    return 0u;
}

int host_threads_run(void (*each)(void), unsigned int count)
{
    if (count > HOST_THREADS_MOST)
    {
        return 0;
    }
    HANDLE threads[HOST_THREADS_MOST];
    int started = 1;
    s_each = each;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        threads[at] = CreateThread(NULL, 0u, host_threads_entry, NULL, 0u, NULL);
        started = started && (threads[at] != NULL);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (threads[at] != NULL)
        {
            (void)WaitForSingleObject(threads[at], INFINITE);
            (void)CloseHandle(threads[at]);
        }
    }
    return started;
}
#else
#include <pthread.h>

/** @brief The work the threads of the current run do, read by each as it starts. */
static void (*s_each)(void);

/** @brief A POSIX thread's entry, doing the run's work once. */
static void *host_threads_entry(void *unused)
{
    (void)unused;
    s_each();
    return NULL;
}

int host_threads_run(void (*each)(void), unsigned int count)
{
    if (count > HOST_THREADS_MOST)
    {
        return 0;
    }
    pthread_t threads[HOST_THREADS_MOST];
    int made[HOST_THREADS_MOST];
    int started = 1;
    s_each = each;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        made[at] = pthread_create(&threads[at], NULL, host_threads_entry, NULL) == 0;
        started = started && made[at];
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (made[at])
        {
            (void)pthread_join(threads[at], NULL);
        }
    }
    return started;
}
#endif
