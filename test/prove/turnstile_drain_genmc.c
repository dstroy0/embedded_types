/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file turnstile_drain_genmc.c
 * @brief Two readers pass through a turnstile set while a writer swaps a published word and drains, in each
 *        execution that the memory model allows. A reader that reads the old word reads it before the writer
 *        releases it, and no thread stays asleep with no wake to come.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * @note A client for GenMC, run by harness.py prove. The host's waits are those of genmc_host.h: a sleep that only
 *       a time limit ends leaves its thread blocked, and GenMC reports that execution.
 */
#include "genmc_host.h"

#include <assert.h>
#include <pthread.h>

/** @brief The set, zeros at rest. */
static EmbedTurnstiles s_set;
/** @brief The published word: 0 is the old one, 1 the new one. */
static embed_atomic_word s_published;
/** @brief Set to 1 when the writer has released the old word. */
static embed_atomic_word s_released;

/** @brief One reader's pass: badge in, read the published word, and leave. */
static void *reader(void *id)
{
    unsigned int at = EMBED_TURNSTILE_NONE;

    while (at == EMBED_TURNSTILE_NONE)
    {
        at = embed_turnstile_enter(&s_set, (embed_atomic_word_value)(uintptr_t)id);
    }
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

    (void)pthread_create(&threads[0], NULL, reader, (void *)(uintptr_t)1u);
    (void)pthread_create(&threads[1], NULL, reader, (void *)(uintptr_t)2u);
    (void)pthread_create(&threads[2], NULL, writer, NULL);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        (void)pthread_join(threads[at], NULL);
    }
    // each pass ended, so no lane is busy. A sleeper flag can stand after a sleep that its compare ended, and the
    // next pass's end clears it.
    assert((embed_atomic_word_load(&s_set.busy) & EMBED_TURNSTILE_LANE_BITS) == 0u);
    return 0;
}
