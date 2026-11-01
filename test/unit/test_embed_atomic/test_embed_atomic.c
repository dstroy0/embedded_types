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
 * @note The contention cases are the ones that tell an atomic word from a plain one: a plain word
 *       loses increments under contention and hands one claim to two takers. The counts here are
 *       exact or the arm is wrong.
 */
#include "embed_atomic.h"
#include "host_threads.h"

#include "unity.h"

/** @brief The threads the contention cases start, within what host_threads_run allows. */
#define ATOMIC_THREADS 8u

/** @brief The adds each thread makes to the counters. */
#define ATOMIC_ROUNDS 20000u

/** @brief The places the claiming case contends for, each taken by exactly one thread. */
#define ATOMIC_SLOTS 256u

static embed_atomic_u32 s_count32 = EMBED_ATOMIC_INIT(0u);
static embed_atomic_u64 s_count64 = EMBED_ATOMIC_INIT(0u);
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
    embed_atomic_u32_store(&s_count32, 0u);
    embed_atomic_u64_store(&s_count64, 0u);
    embed_atomic_u32_store(&s_next_id, 0u);
    embed_atomic_u32_store(&s_high, 0u);
    for (embed_u32 at = 0u; at < ATOMIC_SLOTS; at += 1u)
    {
        embed_atomic_u32_store(&s_slots[at], 0u);
    }
    for (embed_u32 at = 0u; at < ATOMIC_THREADS; at += 1u)
    {
        embed_atomic_u32_store(&s_claimed[at], 0u);
    }
}

void tearDown(void)
{
}

/** @brief One thread of the counting case: ATOMIC_ROUNDS adds to each counter and a raise to its id. */
static void atomic_count_rounds(void)
{
    const embed_u32 id = embed_atomic_u32_fetch_add(&s_next_id, 1u) + 1u;
    for (embed_u32 step = 0u; step < ATOMIC_ROUNDS; step += 1u)
    {
        (void)embed_atomic_u32_fetch_add(&s_count32, 1u);
        (void)embed_atomic_u64_fetch_add(&s_count64, 3u);
    }
    (void)embed_atomic_u32_raise(&s_high, id);
}

/** @brief One thread of the claiming case: every place tried once, each taken by a swap from 0 to the thread's id. */
static void atomic_claim_slots(void)
{
    const embed_u32 id = embed_atomic_u32_fetch_add(&s_next_id, 1u) + 1u;
    embed_u32 won = 0u;
    for (embed_u32 at = 0u; at < ATOMIC_SLOTS; at += 1u)
    {
        embed_u32 free_slot = 0u;
        if (embed_atomic_u32_swap_if(&s_slots[at], &free_slot, id))
        {
            won += 1u;
        }
    }
    embed_atomic_u32_store(&s_claimed[id - 1u], won);
}

void test_a_word_declared_holds_its_value_and_is_one_word_wide(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(7u);
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(9u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(7u, embed_atomic_u32_load(&word), "a 32-bit word holds what it was declared with");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(9u, embed_atomic_u64_load(&wide), "a 64-bit word holds what it was declared with");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, sizeof(embed_atomic_u32), "the 32-bit word is four bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, sizeof(embed_atomic_u64), "the 64-bit word is eight bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(embed_word), sizeof(embed_atomic_word), "the atomic word is the machine word's width");
}

void test_store_load_and_exchange_round_trip(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(0u);
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(0u);
    embed_atomic_u32_store(&word, 0x1234u);
    embed_atomic_u64_store(&wide, 0x123456789abcdef0u);
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0x1234u, embed_atomic_u32_load(&word), "a store is read back");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x123456789abcdef0u, embed_atomic_u64_load(&wide), "a wide store is read back");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0x1234u, embed_atomic_u32_exchange(&word, 0x55u), "an exchange answers the word before");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0x55u, embed_atomic_u32_load(&word), "and leaves the word given");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x123456789abcdef0u, embed_atomic_u64_exchange(&wide, 0x66u), "a wide exchange answers the word before");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0x66u, embed_atomic_u64_load(&wide), "and leaves the word given");
}

void test_swap_if_takes_the_expected_word_alone(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(5u);
    embed_u32 expected = 4u;
    TEST_ASSERT_FALSE_MESSAGE(embed_atomic_u32_swap_if(&word, &expected, 9u), "a wrong expectation swaps nothing");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, expected, "and the expectation takes the word found");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, embed_atomic_u32_load(&word), "the word stands");
    TEST_ASSERT_TRUE_MESSAGE(embed_atomic_u32_swap_if(&word, &expected, 9u), "the right expectation swaps");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, expected, "and the expectation is the word that was there");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(9u, embed_atomic_u32_load(&word), "the word is the one given");
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(5u);
    embed_u64 wide_expected = 5u;
    TEST_ASSERT_TRUE_MESSAGE(embed_atomic_u64_swap_if(&wide, &wide_expected, 1u), "the wide swap takes the expected word");
    TEST_ASSERT_FALSE_MESSAGE(embed_atomic_u64_swap_if(&wide, &wide_expected, 2u), "and refuses a stale one");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(1u, wide_expected, "handing back the word found");
}

