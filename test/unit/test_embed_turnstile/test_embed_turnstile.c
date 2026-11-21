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
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_next_id, .value = 0u);
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

    while ((EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = flag) != value) &&
           (host_clock_ms() < end_ms))
    {
        host_threads_yield();
    }
    return EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = flag) == value;
}

/** @brief This thread's id among the threads of one case, from 0. */
static embed_u32 thread_id(void)
{
    return EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_next_id, .value = 1u);
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
    return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &set->lanes[place].word);
}

void test_a_sleep_on_a_word_that_moved_ends_at_once_reporting_the_move(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);
    const unsigned long long start_ms = host_clock_ms();

    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(embed_turnstile_table.word_wait, EmbedWordWaitCfg, .word = &word,
                                        .old_value = 6u, .nanoseconds = TEST_SECOND_NS));
    TEST_ASSERT_TRUE((host_clock_ms() - start_ms) < 500u);
}

void test_a_sleep_on_a_still_word_ends_with_its_time_and_reports_no_move(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);

    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(embed_turnstile_table.word_wait, EmbedWordWaitCfg, .word = &word,
                                        .old_value = 7u, .nanoseconds = 10u * TEST_MILLISECOND_NS));
}

void test_a_sleep_of_no_time_answers_at_once(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(7u);
    const unsigned long long start_ms = host_clock_ms();

    TEST_ASSERT_EQUAL_INT(0, EMBED_CALL(embed_turnstile_table.word_wait, EmbedWordWaitCfg, .word = &word,
                                        .old_value = 7u, .nanoseconds = 0u));
    TEST_ASSERT_EQUAL_INT(1, EMBED_CALL(embed_turnstile_table.word_wait, EmbedWordWaitCfg, .word = &word,
                                        .old_value = 6u, .nanoseconds = 0u));
    TEST_ASSERT_TRUE((host_clock_ms() - start_ms) < 100u);
}

/** @brief The place that the thread of host_threads_run answered. */
static embed_u32 s_other_place;

/** @brief The work of the other thread: its own place recorded. */
static void other_place_recorded(void)
{
    s_other_place = embed_turnstile_table.thread_place();
}

void test_a_thread_answers_one_place_and_another_thread_answers_a_different_one(void)
{
    const embed_u32 place = embed_turnstile_table.thread_place();

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(place, embed_turnstile_table.thread_place(), "one thread, one place");
    s_other_place = place;
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(other_place_recorded, 1u), "the other thread started");
    TEST_ASSERT_TRUE_MESSAGE(s_other_place != place, "another thread, another place");
}

void test_the_clock_moves_forward(void)
{
    const embed_word start_ns = embed_turnstile_table.clock_ns();

    clock_spin(2u);
    // Explicit cast takes the difference modulo the word, which holds across the roll of the clock. The spin is far
    // below the roll on every word of 32 bits or more.
    const embed_word elapsed_ns = (embed_word)(embed_turnstile_table.clock_ns() - start_ns);

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
        while (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_word) == 0u)
        {
            (void)EMBED_CALL(embed_turnstile_table.word_wait, EmbedWordWaitCfg, .word = &s_word, .old_value = 0u,
                             .nanoseconds = EMBED_WAIT_INFINITE);
        }
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_wake_mark, .value = 1u);
        return;
    }
    clock_spin(30u);
    if (EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_wake_mark) != 0u)
    {
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_early_wake_mark, .value = 1u);
    }
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_word, .value = 1u);
    EMBED_CALL(embed_turnstile_table.word_wake, EmbedWordWakeCfg, .word = &s_word);
}

void test_a_thread_asleep_on_a_word_wakes_as_another_moves_it_and_wakes_its_address(void)
{
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_word, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_wake_mark, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_early_wake_mark, .value = 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(sleeper_and_waker, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_early_wake_mark));
    TEST_ASSERT_EQUAL_UINT32(1u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_wake_mark));
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

        EMBED_CALL(embed_turnstile_table.backoff_begin, EmbedTurnstileBackoffBeginCfg, .backoff = &backoff);
        EMBED_CALL(embed_turnstile_table.backoff_step, EmbedTurnstileBackoffStepCfg, .word = &word, .old_value = 1u,
                   .sleeper_bit = EMBED_TURNSTILE_SLEEPER_BIT, .backoff = &backoff, .sleep_allowed = EMBED_TRUE);
        // Explicit cast converts the test, an int of 0 or 1, to the unsigned type of the count
        pause_count +=
            (unsigned int)((backoff.exponent == 0u) &&
                           (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &word) == 0u));
    }
    TEST_ASSERT_TRUE(pause_count != 0u);
}

