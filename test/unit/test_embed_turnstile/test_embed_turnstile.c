/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_turnstile.c
 * @brief Checks the sleep on a word's address and the turnstiles against their contracts: a sleep ends as the word
 *        moves, a pass badges its lane with its id and stamp and its end clears the badge's state, a drain waits
 *        out the passes under way and no others, a set is one lane at rest, a reader with no free lane opens the
 *        next, passes of one id spill to lanes of their own and stand in stamp order, the bank retires its idle
 *        lanes, and readers never read what a writer released after its drain.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-05
 */
#include "embed_turnstile.h"
#include "host_threads.h"
#include "host_wait.h"

#include "unity.h"

/** @brief The most milliseconds a test waits on another thread before it fails. */
#define TEST_PATIENCE_MS 5000u

/** @brief The readers that read beside the writer, and the writer's swaps. */
#define TEST_READERS 6u
#define TEST_SWAPS 20000u

static embed_atomic_u32 s_next_id = EMBED_ATOMIC_INIT(0u);

void setUp(void)
{
    embed_atomic_u32_store(&s_next_id, 0u);
}

void tearDown(void)
{
}

/** @brief The clock let run milliseconds. */
static void clock_passes(unsigned long long milliseconds)
{
    const unsigned long long until = host_clock_ms() + milliseconds;

    while (host_clock_ms() < until)
    {
    }
}

/** @brief flag read until it reads value or TEST_PATIENCE_MS pass: 1 where it did. */
static int flag_reached(embed_atomic_u32 *flag, embed_u32 value)
{
    const unsigned long long until = host_clock_ms() + TEST_PATIENCE_MS;

    while ((embed_atomic_u32_load(flag) != value) && (host_clock_ms() < until))
    {
    }
    return embed_atomic_u32_load(flag) == value;
}

/** @brief This thread's id among the threads of one case, from 0. */
static embed_u32 thread_id(void)
{
    return embed_atomic_u32_fetch_add(&s_next_id, 1u);
}

/** @brief The id field of a lane's badge. */
static embed_atomic_word_value badge_id(const EmbedTurnstiles *set, unsigned int at)
{
    return (embed_atomic_word_load(&set->lane[at].badge) & EMBED_TURNSTILE_ID_MASK) >> EMBED_TURNSTILE_ID_SHIFT;
}

/** @brief A badge-in that badges in again while the bank is full, as a reader does on EMBED_TURNSTILE_NONE. */
static unsigned int entered(EmbedTurnstiles *set, embed_atomic_word_value id)
{
    unsigned int at = embed_turnstile_enter(set, id);

    while (at == EMBED_TURNSTILE_NONE)
    {
        at = embed_turnstile_enter(set, id);
    }
    return at;
}

void test_a_sleep_on_a_word_that_moved_ends_at_once_reporting_the_move(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);
    const unsigned long long began = host_clock_ms();

    TEST_ASSERT_EQUAL_INT(1, embed_word_wait(&word, 6u, 1000u));
    TEST_ASSERT_TRUE((host_clock_ms() - began) < 500u);
}

void test_a_sleep_on_a_still_word_ends_with_its_time_and_reports_no_move(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);

    TEST_ASSERT_EQUAL_INT(0, embed_word_wait(&word, 7u, 10u));
}

void test_the_clock_moves_forward(void)
{
    const embed_u64 first = embed_clock_ns();

    clock_passes(2u);
    TEST_ASSERT_TRUE(embed_clock_ns() > first);
}

static embed_atomic_word s_word = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_woken = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_woken_early = EMBED_ATOMIC_INIT(0u);

/** @brief Thread 0 asleep on s_word while it reads 0, then marked woken; thread 1 moves the word after a rest. */
static void sleeper_and_waker(void)
{
    if (thread_id() == 0u)
    {
        while (embed_atomic_word_load(&s_word) == 0u)
        {
            (void)embed_word_wait(&s_word, 0u, EMBED_WAIT_FOREVER);
        }
        embed_atomic_u32_store(&s_woken, 1u);
        return;
    }
    clock_passes(30u);
    if (embed_atomic_u32_load(&s_woken) != 0u)
    {
        embed_atomic_u32_store(&s_woken_early, 1u);
    }
    embed_atomic_word_store(&s_word, 1u);
    embed_word_wake(&s_word);
}

