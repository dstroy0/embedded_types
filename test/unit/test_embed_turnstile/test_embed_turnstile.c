/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_turnstile.c
 * @brief Checks the sleep on a word's address and the manifold of turnstiles against their contracts: a sleep ends as
 *        the word moves, an entry takes its address's lane and a busy lane spills, a leave clears busy and counts the
 *        pass, the manifold grows under pressure and contracts as it empties, a drain waits out the passes under way
 *        and no others, and workers never read what a writer released after its drain.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 */
#include "embed_turnstile.h"
#include "host_threads.h"
#include "host_wait.h"

#include "unity.h"

/** @brief The most milliseconds a test waits on another thread before it fails. */
#define TEST_PATIENCE_MS 5000u

/** @brief The workers that read beside the writer, and the writer's swaps. */
#define TEST_READERS 6u
#define TEST_SWAPS 20000u

/** @brief One millisecond and one second in nanoseconds. */
#define TEST_MILLISECOND_NS 1000000u
#define TEST_SECOND_NS 1000000000u

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
        host_threads_yield();
    }
    return embed_atomic_u32_load(flag) == value;
}

/** @brief This thread's id among the threads of one case, from 0. */
static embed_u32 thread_id(void)
{
    return embed_atomic_u32_fetch_add(&s_next_id, 1u);
}

/** @brief An address whose cache line selects lane `lane` of a set with any count of lanes above it. */
static embed_atomic_uptr_value address_of_lane(unsigned int lane)
{
    return (embed_atomic_uptr_value)lane << 6;
}

/** @brief The lane word of lane `at`. */
static embed_atomic_word_value lane_word(const EmbedTurnstiles *set, unsigned int at)
{
    return embed_atomic_word_load(&set->lane[at].word);
}

void test_a_sleep_on_a_word_that_moved_ends_at_once_reporting_the_move(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);
    const unsigned long long began = host_clock_ms();

    TEST_ASSERT_EQUAL_INT(1, embed_word_wait(&word, 6u, TEST_SECOND_NS));
    TEST_ASSERT_TRUE((host_clock_ms() - began) < 500u);
}

void test_a_sleep_on_a_still_word_ends_with_its_time_and_reports_no_move(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);

    TEST_ASSERT_EQUAL_INT(0, embed_word_wait(&word, 7u, 10u * TEST_MILLISECOND_NS));
}

void test_a_sleep_of_no_time_answers_at_once(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);
    const unsigned long long began = host_clock_ms();

    TEST_ASSERT_EQUAL_INT(0, embed_word_wait(&word, 7u, 0u));
    TEST_ASSERT_EQUAL_INT(1, embed_word_wait(&word, 6u, 0u));
    TEST_ASSERT_TRUE((host_clock_ms() - began) < 100u);
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

void test_a_backoff_inside_its_first_unit_only_pauses(void)
{
    // A step just after the start reads less than one unit on the clock, on almost every try. A preemption can put
    // one try past the unit, so the case counts the tries that made no call and needs one.
    unsigned int paused = 0u;

    for (unsigned int attempt = 0u; attempt < 100u; attempt += 1u)
    {
        embed_atomic_word word = EMBED_ATOMIC_INIT(0u);
        EmbedTurnstileBackoff backoff;

        embed_turnstile_backoff_begin(&backoff);
        embed_turnstile_backed_off(&word, 1u, EMBED_TURNSTILE_SLEEPER, &backoff, 1u);
        paused += (unsigned int)((backoff.exponent == 0u) && (embed_atomic_word_load(&word) == 0u));
    }
    TEST_ASSERT_TRUE(paused != 0u);
}

