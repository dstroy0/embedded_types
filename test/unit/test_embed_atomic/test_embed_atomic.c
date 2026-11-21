/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_atomic.c
 * @brief Holds every atomic operation to its contract on one context, and under contention from the
 *        host's threads, on the arm the build takes.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note The contention cases are the ones that tell an atomic word from a plain one. A plain word
 *       loses increments under contention and passes one claim to two takers. The counts here are
 *       exact, or the arm is wrong.
 */
#include "embed_atomic.h"
#include "host_threads.h"

#include "unity.h"

/** @brief The threads the contention cases start, within what host_threads_run allows. */
#define ATOMIC_THREADS 8u

/** @brief The adds each thread makes to the counters. */
#define ATOMIC_ROUNDS 20000u

/** @brief The places that the claim case contends for, and exactly one thread takes each. */
#define ATOMIC_SLOTS 256u

static embed_atomic_u32 s_count32 = EMBED_ATOMIC_INIT(0u);
#if EMBED_ATOMIC_HAS_64
static embed_atomic_u64 s_count64 = EMBED_ATOMIC_INIT(0u);
#endif
static embed_atomic_u32 s_next_id = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_slots[ATOMIC_SLOTS];
static embed_atomic_u32 s_claimed[ATOMIC_THREADS];
static embed_atomic_u32 s_high = EMBED_ATOMIC_INIT(0u);

/** @brief The rounds of the write-then-read case, enough for a store buffer to show where the host has one. */
#define ATOMIC_ORDER_ROUNDS 200000u

/** @brief The arrivals one round makes: two gates, and the case's two threads at each. */
#define ATOMIC_ORDER_ARRIVALS 4u

static embed_atomic_u32 s_order_word[2];
static embed_atomic_u32 s_order_read[2];
static embed_atomic_u32 s_order_arrived = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_order_next_id = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u32 s_order_both_missed = EMBED_ATOMIC_INIT(0u);

void setUp(void)
{
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_count32, .value = 0u);
#if EMBED_ATOMIC_HAS_64
    EMBED_CALL(embed_atomic_u64_table.store, EmbedAtomicU64StoreCfg, .word = &s_count64, .value = 0u);
#endif
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_next_id, .value = 0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_high, .value = 0u);
    for (embed_u32 at = 0u; at < ATOMIC_SLOTS; at += 1u)
    {
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_slots[at], .value = 0u);
    }
    for (embed_u32 at = 0u; at < ATOMIC_THREADS; at += 1u)
    {
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_claimed[at], .value = 0u);
    }
}

void tearDown(void)
{
    // Each case has nothing to release
}

/** @brief One thread of the count case: ATOMIC_ROUNDS adds to each counter and a raise to its id. */
static void atomic_count_rounds(void)
{
    const embed_u32 id =
        EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_next_id, .value = 1u) + 1u;
    for (embed_u32 step = 0u; step < ATOMIC_ROUNDS; step += 1u)
    {
        (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_count32, .value = 1u);
#if EMBED_ATOMIC_HAS_64
        (void)EMBED_CALL(embed_atomic_u64_table.fetch_add, EmbedAtomicU64FetchAddCfg, .word = &s_count64, .value = 3u);
#endif
    }
    (void)EMBED_CALL(embed_atomic_u32_table.raise, EmbedAtomicU32RaiseCfg, .word = &s_high, .value = id);
}

/** @brief The raises each thread makes in the raise case. */
#define ATOMIC_RAISES 100000u

static embed_atomic_u32 s_raised32 = EMBED_ATOMIC_INIT(0u);
#if EMBED_ATOMIC_HAS_64
static embed_atomic_u64 s_raised64 = EMBED_ATOMIC_INIT(0u);
#endif

/**
 * @brief One thread of the raise case: ATOMIC_RAISES raises on each width, each value above the one before. The
 *        values of the threads interleave, so a raise often finds that another thread wrote between its load and its
 *        swap.
 *        The swap then fails, and the raise tries again over the word it found.
 */
static void atomic_raise_rounds(void)
{
    const embed_u32 id =
        EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_next_id, .value = 1u) + 1u;
    for (embed_u32 step = 1u; step <= ATOMIC_RAISES; step += 1u)
    {
        const embed_u32 value = (step * ATOMIC_THREADS) + id;
        (void)EMBED_CALL(embed_atomic_u32_table.raise, EmbedAtomicU32RaiseCfg, .word = &s_raised32, .value = value);
#if EMBED_ATOMIC_HAS_64
        (void)EMBED_CALL(embed_atomic_u64_table.raise, EmbedAtomicU64RaiseCfg, .word = &s_raised64, .value = value);
#endif
    }
}

/** @brief One thread of the claim case: it tries every place once, and a swap from 0 to its id takes a place. */
static void atomic_claim_slots(void)
{
    const embed_u32 id =
        EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_next_id, .value = 1u) + 1u;
    embed_u32 won = 0u;
    for (embed_u32 at = 0u; at < ATOMIC_SLOTS; at += 1u)
    {
        embed_u32 free_slot = 0u;
        if (EMBED_CALL(embed_atomic_u32_table.swap_if, EmbedAtomicU32SwapIfCfg, .word = &s_slots[at],
                       .expected = &free_slot, .value = id))
        {
            won += 1u;
        }
    }
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_claimed[id - 1u], .value = won);
}