void test_a_thread_asleep_on_a_word_wakes_as_another_moves_it_and_wakes_its_address(void)
{
    embed_atomic_word_store(&s_word, 0u);
    embed_atomic_u32_store(&s_woken, 0u);
    embed_atomic_u32_store(&s_woken_early, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(sleeper_and_waker, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_woken_early));
    TEST_ASSERT_EQUAL_UINT32(1u, embed_atomic_u32_load(&s_woken));
}

void test_a_set_at_rest_is_one_lane(void)
{
    static EmbedTurnstiles s_set;

    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
}

void test_a_pass_badges_its_lane_with_its_id_and_stamp_and_its_end_clears_the_state(void)
{
    static EmbedTurnstiles s_set;
    const embed_u64 before = embed_clock_ns();
    const unsigned int at = embed_turnstile_enter(&s_set, 7u);

    TEST_ASSERT_TRUE(at < EMBED_TURNSTILE_LANES_MOST);
    const embed_atomic_word_value during = embed_atomic_word_load(&s_set.lane[at].badge);

    TEST_ASSERT_EQUAL_UINT64(7u, badge_id(&s_set, at));
    TEST_ASSERT_TRUE((during & EMBED_TURNSTILE_COMPLETE) != 0u);
    TEST_ASSERT_TRUE((embed_atomic_word_load(&s_set.busy) & ((embed_atomic_word_value)1u << at)) != 0u);
    // the stamp is the clock's low bits, read between the two readings around the entry
    const embed_atomic_word_value stamp = during & EMBED_TURNSTILE_TIME_MASK;
    const embed_atomic_word_value low = (embed_atomic_word_value)before & EMBED_TURNSTILE_TIME_MASK;
    const embed_atomic_word_value high = (embed_atomic_word_value)embed_clock_ns() & EMBED_TURNSTILE_TIME_MASK;

    TEST_ASSERT_TRUE((low > high) || ((stamp >= low) && (stamp <= high)));
    embed_turnstile_leave(&s_set, at);
    const embed_atomic_word_value after = embed_atomic_word_load(&s_set.lane[at].badge);

    TEST_ASSERT_EQUAL_UINT64(0u, after & EMBED_TURNSTILE_STATE_MASK);
    TEST_ASSERT_EQUAL_UINT64(during & EMBED_TURNSTILE_PASS_MASK, after & EMBED_TURNSTILE_PASS_MASK);
    TEST_ASSERT_EQUAL_UINT64(0u, embed_atomic_word_load(&s_set.busy) & EMBED_TURNSTILE_LANE_BITS);
}

void test_a_drain_with_no_pass_under_way_returns_at_once(void)
{
    static EmbedTurnstiles s_set;
    const unsigned long long began = host_clock_ms();

    embed_turnstiles_drain(&s_set);
    TEST_ASSERT_TRUE((host_clock_ms() - began) < 100u);
}

void test_a_reader_with_no_free_lane_opens_the_next_and_passes_of_one_id_spill_in_stamp_order(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int first = embed_turnstile_enter(&s_set, 3u);
    const unsigned int second = embed_turnstile_enter(&s_set, 3u);

    TEST_ASSERT_TRUE(first != second);
    TEST_ASSERT_EQUAL_UINT(2u, embed_turnstile_lanes(&s_set));
    TEST_ASSERT_EQUAL_UINT64(3u, badge_id(&s_set, first));
    TEST_ASSERT_EQUAL_UINT64(3u, badge_id(&s_set, second));
    // the second pass is stamped at or after the first, where the clock did not roll between them
    const embed_atomic_word_value one = embed_atomic_word_load(&s_set.lane[first].badge) & EMBED_TURNSTILE_TIME_MASK;
    const embed_atomic_word_value two = embed_atomic_word_load(&s_set.lane[second].badge) & EMBED_TURNSTILE_TIME_MASK;

    TEST_ASSERT_TRUE((two >= one) || (one > (EMBED_TURNSTILE_TIME_MASK - 1000000000u)));
    embed_turnstile_leave(&s_set, second);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
    embed_turnstile_leave(&s_set, first);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
}

