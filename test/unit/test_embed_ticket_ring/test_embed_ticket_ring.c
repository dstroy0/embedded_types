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

/** @brief The bits of a job pointer, which the check of the job folds. */
#define POINTER_BITS (8u * (unsigned int)sizeof(uintptr_t))

/** @brief The flags of a slot after each step of its life, lane bits aside. */
#define LIFE_GRANT_BITS EMBED_TICKET_GRANTED_BIT
#define LIFE_PUSH_BITS (LIFE_GRANT_BITS | EMBED_TICKET_PUSHED_BIT)
#define LIFE_POP_BITS (LIFE_PUSH_BITS | EMBED_TICKET_POPPED_BIT)
#define LIFE_RELEASE_BITS (LIFE_POP_BITS | EMBED_TICKET_RELEASED_BIT)

static EmbedTicketRingSlot s_slots[RING_SLOTS];
static EmbedTicketRing s_ring;
static EmbedTurnstiles s_set;
static embed_atomic_u32 s_next_ticket = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_refusal_count = EMBED_ATOMIC_INIT(0u);
static embed_u32 s_jobs[RING_SLOTS];

void setUp(void)
{
    for (unsigned int slot_index = 0u; slot_index < RING_SLOTS; slot_index += 1u)
    {
        EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_slots[slot_index].stamp,
                   .value = 0u);
        s_slots[slot_index].job = NULL;
        s_jobs[slot_index] = slot_index;
    }
    EMBED_CALL(embed_ticket_ring_table.begin, EmbedTicketRingBeginCfg, .ring = &s_ring, .slots = s_slots,
               .shift = RING_SHIFT);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_next_ticket, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_refusal_count, .value = 0u);
    for (unsigned int place = 0u; place < EMBED_TURNSTILE_LANES; place += 1u)
    {
        EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_set.lanes[place].word,
                   .value = 0u);
    }
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_set.top_open_lane, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_set.queue_length, .value = 0u);
}

void tearDown(void)
{
    // Each case has nothing to release
}

/** @brief The stamp of the fields of a stamp as the stamp holds them, flags aside. */
static embed_word stamp_of_fields(embed_word epoch, embed_word ticket)
{
    return EMBED_CALL(embed_ticket_ring_table.ticket_stamp, EmbedTicketStampCfg, .epoch = epoch, .ticket = ticket);
}

void test_the_stamp_gives_its_epoch_and_ticket_back(void)
{
    const embed_word epoch = (embed_word)0x1234u & EMBED_TICKET_EPOCH_MASK;
    const embed_word ticket = (embed_word)0x9abcdef0u & EMBED_TICKET_MASK;
    const embed_word stamp =
        EMBED_CALL(embed_ticket_ring_table.ticket_stamp, EmbedTicketStampCfg, .epoch = epoch, .ticket = ticket);

    TEST_ASSERT_EQUAL_HEX64((epoch << EMBED_TICKET_EPOCH_SHIFT) | ticket, stamp);
    TEST_ASSERT_EQUAL_HEX64(epoch,
                            EMBED_CALL(embed_ticket_ring_table.epoch_of_stamp, EmbedEpochOfStampCfg, .stamp = stamp));
    TEST_ASSERT_EQUAL_HEX64(ticket,
                            EMBED_CALL(embed_ticket_ring_table.ticket_of_stamp, EmbedTicketOfStampCfg, .stamp = stamp));
    TEST_ASSERT_EQUAL_HEX16(
        0u, EMBED_CALL(embed_ticket_ring_table.ticket_flags_of_stamp, EmbedTicketFlagsOfStampCfg, .stamp = stamp));
    TEST_ASSERT_EQUAL_HEX64(stamp, EMBED_CALL(embed_ticket_ring_table.ticket_fields_of_stamp,
                                              EmbedTicketFieldsOfStampCfg, .stamp = stamp | LIFE_POP_BITS));
}

void test_a_wide_field_keeps_only_its_own_bits(void)
{
    const embed_word stamp = EMBED_CALL(embed_ticket_ring_table.ticket_stamp, EmbedTicketStampCfg,
                                        .epoch = ~(embed_word)0u, .ticket = ~(embed_word)0u);

    TEST_ASSERT_EQUAL_HEX64(EMBED_TICKET_EPOCH_MASK,
                            EMBED_CALL(embed_ticket_ring_table.epoch_of_stamp, EmbedEpochOfStampCfg, .stamp = stamp));
    TEST_ASSERT_EQUAL_HEX64(EMBED_TICKET_MASK,
                            EMBED_CALL(embed_ticket_ring_table.ticket_of_stamp, EmbedTicketOfStampCfg, .stamp = stamp));
    TEST_ASSERT_EQUAL_HEX16(
        0u, EMBED_CALL(embed_ticket_ring_table.ticket_flags_of_stamp, EmbedTicketFlagsOfStampCfg, .stamp = stamp));
}