void test_a_backoff_past_its_first_unit_waits_and_raises_its_exponent_to_the_ceiling(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(0u);
    EmbedTurnstileBackoff backoff;

    EMBED_CALL(embed_turnstile_table.backoff_begin, EmbedTurnstileBackoffBeginCfg, .backoff = &backoff);
    backoff.start_ns = embed_turnstile_table.clock_ns() - TEST_SECOND_NS;
    // The word reads other than the value passed, so each wait answers at once
    for (unsigned int step = 0u; step < (EMBED_TURNSTILE_EXPONENT_MAX + 4u); step += 1u)
    {
        EMBED_CALL(embed_turnstile_table.backoff_step, EmbedTurnstileBackoffStepCfg, .word = &word, .old_value = 1u,
                   .sleeper_bit = EMBED_TURNSTILE_SLEEPER_BIT, .backoff = &backoff, .sleep_allowed = EMBED_TRUE);
    }
    TEST_ASSERT_EQUAL_UINT(EMBED_TURNSTILE_EXPONENT_MAX, backoff.exponent);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_SLEEPER_BIT,
                             EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &word));
}

void test_a_backoff_that_may_not_sleep_only_pauses(void)
{
    embed_atomic_word word = EMBED_ATOMIC_INIT(0u);
    EmbedTurnstileBackoff backoff;

    EMBED_CALL(embed_turnstile_table.backoff_begin, EmbedTurnstileBackoffBeginCfg, .backoff = &backoff);
    backoff.start_ns = embed_turnstile_table.clock_ns() - TEST_SECOND_NS;
    EMBED_CALL(embed_turnstile_table.backoff_step, EmbedTurnstileBackoffStepCfg, .word = &word, .old_value = 0u,
               .sleeper_bit = EMBED_TURNSTILE_SLEEPER_BIT, .backoff = &backoff, .sleep_allowed = EMBED_FALSE);
    TEST_ASSERT_EQUAL_UINT(0u, backoff.exponent);
    TEST_ASSERT_EQUAL_UINT64(0u, EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &word));
}

void test_a_set_at_rest_is_one_lane(void)
{
    static EmbedTurnstiles s_set;

    TEST_ASSERT_EQUAL_UINT(
        1u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
}

void test_an_entry_takes_a_free_lane_and_its_leave_clears_busy_and_counts_the_pass(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(0u, place);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY_BIT, lane_word(&s_set, 0u));
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = place);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_GENERATION_UNIT, lane_word(&s_set, 0u));
    TEST_ASSERT_EQUAL_UINT32(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_set.queue_length));
}

void test_an_entry_on_a_full_set_grows_it_and_the_last_leave_on_top_closes_the_lane(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int first_place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(0u));
    const unsigned int second_place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(0u, first_place);
    TEST_ASSERT_EQUAL_UINT(1u, second_place);
    TEST_ASSERT_EQUAL_UINT(
        2u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
    TEST_ASSERT_EQUAL_UINT32(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_set.queue_length));
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = second_place);
    // The top lane left with the queue empty: the contraction closed it, busy and closed set, the close counted
    TEST_ASSERT_EQUAL_UINT(
        1u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT |
                                 (2u * EMBED_TURNSTILE_GENERATION_UNIT),
                             lane_word(&s_set, 1u));
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = first_place);
    TEST_ASSERT_EQUAL_UINT(
        1u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
}

void test_a_growth_reopens_a_closed_lane(void)
{
    static EmbedTurnstiles s_set;

    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set,
               .place = EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set,
                                   .address = address_of_lane(0u)));
    const unsigned int first_place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(0u));
    const unsigned int second_place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(0u));

    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = second_place);
    TEST_ASSERT_EQUAL_UINT(
        1u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
    // The second pass on lane 0's address finds lane 0 busy and grows the set: lane 1 reopens, and the pass takes it
    const unsigned int reopened_place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(0u));

    TEST_ASSERT_EQUAL_UINT(1u, reopened_place);
    TEST_ASSERT_EQUAL_UINT(
        2u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
    TEST_ASSERT_EQUAL_UINT64(0u, lane_word(&s_set, 1u) & EMBED_TURNSTILE_CLOSED_BIT);
    TEST_ASSERT_EQUAL_UINT64(EMBED_TURNSTILE_BUSY_BIT, lane_word(&s_set, 1u) & EMBED_TURNSTILE_BUSY_BIT);
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = reopened_place);
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = first_place);
}

