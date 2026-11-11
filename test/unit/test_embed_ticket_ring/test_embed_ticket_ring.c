/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_ticket_ring.c
 * @brief Holds the ticket ring to its contract: the stamp's two fields, the place of a ticket and its roll at the
 *        count of slots and at 32 bits, a pop that reads what the push of its ticket wrote, and pushes from the
 *        host's threads inside passes through a turnstile, each read whole after the drain.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-11
 */
#include "embed_ticket_ring.h"
#include "host_threads.h"
#include "host_wait.h"

#include "unity.h"

/** @brief The ring of the cases: 2^RING_SHIFT slots. */
#define RING_SHIFT 6u
#define RING_SLOTS (1u << RING_SHIFT)

/** @brief The threads of the contention case, and the pushes each makes, which together fill the ring once. */
#define RING_THREADS 8u
#define RING_PUSHES (RING_SLOTS / RING_THREADS)

static EmbedTicketRingSlot s_slots[RING_SLOTS];
static EmbedTicketRing s_ring;
static EmbedTurnstiles s_set;
static embed_atomic_u32 s_next_ticket = EMBED_ATOMIC_INIT(0u);
static embed_u32 s_jobs[RING_SLOTS];

void setUp(void)
{
    for (unsigned int at = 0u; at < RING_SLOTS; at += 1u)
    {
        s_slots[at].stamp = 0u;
        s_slots[at].job = NULL;
        s_jobs[at] = at;
    }
    embed_ticket_ring_begin(&s_ring, s_slots, RING_SHIFT);
    embed_atomic_u32_store(&s_next_ticket, 0u);
}

void tearDown(void)
{
}

void test_the_stamp_gives_its_epoch_and_ticket_back(void)
{
    const embed_u64 stamp = embed_ticket_stamp(0x12345678u, 0x9abcdef0u);

    TEST_ASSERT_EQUAL_HEX64(0x123456789abcdef0ull, stamp);
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, embed_epoch_of_stamp(stamp));
    TEST_ASSERT_EQUAL_HEX32(0x9abcdef0u, embed_ticket_of_stamp(stamp));
}

void test_the_mask_is_the_count_of_slots_less_one(void)
{
    TEST_ASSERT_EQUAL_UINT64(RING_SLOTS - 1u, s_ring.mask);
    TEST_ASSERT_EQUAL_PTR(s_slots, s_ring.slots);
}

void test_the_place_is_the_low_bits_of_the_ticket(void)
{
    for (embed_u32 ticket = 0u; ticket < 4u * RING_SLOTS; ticket += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(ticket % RING_SLOTS, embed_ticket_ring_place(&s_ring, embed_ticket_stamp(7u, ticket)));
    }
}

void test_a_ticket_a_ring_on_lands_at_the_same_place(void)
{
    for (embed_u32 ticket = 0u; ticket < RING_SLOTS; ticket += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(embed_ticket_ring_place(&s_ring, embed_ticket_stamp(0u, ticket)),
                                 embed_ticket_ring_place(&s_ring, embed_ticket_stamp(0u, ticket + RING_SLOTS)));
    }
}

void test_the_ticket_rolls_at_32_bits_onto_the_next_place(void)
{
    TEST_ASSERT_EQUAL_UINT64(RING_SLOTS - 1u, embed_ticket_ring_place(&s_ring, embed_ticket_stamp(0u, 0xffffffffu)));
    TEST_ASSERT_EQUAL_UINT64(0u, embed_ticket_ring_place(&s_ring, embed_ticket_stamp(1u, 0u)));
}

void test_the_epoch_does_not_move_the_place(void)
{
    TEST_ASSERT_EQUAL_UINT64(embed_ticket_ring_place(&s_ring, embed_ticket_stamp(0u, 5u)),
                             embed_ticket_ring_place(&s_ring, embed_ticket_stamp(0xffffffffu, 5u)));
}