void test_the_flags_sit_between_the_ticket_and_the_epoch(void)
{
    const embed_word stamp = EMBED_CALL(embed_ticket_ring_table.ticket_stamp, EmbedTicketStampCfg,
                                        .epoch = EMBED_TICKET_EPOCH_MASK, .ticket = EMBED_TICKET_MASK) |
                             LIFE_RELEASE_BITS;

    TEST_ASSERT_EQUAL_HEX64(EMBED_TICKET_EPOCH_MASK,
                            EMBED_CALL(embed_ticket_ring_table.epoch_of_stamp, EmbedEpochOfStampCfg, .stamp = stamp));
    TEST_ASSERT_EQUAL_HEX64(EMBED_TICKET_MASK,
                            EMBED_CALL(embed_ticket_ring_table.ticket_of_stamp, EmbedTicketOfStampCfg, .stamp = stamp));
    TEST_ASSERT_EQUAL_HEX16(
        0x17u, EMBED_CALL(embed_ticket_ring_table.ticket_flags_of_stamp, EmbedTicketFlagsOfStampCfg, .stamp = stamp));
    TEST_ASSERT_EQUAL_UINT(5u, EMBED_CALL(embed_ticket_ring_table.ticket_lane_of_stamp, EmbedTicketLaneOfStampCfg,
                                          .stamp = stamp | ((embed_word)5u << EMBED_TICKET_LANE_SHIFT)));
}

void test_the_fields_fill_the_word(void)
{
    TEST_ASSERT_EQUAL_UINT(EMBED_WORD_BITS, EMBED_TICKET_BITS + EMBED_TICKET_FLAG_BITS + EMBED_TICKET_EPOCH_BITS);
    TEST_ASSERT_EQUAL_HEX64(~(embed_word)0u,
                            EMBED_TICKET_MASK | EMBED_TICKET_FLAG_MASK |
                                (EMBED_TICKET_EPOCH_MASK << EMBED_TICKET_EPOCH_SHIFT));
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
        TEST_ASSERT_EQUAL_UINT64(ticket % RING_SLOTS,
                                 EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &s_ring,
                                            .stamp = stamp_of_fields(7u, ticket)));
    }
}

void test_a_ticket_a_ring_on_lands_at_the_same_place(void)
{
    for (embed_u32 ticket = 0u; ticket < RING_SLOTS; ticket += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &s_ring,
                                            .stamp = stamp_of_fields(0u, ticket)),
                                 EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &s_ring,
                                            .stamp = stamp_of_fields(0u, ticket + RING_SLOTS)));
    }
}

void test_the_ticket_rolls_at_its_width_onto_the_next_place(void)
{
    TEST_ASSERT_EQUAL_UINT64(RING_SLOTS - 1u,
                             EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &s_ring,
                                        .stamp = stamp_of_fields(0u, EMBED_TICKET_MASK)));
    TEST_ASSERT_EQUAL_UINT64(0u, EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &s_ring,
                                            .stamp = stamp_of_fields(1u, 0u)));
}

void test_the_epoch_and_the_flags_do_not_move_the_place(void)
{
    TEST_ASSERT_EQUAL_UINT64(EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &s_ring,
                                        .stamp = stamp_of_fields(0u, 5u)),
                             EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &s_ring,
                                        .stamp = stamp_of_fields(EMBED_TICKET_EPOCH_MASK, 5u) | LIFE_RELEASE_BITS));
}