void test_an_entry_on_a_busy_lane_spills_to_the_next_open_lane(void)
{
    static EmbedTurnstiles s_set;
    const unsigned int first_place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(0u));
    const unsigned int second_place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(0u));
    const unsigned int third_place =
        EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_set, .address = address_of_lane(1u));

    // Three passes on two start lanes: the third finds lanes 1 and 0 busy and grows the set to lane 2
    TEST_ASSERT_EQUAL_UINT(2u, third_place);
    TEST_ASSERT_EQUAL_UINT(
        3u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = first_place);
    // A leave below the top does not contract
    TEST_ASSERT_EQUAL_UINT(
        3u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = second_place);
    TEST_ASSERT_EQUAL_UINT(
        3u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
    // The leave on the top lane closes each free lane from the top down to one
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_set, .place = third_place);
    TEST_ASSERT_EQUAL_UINT(
        1u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_set));
}

void test_a_drain_with_no_pass_under_way_returns_at_once(void)
{
    static EmbedTurnstiles s_set;
    const unsigned long long start_ms = host_clock_ms();

    EMBED_CALL(embed_turnstile_table.drain, EmbedTurnstilesDrainCfg, .set = &s_set);
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
        const unsigned int place = EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_long_set,
                                              .address = address_of_lane(0u));

        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_entry_mark, .value = 1u);
        clock_spin(60u);
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_finish_mark, .value = 1u);
        EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_long_set, .place = place);
        return;
    }
    if (!flag_wait(&s_entry_mark, 1u))
    {
        return;
    }
    EMBED_CALL(embed_turnstile_table.drain, EmbedTurnstilesDrainCfg, .set = &s_long_set);
    if (EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_finish_mark) == 0u)
    {
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_early_drain_mark, .value = 1u);
    }
}

void test_a_drain_waits_out_a_pass_begun_before_it(void)
{
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_entry_mark, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_finish_mark, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_early_drain_mark, .value = 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(long_pass_and_drain, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32(1u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_entry_mark));
    TEST_ASSERT_EQUAL_UINT32(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_early_drain_mark));
}

static EmbedTurnstiles s_crowd;
static embed_atomic_u32 s_crowd_passes = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_crowd_lanes_max = EMBED_ATOMIC_INIT(0u);

/** @brief Each thread passes through s_crowd for a rest on one start lane, and it records the lanes it saw open. */
static void crowd(void)
{
    (void)thread_id();
    const unsigned int place = EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_crowd,
                                          .address = address_of_lane(0u));

    // Explicit cast converts the count of open lanes, at max EMBED_TURNSTILE_LANES, to the width of the counter
    (void)EMBED_CALL(embed_atomic_u32_table.raise, EmbedAtomicU32RaiseCfg, .word = &s_crowd_lanes_max,
                     .value = (embed_u32)EMBED_CALL(embed_turnstile_table.open_lane_count,
                                                    EmbedTurnstileOpenLaneCountCfg, .set = &s_crowd));
    clock_spin(5u);
    (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_crowd_passes, .value = 1u);
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_crowd, .place = place);
}