void test_an_idle_lane_under_a_busy_one_stays_open_until_the_busy_one_ends(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int first = embed_turnstile_enter(&s_set, 1u);
    const unsigned int second = embed_turnstile_enter(&s_set, 2u);

    embed_turnstile_leave(&s_set, first);
    TEST_ASSERT_EQUAL_UINT(2u, embed_turnstile_lanes(&s_set));
    embed_turnstile_leave(&s_set, second);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
}

static EmbedTurnstiles s_reading;
static embed_atomic_u32 s_entered = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_finished = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_drained_early = EMBED_ATOMIC_INIT(0u);

/** @brief Thread 0 passes through s_reading for 60 milliseconds; thread 1 drains once the pass began. */
static void long_pass_and_drain(void)
{
    if (thread_id() == 0u)
    {
        const unsigned int at = entered(&s_reading, 1u);

        embed_atomic_u32_store(&s_entered, 1u);
        clock_passes(60u);
        embed_atomic_u32_store(&s_finished, 1u);
        embed_turnstile_leave(&s_reading, at);
        return;
    }
    if (!flag_reached(&s_entered, 1u))
    {
        return;
    }
    embed_turnstiles_drain(&s_reading);
    if (embed_atomic_u32_load(&s_finished) == 0u)
    {
        embed_atomic_u32_store(&s_drained_early, 1u);
    }
}

void test_a_drain_waits_out_a_pass_begun_before_it(void)
{
    embed_atomic_u32_store(&s_entered, 0u);
    embed_atomic_u32_store(&s_finished, 0u);
    embed_atomic_u32_store(&s_drained_early, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(long_pass_and_drain, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32(1u, embed_atomic_u32_load(&s_entered));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_drained_early));
}

static EmbedTurnstiles s_claimed;

/**
 * @brief Thread 0 holds lane 0 of s_claimed between its claim and its badge for 60 milliseconds, as a reader does
 *        for an instant, its badge the one the lane's last pass left; thread 1 drains once the claim stands.
 */
static void claim_before_badge_and_drain(void)
{
    if (thread_id() == 0u)
    {
        (void)embed_atomic_word_fetch_or(&s_claimed.busy, 1u);
        embed_atomic_u32_store(&s_entered, 1u);
        clock_passes(60u);
        embed_atomic_word_store(&s_claimed.lane[0].badge, embed_turnstile_badge(1u));
        embed_atomic_u32_store(&s_finished, 1u);
        embed_turnstile_leave(&s_claimed, 0u);
        return;
    }
    if (!flag_reached(&s_entered, 1u))
    {
        return;
    }
    embed_turnstiles_drain(&s_claimed);
    if (embed_atomic_u32_load(&s_finished) == 0u)
    {
        embed_atomic_u32_store(&s_drained_early, 1u);
    }
}

void test_a_drain_waits_out_a_lane_claimed_before_its_badge_is_written(void)
{
    const unsigned int at = entered(&s_claimed, 1u);

    embed_turnstile_leave(&s_claimed, at);
    embed_atomic_u32_store(&s_entered, 0u);
    embed_atomic_u32_store(&s_finished, 0u);
    embed_atomic_u32_store(&s_drained_early, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(claim_before_badge_and_drain, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32(1u, embed_atomic_u32_load(&s_entered));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_drained_early));
}

static EmbedTurnstiles s_crowd;
static embed_atomic_u32 s_crowd_passed = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_crowd_lanes_most = EMBED_ATOMIC_INIT(0u);

/** @brief Each thread passes through s_crowd for a rest, recording the lanes it saw open. */
static void crowd(void)
{
    const unsigned int at = entered(&s_crowd, (embed_atomic_word_value)thread_id() + 1u);

    (void)embed_atomic_u32_raise(&s_crowd_lanes_most, (embed_u32)embed_turnstile_lanes(&s_crowd));
    clock_passes(5u);
    (void)embed_atomic_u32_fetch_add(&s_crowd_passed, 1u);
    embed_turnstile_leave(&s_crowd, at);
}