void test_the_life_of_a_slot_lands_each_step_once_in_order(void)
{
    const embed_word stamp = stamp_of_fields(3u, 9u);
    embed_word found_stamp = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring,
                                        .stamp = stamp, .found_stamp = &found_stamp),
                             "the grant lands on an empty word");
    TEST_ASSERT_EQUAL_HEX64(0u, found_stamp);
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring,
                                         .stamp = stamp, .found_stamp = &found_stamp),
                              "a second grant finds the granted word");
    TEST_ASSERT_EQUAL_HEX64(stamp | LIFE_GRANT_BITS, found_stamp);

    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                         .stamp = stamp, .lane_place = 0u, .job = &job, .found_stamp = &found_stamp),
                              "a pop before the push fails");
    TEST_ASSERT_NULL(job);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring,
                                        .stamp = stamp, .job = &s_jobs[9], .found_stamp = &found_stamp),
                             "the push lands");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring,
                                         .stamp = stamp, .job = &s_jobs[9], .found_stamp = &found_stamp),
                              "a second push fails");
    TEST_ASSERT_EQUAL_HEX64(
        stamp | LIFE_PUSH_BITS |
            EMBED_CALL(embed_ticket_ring_table.ticket_job_check_bits, EmbedTicketJobCheckBitsCfg, .job = &s_jobs[9]),
        found_stamp);

    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.release, EmbedTicketRingReleaseCfg, .ring = &s_ring,
                                         .stamp = stamp, .found_stamp = &found_stamp),
                              "a release before the pop fails");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                        .stamp = stamp, .lane_place = 5u, .job = &job, .found_stamp = &found_stamp),
                             "the pop lands");
    TEST_ASSERT_EQUAL_PTR(&s_jobs[9], job);
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                         .stamp = stamp, .lane_place = 6u, .job = &job, .found_stamp = &found_stamp),
                              "a second pop fails");
    TEST_ASSERT_NULL(job);
    TEST_ASSERT_EQUAL_HEX64(
        stamp | LIFE_POP_BITS |
            EMBED_CALL(embed_ticket_ring_table.ticket_job_check_bits, EmbedTicketJobCheckBitsCfg, .job = &s_jobs[9]) |
            ((embed_word)5u << EMBED_TICKET_LANE_SHIFT),
        found_stamp);
    TEST_ASSERT_EQUAL_UINT(
        5u, EMBED_CALL(embed_ticket_ring_table.ticket_lane_of_stamp, EmbedTicketLaneOfStampCfg, .stamp = found_stamp));

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.replay, EmbedTicketRingReplayCfg, .ring = &s_ring,
                                        .stamp = stamp, .job = &job, .found_stamp = &found_stamp),
                             "the replay lands");
    TEST_ASSERT_EQUAL_PTR(&s_jobs[9], job);
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.replay, EmbedTicketRingReplayCfg, .ring = &s_ring,
                                         .stamp = stamp, .job = &job, .found_stamp = &found_stamp),
                              "a second replay fails");
    TEST_ASSERT_NULL(job);

    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.release, EmbedTicketRingReleaseCfg, .ring = &s_ring,
                                        .stamp = stamp, .found_stamp = &found_stamp),
                             "the release lands after the replay");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.release, EmbedTicketRingReleaseCfg, .ring = &s_ring,
                                         .stamp = stamp, .found_stamp = &found_stamp),
                              "a second release fails");
    // Explicit cast narrows the five state bits of the flag field to the width that the assertion compares
    TEST_ASSERT_EQUAL_HEX16(0x1fu, (embed_u16)(EMBED_CALL(embed_ticket_ring_table.ticket_flags_of_stamp,
                                                          EmbedTicketFlagsOfStampCfg, .stamp = found_stamp) &
                                               0x1fu));
}

void test_a_release_lands_with_no_replay(void)
{
    const embed_word stamp = stamp_of_fields(1u, 2u);
    embed_word found_stamp = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring, .stamp = stamp,
                                .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring, .stamp = stamp,
                                .job = &s_jobs[2], .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring, .stamp = stamp,
                                .lane_place = 0u, .job = &job, .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.release, EmbedTicketRingReleaseCfg, .ring = &s_ring,
                                .stamp = stamp, .found_stamp = &found_stamp));
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.replay, EmbedTicketRingReplayCfg, .ring = &s_ring,
                                         .stamp = stamp, .job = &job, .found_stamp = &found_stamp),
                              "a replay after the release fails");
}