void test_a_word_declared_holds_its_value_and_is_one_word_wide(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(7u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(7u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word),
                                     "a 32-bit word holds what it was declared with");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, sizeof(embed_atomic_u32), "the 32-bit word is four bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(embed_word), sizeof(embed_atomic_word),                                    \
                                     "the atomic word is the machine word's width");
#if EMBED_ATOMIC_HAS_64
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(9u);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(9u, EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &wide),
                                     "a 64-bit word holds what it was declared with");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, sizeof(embed_atomic_u64), "the 64-bit word is eight bytes");
#endif
}

void test_store_load_and_exchange_round_trip(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(0u);
    EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &word, .value = 0x1234u);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(
        0x1234u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word), "a store is read back");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(
        0x1234u, EMBED_CALL(embed_atomic_u32_table.exchange, EmbedAtomicU32ExchangeCfg, .word = &word, .value = 0x55u),
        "an exchange answers the word before");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0x55u,
                                    EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word),
                                    "and leaves the word given");
#if EMBED_ATOMIC_HAS_64
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(0u);
    EMBED_CALL(embed_atomic_u64_table.store, EmbedAtomicU64StoreCfg, .word = &wide, .value = 0x123456789abcdef0u);
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x123456789abcdef0u,
                                    EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &wide),
                                    "a wide store is read back");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        0x123456789abcdef0u,
        EMBED_CALL(embed_atomic_u64_table.exchange, EmbedAtomicU64ExchangeCfg, .word = &wide, .value = 0x66u),
        "a wide exchange answers the word before");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x66u,
                                    EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &wide),
                                    "and leaves the word given");
#endif
}

void test_swap_if_takes_the_expected_word_alone(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(5u);
    embed_u32 expected = 4u;
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_atomic_u32_table.swap_if, EmbedAtomicU32SwapIfCfg, .word = &word,
                                         .expected = &expected, .value = 9u),
                              "a wrong expectation swaps nothing");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, expected, "and the expectation takes the word found");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word),
                                     "the word is");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_atomic_u32_table.swap_if, EmbedAtomicU32SwapIfCfg, .word = &word,
                                        .expected = &expected, .value = 9u),
                             "the right expectation swaps");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, expected, "and the expectation is the word that was there");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(9u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word),
                                     "the word is the one given");
#if EMBED_ATOMIC_HAS_64
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(5u);
    embed_u64 wide_expected = 5u;
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(embed_atomic_u64_table.swap_if, EmbedAtomicU64SwapIfCfg, .word = &wide,
                                        .expected = &wide_expected, .value = 1u),
                             "the wide swap takes the expected word");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_CALL(embed_atomic_u64_table.swap_if, EmbedAtomicU64SwapIfCfg, .word = &wide,
                                         .expected = &wide_expected, .value = 2u),
                              "and refuses a stale one");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1u, wide_expected, "handing back the word found");
#endif
}

void test_the_fetch_operations_answer_the_word_before_and_wrap(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(10u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        10u, EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &word, .value = 5u),
        "add answers the word before");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        15u, EMBED_CALL(embed_atomic_u32_table.fetch_sub, EmbedAtomicU32FetchSubCfg, .word = &word, .value = 15u),
        "sub answers the word before");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0u, EMBED_CALL(embed_atomic_u32_table.fetch_sub, EmbedAtomicU32FetchSubCfg, .word = &word, .value = 1u),
        "sub below zero wraps");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(
        0xffffffffu, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word), "to every bit set");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(
        0xffffffffu,
        EMBED_CALL(embed_atomic_u32_table.fetch_and, EmbedAtomicU32FetchAndCfg, .word = &word, .value = 0xff00u),
        "and answers the word before");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(
        0xff00u, EMBED_CALL(embed_atomic_u32_table.fetch_or, EmbedAtomicU32FetchOrCfg, .word = &word, .value = 0x0fu),
        "or answers the word before");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xff0fu,
                                    EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word),
                                    "and leaves the bits combined");
#if EMBED_ATOMIC_HAS_64
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(0u);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
        0u, EMBED_CALL(embed_atomic_u64_table.fetch_sub, EmbedAtomicU64FetchSubCfg, .word = &wide, .value = 1u),
        "a wide sub below zero wraps");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        0xffffffffffffffffu,
        EMBED_CALL(embed_atomic_u64_table.fetch_and, EmbedAtomicU64FetchAndCfg, .word = &wide, .value = 0xf0u),
        "to every bit set");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        0xf0u, EMBED_CALL(embed_atomic_u64_table.fetch_or, EmbedAtomicU64FetchOrCfg, .word = &wide, .value = 0x0fu),
        "and the mask stays");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(
        0xffu, EMBED_CALL(embed_atomic_u64_table.fetch_add, EmbedAtomicU64FetchAddCfg, .word = &wide, .value = 1u),
        "or combined the bits");
