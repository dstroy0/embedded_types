/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file turnstile_drain_genmc.c
 * @brief Two workers pass through a set of one lane on one start lane, while a writer swaps a published word and
 *        drains. GenMC runs each execution that the memory model allows. The second worker finds the lane busy and
 *        grows the set. A worker that reads the old word reads it before the writer releases it. No thread stays
 *        asleep with no wake to come, and each lane is free or closed at the end.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note A client for GenMC, run by harness.py prove. The host's waits are those of genmc_host.h: a sleep that only
 *       a time limit ends leaves its thread blocked, and GenMC reports that execution.
 */
// Two lanes: the start lane and the lane the second worker grows the set to
#define EMBED_TURNSTILE_LANES 2u

#include "genmc_host.h"

#include <assert.h>
#include <pthread.h>

/** @brief The set, zeros at rest: one lane open. */
static EmbedTurnstiles s_set;
/** @brief The published word: 0 is the old one, 1 the new one. */
static embed_atomic_word s_published;
/** @brief 1 after the writer releases the old word. */
static embed_atomic_word s_released;

/** @brief One worker's pass from start lane 0: enter, read the published word, and leave. */
static void *worker(void *unused)
{
    (void)unused;
    const unsigned int at = embed_turnstile_enter(&s_set, 0u);
    const embed_atomic_word_value seen = embed_atomic_word_load(&s_published);
    const embed_atomic_word_value released = embed_atomic_word_load(&s_released);

    // a pass that reads the old word reads it before its release
    assert((seen != 0u) || (released == 0u));
    embed_turnstile_leave(&s_set, at);
    return NULL;
}

/** @brief The writer: swap the published word, drain, and release the old word. */
static void *writer(void *unused)
{
    (void)unused;
    embed_atomic_word_store(&s_published, 1u);
    embed_turnstiles_drain(&s_set);
    embed_atomic_word_store(&s_released, 1u);
    return NULL;
}

int main(void)
{
    pthread_t threads[3];

    (void)pthread_create(&threads[0], NULL, worker, NULL);
    (void)pthread_create(&threads[1], NULL, worker, NULL);
    (void)pthread_create(&threads[2], NULL, writer, NULL);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        (void)pthread_join(threads[at], NULL);
    }
    // each pass ended: each lane is free, or closed above the open count, and the queue is empty
    const unsigned int count = embed_turnstile_lanes(&s_set);

    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const embed_atomic_word_value word = embed_atomic_word_load(&s_set.lane[at].word);
        const embed_atomic_word_value state = word & (EMBED_TURNSTILE_BUSY | EMBED_TURNSTILE_CLOSED);

        assert((state == 0u) || ((state == (EMBED_TURNSTILE_BUSY | EMBED_TURNSTILE_CLOSED)) && (at >= count)));
    }
    assert(embed_atomic_u32_load(&s_set.queue) == 0u);
    return 0;
}