void test_the_next_grant_takes_a_released_word_over(void)
{
    const embed_word first_stamp = stamp_of_fields(1u, 4u);
    const embed_word later_stamp = stamp_of_fields(2u, 4u + RING_SLOTS);
    embed_word found_stamp = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring,
                                .stamp = first_stamp, .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring,
                                .stamp = first_stamp, .job = &s_jobs[4], .found_stamp = &found_stamp));
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring,
                                         .stamp = later_stamp, .found_stamp = &found_stamp),
                              "a grant on a live word fails");
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                .stamp = first_stamp, .lane_place = 0u, .job = &job, .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.release, EmbedTicketRingReleaseCfg, .ring = &s_ring,
                                .stamp = first_stamp, .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring,
                                        .stamp = later_stamp, .found_stamp = &found_stamp),
                             "a grant on a released word lands");
    TEST_ASSERT_EQUAL_HEX64(
        first_stamp | LIFE_RELEASE_BITS |
            EMBED_CALL(embed_ticket_ring_table.ticket_job_check_bits, EmbedTicketJobCheckBitsCfg, .job = &s_jobs[4]),
        found_stamp);
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring,
                                .stamp = later_stamp, .job = &s_jobs[5], .found_stamp = &found_stamp));
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                         .stamp = first_stamp, .lane_place = 0u, .job = &job,
                                         .found_stamp = &found_stamp),
                              "the old stamp finds the new word");
    TEST_ASSERT_EQUAL_HEX64(
        later_stamp | LIFE_PUSH_BITS |
            EMBED_CALL(embed_ticket_ring_table.ticket_job_check_bits, EmbedTicketJobCheckBitsCfg, .job = &s_jobs[5]),
        found_stamp);
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                .stamp = later_stamp, .lane_place = 0u, .job = &job, .found_stamp = &found_stamp));
    TEST_ASSERT_EQUAL_PTR(&s_jobs[5], job);
}

void test_a_flipped_stamp_refuses_every_step(void)
{
    const embed_word stamp = stamp_of_fields(6u, 11u);
    embed_word found_stamp = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring, .stamp = stamp,
                                .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring, .stamp = stamp,
                                .job = &s_jobs[11], .found_stamp = &found_stamp));
    for (unsigned int bit = 0u; bit < EMBED_WORD_BITS; bit += 1u)
    {
        const embed_word flipped_stamp = stamp ^ ((embed_word)1u << bit);

        // A flip in the epoch or the ticket lands on another word, or on this one with another expectation. A flip
        // in the flags expects a word that no step lands
        TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                             .stamp = flipped_stamp, .lane_place = 0u, .job = &job,
                                             .found_stamp = &found_stamp),
                                  "a flipped pop fails");
        TEST_ASSERT_NULL(job);
    }
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                        .stamp = stamp, .lane_place = 0u, .job = &job, .found_stamp = &found_stamp),
                             "the true stamp still pops");
}

void test_the_push_records_the_check_of_the_job_pointer(void)
{
    const embed_word stamp = stamp_of_fields(4u, 13u);
    embed_word found_stamp = 0u;

    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring, .stamp = stamp,
                                .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring, .stamp = stamp,
                                .job = &s_jobs[13], .found_stamp = &found_stamp));
    TEST_ASSERT_EQUAL_HEX64(
        EMBED_CALL(embed_ticket_ring_table.ticket_job_check_bits, EmbedTicketJobCheckBitsCfg, .job = &s_jobs[13]),
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_slots[13].stamp) &
            EMBED_TICKET_CHECK_MASK);
    TEST_ASSERT_EQUAL_HEX64(
        0u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check_bits, EmbedTicketJobCheckBitsCfg, .job = NULL));
    // The remainders of x^0 to x^7 modulo x^3 + x + 1: 1, x, x^2, x + 1, x^2 + x, x^2 + x + 1, x^2 + 1, 1. Explicit
    // casts carry each integer as a pointer value that nothing dereferences.
    TEST_ASSERT_EQUAL_UINT64(1u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)1u));
    TEST_ASSERT_EQUAL_UINT64(2u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)2u));
    TEST_ASSERT_EQUAL_UINT64(4u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)4u));
    TEST_ASSERT_EQUAL_UINT64(3u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)8u));
    TEST_ASSERT_EQUAL_UINT64(6u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)16u));
    TEST_ASSERT_EQUAL_UINT64(7u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)32u));
    TEST_ASSERT_EQUAL_UINT64(5u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)64u));
    TEST_ASSERT_EQUAL_UINT64(1u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)128u));
    TEST_ASSERT_EQUAL_UINT64(0u, EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg,
                                            .job = (const void *)(uintptr_t)129u));
}

