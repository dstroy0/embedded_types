/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_turnstile.c
 * @brief Checks the sleep on the address of a word and the manifold of turnstiles against their contracts. A sleep
 *        ends as the word moves. An entry takes the lane of its address, and a busy lane spills. A leave clears busy
 *        and counts the pass. The manifold grows under pressure and contracts as it empties.
 *
 * A drain waits out the passes under way and no others. Workers never read what a writer released after its drain.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 */
#include "embed_turnstile.h"
#include "host_threads.h"
#include "host_wait.h"

#include "unity.h"

/** @brief The max milliseconds a test waits on another thread before it fails. */
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
    // Each case has nothing to release
}

/** @brief A spin on the clock while it moves on milliseconds. */
static void clock_spin(unsigned long long milliseconds)
{
    const unsigned long long end_ms = host_clock_ms() + milliseconds;

    while (host_clock_ms() < end_ms)
    {
        // The condition reads the clock, and the loop has no other step
    }
}

/** @brief A wait on flag until it reads value or TEST_PATIENCE_MS pass: 1 where it reads value. */
static int flag_wait(const embed_atomic_u32 *flag, embed_u32 value)
{
    const unsigned long long end_ms = host_clock_ms() + TEST_PATIENCE_MS;

    while ((embed_atomic_u32_load(flag) != value) && (host_clock_ms() < end_ms))
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

/** @brief An address whose cache line selects the lane at `place` of a set with any count of lanes above it. */
static embed_atomic_uptr_value address_of_lane(unsigned int place)
{
    // Explicit cast widens the place to the address, and EMBED_TURNSTILE_LANES bounds it at 64
    return (embed_atomic_uptr_value)place << 6u;
}

/** @brief The lane word of the lane at `place`. */
static embed_atomic_word_value lane_word(const EmbedTurnstiles *set, unsigned int place)
{
    return embed_atomic_word_load(&set->lanes[place].word);
}

void test_a_sleep_on_a_word_that_moved_ends_at_once_reporting_the_move(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);
    const unsigned long long start_ms = host_clock_ms();

    TEST_ASSERT_EQUAL_INT(1, embed_word_wait(&word, 6u, TEST_SECOND_NS));
    TEST_ASSERT_TRUE((host_clock_ms() - start_ms) < 500u);
}

void test_a_sleep_on_a_still_word_ends_with_its_time_and_reports_no_move(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);

    TEST_ASSERT_EQUAL_INT(0, embed_word_wait(&word, 7u, 10u * TEST_MILLISECOND_NS));
}

void test_a_sleep_of_no_time_answers_at_once(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);
    const unsigned long long start_ms = host_clock_ms();

    TEST_ASSERT_EQUAL_INT(0, embed_word_wait(&word, 7u, 0u));
    TEST_ASSERT_EQUAL_INT(1, embed_word_wait(&word, 6u, 0u));
    TEST_ASSERT_TRUE((host_clock_ms() - start_ms) < 100u);
}

void test_the_clock_moves_forward(void)
{
    const embed_word start_ns = embed_clock_ns();

    clock_spin(2u);
    // Explicit cast takes the difference modulo the word, which holds across the roll of the clock. The spin is far
    // below the roll on every word of 32 bits or more.
    const embed_word elapsed_ns = (embed_word)(embed_clock_ns() - start_ns);

    TEST_ASSERT_TRUE(elapsed_ns != 0u);
}

static embed_atomic_word s_word = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_wake_mark = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_early_wake_mark = EMBED_ATOMIC_INIT(0u);

/** @brief Thread 0 sleeps on s_word while it reads 0, then marks the wake. Thread 1 moves the word after a rest. */
static void sleeper_and_waker(void)
{
    if (thread_id() == 0u)
    {
        while (embed_atomic_word_load(&s_word) == 0u)
        {
            (void)embed_word_wait(&s_word, 0u, EMBED_WAIT_INFINITE);
        }
        embed_atomic_u32_store(&s_wake_mark, 1u);
        return;
    }
    clock_spin(30u);
    if (embed_atomic_u32_load(&s_wake_mark) != 0u)
    {
        embed_atomic_u32_store(&s_early_wake_mark, 1u);
    }
    embed_atomic_word_store(&s_word, 1u);
    embed_word_wake(&s_word);
}