void test_a_crowd_grows_the_set_to_its_pressure_and_no_lane_stays_busy(void)
{
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_crowd_passes, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_crowd_lanes_max, .value = 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(crowd, 4u), "every thread started");
    TEST_ASSERT_EQUAL_UINT32(4u,
                             EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_crowd_passes));
    TEST_ASSERT_TRUE_MESSAGE(
        EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_crowd_lanes_max) > 1u,
        "a lane opened under the crowd");
    TEST_ASSERT_EQUAL_UINT32(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_crowd.queue_length));
    const unsigned int open_lane_count =
        EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_crowd);

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
            s_full_places[lane] = EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_full_set,
                                             .address = address_of_lane(0u));
        }
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_full_mark, .value = 1u);
        // The queue holds thread 1 after it found every open lane busy, and its growth then reads the full count
        if (flag_wait(&s_full_set.queue_length, 1u))
        {
            EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_full_set,
                       .place = s_full_places[0]);
            (void)flag_wait(&s_full_set.queue_length, 0u);
        }
        for (unsigned int lane = 1u; lane < EMBED_TURNSTILE_LANES; lane += 1u)
        {
            EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_full_set,
                       .place = s_full_places[lane]);
        }
        return;
    }
    if (!flag_wait(&s_full_mark, 1u))
    {
        return;
    }
    const unsigned int place = EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_full_set,
                                          .address = address_of_lane(0u));

    // Explicit cast converts the place, below EMBED_TURNSTILE_LANES, to the width of the mark, offset by one
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_late_place,
               .value = (embed_u32)place + 1u);
    EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_full_set, .place = place);
}

void test_an_entry_on_a_set_with_every_lane_open_and_busy_waits_for_a_lane_to_free(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(full_set_and_late_entry, 2u), "both threads started");
    for (unsigned int lane = 0u; lane < EMBED_TURNSTILE_LANES; lane += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(lane, s_full_places[lane]);
    }
    // The late entry took lane 0, the lane that the free gave back
    TEST_ASSERT_EQUAL_UINT32(1u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_late_place));
    TEST_ASSERT_EQUAL_UINT32(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_full_set.queue_length));
    TEST_ASSERT_EQUAL_UINT(
        1u, EMBED_CALL(embed_turnstile_table.open_lane_count, EmbedTurnstileOpenLaneCountCfg, .set = &s_full_set));
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
    return (TestRecord *)(embed_atomic_uptr_value)EMBED_CALL(embed_atomic_uptr_table.load, EmbedAtomicUptrLoadCfg,
                                                             .word = &s_publication);
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
            EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_write_mark, .value = 0u);
            return;
        }
        for (embed_u32 swap_index = 0u; swap_index < TEST_SWAPS; swap_index += 1u)
        {
            TestRecord *const old_record = published_record();
            TestRecord *const new_record = (old_record == &s_records[0]) ? &s_records[1] : &s_records[0];

            EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &new_record->live_mark,
                       .value = 1u);
            // Explicit cast carries the address of the new record as the integer that the publication holds
            (void)EMBED_CALL(embed_atomic_uptr_table.exchange, EmbedAtomicUptrExchangeCfg, .word = &s_publication,
                             .value = (embed_atomic_uptr_value)new_record);
            EMBED_CALL(embed_turnstile_table.drain, EmbedTurnstilesDrainCfg, .set = &s_swap_set);
            EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &old_record->live_mark,
                       .value = 0u);
        }
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_write_mark, .value = 0u);
        return;
    }
    (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_reader_count, .value = 1u);
    while (EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_write_mark) != 0u)
    {
        const unsigned int place = EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &s_swap_set,
                                              .address = address_of_lane(id));
        const TestRecord *const record = published_record();

        if (EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &record->live_mark) == 0u)
        {
            (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_stale_reads,
                             .value = 1u);
        }
        (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_read_count,
                         .value = 1u);
        EMBED_CALL(embed_turnstile_table.leave, EmbedTurnstileLeaveCfg, .set = &s_swap_set, .place = place);
    }
}

void test_workers_never_read_a_record_the_writer_released_after_its_drain(void)
{
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_records[0].live_mark, .value = 1u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_records[1].live_mark, .value = 0u);
    // Explicit cast carries the address of the first record as the integer that the publication holds
    EMBED_CALL(embed_atomic_uptr_table.store, EmbedAtomicUptrStoreCfg, .word = &s_publication,
               .value = (embed_atomic_uptr_value)&s_records[0]);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_write_mark, .value = 1u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_reader_count, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_stale_reads, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_read_count, .value = 0u);
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(writer_and_readers, TEST_READERS + 1u), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        TEST_READERS, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_reader_count),
        "every worker read");
    TEST_ASSERT_TRUE(EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_read_count) != 0u);
    TEST_ASSERT_EQUAL_UINT32(0u,
                             EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_stale_reads));
    TEST_ASSERT_EQUAL_UINT32(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_swap_set.queue_length));
}