void test_a_pop_reads_what_the_push_of_its_ticket_wrote(void)
{
    const embed_u64 stamp = embed_ticket_stamp(3u, 9u);
    embed_u64 read = 0u;

    embed_ticket_ring_push(&s_ring, stamp, &s_jobs[9]);
    TEST_ASSERT_EQUAL_PTR(&s_jobs[9], embed_ticket_ring_pop(&s_ring, stamp, &read));
    TEST_ASSERT_EQUAL_HEX64(stamp, read);
}

void test_a_push_at_another_place_leaves_a_slot(void)
{
    const embed_u64 kept = embed_ticket_stamp(1u, 2u);
    embed_u64 read = 0u;

    embed_ticket_ring_push(&s_ring, kept, &s_jobs[2]);
    embed_ticket_ring_push(&s_ring, embed_ticket_stamp(1u, 3u), &s_jobs[3]);
    TEST_ASSERT_EQUAL_PTR(&s_jobs[2], embed_ticket_ring_pop(&s_ring, kept, &read));
    TEST_ASSERT_EQUAL_HEX64(kept, read);
}

void test_a_push_a_ring_on_writes_over_the_slot(void)
{
    const embed_u64 later = embed_ticket_stamp(2u, 4u + RING_SLOTS);
    embed_u64 read = 0u;

    embed_ticket_ring_push(&s_ring, embed_ticket_stamp(1u, 4u), &s_jobs[4]);
    embed_ticket_ring_push(&s_ring, later, &s_jobs[5]);
    TEST_ASSERT_EQUAL_PTR(&s_jobs[5], embed_ticket_ring_pop(&s_ring, embed_ticket_stamp(1u, 4u), &read));
    TEST_ASSERT_EQUAL_HEX64(later, read);
}

void test_the_pushes_of_one_window_each_pop_whole(void)
{
    const embed_u32 first = 0xfffffff0u;

    for (embed_u32 step = 0u; step < RING_SLOTS; step += 1u)
    {
        embed_ticket_ring_push(&s_ring, embed_ticket_stamp(step, first + step), &s_jobs[step]);
    }
    for (embed_u32 step = 0u; step < RING_SLOTS; step += 1u)
    {
        embed_u64 read = 0u;

        TEST_ASSERT_EQUAL_PTR(&s_jobs[step], embed_ticket_ring_pop(&s_ring, embed_ticket_stamp(0u, first + step), &read));
        TEST_ASSERT_EQUAL_HEX64(embed_ticket_stamp(step, first + step), read);
    }
}

/** @brief One thread's pushes: each ticket from the counter, each push inside a pass at this thread's address. */
static void pushes_in_passes(void)
{
    embed_u32 own = 0u;
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(uintptr_t)&own;

    for (unsigned int round = 0u; round < RING_PUSHES; round += 1u)
    {
        const embed_u32 ticket = embed_atomic_u32_fetch_add(&s_next_ticket, 1u);

        embed_ticket_ring_pushed(&s_set, address, &s_ring, embed_ticket_stamp(round, ticket), &s_jobs[ticket]);
    }
}

void test_pushes_inside_passes_are_each_whole_after_the_drain(void)
{
    for (unsigned int at = 0u; at < EMBED_TURNSTILE_LANES; at += 1u)
    {
        embed_atomic_word_store(&s_set.lane[at].word, 0u);
    }
    embed_atomic_word_store(&s_set.open, 0u);
    embed_atomic_u32_store(&s_set.queue, 0u);

    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(pushes_in_passes, RING_THREADS), "every thread started");
    embed_turnstiles_drain(&s_set);
    TEST_ASSERT_EQUAL_UINT32(RING_SLOTS, embed_atomic_u32_load(&s_next_ticket));
    for (embed_u32 ticket = 0u; ticket < RING_SLOTS; ticket += 1u)
    {
        embed_u64 read = 0u;
        void *const job = embed_ticket_ring_popped(&s_set, (embed_atomic_uptr_value)(uintptr_t)&read, &s_ring,
                                                   embed_ticket_stamp(0u, ticket), &read);

        TEST_ASSERT_EQUAL_PTR(&s_jobs[ticket], job);
        TEST_ASSERT_EQUAL_HEX32(ticket, embed_ticket_of_stamp(read));
    }
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
}