void test_a_thread_asleep_on_a_word_wakes_as_another_moves_it_and_wakes_its_address(void)
{
    embed_atomic_word_store(&s_word, 0u);
    embed_atomic_u32_store(&s_wake_mark, 0u);
    embed_atomic_u32_store(&s_early_wake_mark, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(sleeper_and_waker, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_early_wake_mark));
    TEST_ASSERT_EQUAL_UINT32(1u, embed_atomic_u32_load(&s_wake_mark));
}

void test_a_backoff_inside_its_first_unit_only_pauses(void)
{
    // A step just after the start reads less than one unit on the clock, on almost every try. A preemption can put
    // one try past the unit, so the case counts the tries that made no call and needs one.
    unsigned int pause_count = 0u;

    for (unsigned int attempt = 0u; attempt < 100u; attempt += 1u)
    {
        embed_atomic_word word = EMBED_ATOMIC_INIT(0u);
        EmbedTurnstileBackoff backoff;

        embed_turnstile_backoff_begin(&backoff);
        embed_turnstile_backoff_step(&word, 1u, EMBED_TURNSTILE_SLEEPER_BIT, &backoff, EMBED_TRUE);
        // Explicit cast converts the test, an int of 0 or 1, to the unsigned type of the count
        pause_count += (unsigned int)((backoff.exponent == 0u) && (embed_atomic_word_load(&word) == 0u));
    }
    TEST_ASSERT_TRUE(pause_count != 0u);
}

void test_a_backoff_past_its_first_unit_waits_and_raises_its_exponent_to_the_ceiling(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(0u);
    EmbedTurnstileBackoff backoff;

    embed_turnstile_backoff_begin(&backoff);
    backoff.start_ns = embed_clock_ns() - TEST_SECOND_NS;
    // The word reads other than the value passed, so each wait answers at once
    for (unsigned int step = 0u; step < (EMBED_TURNSTILE_EXPONENT_MAX + 4u); step += 1u)
    {
        embed_turnstile_backoff_step(&word, 1u, EMBED_TURNSTILE_SLEEPER_BIT, &backoff, EMBED_TRUE);
    }
    TEST_ASSERT_EQUAL_UINT(EMBED_TURNSTILE_EXPONENT_MAX, backoff.exponent);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_SLEEPER_BIT, embed_atomic_word_load(&word));
}

void test_a_backoff_that_may_not_sleep_only_pauses(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(0u);
    EmbedTurnstileBackoff backoff;

    embed_turnstile_backoff_begin(&backoff);
    backoff.start_ns = embed_clock_ns() - TEST_SECOND_NS;
    embed_turnstile_backoff_step(&word, 0u, EMBED_TURNSTILE_SLEEPER_BIT, &backoff, EMBED_FALSE);
    TEST_ASSERT_EQUAL_UINT(0u, backoff.exponent);
    TEST_ASSERT_EQUAL_UINT64(0u, embed_atomic_word_load(&word));
}

void test_a_set_at_rest_is_one_lane(void)
{
    static EmbedTurnstiles s_set;

    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_open_lane_count(&s_set));
}

void test_an_entry_takes_a_free_lane_and_its_leave_clears_busy_and_counts_the_pass(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int place = embed_turnstile_enter(&s_set, address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(0u, place);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY_BIT, lane_word(&s_set, 0u));
    embed_turnstile_leave(&s_set, place);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_GENERATION_UNIT, lane_word(&s_set, 0u));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_set.queue_length));
}

void test_an_entry_on_a_full_set_grows_it_and_the_last_leave_on_top_closes_the_lane(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int first_place = embed_turnstile_enter(&s_set, address_of_lane(0u));
    const unsigned int second_place = embed_turnstile_enter(&s_set, address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(0u, first_place);
    TEST_ASSERT_EQUAL_UINT(1u, second_place);
    TEST_ASSERT_EQUAL_UINT(2u, embed_turnstile_open_lane_count(&s_set));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_set.queue_length));
    embed_turnstile_leave(&s_set, second_place);
    // The top lane left with the queue empty: the contraction closed it, busy and closed set, the close counted
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_open_lane_count(&s_set));
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT |
                                 (2u * EMBED_TURNSTILE_GENERATION_UNIT),
                             lane_word(&s_set, 1u));
    embed_turnstile_leave(&s_set, first_place);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_open_lane_count(&s_set));
}

