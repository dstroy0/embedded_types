/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file host_threads.h
 * @brief Host threads for a suite that needs contention, started where the platform's headers are
 *        out of the suite's sight.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-23
 *
 * @note Unity's generator copies every #include of a suite into its runner and reads no #if, so a
 *       suite that included windows.h on one arm and pthread.h on the other would hand the runner
 *       both. The platform's headers live in host_threads.c alone.
 */
#ifndef HOST_THREADS_H
#define HOST_THREADS_H

/** @brief The most threads host_threads_run starts at once. */
#define HOST_THREADS_MOST 16u

/**
 * @brief Runs count host threads, each calling each once, and waits for every one to end.
 *
 * @param[in] each  The work every thread does.
 * @param[in] count Threads to start, at most HOST_THREADS_MOST.
 * @return          1 where every thread started and ended, 0 where the system refused one or count
 *                  passed HOST_THREADS_MOST.
 */
int host_threads_run(void (*each)(void), unsigned int count);

#endif