void test_a_crowd_opens_lanes_to_its_pressure_and_the_bank_retires_them_as_it_empties(void)
{
    embed_atomic_u32_store(&s_crowd_passed, 0u);
    embed_atomic_u32_store(&s_crowd_lanes_most, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(crowd, 4u), "every thread started");
    TEST_ASSERT_EQUAL_UINT32(4u, embed_atomic_u32_load(&s_crowd_passed));
    TEST_ASSERT_TRUE_MESSAGE(embed_atomic_u32_load(&s_crowd_lanes_most) > 1u, "a lane opened under the crowd");
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_crowd));
}

/** @brief A record a writer publishes: 1 while published or a pass may still read it, 0 once the writer released it. */
typedef struct
{
    embed_atomic_u32 live;
} TestRecord;

static TestRecord s_records[2];
static embed_atomic_uptr s_published = EMBED_ATOMIC_INIT(0u);
static EmbedTurnstiles s_swapped;
static embed_atomic_u32 s_writing = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_reading_in = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_released_reads = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_reads = EMBED_ATOMIC_INIT(0u);

/** @brief The record s_published leads to. */
static TestRecord *published_record(void)
{
    return (TestRecord *)(embed_atomic_uptr_value)embed_atomic_uptr_load(&s_published);
}

/**
 * @brief Thread 0 writes, once every reader is reading: each swap publishes the other record, drains, and releases
 *        the one swapped out. The readers pass through s_swapped while it writes, each reading the published
 *        record and counting a read of one released.
 */
static void writer_and_readers(void)
{
    const embed_u32 id = thread_id();

    if (id == 0u)
    {
        if (!flag_reached(&s_reading_in, TEST_READERS))
        {
            embed_atomic_u32_store(&s_writing, 0u);
            return;
        }
        for (embed_u32 swap = 0u; swap < TEST_SWAPS; swap += 1u)
        {
            TestRecord *const old = published_record();
            TestRecord *const next = (old == &s_records[0]) ? &s_records[1] : &s_records[0];

            embed_atomic_u32_store(&next->live, 1u);
            (void)embed_atomic_uptr_exchange(&s_published, (embed_atomic_uptr_value)next);
            embed_turnstiles_drain(&s_swapped);
            embed_atomic_u32_store(&old->live, 0u);
        }
        embed_atomic_u32_store(&s_writing, 0u);
        return;
    }
    (void)embed_atomic_u32_fetch_add(&s_reading_in, 1u);
    while (embed_atomic_u32_load(&s_writing) != 0u)
    {
        const unsigned int at = entered(&s_swapped, (embed_atomic_word_value)id);
        const TestRecord *const record = published_record();

        if (embed_atomic_u32_load(&record->live) == 0u)
        {
            (void)embed_atomic_u32_fetch_add(&s_released_reads, 1u);
        }
        (void)embed_atomic_u32_fetch_add(&s_reads, 1u);
        embed_turnstile_leave(&s_swapped, at);
    }
}

void test_readers_never_read_a_record_the_writer_released_after_its_drain(void)
{
    embed_atomic_u32_store(&s_records[0].live, 1u);
    embed_atomic_u32_store(&s_records[1].live, 0u);
    embed_atomic_uptr_store(&s_published, (embed_atomic_uptr_value)&s_records[0]);
    embed_atomic_u32_store(&s_writing, 1u);
    embed_atomic_u32_store(&s_reading_in, 0u);
    embed_atomic_u32_store(&s_released_reads, 0u);
    embed_atomic_u32_store(&s_reads, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(writer_and_readers, TEST_READERS + 1u), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(TEST_READERS, embed_atomic_u32_load(&s_reading_in), "every reader read");
    TEST_ASSERT_TRUE(embed_atomic_u32_load(&s_reads) != 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_released_reads));
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_swapped));
}
