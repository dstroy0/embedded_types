/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file ticket_ring_genmc.c
 * @brief The life of one slot under contention, in each execution that the memory model allows. Two workers pop one
 *        stamp inside passes, as a worker with a flipped ticket and the owner collide. One pop lands and reads the
 *        job that the push stored. A writer drains and releases the slot. At the end the word has every flag of
 *        the life, the lane of the pop that landed, and no pass holds a lane.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-11
 *
 * @note A client for GenMC, run by harness.py prove. The waits are those of genmc_host.h.
 */
// Two lanes: the start lane and the lane the second worker grows the set to
#define EMBED_TURNSTILE_LANES 2u

#include "genmc_host.h"

#include "embed_ticket_ring.h"

#include <assert.h>
#include <pthread.h>

/** @brief The set, zeros at rest: one lane open. */
static EmbedTurnstiles s_set;
/** @brief One slot, and the ring over it. */
static EmbedTicketRingSlot s_slots[1];
static EmbedTicketRing s_ring;
/** @brief The job of the push. */
static int s_job;
/** @brief The pops that landed. */
static embed_atomic_u32 s_landed;
/** @brief 1 after the release lands. */
static embed_atomic_u32 s_released;

/** @brief One worker: a pop of the stamp inside a pass at its own address, and the count of the pop in the pass. */
static void *worker(void *unused)
{
    embed_u64 seen = 0u;
    void *job = NULL;
    (void)unused;
    const unsigned int lane = embed_turnstile_enter(&s_set, (embed_atomic_uptr_value)(uintptr_t)&seen);

    if (embed_ticket_ring_pop(&s_ring, embed_ticket_stamp(1u, 0u), lane, &job, &seen))
    {
        // the pop that succeeds reads the job of the push, and its count completes before the pass ends
        assert(job == &s_job);
        (void)embed_atomic_u32_fetch_add(&s_landed, 1u);
    }
    else
    {
        // the pop that fails finds the popped flag in the word
        assert(job == NULL);
        assert((seen & EMBED_TICKET_PUSHED) != 0u);
    }
    embed_turnstile_leave(&s_set, lane);
    return NULL;
}

/** @brief The writer: where it observes the popped bit, the drain and then the release of the stamp. */
static void *writer(void *unused)
{
    embed_u64 seen = 0u;
    (void)unused;
    const embed_u64 word = embed_atomic_u64_load(&s_slots[0].stamp);

    if ((word & EMBED_TICKET_POPPED) != 0u)
    {
        // the pass of the observed pop started before the drain, so the drain waits for it, and its count is complete
        // when the release runs
        assert(embed_ticket_ring_released(&s_set, &s_ring, embed_ticket_stamp(1u, 0u), &seen));
        assert(embed_atomic_u32_load(&s_landed) == 1u);
        embed_atomic_u32_store(&s_released, 1u);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[3];
    embed_u64 seen = 0u;
    const embed_u64 stamp = embed_ticket_stamp(1u, 0u);

    embed_ticket_ring_begin(&s_ring, s_slots, 0u);
    assert(embed_ticket_ring_grant(&s_ring, stamp, &seen));
    assert(!embed_ticket_ring_grant(&s_ring, stamp, &seen));
    assert(embed_ticket_ring_push(&s_ring, stamp, &s_job, &seen));
    assert(!embed_ticket_ring_push(&s_ring, stamp, &s_job, &seen));

    (void)pthread_create(&threads[0], NULL, worker, NULL);
    (void)pthread_create(&threads[1], NULL, worker, NULL);
    (void)pthread_create(&threads[2], NULL, writer, NULL);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        (void)pthread_join(threads[at], NULL);
    }
    // one pop of the two landed, and the other failed
    assert(embed_atomic_u32_load(&s_landed) == 1u);
    // the release landed where the drain ran after the pop, and the word then carries every flag of the life
    const embed_u64 word = embed_atomic_u64_load(&s_slots[0].stamp);

    assert((word & (EMBED_TICKET_GRANTED | EMBED_TICKET_PUSHED | EMBED_TICKET_POPPED)) ==
           (EMBED_TICKET_GRANTED | EMBED_TICKET_PUSHED | EMBED_TICKET_POPPED));
    assert(((word & EMBED_TICKET_RELEASED) != 0u) == (embed_atomic_u32_load(&s_released) == 1u));
    assert(embed_ticket_fields_of_stamp(word) == stamp);
    assert(embed_ticket_lane_of_stamp(word) < EMBED_TURNSTILE_LANES);
    // each pass ended: each lane is free, or closed above the open count, and the queue is empty
    const unsigned int count = embed_turnstile_lanes(&s_set);

    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const embed_atomic_word_value lane = embed_atomic_word_load(&s_set.lane[at].word);
        const embed_atomic_word_value state = lane & (EMBED_TURNSTILE_BUSY | EMBED_TURNSTILE_CLOSED);

        assert((state == 0u) || ((state == (EMBED_TURNSTILE_BUSY | EMBED_TURNSTILE_CLOSED)) && (at >= count)));
    }
    assert(embed_atomic_u32_load(&s_set.queue) == 0u);
    return 0;
}