void test_a_growth_reopens_a_closed_lane(void)
{
    static EmbedTurnstiles s_set;

    embed_turnstile_leave(&s_set, embed_turnstile_enter(&s_set, address_of_lane(0u)));
    const unsigned int first_place = embed_turnstile_enter(&s_set, address_of_lane(0u));
    const unsigned int second_place = embed_turnstile_enter(&s_set, address_of_lane(0u));

    embed_turnstile_leave(&s_set, second_place);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_open_lane_count(&s_set));
    // The second pass on lane 0's address finds lane 0 busy and grows the set: lane 1 reopens, and the pass takes it
    const unsigned int reopened_place = embed_turnstile_enter(&s_set, address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(1u, reopened_place);
    TEST_ASSERT_EQUAL_UINT(2u, embed_turnstile_open_lane_count(&s_set));
    TEST_ASSERT_EQUAL_UINT64(0u, lane_word(&s_set, 1u) & EMBED_TURNSTILE_CLOSED_BIT);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY_BIT, lane_word(&s_set, 1u) & EMBED_TURNSTILE_BUSY_BIT);
    embed_turnstile_leave(&s_set, reopened_place);
    embed_turnstile_leave(&s_set, first_place);
}

void test_an_entry_on_a_busy_lane_spills_to_the_next_open_lane(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int first_place = embed_turnstile_enter(&s_set, address_of_lane(0u));
    const unsigned int second_place = embed_turnstile_enter(&s_set, address_of_lane(0u));
    const unsigned int third_place = embed_turnstile_enter(&s_set, address_of_lane(1u));

    // Three passes on two start lanes: the third finds lanes 1 and 0 busy and grows the set to lane 2
    TEST_ASSERT_EQUAL_UINT(2u, third_place);
    TEST_ASSERT_EQUAL_UINT(3u, embed_turnstile_open_lane_count(&s_set));
    embed_turnstile_leave(&s_set, first_place);
    // A leave below the top does not contract
    TEST_ASSERT_EQUAL_UINT(3u, embed_turnstile_open_lane_count(&s_set));
    embed_turnstile_leave(&s_set, second_place);
    TEST_ASSERT_EQUAL_UINT(3u, embed_turnstile_open_lane_count(&s_set));
    // The leave on the top lane closes each free lane from the top down to one
    embed_turnstile_leave(&s_set, third_place);
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_open_lane_count(&s_set));
}

void test_a_drain_with_no_pass_under_way_returns_at_once(void)
{
    static EmbedTurnstiles s_set;
    const unsigned long long start_ms = host_clock_ms();

    embed_turnstiles_drain(&s_set);
    TEST_ASSERT_TRUE((host_clock_ms() - start_ms) < 100u);
}

static EmbedTurnstiles s_long_set;
static embed_atomic_u32 s_entry_mark = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_finish_mark = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_early_drain_mark = EMBED_ATOMIC_INIT(0u);

/** @brief Thread 0 passes through the long set for 60 milliseconds. Thread 1 drains after the pass began. */
static void long_pass_and_drain(void)
{
    if (thread_id() == 0u)
    {
        const unsigned int place = embed_turnstile_enter(&s_long_set, address_of_lane(0u));

        embed_atomic_u32_store(&s_entry_mark, 1u);
        clock_spin(60u);
        embed_atomic_u32_store(&s_finish_mark, 1u);
        embed_turnstile_leave(&s_long_set, place);
        return;
    }
    if (!flag_wait(&s_entry_mark, 1u))
    {
        return;
    }
    embed_turnstiles_drain(&s_long_set);
    if (embed_atomic_u32_load(&s_finish_mark) == 0u)
    {
        embed_atomic_u32_store(&s_early_drain_mark, 1u);
    }
}