void test_a_backoff_past_its_first_unit_waits_and_raises_its_exponent_to_the_ceiling(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(0u);
    EmbedTurnstileBackoff backoff;

    embed_turnstile_backoff_begin(&backoff);
    backoff.start = embed_clock_ns() - TEST_SECOND_NS;
    // The word reads other than the value passed, so each wait answers at once
    for (unsigned int step = 0u; step < (EMBED_TURNSTILE_K_MOST + 4u); step += 1u)
    {
        embed_turnstile_backed_off(&word, 1u, EMBED_TURNSTILE_SLEEPER, &backoff, 1u);
    }
    TEST_ASSERT_EQUAL_UINT(EMBED_TURNSTILE_K_MOST, backoff.exponent);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_SLEEPER, embed_atomic_word_load(&word));
}

void test_a_backoff_that_may_not_sleep_only_pauses(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(0u);
    EmbedTurnstileBackoff backoff;

    embed_turnstile_backoff_begin(&backoff);
    backoff.start = embed_clock_ns() - TEST_SECOND_NS;
    embed_turnstile_backed_off(&word, 0u, EMBED_TURNSTILE_SLEEPER, &backoff, 0u);
    TEST_ASSERT_EQUAL_UINT(0u, backoff.exponent);
    TEST_ASSERT_EQUAL_UINT64(0u, embed_atomic_word_load(&word));
}

void test_a_set_at_rest_is_one_lane(void)
{
    static EmbedTurnstiles s_set;

    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
}

void test_an_entry_takes_a_free_lane_and_its_leave_clears_busy_and_counts_the_pass(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int at = embed_turnstile_enter(&s_set, address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(0u, at);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY, lane_word(&s_set, 0u));
    embed_turnstile_leave(&s_set, at);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_GENERATION, lane_word(&s_set, 0u));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_set.queue));
}

void test_an_entry_on_a_full_set_grows_it_and_the_last_leave_on_top_closes_the_lane(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int first = embed_turnstile_enter(&s_set, address_of_lane(0u));
    const unsigned int second = embed_turnstile_enter(&s_set, address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(0u, first);
    TEST_ASSERT_EQUAL_UINT(1u, second);
    TEST_ASSERT_EQUAL_UINT(2u, embed_turnstile_lanes(&s_set));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_set.queue));
    embed_turnstile_leave(&s_set, second);
    // the top lane left with the queue empty: the contraction closed it, busy and closed set, the close counted
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY | EMBED_TURNSTILE_CLOSED | (2u * EMBED_TURNSTILE_GENERATION),
                             lane_word(&s_set, 1u));
    embed_turnstile_leave(&s_set, first);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
}

void test_a_growth_reopens_a_closed_lane(void)
{
    static EmbedTurnstiles s_set;

    embed_turnstile_leave(&s_set, embed_turnstile_enter(&s_set, address_of_lane(0u)));
    const unsigned int first = embed_turnstile_enter(&s_set, address_of_lane(0u));
    const unsigned int second = embed_turnstile_enter(&s_set, address_of_lane(0u));

    embed_turnstile_leave(&s_set, second);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
    // the second pass on lane 0's address finds lane 0 busy and grows the set: lane 1 reopens, and the pass takes it
    const unsigned int again = embed_turnstile_enter(&s_set, address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(1u, again);
    TEST_ASSERT_EQUAL_UINT(2u, embed_turnstile_lanes(&s_set));
    TEST_ASSERT_EQUAL_UINT64(0u, lane_word(&s_set, 1u) & EMBED_TURNSTILE_CLOSED);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY, lane_word(&s_set, 1u) & EMBED_TURNSTILE_BUSY);
    embed_turnstile_leave(&s_set, again);
    embed_turnstile_leave(&s_set, first);
}

void test_an_entry_on_a_busy_lane_spills_to_the_next_open_lane(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int first = embed_turnstile_enter(&s_set, address_of_lane(0u));
    const unsigned int second = embed_turnstile_enter(&s_set, address_of_lane(0u));
    const unsigned int third = embed_turnstile_enter(&s_set, address_of_lane(1u));

    // three passes on two start lanes: the third finds lanes 1 and 0 busy and grows the set to lane 2
    TEST_ASSERT_EQUAL_UINT(2u, third);
    TEST_ASSERT_EQUAL_UINT(3u, embed_turnstile_lanes(&s_set));
    embed_turnstile_leave(&s_set, first);
    // a leave below the top does not contract
    TEST_ASSERT_EQUAL_UINT(3u, embed_turnstile_lanes(&s_set));
    embed_turnstile_leave(&s_set, second);
    TEST_ASSERT_EQUAL_UINT(3u, embed_turnstile_lanes(&s_set));
    // the leave on the top lane closes each free lane from the top down to one
    embed_turnstile_leave(&s_set, third);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_lanes(&s_set));
}

