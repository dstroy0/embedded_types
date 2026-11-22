/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file thread_host.h
 * @brief The host that a thread client runs on under GenMC. It has no processor pause, a clock for each thread that
 *        moves forward one unit at each read, and the address wait and wake of the futex contract.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note Include it first in a client, before each other header of this library, and one time.
 * @note The client defines thread_host_wake_count over each word that a thread sleeps on. Each word has a count of
 *       its own, as a futex has, so a wake on one word ends no sleep on another.
 * @note The clock moves one unit at each read, so each backoff step after the first read waits. A step inside the first
 *       unit only pauses and reads the word again, which changes no shared word. A wait that returns at once does the
 *       same, so the client covers that band. The Backoff proof under theory/ proves that the band makes no call.
 * @note The clock of a thread is a word of that thread alone. A backoff takes the difference of two reads of one
 *       thread, so no other thread reads the clock, and no order of the reads of two threads changes a result.
 * @note The wait reads the count of wakes of its word, then the word, and returns where the low 32 bits of the word
 *       moved. A wait of 0 ns returns at once, as the host's does. Each other wait ends only where the count moves
 *       past the one it read.
 * @note The end of the wait is a loop that reads only the count. GenMC takes the loop as a wait, and its liveness check
 *       reports a sleep that no wake ends: a lost wake.
 */
#ifndef EMBED_TEST_THREAD_HOST_H
#define EMBED_TEST_THREAD_HOST_H

#include <genmc.h>
#include <stdint.h>

// No processor runs the client: a spin's pause is no instruction
#define EMBED_TURNSTILE_SPIN_PAUSE() ((void)0)

#include "embed_turnstile.h"

/** @brief The count of the wakes of word. The client defines it over each word that a thread sleeps on. */
static embed_atomic_word *thread_host_wake_count(const embed_atomic_word *word);
/** @brief The clock of each thread, in nanoseconds. */
static _Thread_local embed_word t_thread_host_clock;

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    embed_atomic_word *const wakes = thread_host_wake_count(args->word);
    const embed_atomic_word_value wake_count =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = wakes);

    // Explicit casts retain the low 32 bits of each value, the bits the futex compares
    if ((uint32_t)EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = args->word) !=
        (uint32_t)args->old_value)
    {
        return 1;
    }
    if (args->nanoseconds == 0u)
    {
        return 0;
    }
    // A loop that reads only the count of wakes. The spin-assume of GenMC takes it as a wait, and its liveness check
    // reports an execution in which no wake ever ends the wait
    while (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = wakes) == wake_count)
    {
        // The condition reads the count of wakes, and the loop has no other step
    }
    return 0;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    (void)EMBED_CALL(embed_atomic_word_table.fetch_add, EmbedAtomicWordFetchAddCfg,
                     .word = thread_host_wake_count(args->word), .value = 1u);
}

embed_word embed_clock_ns(void)
{
    t_thread_host_clock += (embed_word)1u << EMBED_TURNSTILE_UNIT_SHIFT;
    return t_thread_host_clock;
}

#endif
