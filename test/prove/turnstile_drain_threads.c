/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file turnstile_drain_threads.c
 * @brief Two workers pass through a set of one lane on one start lane, while a writer swaps a published word and
 *        drains. GenMC runs each execution that the memory model allows. The second worker finds the lane busy and
 *        grows the set. A worker that reads the old word reads it before the writer releases it. No thread stays
 *        asleep with no wake to come, and each lane is free or closed at the end.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note A client for GenMC, run by harness.py prove. The host's waits are those of thread_host.h: a sleep that only
 *       a time limit ends leaves its thread blocked, and GenMC reports that execution.
 */
// Two lanes: the start lane and the lane the second worker grows the set to
#define EMBED_TURNSTILE_LANES 2u

#include "thread_host.h"

#include <assert.h>
#include <pthread.h>

/** @brief The set, zeros at rest: one lane open. */
static EmbedTurnstiles s_set;
/** @brief The published word: 0 is the old one, 1 the new one. */
static embed_atomic_word s_published;
/** @brief 1 after the writer releases the old word. */
static embed_atomic_word s_released;
/** @brief The count of the wakes of each lane word. */
static embed_atomic_word s_wake_counts[EMBED_TURNSTILE_LANES];

/** @brief The count of the wakes of word: a lane word of the set, the only word that a thread sleeps on. */
static embed_atomic_word *thread_host_wake_count(const embed_atomic_word *word)
{
    assert((word == &s_set.lanes[0].word) || (word == &s_set.lanes[1].word));
    return &s_wake_counts[word == &s_set.lanes[1].word];
}

/** @brief One worker's pass from start lane 0: enter, read the published word, and leave. */
static void *worker(void *unused)
{
    (void)unused;
    const unsigned int place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = 0u);
    const embed_atomic_word_value published_word =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_published);
    const embed_atomic_word_value release_word =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_released);

    // A pass that reads the old word reads it before its release
    assert((published_word != 0u) || (release_word == 0u));
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = place);
    return NULL;
}

/** @brief The writer: swap the published word, drain, and release the old word. */
static void *writer(void *unused)
{
    (void)unused;
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_published, .value = 1u);
    EMBED_CALL(embed_turnstile_table.drain, EmbedTurnstilesDrainCfg, .set = &s_set);
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_released, .value = 1u);
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
    // Each pass ended: each lane is free, or closed above the open count, and the queue is empty
    const unsigned int open_lane_count =
        EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set);

    for (unsigned int place = 0u; place < 2u; place += 1u)
    {
        const embed_atomic_word_value lane_word =
            EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_set.lanes[place].word);
        const embed_atomic_word_value state = lane_word & (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT);

        assert((state == 0u) ||
               ((state == (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT)) && (place >= open_lane_count)));
    }
    assert(EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_set.queue_length) == 0u);
    return 0;
}
