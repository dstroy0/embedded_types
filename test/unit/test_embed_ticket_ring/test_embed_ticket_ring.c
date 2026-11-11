/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_ticket_ring.c
 * @brief Holds the ticket ring to its contract. The stamp has three fields. The place of a ticket rolls at the count
 *        of slots and at 32 bits. Each step of the life of a slot lands one time, in its order, and a flipped stamp
 *        refuses it. Pushes from the threads of the host run inside passes through a turnstile, and each pop after
 *        the drain reads its own job.
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

/** @brief The flags of a slot after each step of its life, lane bits aside. */
#define LIFE_GRANTED EMBED_TICKET_GRANTED
#define LIFE_PUSHED (LIFE_GRANTED | EMBED_TICKET_PUSHED)
#define LIFE_POPPED (LIFE_PUSHED | EMBED_TICKET_POPPED)
#define LIFE_RELEASED (LIFE_POPPED | EMBED_TICKET_RELEASED)

static EmbedTicketRingSlot s_slots[RING_SLOTS];
static EmbedTicketRing s_ring;
static EmbedTurnstiles s_set;
static embed_atomic_u32 s_next_ticket = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_refused = EMBED_ATOMIC_INIT(0u);
static embed_u32 s_jobs[RING_SLOTS];

void setUp(void)
{
    for (unsigned int at = 0u; at < RING_SLOTS; at += 1u)
    {
        embed_atomic_u64_store(&s_slots[at].stamp, 0u);
        s_slots[at].job = NULL;
        s_jobs[at] = at;
    }
    embed_ticket_ring_begin(&s_ring, s_slots, RING_SHIFT);
    embed_atomic_u32_store(&s_next_ticket, 0u);
    embed_atomic_u32_store(&s_refused, 0u);
    for (unsigned int at = 0u; at < EMBED_TURNSTILE_LANES; at += 1u)
    {
        embed_atomic_word_store(&s_set.lane[at].word, 0u);
    }
    embed_atomic_word_store(&s_set.open, 0u);
    embed_atomic_u32_store(&s_set.queue, 0u);
}

void tearDown(void)
{
}

/** @brief The three fields of a stamp as the stamp holds them, flags aside. */
static embed_u64 fields(embed_u16 epoch, embed_u32 ticket)
{
    return embed_ticket_stamp(epoch, ticket);
}

void test_the_stamp_gives_its_epoch_and_ticket_back(void)
{
    const embed_u64 stamp = embed_ticket_stamp(0x1234u, 0x9abcdef0u);

    TEST_ASSERT_EQUAL_HEX64(0x123400009abcdef0ull, stamp);
    TEST_ASSERT_EQUAL_HEX16(0x1234u, embed_epoch_of_stamp(stamp));
    TEST_ASSERT_EQUAL_HEX32(0x9abcdef0u, embed_ticket_of_stamp(stamp));
    TEST_ASSERT_EQUAL_HEX16(0u, embed_ticket_flags_of_stamp(stamp));
    TEST_ASSERT_EQUAL_HEX64(stamp, embed_ticket_fields_of_stamp(stamp | LIFE_POPPED));
}

void test_the_flags_sit_between_the_ticket_and_the_epoch(void)
{
    const embed_u64 stamp = embed_ticket_stamp(0xffffu, 0xffffffffu) | LIFE_RELEASED;

    TEST_ASSERT_EQUAL_HEX16(0xffffu, embed_epoch_of_stamp(stamp));
    TEST_ASSERT_EQUAL_HEX32(0xffffffffu, embed_ticket_of_stamp(stamp));
    TEST_ASSERT_EQUAL_HEX16(0x17u, embed_ticket_flags_of_stamp(stamp));
    TEST_ASSERT_EQUAL_UINT(5u, embed_ticket_lane_of_stamp(stamp | ((embed_u64)5u << EMBED_TICKET_LANE_SHIFT)));
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
        TEST_ASSERT_EQUAL_UINT64(ticket % RING_SLOTS, embed_ticket_ring_place(&s_ring, fields(7u, ticket)));
    }
}

void test_a_ticket_a_ring_on_lands_at_the_same_place(void)
{
    for (embed_u32 ticket = 0u; ticket < RING_SLOTS; ticket += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(embed_ticket_ring_place(&s_ring, fields(0u, ticket)),
                                 embed_ticket_ring_place(&s_ring, fields(0u, ticket + RING_SLOTS)));
    }
}

void test_the_ticket_rolls_at_32_bits_onto_the_next_place(void)
{
    TEST_ASSERT_EQUAL_UINT64(RING_SLOTS - 1u, embed_ticket_ring_place(&s_ring, fields(0u, 0xffffffffu)));
    TEST_ASSERT_EQUAL_UINT64(0u, embed_ticket_ring_place(&s_ring, fields(1u, 0u)));
}