/** @brief The slot of ticket 14 set to the pushed state with the check of the intact job, and the job set to value. */
static void slot_14_pushed_with(embed_word stamp, uintptr_t value)
{
    // Explicit cast carries the integer back as a pointer value that nothing dereferences
    s_slots[14].job = (void *)value;
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_slots[14].stamp,
               .value = stamp | LIFE_PUSH_BITS |
                        EMBED_CALL(embed_ticket_ring_table.ticket_job_check_bits, EmbedTicketJobCheckBitsCfg,
                                   .job = &s_jobs[14]));
}

void test_a_single_bit_error_in_the_job_pointer_fails_the_pop_and_the_replay(void)
{
#if EMBED_TICKET_JOB_CHECK
    const embed_word stamp = stamp_of_fields(4u, 14u);
    // Explicit cast carries the pointer as an integer for the flips
    const uintptr_t intact_job = (uintptr_t)&s_jobs[14];
    embed_word found_stamp = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring, .stamp = stamp,
                                .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring, .stamp = stamp,
                                .job = &s_jobs[14], .found_stamp = &found_stamp));
    for (unsigned int bit = 0u; bit < POINTER_BITS; bit += 1u)
    {
        slot_14_pushed_with(stamp, intact_job ^ ((uintptr_t)1u << bit));
        TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                             .stamp = stamp, .lane_place = 0u, .job = &job,
                                             .found_stamp = &found_stamp),
                                  "the pop refuses the job");
        TEST_ASSERT_NULL(job);
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(EMBED_TICKET_RESERVED_BIT, found_stamp & EMBED_TICKET_RESERVED_BIT,
                                        "the found stamp marks corruption");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(
            EMBED_TICKET_POPPED_BIT,
            EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_slots[14].stamp) &
                EMBED_TICKET_POPPED_BIT,
            "the slot stays popped");
        TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.replay, EmbedTicketRingReplayCfg, .ring = &s_ring,
                                             .stamp = stamp, .job = &job, .found_stamp = &found_stamp),
                                  "the replay refuses the job");
        TEST_ASSERT_NULL(job);
    }
    slot_14_pushed_with(stamp, intact_job);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                        .stamp = stamp, .lane_place = 0u, .job = &job, .found_stamp = &found_stamp),
                             "the intact job pops");
    TEST_ASSERT_EQUAL_PTR(&s_jobs[14], job);
#else
    TEST_IGNORE_MESSAGE("EMBED_TICKET_JOB_CHECK is 0 here, so the pop does not verify the job pointer");
#endif
}

void test_a_double_bit_error_fails_the_pop_unless_seven_apart(void)
{
#if EMBED_TICKET_JOB_CHECK
    const embed_word stamp = stamp_of_fields(4u, 14u);
    // Explicit cast carries the pointer as an integer for the flips
    const uintptr_t intact_job = (uintptr_t)&s_jobs[14];
    embed_word found_stamp = 0u;
    void *job = NULL;

    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring, .stamp = stamp,
                                .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring, .stamp = stamp,
                                .job = &s_jobs[14], .found_stamp = &found_stamp));
    for (unsigned int first_bit = 0u; first_bit < POINTER_BITS; first_bit += 1u)
    {
        for (unsigned int second_bit = first_bit + 1u; second_bit < POINTER_BITS; second_bit += 1u)
        {
            // Explicit cast converts the test, an int of 0 or 1, to the project boolean
            const embed_bool error_detection = (embed_bool)(((second_bit - first_bit) % 7u) != 0u);

            slot_14_pushed_with(stamp, intact_job ^ ((uintptr_t)1u << first_bit) ^ ((uintptr_t)1u << second_bit));
            TEST_ASSERT_EQUAL_MESSAGE(!error_detection,
                                      EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring,
                                                 .stamp = stamp, .lane_place = 0u, .job = &job,
                                                 .found_stamp = &found_stamp),
                                      "the pop refuses a double error exactly where the places are not seven apart");
        }
    }
#else
    TEST_IGNORE_MESSAGE("EMBED_TICKET_JOB_CHECK is 0 here, so the pop does not verify the job pointer");
#endif
}

