/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file turnstile_resize_drain_threads.c
 * @brief A drain while the set grows and contracts. One worker makes two passes and one worker makes one, through a
 *        set of one lane on one start lane, while a writer drains. GenMC runs each execution that the memory model
 *        allows. Each pass holds a lane below the open count, and the drain ends. At the end, no lane below the open
 *        count has the closed bit or the busy bit.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note A client for GenMC, run by harness.py prove. The host's waits are those of thread_host.h: a sleep that only
 *       a time limit ends leaves its thread blocked, and GenMC reports that execution.
 * @note The second pass of the first worker races the growth and the contraction of the second worker, while the drain
 *       reads the lanes. turnstile_resize_threads.c races two passes of each worker with no writer.
 */
// Two lanes: the start lane and the lane a growth opens
#define EMBED_TURNSTILE_LANES 2u

#include "thread_host.h"

#include <assert.h>
#include <pthread.h>

/** @brief The set, zeros at rest: one lane open. */
static EmbedTurnstiles s_set;
/** @brief The count of the wakes of each lane word. */
static embed_atomic_word s_wake_counts[EMBED_TURNSTILE_LANES];

/** @brief The count of the wakes of word: a lane word of the set, the only word that a thread sleeps on. */
static embed_atomic_word *thread_host_wake_count(const embed_atomic_word *word)
{
    assert((word == &s_set.lanes[0].word) || (word == &s_set.lanes[1].word));
    return &s_wake_counts[word == &s_set.lanes[1].word];
}

/** @brief A count of passes from start lane 0. */
static void passes(unsigned int pass_count)
{
    for (unsigned int pass_index = 0u; pass_index < pass_count; pass_index += 1u)
    {
        const unsigned int place =
            EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = 0u);
        const embed_atomic_word_value state =
            EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_set.lanes[place].word) &
            (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT);

        // The lane that a pass holds is open: it has the busy bit and not the closed bit
        assert(state == EMBED_TURNSTILE_BUSY_BIT);
        EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = place);
    }
}

/** @brief The first worker: two passes. */
static void *first_worker(void *unused)
{
    (void)unused;
    passes(2u);
    return NULL;
}

/** @brief The second worker: one pass. */
static void *second_worker(void *unused)
{
    (void)unused;
    passes(1u);
    return NULL;
}

/** @brief The writer: one drain. */
static void *writer(void *unused)
{
    (void)unused;
    EMBED_CALL(embed_turnstile_table.drain, EmbedTurnstilesDrainCfg, .set = &s_set);
    return NULL;
}

int main(void)
{
    pthread_t threads[3];

    (void)pthread_create(&threads[0], NULL, first_worker, NULL);
    (void)pthread_create(&threads[1], NULL, second_worker, NULL);
    (void)pthread_create(&threads[2], NULL, writer, NULL);
    for (unsigned int thread_index = 0u; thread_index < 3u; thread_index += 1u)
    {
        (void)pthread_join(threads[thread_index], NULL);
    }
    // Each pass ended: each lane below the open count is free, each lane above it is free or closed
    const unsigned int open_lane_count =
        EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set);

    for (unsigned int place = 0u; place < 2u; place += 1u)
    {
        const embed_atomic_word_value state =
            EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_set.lanes[place].word) &
            (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT);

        assert((place < open_lane_count)
                   ? (state == 0u)
                   : ((state == 0u) || (state == (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT))));
    }
    assert(EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_set.queue_length) == 0u);
    return 0;
}