void test_a_drain_with_no_pass_under_way_returns_at_once(void)
{
    static EmbedTurnstiles s_set;
    const unsigned long long began = host_clock_ms();

    embed_turnstiles_drain(&s_set);
    TEST_ASSERT_TRUE((host_clock_ms() - began) < 100u);
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
        const unsigned int at = embed_turnstile_enter(&s_reading, address_of_lane(0u));

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

static EmbedTurnstiles s_crowd;
static embed_atomic_u32 s_crowd_passed = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_crowd_lanes_most = EMBED_ATOMIC_INIT(0u);

/** @brief Each thread passes through s_crowd for a rest on one start lane, recording the lanes it saw open. */
static void crowd(void)
{
    (void)thread_id();
    const unsigned int at = embed_turnstile_enter(&s_crowd, address_of_lane(0u));

    (void)embed_atomic_u32_raise(&s_crowd_lanes_most, (embed_u32)embed_turnstile_lanes(&s_crowd));
    clock_passes(5u);
    (void)embed_atomic_u32_fetch_add(&s_crowd_passed, 1u);
    embed_turnstile_leave(&s_crowd, at);
}

void test_a_crowd_grows_the_set_to_its_pressure_and_no_lane_stays_busy(void)
{
    embed_atomic_u32_store(&s_crowd_passed, 0u);
    embed_atomic_u32_store(&s_crowd_lanes_most, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(crowd, 4u), "every thread started");
    TEST_ASSERT_EQUAL_UINT32(4u, embed_atomic_u32_load(&s_crowd_passed));
    TEST_ASSERT_TRUE_MESSAGE(embed_atomic_u32_load(&s_crowd_lanes_most) > 1u, "a lane opened under the crowd");
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_crowd.queue));
    const unsigned int count = embed_turnstile_lanes(&s_crowd);

    for (unsigned int at = 0u; at < count; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(0u, lane_word(&s_crowd, at) & EMBED_TURNSTILE_BUSY);
    }
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
 * @brief Thread 0 writes, once every worker is reading: each swap publishes the other record, drains, and releases
 *        the one swapped out. The workers pass through s_swapped while it writes, each from its own address, each
 *        reading the published record and counting a read of one released.
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
        const unsigned int at = embed_turnstile_enter(&s_swapped, address_of_lane(id));
        const TestRecord *const record = published_record();

        if (embed_atomic_u32_load(&record->live) == 0u)
        {
            (void)embed_atomic_u32_fetch_add(&s_released_reads, 1u);
        }
        (void)embed_atomic_u32_fetch_add(&s_reads, 1u);
        embed_turnstile_leave(&s_swapped, at);
    }
}

void test_workers_never_read_a_record_the_writer_released_after_its_drain(void)
{
    embed_atomic_u32_store(&s_records[0].live, 1u);
    embed_atomic_u32_store(&s_records[1].live, 0u);
    embed_atomic_uptr_store(&s_published, (embed_atomic_uptr_value)&s_records[0]);
    embed_atomic_u32_store(&s_writing, 1u);
    embed_atomic_u32_store(&s_reading_in, 0u);
    embed_atomic_u32_store(&s_released_reads, 0u);
    embed_atomic_u32_store(&s_reads, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(writer_and_readers, TEST_READERS + 1u), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(TEST_READERS, embed_atomic_u32_load(&s_reading_in), "every worker read");
    TEST_ASSERT_TRUE(embed_atomic_u32_load(&s_reads) != 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_released_reads));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_swapped.queue));
}