/** @brief One thread of the contention case: each ticket from the counter, each step inside a pass at its address. */
static void lives_in_passes(void)
{
    embed_u32 address_anchor = 0u;
    // Explicit casts carry the address of a local of this thread as the integer that the entry takes
    const embed_atomic_uptr_value address = (embed_atomic_uptr_value)(uintptr_t)&address_anchor;

    for (unsigned int round_index = 0u; round_index < RING_PUSHES; round_index += 1u)
    {
        const embed_u32 ticket = EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg,
                                            .word = &s_next_ticket, .value = 1u);
        // The epoch follows the ticket and not the round. So the check after the run rebuilds each stamp, in each
        // order of the tickets of the threads
        const embed_word stamp = stamp_of_fields(ticket % RING_PUSHES, ticket);
        embed_word found_stamp = 0u;

        if (!EMBED_CALL(embed_ticket_ring_table.pass_grant, EmbedTicketRingPassGrantCfg, .set = &s_set,
                        .address = address, .ring = &s_ring, .stamp = stamp, .found_stamp = &found_stamp) ||
            !EMBED_CALL(embed_ticket_ring_table.pass_push, EmbedTicketRingPassPushCfg, .set = &s_set,
                        .address = address, .ring = &s_ring, .stamp = stamp, .job = &s_jobs[ticket],
                        .found_stamp = &found_stamp))
        {
            (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_refusal_count,
                             .value = 1u);
        }
    }
}

void test_lives_inside_passes_each_pop_whole_after_the_drain(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(lives_in_passes, RING_THREADS), "every thread started");
    EMBED_CALL(embed_turnstile_table.drain, EmbedTurnstilesDrainCfg, .set = &s_set);
    TEST_ASSERT_EQUAL_UINT32(RING_SLOTS,
                             EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_next_ticket));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_refusal_count),
        "every grant and push landed");
    for (embed_u32 ticket = 0u; ticket < RING_SLOTS; ticket += 1u)
    {
        const embed_word stamp = stamp_of_fields(ticket % RING_PUSHES, ticket);
        embed_word found_stamp = 0u;
        void *job = NULL;

        // Explicit casts carry the address of a local as the integer that the entry takes
        TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.pass_pop, EmbedTicketRingPassPopCfg, .set = &s_set,
                                    .address = (embed_atomic_uptr_value)(uintptr_t)&found_stamp, .ring = &s_ring,
                                    .stamp = stamp, .job = &job, .found_stamp = &found_stamp));
        TEST_ASSERT_EQUAL_PTR(&s_jobs[ticket], job);
        // A replay inside a pass reads the same job back, and the release then takes the replayed word
        job = NULL;
        // Explicit casts carry the address of a local as the integer that the entry takes
        TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.pass_replay, EmbedTicketRingPassReplayCfg, .set = &s_set,
                                    .address = (embed_atomic_uptr_value)(uintptr_t)&found_stamp, .ring = &s_ring,
                                    .stamp = stamp, .job = &job, .found_stamp = &found_stamp));
        TEST_ASSERT_EQUAL_PTR(&s_jobs[ticket], job);
        TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.drain_release, EmbedTicketRingDrainReleaseCfg,
                                    .set = &s_set, .ring = &s_ring, .stamp = stamp, .found_stamp = &found_stamp));
    }
    TEST_ASSERT_EQUAL_UINT(
        1u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
}

/** @brief The two pops of one stamp from two threads: the collision case. */
static embed_word s_collision_stamp;
static embed_atomic_u32 s_collision_landings = EMBED_ATOMIC_INIT(0u);

static void pop_collision(void)
{
    embed_u32 address_anchor = 0u;
    embed_word found_stamp = 0u;
    void *job = NULL;

    // Explicit casts carry the address of a local of this thread as the integer that the entry takes
    if (EMBED_CALL(embed_ticket_ring_table.pass_pop, EmbedTicketRingPassPopCfg, .set = &s_set,
                   .address = (embed_atomic_uptr_value)(uintptr_t)&address_anchor, .ring = &s_ring,
                   .stamp = s_collision_stamp, .job = &job, .found_stamp = &found_stamp))
    {
        (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_collision_landings,
                         .value = 1u);
    }
}

void test_two_pops_of_one_stamp_land_once(void)
{
    embed_word found_stamp = 0u;

    s_collision_stamp = stamp_of_fields(2u, 7u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_collision_landings, .value = 0u);
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring,
                                .stamp = s_collision_stamp, .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE(EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring,
                                .stamp = s_collision_stamp, .job = &s_jobs[7], .found_stamp = &found_stamp));
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(pop_collision, RING_THREADS), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        1u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_collision_landings),
        "one pop of the stamp landed");
}