void test_a_drain_waits_out_a_pass_begun_before_it(void)
{
    embed_atomic_u32_store(&s_entry_mark, 0u);
    embed_atomic_u32_store(&s_finish_mark, 0u);
    embed_atomic_u32_store(&s_early_drain_mark, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(long_pass_and_drain, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32(1u, embed_atomic_u32_load(&s_entry_mark));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_early_drain_mark));
}

static EmbedTurnstiles s_crowd;
static embed_atomic_u32 s_crowd_passes = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_crowd_lanes_max = EMBED_ATOMIC_INIT(0u);

/** @brief Each thread passes through s_crowd for a rest on one start lane, and it records the lanes it saw open. */
static void crowd(void)
{
    (void)thread_id();
    const unsigned int place = embed_turnstile_enter(&s_crowd, address_of_lane(0u));

    // Explicit cast converts the count of open lanes, at max EMBED_TURNSTILE_LANES, to the width of the counter
    (void)embed_atomic_u32_raise(&s_crowd_lanes_max, (embed_u32)embed_turnstile_open_lane_count(&s_crowd));
    clock_spin(5u);
    (void)embed_atomic_u32_fetch_add(&s_crowd_passes, 1u);
    embed_turnstile_leave(&s_crowd, place);
}

void test_a_crowd_grows_the_set_to_its_pressure_and_no_lane_stays_busy(void)
{
    embed_atomic_u32_store(&s_crowd_passes, 0u);
    embed_atomic_u32_store(&s_crowd_lanes_max, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(crowd, 4u), "every thread started");
    TEST_ASSERT_EQUAL_UINT32(4u, embed_atomic_u32_load(&s_crowd_passes));
    TEST_ASSERT_TRUE_MESSAGE(embed_atomic_u32_load(&s_crowd_lanes_max) > 1u, "a lane opened under the crowd");
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_crowd.queue_length));
    const unsigned int open_lane_count = embed_turnstile_open_lane_count(&s_crowd);

    for (unsigned int place = 0u; place < open_lane_count; place += 1u)
    {
        TEST_ASSERT_EQUAL_UINT64(0u, lane_word(&s_crowd, place) & EMBED_TURNSTILE_BUSY_BIT);
    }
}

static EmbedTurnstiles s_full_set;
static unsigned int s_full_places[EMBED_TURNSTILE_LANES];
static embed_atomic_u32 s_full_mark = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_late_place = EMBED_ATOMIC_INIT(0u);

/**
 * @brief Thread 0 takes every lane, and frees lane 0 after thread 1 joins the queue. Thread 1 enters the full set
 *        from the address of lane 0. It finds no lane to grow to, backs off on lane 0, and takes it after the free.
 */
static void full_set_and_late_entry(void)
{
    if (thread_id() == 0u)
    {
        for (unsigned int lane = 0u; lane < EMBED_TURNSTILE_LANES; lane += 1u)
        {
            s_full_places[lane] = embed_turnstile_enter(&s_full_set, address_of_lane(0u));
        }
        embed_atomic_u32_store(&s_full_mark, 1u);
        // The queue holds thread 1 after it found every open lane busy, and its growth then reads the full count
        if (flag_wait(&s_full_set.queue_length, 1u))
        {
            embed_turnstile_leave(&s_full_set, s_full_places[0]);
            (void)flag_wait(&s_full_set.queue_length, 0u);
        }
        for (unsigned int lane = 1u; lane < EMBED_TURNSTILE_LANES; lane += 1u)
        {
            embed_turnstile_leave(&s_full_set, s_full_places[lane]);
        }
        return;
    }
    if (!flag_wait(&s_full_mark, 1u))
    {
        return;
    }
    const unsigned int place = embed_turnstile_enter(&s_full_set, address_of_lane(0u));

    // Explicit cast converts the place, below EMBED_TURNSTILE_LANES, to the width of the mark, offset by one
    embed_atomic_u32_store(&s_late_place, (embed_u32)place + 1u);
    embed_turnstile_leave(&s_full_set, place);
}

