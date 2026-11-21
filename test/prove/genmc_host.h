/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file genmc_host.h
 * @brief The host that a GenMC client runs on. It has no processor pause, a clock that moves forward one unit at each
 *        read, and the address wait and wake of the futex contract.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note Include it first in a client, before each other header of this library, and one time.
 * @note The clock moves one unit at each read, so each backoff step after the first read waits. A step inside the first
 *       unit only pauses and reads the word again, which changes no shared word. A wait that returns at once does the
 *       same, so the client covers that band. The Backoff proof under theory/ proves that the band makes no call.
 * @note The wait reads the count of wakes, then the word, and returns where the low 32 bits of the word moved. A
 *       wait of 0 ns returns at once, as the host's does. Each other wait ends only where the count of wakes moves
 *       past the one it read. Thus a sleep that only its time limit ends stays asleep here, and GenMC reports the
 *       execution as blocked: a lost wake.
 * @note A wake adds one to the count, and each wait that the count moves past ends. One count serves each word, as
 *       the contract allows a spurious wake.
 */
#ifndef EMBED_TEST_GENMC_HOST_H
#define EMBED_TEST_GENMC_HOST_H

#include <genmc.h>
#include <stdint.h>

// No processor runs the client: a spin's pause is no instruction
#define EMBED_TURNSTILE_SPIN_PAUSE() ((void)0)

#include "embed_turnstile.h"

/** @brief The wakes issued, each word's. */
static embed_atomic_word s_genmc_wakes;
/** @brief The clock, in nanoseconds. */
static embed_atomic_word s_genmc_clock;

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    const embed_atomic_word_value wake_count =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_genmc_wakes);

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
    while (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_genmc_wakes) == wake_count)
    {
        // The condition reads the count of wakes, and the loop has no other step
    }
    return 0;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    (void)args->word;
    (void)EMBED_CALL(embed_atomic_word_table.fetch_add, EmbedAtomicWordFetchAddCfg, .word = &s_genmc_wakes,
                     .value = 1u);
}

embed_word embed_clock_ns(void)
{
    const embed_atomic_word_value step = (embed_atomic_word_value)1u << EMBED_TURNSTILE_UNIT_SHIFT;

    // Explicit cast takes the clock modulo the word, which is the contract of the clock
    return (embed_word)(EMBED_CALL(embed_atomic_word_table.fetch_add, EmbedAtomicWordFetchAddCfg,
                                   .word = &s_genmc_clock, .value = step) +
                        step);
}

#endif