void test_the_epoch_and_the_flags_do_not_move_the_place(void)
{
    TEST_ASSERT_EQUAL_UINT64(embed_ticket_ring_place(&s_ring, fields(0u, 5u)),
                             embed_ticket_ring_place(&s_ring, fields(0xffffu, 5u) | LIFE_RELEASED));
}

void test_the_life_of_a_slot_lands_each_step_once_in_order(void)
{
    const embed_u64 stamp = fields(3u, 9u);
    embed_u64 seen = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE_MESSAGE(embed_ticket_ring_grant(&s_ring, stamp, &seen), "the grant lands on an empty word");
    TEST_ASSERT_EQUAL_HEX64(0u, seen);
    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_grant(&s_ring, stamp, &seen), "a second grant finds the granted word");
    TEST_ASSERT_EQUAL_HEX64(stamp | LIFE_GRANTED, seen);

    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_pop(&s_ring, stamp, 0u, &job, &seen), "a pop before the push fails");
    TEST_ASSERT_NULL(job);
    TEST_ASSERT_TRUE_MESSAGE(embed_ticket_ring_push(&s_ring, stamp, &s_jobs[9], &seen), "the push lands");
    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_push(&s_ring, stamp, &s_jobs[9], &seen), "a second push fails");
    TEST_ASSERT_EQUAL_HEX64(stamp | LIFE_PUSHED, seen);

    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_release(&s_ring, stamp, &seen), "a release before the pop fails");
    TEST_ASSERT_TRUE_MESSAGE(embed_ticket_ring_pop(&s_ring, stamp, 5u, &job, &seen), "the pop lands");
    TEST_ASSERT_EQUAL_PTR(&s_jobs[9], job);
    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_pop(&s_ring, stamp, 6u, &job, &seen), "a second pop fails");
    TEST_ASSERT_NULL(job);
    TEST_ASSERT_EQUAL_HEX64(stamp | LIFE_POPPED | ((embed_u64)5u << EMBED_TICKET_LANE_SHIFT), seen);
    TEST_ASSERT_EQUAL_UINT(5u, embed_ticket_lane_of_stamp(seen));

    TEST_ASSERT_TRUE_MESSAGE(embed_ticket_ring_replay(&s_ring, stamp, &job, &seen), "the replay lands");
    TEST_ASSERT_EQUAL_PTR(&s_jobs[9], job);
    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_replay(&s_ring, stamp, &job, &seen), "a second replay fails");
    TEST_ASSERT_NULL(job);

    TEST_ASSERT_TRUE_MESSAGE(embed_ticket_ring_release(&s_ring, stamp, &seen), "the release lands after the replay");
    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_release(&s_ring, stamp, &seen), "a second release fails");
    TEST_ASSERT_EQUAL_HEX16(0x1fu, (embed_u16)(embed_ticket_flags_of_stamp(seen) & 0x1fu));
}

void test_a_release_lands_with_no_replay(void)
{
    const embed_u64 stamp = fields(1u, 2u);
    embed_u64 seen = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE(embed_ticket_ring_grant(&s_ring, stamp, &seen));
    TEST_ASSERT_TRUE(embed_ticket_ring_push(&s_ring, stamp, &s_jobs[2], &seen));
    TEST_ASSERT_TRUE(embed_ticket_ring_pop(&s_ring, stamp, 0u, &job, &seen));
    TEST_ASSERT_TRUE(embed_ticket_ring_release(&s_ring, stamp, &seen));
    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_replay(&s_ring, stamp, &job, &seen), "a replay after the release fails");
}

void test_the_next_grant_takes_a_released_word_over(void)
{
    const embed_u64 first = fields(1u, 4u);
    const embed_u64 later = fields(2u, 4u + RING_SLOTS);
    embed_u64 seen = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE(embed_ticket_ring_grant(&s_ring, first, &seen));
    TEST_ASSERT_TRUE(embed_ticket_ring_push(&s_ring, first, &s_jobs[4], &seen));
    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_grant(&s_ring, later, &seen), "a grant on a live word fails");
    TEST_ASSERT_TRUE(embed_ticket_ring_pop(&s_ring, first, 0u, &job, &seen));
    TEST_ASSERT_TRUE(embed_ticket_ring_release(&s_ring, first, &seen));
    TEST_ASSERT_TRUE_MESSAGE(embed_ticket_ring_grant(&s_ring, later, &seen), "a grant on a released word lands");
    TEST_ASSERT_EQUAL_HEX64(first | LIFE_RELEASED, seen);
    TEST_ASSERT_TRUE(embed_ticket_ring_push(&s_ring, later, &s_jobs[5], &seen));
    TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_pop(&s_ring, first, 0u, &job, &seen), "the old stamp finds the new word");
    TEST_ASSERT_EQUAL_HEX64(later | LIFE_PUSHED, seen);
    TEST_ASSERT_TRUE(embed_ticket_ring_pop(&s_ring, later, 0u, &job, &seen));
    TEST_ASSERT_EQUAL_PTR(&s_jobs[5], job);
}

