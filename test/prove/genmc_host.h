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
#define EMBED_TURNSTILE_PAUSE() ((void)0)

#include "embed_turnstile.h"

/** @brief The wakes issued, each word's. */
static embed_atomic_word s_genmc_wakes;
/** @brief The clock, in nanoseconds. */
static embed_atomic_word s_genmc_clock;

int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u64 nanoseconds)
{
    const embed_atomic_word_value epoch = embed_atomic_word_load(&s_genmc_wakes);

    // Explicit casts keep the low 32 bits of each value, the bits the futex compares
    if ((uint32_t)embed_atomic_word_load(word) != (uint32_t)seen)
    {
        return 1;
    }
    if (nanoseconds == 0u)
    {
        return 0;
    }
    // A loop that reads only the count of wakes. The spin-assume of GenMC takes it as a wait, and its liveness check
    // reports an execution in which no wake ever ends the wait
    while (embed_atomic_word_load(&s_genmc_wakes) == epoch)
    {
    }
    return 0;
}

void embed_word_wake(embed_atomic_word *word)
{
    (void)word;
    (void)embed_atomic_word_fetch_add(&s_genmc_wakes, 1u);
}

embed_u64 embed_clock_ns(void)
{
    const embed_atomic_word_value step = (embed_atomic_word_value)1u << EMBED_TURNSTILE_UNIT_SHIFT;

    return embed_atomic_word_fetch_add(&s_genmc_clock, step) + step;
}

#endif
