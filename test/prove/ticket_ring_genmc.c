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
/** @brief The count of the pops that landed. */
static embed_atomic_u32 s_landed_pops;
/** @brief 1 after the release lands. */
static embed_atomic_u32 s_release_mark;

/** @brief One worker: a pop of the stamp inside a pass at its own address, and the count of the pop in the pass. */
static void *worker(void *unused)
{
    embed_word found_stamp = 0u;
    void *job = NULL;
    (void)unused;
    // Explicit casts carry the address of a local of this worker as the integer that the entry takes
    const unsigned int place = embed_turnstile_enter(&s_set, (embed_atomic_uptr_value)(uintptr_t)&found_stamp);

    if (embed_ticket_ring_pop(&s_ring, embed_ticket_stamp(1u, 0u), place, &job, &found_stamp))
    {
        // The pop that succeeds reads the job of the push, and its count completes before the pass ends
        assert(job == &s_job);
        (void)embed_atomic_u32_fetch_add(&s_landed_pops, 1u);
    }
    else
    {
        // The pop that fails finds the pushed bit in the found stamp
        assert(job == NULL);
        assert((found_stamp & EMBED_TICKET_PUSHED_BIT) != 0u);
    }
    embed_turnstile_leave(&s_set, place);
    return NULL;
}

/** @brief The writer: where it observes the popped bit, the drain and then the release of the stamp. */
static void *writer(void *unused)
{
    embed_word found_stamp = 0u;
    (void)unused;
    const embed_word slot_stamp = embed_atomic_word_load(&s_slots[0].stamp);

    if ((slot_stamp & EMBED_TICKET_POPPED_BIT) != 0u)
    {
        // The pass of the observed pop started before the drain, so the drain waits for it, and its count is complete
        // when the release runs
        assert(embed_ticket_ring_drain_release(&s_set, &s_ring, embed_ticket_stamp(1u, 0u), &found_stamp));
        assert(embed_atomic_u32_load(&s_landed_pops) == 1u);
        embed_atomic_u32_store(&s_release_mark, 1u);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[3];
    embed_word found_stamp = 0u;
    const embed_word stamp = embed_ticket_stamp(1u, 0u);
    const embed_word life_bits = EMBED_TICKET_GRANTED_BIT | EMBED_TICKET_PUSHED_BIT | EMBED_TICKET_POPPED_BIT;

    embed_ticket_ring_begin(&s_ring, s_slots, 0u);
    assert(embed_ticket_ring_grant(&s_ring, stamp, &found_stamp));
    assert(!embed_ticket_ring_grant(&s_ring, stamp, &found_stamp));
    assert(embed_ticket_ring_push(&s_ring, stamp, &s_job, &found_stamp));
    assert(!embed_ticket_ring_push(&s_ring, stamp, &s_job, &found_stamp));

    (void)pthread_create(&threads[0], NULL, worker, NULL);
    (void)pthread_create(&threads[1], NULL, worker, NULL);
    (void)pthread_create(&threads[2], NULL, writer, NULL);
    for (unsigned int thread_index = 0u; thread_index < 3u; thread_index += 1u)
    {
        (void)pthread_join(threads[thread_index], NULL);
    }
    // One pop of the two landed, and the other failed
    assert(embed_atomic_u32_load(&s_landed_pops) == 1u);
    // The release landed where the drain ran after the pop, and the slot stamp then carries every flag of the life
    const embed_word slot_stamp = embed_atomic_word_load(&s_slots[0].stamp);

    assert((slot_stamp & life_bits) == life_bits);
    assert(((slot_stamp & EMBED_TICKET_RELEASED_BIT) != 0u) == (embed_atomic_u32_load(&s_release_mark) == 1u));
    assert(embed_ticket_fields_of_stamp(slot_stamp) == stamp);
    assert(embed_ticket_lane_of_stamp(slot_stamp) < EMBED_TURNSTILE_LANES);
    // Each pass ended: each lane is free, or closed above the open count, and the queue is empty
    const unsigned int open_lane_count = embed_turnstile_open_lane_count(&s_set);

    for (unsigned int place = 0u; place < 2u; place += 1u)
    {
        const embed_atomic_word_value lane_word = embed_atomic_word_load(&s_set.lanes[place].word);
        const embed_atomic_word_value state = lane_word & (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT);

        assert((state == 0u) ||
               ((state == (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT)) && (place >= open_lane_count)));
    }
    assert(embed_atomic_u32_load(&s_set.queue_length) == 0u);
    return 0;
}