#endif
}

void test_raise_moves_the_word_up_and_never_down(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(10u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        10u, EMBED_CALL(embed_atomic_u32_table.raise, EmbedAtomicU32RaiseCfg, .word = &word, .value = 4u),
        "a lower value answers the word and leaves it");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(10u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word),
                                     "the word is");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        10u, EMBED_CALL(embed_atomic_u32_table.raise, EmbedAtomicU32RaiseCfg, .word = &word, .value = 12u),
        "a higher value answers the word before");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(12u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &word),
                                     "and raises it");
#if EMBED_ATOMIC_HAS_64
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(3u);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
        3u, EMBED_CALL(embed_atomic_u64_table.raise, EmbedAtomicU64RaiseCfg, .word = &wide, .value = 3u),
        "an equal value leaves the word");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
        3u, EMBED_CALL(embed_atomic_u64_table.raise, EmbedAtomicU64RaiseCfg, .word = &wide, .value = 30u),
        "a higher one raises it");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(30u, EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &wide),
                                     "to the value given");
#endif
}

/** @brief Arrives at a gate and waits for the other thread to, so that both leave it together. */
static void order_gate(embed_u32 arrivals)
{
    (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_order_arrived,
                     .value = 1u);
    while (EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_order_arrived) < arrivals)
    {
        // The condition reads the count of arrivals, and the loop has no other step
    }
}

/** @brief Each round writes this thread's word, orders that ahead of its read, and reads the other's. */
static void order_rounds(void)
{
    const embed_u32 mine =
        EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_order_next_id, .value = 1u);
    const embed_u32 theirs = mine ^ 1u;

    for (embed_u32 round = 1u; round <= ATOMIC_ORDER_ROUNDS; round += 1u)
    {
        order_gate((round * ATOMIC_ORDER_ARRIVALS) - 2u);
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_order_word[mine], .value = round);
        embed_atomic_ordered();
        EMBED_CALL(embed_atomic_u32_table.store, EmbedAtomicU32StoreCfg, .word = &s_order_read[mine],
                   .value =
                       EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_order_word[theirs]));
        order_gate(round * ATOMIC_ORDER_ARRIVALS);
        if ((mine == 0u) &&
            (EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_order_read[0]) < round) &&
            (EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_order_read[1]) < round))
        {
            (void)EMBED_CALL(embed_atomic_u32_table.fetch_add, EmbedAtomicU32FetchAddCfg, .word = &s_order_both_missed,
                             .value = 1u);
        }
    }
}

void test_ordered_retains_two_threads_from_both_missing_each_others_write(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(order_rounds, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        0u, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_order_both_missed),
        "whichever read comes second sees the other's write");
}

void test_contending_threads_lose_no_add_and_raise_to_the_highest(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(atomic_count_rounds, ATOMIC_THREADS), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ATOMIC_THREADS * ATOMIC_ROUNDS,
                                     EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_count32),
                                     "every add to the 32-bit counter is counted");
#if EMBED_ATOMIC_HAS_64
    TEST_ASSERT_EQUAL_UINT64_MESSAGE((embed_u64)ATOMIC_THREADS * ATOMIC_ROUNDS * 3u,
                                     EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &s_count64),
                                     "every add to the 64-bit counter is counted");
#endif
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ATOMIC_THREADS,
                                     EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_high),
                                     "the raise holds the highest id");
}

void test_contending_raises_end_at_the_highest_value_of_any_thread(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(atomic_raise_rounds, ATOMIC_THREADS), "every thread started");
    // The highest value is the last raise of the thread with the highest id
    const embed_u32 highest = (ATOMIC_RAISES * ATOMIC_THREADS) + ATOMIC_THREADS;

    TEST_ASSERT_EQUAL_UINT32_MESSAGE(
        highest, EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_raised32),
        "the 32-bit word holds the highest");
#if EMBED_ATOMIC_HAS_64
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(
        highest, EMBED_CALL(embed_atomic_u64_table.load, EmbedAtomicU64LoadCfg, .word = &s_raised64),
        "the 64-bit word holds the highest");
#endif
}

void test_contending_threads_claim_each_slot_once(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(atomic_claim_slots, ATOMIC_THREADS), "every thread started");
    embed_u32 won = 0u;
    for (embed_u32 at = 0u; at < ATOMIC_THREADS; at += 1u)
    {
        won += EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_claimed[at]);
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ATOMIC_SLOTS, won, "the slots won add up to the slots, each claimed once");
    for (embed_u32 at = 0u; at < ATOMIC_SLOTS; at += 1u)
    {
        const embed_u32 holder = EMBED_CALL(embed_atomic_u32_table.load, EmbedAtomicU32LoadCfg, .word = &s_slots[at]);
        TEST_ASSERT_TRUE_MESSAGE((holder >= 1u) && (holder <= ATOMIC_THREADS), "every slot holds one thread's id");
    }
}