void test_the_fetch_operations_answer_the_word_before_and_wrap(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(10u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(10u, embed_atomic_u32_fetch_add(&word, 5u), "add answers the word before");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(15u, embed_atomic_u32_fetch_sub(&word, 15u), "sub answers the word before");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, embed_atomic_u32_fetch_sub(&word, 1u), "sub below zero wraps");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xffffffffu, embed_atomic_u32_load(&word), "to every bit set");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xffffffffu, embed_atomic_u32_fetch_and(&word, 0xff00u), "and answers the word before");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xff00u, embed_atomic_u32_fetch_or(&word, 0x0fu), "or answers the word before");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xff0fu, embed_atomic_u32_load(&word), "and leaves the bits combined");
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(0u);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(0u, embed_atomic_u64_fetch_sub(&wide, 1u), "a wide sub below zero wraps");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xffffffffffffffffu, embed_atomic_u64_fetch_and(&wide, 0xf0u), "to every bit set");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xf0u, embed_atomic_u64_fetch_or(&wide, 0x0fu), "and the mask stays");
    TEST_ASSERT_EQUAL_HEX64_MESSAGE(0xffu, embed_atomic_u64_fetch_add(&wide, 1u), "or combined the bits");
}

void test_raise_moves_the_word_up_and_never_down(void)
{
    embed_atomic_u32 word = EMBED_ATOMIC_INIT(10u);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(10u, embed_atomic_u32_raise(&word, 4u), "a lower value answers the word and leaves it");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(10u, embed_atomic_u32_load(&word), "the word stands");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(10u, embed_atomic_u32_raise(&word, 12u), "a higher value answers the word before");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(12u, embed_atomic_u32_load(&word), "and raises it");
    embed_atomic_u64 wide = EMBED_ATOMIC_INIT(3u);
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(3u, embed_atomic_u64_raise(&wide, 3u), "an equal value leaves the word");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(3u, embed_atomic_u64_raise(&wide, 30u), "a higher one raises it");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(30u, embed_atomic_u64_load(&wide), "to the value given");
}

/** @brief Arrives at a gate and waits for the other thread to, so that both leave it together. */
static void order_gate(embed_u32 arrivals)
{
    (void)embed_atomic_u32_fetch_add(&s_order_arrived, 1u);
    while (embed_atomic_u32_load(&s_order_arrived) < arrivals)
    {
    }
}

/** @brief Each round writes this thread's word, orders that ahead of its read, and reads the other's. */
static void order_rounds(void)
{
    const embed_u32 mine = embed_atomic_u32_fetch_add(&s_order_next_id, 1u);
    const embed_u32 theirs = mine ^ 1u;

    for (embed_u32 round = 1u; round <= ATOMIC_ORDER_ROUNDS; round += 1u)
    {
        order_gate((round * ATOMIC_ORDER_ARRIVALS) - 2u);
        embed_atomic_u32_store(&s_order_word[mine], round);
        embed_atomic_ordered();
        embed_atomic_u32_store(&s_order_read[mine], embed_atomic_u32_load(&s_order_word[theirs]));
        order_gate(round * ATOMIC_ORDER_ARRIVALS);
        if ((mine == 0u) && (embed_atomic_u32_load(&s_order_read[0]) < round) &&
            (embed_atomic_u32_load(&s_order_read[1]) < round))
        {
            (void)embed_atomic_u32_fetch_add(&s_order_both_missed, 1u);
        }
    }
}

void test_ordered_keeps_two_threads_from_both_missing_each_others_write(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(order_rounds, 2u), "both threads started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, embed_atomic_u32_load(&s_order_both_missed),
                                     "whichever read comes second sees the other's write");
}

void test_contending_threads_lose_no_add_and_raise_to_the_highest(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(atomic_count_rounds, ATOMIC_THREADS), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ATOMIC_THREADS * ATOMIC_ROUNDS, embed_atomic_u32_load(&s_count32),
                                     "every add to the 32-bit counter is counted");
    TEST_ASSERT_EQUAL_UINT64_MESSAGE((embed_u64)ATOMIC_THREADS * ATOMIC_ROUNDS * 3u, embed_atomic_u64_load(&s_count64),
                                     "every add to the 64-bit counter is counted");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ATOMIC_THREADS, embed_atomic_u32_load(&s_high), "the raise holds the highest id");
}

void test_contending_threads_claim_each_slot_once(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(atomic_claim_slots, ATOMIC_THREADS), "every thread started");
    embed_u32 won = 0u;
    for (embed_u32 at = 0u; at < ATOMIC_THREADS; at += 1u)
    {
        won += embed_atomic_u32_load(&s_claimed[at]);
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ATOMIC_SLOTS, won, "the slots won add up to the slots, each claimed once");
    for (embed_u32 at = 0u; at < ATOMIC_SLOTS; at += 1u)
    {
        const embed_u32 holder = embed_atomic_u32_load(&s_slots[at]);
        TEST_ASSERT_TRUE_MESSAGE((holder >= 1u) && (holder <= ATOMIC_THREADS), "every slot holds one thread's id");
    }
}
