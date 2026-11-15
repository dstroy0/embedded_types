/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file turnstile_resize_genmc.c
 * @brief Two workers make two passes each through a set of one lane on one start lane, while a writer drains. GenMC
 *        runs each execution that the memory model allows. The passes grow the set and their ends contract it, and
 *        each growth races each contraction. Each pass holds a lane below the open count, and the drain ends. At the
 *        end, no lane below the open count has the closed bit or the busy bit.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note A client for GenMC, run by harness.py prove. The host's waits are those of genmc_host.h: a sleep that only
 *       a time limit ends leaves its thread blocked, and GenMC reports that execution.
 */
// Two lanes: the start lane and the lane a growth opens
#define EMBED_TURNSTILE_LANES 2u

#include "genmc_host.h"

#include <assert.h>
#include <pthread.h>

/** @brief The set, zeros at rest: one lane open. */
static EmbedTurnstiles s_set;

/** @brief One worker's two passes from start lane 0. */
static void *worker(void *unused)
{
    (void)unused;
    for (unsigned int pass_index = 0u; pass_index < 2u; pass_index += 1u)
    {
        const unsigned int place = embed_turnstile_enter(&s_set, 0u);
        const embed_atomic_word_value state =
            embed_atomic_word_load(&s_set.lanes[place].word) & (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT);

        // The lane that a pass holds is open: it has the busy bit and not the closed bit
        assert(state == EMBED_TURNSTILE_BUSY_BIT);
        embed_turnstile_leave(&s_set, place);
    }
    return NULL;
}

/** @brief The writer: one drain. */
static void *writer(void *unused)
{
    (void)unused;
    embed_turnstiles_drain(&s_set);
    return NULL;
}

int main(void)
{
    pthread_t threads[3];

    (void)pthread_create(&threads[0], NULL, worker, NULL);
    (void)pthread_create(&threads[1], NULL, worker, NULL);
    (void)pthread_create(&threads[2], NULL, writer, NULL);
    for (unsigned int thread_index = 0u; thread_index < 3u; thread_index += 1u)
    {
        (void)pthread_join(threads[thread_index], NULL);
    }
    // Each pass ended: each lane below the open count is free, each lane above it is free or closed
    const unsigned int open_lane_count = embed_turnstile_open_lane_count(&s_set);

    for (unsigned int place = 0u; place < 2u; place += 1u)
    {
        const embed_atomic_word_value state =
            embed_atomic_word_load(&s_set.lanes[place].word) & (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT);

        assert((place < open_lane_count)
                   ? (state == 0u)
                   : ((state == 0u) || (state == (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT))));
    }
    assert(embed_atomic_u32_load(&s_set.queue_length) == 0u);
    return 0;
}