void test_a_flipped_stamp_refuses_every_step(void)
{
    const embed_u64 stamp = fields(6u, 11u);
    embed_u64 seen = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE(embed_ticket_ring_grant(&s_ring, stamp, &seen));
    TEST_ASSERT_TRUE(embed_ticket_ring_push(&s_ring, stamp, &s_jobs[11], &seen));
    for (unsigned int bit = 0u; bit < 64u; bit += 1u)
    {
        const embed_u64 flipped = stamp ^ ((embed_u64)1u << bit);

        // A flip in the epoch or the ticket lands on another word, or on this one with another expectation. A flip
        // in the flags expects a word that no step lands
        TEST_ASSERT_FALSE_MESSAGE(embed_ticket_ring_pop(&s_ring, flipped, 0u, &job, &seen), "a flipped pop fails");
        TEST_ASSERT_NULL(job);
    }
    TEST_ASSERT_TRUE_MESSAGE(embed_ticket_ring_pop(&s_ring, stamp, 0u, &job, &seen), "the true stamp still pops");
}

/** @brief One thread of the contention case: each ticket from the counter, each step inside a pass at its address. */
static void lives_in_passes(void)
{
    embed_u32 own = 0u;
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(uintptr_t)&own;

    for (unsigned int round = 0u; round < RING_PUSHES; round += 1u)
    {
        const embed_u32 ticket = embed_atomic_u32_fetch_add(&s_next_ticket, 1u);
        const embed_u64 stamp = fields((embed_u16)round, ticket);
        embed_u64 seen = 0u;

        if (!embed_ticket_ring_granted(&s_set, address, &s_ring, stamp, &seen) ||
            !embed_ticket_ring_pushed(&s_set, address, &s_ring, stamp, &s_jobs[ticket], &seen))
        {
            (void)embed_atomic_u32_fetch_add(&s_refused, 1u);
        }
    }
}

void test_lives_inside_passes_each_pop_whole_after_the_drain(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(lives_in_passes, RING_THREADS), "every thread started");
    embed_turnstiles_drain(&s_set);
    TEST_ASSERT_EQUAL_UINT32(RING_SLOTS, embed_atomic_u32_load(&s_next_ticket));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, embed_atomic_u32_load(&s_refused), "every grant and push landed");
    for (embed_u32 ticket = 0u; ticket < RING_SLOTS; ticket += 1u)
    {
        const embed_u64 stamp = fields((embed_u16)(ticket % RING_PUSHES), ticket);
        embed_u64 seen = 0u;
        void *job = NULL;

        TEST_ASSERT_TRUE(embed_ticket_ring_popped(&s_set, (embed_atomic_uptr_value)(uintptr_t)&seen, &s_ring, stamp,
                                                  &job, &seen));
        TEST_ASSERT_EQUAL_PTR(&s_jobs[ticket], job);
        TEST_ASSERT_TRUE(embed_ticket_ring_released(&s_set, &s_ring, stamp, &seen));
    }
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
}

/** @brief The two pops of one stamp from two threads: the collision case. */
static embed_u64 s_collision_stamp;
static embed_atomic_u32 s_collision_landed = EMBED_ATOMIC_INIT(0u);

static void pops_collide(void)
{
    embed_u32 own = 0u;
    embed_u64 seen = 0u;
    void *job = NULL;

    if (embed_ticket_ring_popped(&s_set, (embed_atomic_uptr_value)(uintptr_t)&own, &s_ring, s_collision_stamp, &job,
                                 &seen))
    {
        (void)embed_atomic_u32_fetch_add(&s_collision_landed, 1u);
    }
}

void test_two_pops_of_one_stamp_land_once(void)
{
    embed_u64 seen = 0u;

    s_collision_stamp = fields(2u, 7u);
    embed_atomic_u32_store(&s_collision_landed, 0u);
    TEST_ASSERT_TRUE(embed_ticket_ring_grant(&s_ring, s_collision_stamp, &seen));
    TEST_ASSERT_TRUE(embed_ticket_ring_push(&s_ring, s_collision_stamp, &s_jobs[7], &seen));
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(pops_collide, RING_THREADS), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, embed_atomic_u32_load(&s_collision_landed), "one pop of the stamp landed");
}
