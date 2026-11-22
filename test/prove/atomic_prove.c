/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file atomic_prove.c
 * @brief The atomic operations of Atomic.lean, on the tables of the 32-bit word and the 64-bit word. Each operation
 *        runs on a word of any value with a value of any value. It answers the word before, and it leaves the word
 *        that its contract names.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note A harness for CBMC, which harness.py prove runs with the unwind of its bound. The raise loops one time more
 *       than its swaps, and with one thread it swaps one time at max.
 */
#include "prove.h"

#include "embed_atomic.h"

/** @brief The word of each width. */
static embed_atomic_u32 s_word_u32;
static embed_atomic_u64 s_word_u64;

/**
 * @brief The checks of one width: the table name_ of the width Width_, on words of type_ that nondet_ gives.
 * @note Each check stores a start word of any value, runs one operation, and reads the word after it.
 * @note cas_contract: the swap lands exactly on the expected word, and else answers the word it found.
 * @note The fetch operations answer the word before and combine the value with it, and the sum wraps.
 * @note raise_round and raise_exit: the raise answers the word before and leaves the max of the two.
 */
#define PROVE_ATOMIC_WIDTH(name_, Width_, type_, nondet_)                                                              \
    static void prove_atomic_##name_(void)                                                                             \
    {                                                                                                                  \
        const type_ start_word = nondet_();                                                                            \
        const type_ value = nondet_();                                                                                 \
        const type_ expected_word = nondet_();                                                                         \
        embed_atomic_##name_ *const word = &s_word_##name_;                                                            \
        type_ expected = expected_word;                                                                                \
                                                                                                                       \
        EMBED_CALL(embed_atomic_##name_##_table.store, EmbedAtomic##Width_##StoreCfg, .word = word,                    \
                   .value = start_word);                                                                               \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.load, EmbedAtomic##Width_##LoadCfg, .word = word) ==      \
                         start_word,                                                                                   \
                     "a load after a store reads the value of the store");                                             \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.exchange, EmbedAtomic##Width_##ExchangeCfg, .word = word, \
                                .value = value) == start_word,                                                         \
                     "an exchange answers the word before");                                                           \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.load, EmbedAtomic##Width_##LoadCfg, .word = word) ==      \
                         value,                                                                                        \
                     "an exchange leaves its value");                                                                  \
        EMBED_CALL(embed_atomic_##name_##_table.store, EmbedAtomic##Width_##StoreCfg, .word = word,                    \
                   .value = start_word);                                                                               \
        const embed_bool swapped = EMBED_CALL(embed_atomic_##name_##_table.swap_if, EmbedAtomic##Width_##SwapIfCfg,    \
                                              .word = word, .expected = &expected, .value = value);                    \
        PROVE_ASSERT(swapped == (start_word == expected_word), "cas_contract: the swap lands on the expected word");   \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.load, EmbedAtomic##Width_##LoadCfg, .word = word) ==      \
                         (swapped ? value : start_word),                                                               \
                     "cas_contract: a swap writes its value, and a failed one changes nothing");                       \
        PROVE_ASSERT(expected == start_word, "cas_contract: expected takes the word that the swap found");             \
        EMBED_CALL(embed_atomic_##name_##_table.store, EmbedAtomic##Width_##StoreCfg, .word = word,                    \
                   .value = start_word);                                                                               \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.fetch_add, EmbedAtomic##Width_##FetchAddCfg,              \
                                .word = word, .value = value) == start_word,                                           \
                     "a fetch-add answers the word before");                                                           \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.load, EmbedAtomic##Width_##LoadCfg, .word = word) ==      \
                         (type_)(start_word + value),                                                                  \
                     "a fetch-add leaves the sum modulo the width");                                                   \
        EMBED_CALL(embed_atomic_##name_##_table.store, EmbedAtomic##Width_##StoreCfg, .word = word,                    \
                   .value = start_word);                                                                               \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.fetch_sub, EmbedAtomic##Width_##FetchSubCfg,              \
                                .word = word, .value = value) == start_word,                                           \
                     "a fetch-subtract answers the word before");                                                      \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.load, EmbedAtomic##Width_##LoadCfg, .word = word) ==      \
                         (type_)(start_word - value),                                                                  \
                     "sub_by_add: a fetch-subtract leaves the difference modulo the width");                           \
        EMBED_CALL(embed_atomic_##name_##_table.store, EmbedAtomic##Width_##StoreCfg, .word = word,                    \
                   .value = start_word);                                                                               \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.fetch_or, EmbedAtomic##Width_##FetchOrCfg, .word = word,  \
                                .value = value) == start_word,                                                         \
                     "a fetch-or answers the word before");                                                            \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.load, EmbedAtomic##Width_##LoadCfg, .word = word) ==      \
                         (start_word | value),                                                                         \
                     "a fetch-or leaves the or");                                                                      \
        EMBED_CALL(embed_atomic_##name_##_table.store, EmbedAtomic##Width_##StoreCfg, .word = word,                    \
                   .value = start_word);                                                                               \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.fetch_and, EmbedAtomic##Width_##FetchAndCfg,              \
                                .word = word, .value = value) == start_word,                                           \
                     "a fetch-and answers the word before");                                                           \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.load, EmbedAtomic##Width_##LoadCfg, .word = word) ==      \
                         (start_word & value),                                                                         \
                     "a fetch-and leaves the and");                                                                    \
        EMBED_CALL(embed_atomic_##name_##_table.store, EmbedAtomic##Width_##StoreCfg, .word = word,                    \
                   .value = start_word);                                                                               \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.raise, EmbedAtomic##Width_##RaiseCfg, .word = word,       \
                                .value = value) == start_word,                                                         \
                     "raise_round: a raise answers the word before");                                                  \
        PROVE_ASSERT(EMBED_CALL(embed_atomic_##name_##_table.load, EmbedAtomic##Width_##LoadCfg, .word = word) ==      \
                         ((value > start_word) ? value : start_word),                                                  \
                     "raise_exit: a raise leaves the max, and never lowers the word");                                 \
    }

PROVE_ATOMIC_WIDTH(u32, U32, embed_u32, nondet_u32)
PROVE_ATOMIC_WIDTH(u64, U64, embed_u64, nondet_u64)

int main(void)
{
    prove_atomic_u32();
    prove_atomic_u64();
    PROVE_DONE();
    return 0;
}