void test_an_entry_on_a_set_with_every_lane_open_and_busy_waits_for_a_lane_to_free(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(full_set_and_late_entry, 2u), "both threads started");
    for (unsigned int lane = 0u; lane < EMBED_TURNSTILE_LANES; lane += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(lane, s_full_places[lane]);
    }
    // The late entry took lane 0, the lane that the free gave back
    TEST_ASSERT_EQUAL_UINT32(1u, embed_atomic_u32_load(&s_late_place));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_full_set.queue_length));
    TEST_ASSERT_EQUAL_UINT(1u, embed_turnstile_open_lane_count(&s_full_set));
    TEST_ASSERT_EQUAL_UINT64(0u, lane_word(&s_full_set, 0u) & EMBED_TURNSTILE_BUSY_BIT);
}

/** @brief A record a writer publishes: 1 while published or a pass may still read it, 0 once the writer released it. */
typedef struct
{
    embed_atomic_u32 live_mark;
} TestRecord;

static TestRecord s_records[2];
static embed_atomic_uptr s_publication = EMBED_ATOMIC_INIT(0u);
static EmbedTurnstiles s_swap_set;
static embed_atomic_u32 s_write_mark = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_reader_count = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_stale_reads = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_read_count = EMBED_ATOMIC_INIT(0u);

/** @brief The record that s_publication leads to. */
static TestRecord *published_record(void)
{
    // Explicit cast converts the published address back to the record that the writer stored there
    return (TestRecord *)(embed_atomic_uptr_value)embed_atomic_uptr_load(&s_publication);
}

/**
 * @brief Thread 0 writes after every worker reads. Each swap publishes the other record, drains, and releases the
 *        one that it swapped out. The workers pass through s_swap_set while it writes, each from its own address.
 *        Each worker reads the published record and counts a read of a released one.
 */
static void writer_and_readers(void)
{
    const embed_u32 id = thread_id();

    if (id == 0u)
    {
        if (!flag_wait(&s_reader_count, TEST_READERS))
        {
            embed_atomic_u32_store(&s_write_mark, 0u);
            return;
        }
        for (embed_u32 swap_index = 0u; swap_index < TEST_SWAPS; swap_index += 1u)
        {
            TestRecord *const old_record = published_record();
            TestRecord *const new_record = (old_record == &s_records[0]) ? &s_records[1] : &s_records[0];

            embed_atomic_u32_store(&new_record->live_mark, 1u);
            // Explicit cast carries the address of the new record as the integer that the publication holds
            (void)embed_atomic_uptr_exchange(&s_publication, (embed_atomic_uptr_value)new_record);
            embed_turnstiles_drain(&s_swap_set);
            embed_atomic_u32_store(&old_record->live_mark, 0u);
        }
        embed_atomic_u32_store(&s_write_mark, 0u);
        return;
    }
    (void)embed_atomic_u32_fetch_add(&s_reader_count, 1u);
    while (embed_atomic_u32_load(&s_write_mark) != 0u)
    {
        const unsigned int place = embed_turnstile_enter(&s_swap_set, address_of_lane(id));
        const TestRecord *const record = published_record();

        if (embed_atomic_u32_load(&record->live_mark) == 0u)
        {
            (void)embed_atomic_u32_fetch_add(&s_stale_reads, 1u);
        }
        (void)embed_atomic_u32_fetch_add(&s_read_count, 1u);
        embed_turnstile_leave(&s_swap_set, place);
    }
}

void test_workers_never_read_a_record_the_writer_released_after_its_drain(void)
{
    embed_atomic_u32_store(&s_records[0].live_mark, 1u);
    embed_atomic_u32_store(&s_records[1].live_mark, 0u);
    // Explicit cast carries the address of the first record as the integer that the publication holds
    embed_atomic_uptr_store(&s_publication, (embed_atomic_uptr_value)&s_records[0]);
    embed_atomic_u32_store(&s_write_mark, 1u);
    embed_atomic_u32_store(&s_reader_count, 0u);
    embed_atomic_u32_store(&s_stale_reads, 0u);
    embed_atomic_u32_store(&s_read_count, 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(writer_and_readers, TEST_READERS + 1u), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(TEST_READERS, embed_atomic_u32_load(&s_reader_count), "every worker read");
    TEST_ASSERT_TRUE(embed_atomic_u32_load(&s_read_count) != 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_stale_reads));
    TEST_ASSERT_EQUAL_UINT32(0u, embed_atomic_u32_load(&s_swap_set.queue_length));
}
