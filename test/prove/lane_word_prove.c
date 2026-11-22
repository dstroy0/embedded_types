/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file lane_word_prove.c
 * @brief The lane word of LaneWord.lean and the spill of Open.lean, on the C operations of the turnstile. One
 *        operation runs on lane words of any value outside class 2, with a top lane and an open count of any value.
 *        Each word after it has the value that the theorem names, and no word is in class 2.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note A harness for CBMC, which harness.py prove runs on each word width of its bound. A count that the caller
 *       read before a change of the top lane is a stale count, and the harness takes it too.
 */
// Four lanes: a spill over more than two lanes, and a growth below the last lane
#define EMBED_TURNSTILE_LANES 4u

#include "prove.h"

#include "embed_turnstile.h"

/** @brief The calls of the host's wait and wake. */
static unsigned int s_wait_calls;
static unsigned int s_wake_calls;

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    (void)args;
    s_wait_calls += 1u;
    return 0;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    (void)args;
    s_wake_calls += 1u;
}

embed_word embed_clock_ns(void)
{
    return 0u;
}

embed_u32 embed_thread_place(void)
{
    return 0u;
}

/** @brief The operations of the harness. */
enum
{
    PROVE_SPILL,
    PROVE_LEAVE,
    PROVE_CONTRACT,
    PROVE_GROW,
    PROVE_OPERATION_COUNT
};

/** @brief The set. */
static EmbedTurnstiles s_set;

/** @brief The class of a lane word: 0 free, 1 taken, 3 closed, and 2 for closed with busy clear. */
static embed_atomic_word_value prove_class(embed_atomic_word_value word)
{
    return word & (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT);
}

/** @brief The generation of a lane word, and the generation one count later. */
static embed_atomic_word_value prove_generation(embed_atomic_word_value word)
{
    return word >> EMBED_TURNSTILE_GENERATION_SHIFT;
}

static embed_atomic_word_value prove_counted(embed_atomic_word_value word)
{
    return prove_generation(word + EMBED_TURNSTILE_GENERATION_UNIT);
}

/** @brief The word of lane place. */
static embed_atomic_word_value prove_lane(unsigned int place)
{
    return EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_set.lanes[place].word);
}

/** @brief The word that one fetch-add of the addend leaves on word, the sleeper bit cleared, as lane_add leaves it. */
static embed_atomic_word_value prove_added(embed_atomic_word_value word, embed_atomic_word_value addend)
{
    return (word + addend) & ~EMBED_TURNSTILE_SLEEPER_BIT;
}

int main(void)
{
    embed_atomic_word_value start_words[EMBED_TURNSTILE_LANES];
    const unsigned int operation = nondet_uint();
    const unsigned int open_lane_count = nondet_uint();
    const unsigned int place = nondet_uint();
    const embed_atomic_word_value top_open_lane = (embed_atomic_word_value)nondet_word();

    PROVE_ASSUME(operation < PROVE_OPERATION_COUNT);
    PROVE_ASSUME((open_lane_count >= 1u) && (open_lane_count <= EMBED_TURNSTILE_LANES));
    PROVE_ASSUME(top_open_lane < EMBED_TURNSTILE_LANES);
    for (unsigned int lane = 0u; lane < EMBED_TURNSTILE_LANES; lane += 1u)
    {
        start_words[lane] = (embed_atomic_word_value)nondet_word();
        // class_two_never: no operation leaves a word in class 2, so no lane word starts there
        PROVE_ASSUME(prove_class(start_words[lane]) != EMBED_TURNSTILE_CLOSED_BIT);
        EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_set.lanes[lane].word,
                   .value = start_words[lane]);
    }
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_set.top_open_lane,
               .value = top_open_lane);

    // Explicit cast converts the test of the top lane, an int of 0 or 1, to the project boolean
    const embed_bool count_current = (embed_bool)(top_open_lane + 1u == open_lane_count);
    unsigned int changed_place = EMBED_TURNSTILE_NO_LANE;
    embed_atomic_word_value changed_word = 0u;
    embed_atomic_word_value end_top_lane = top_open_lane;
    unsigned int expected_wakes = 0u;

    if (operation == PROVE_SPILL)
    {
        PROVE_ASSUME(place < open_lane_count);
        const unsigned int taken = EMBED_CALL(embed_turnstile_spill, EmbedTurnstileSpillCfg, .set = &s_set,
                                              .open_lane_count = open_lane_count, .start_lane = place);
        unsigned int first_free = EMBED_TURNSTILE_NO_LANE;

        // spill_takes_first: the first open lane from the start lane, modulo the count, with busy clear
        for (unsigned int tries = 0u; tries < open_lane_count; tries += 1u)
        {
            const unsigned int lane = (place + tries) % open_lane_count;

            if ((first_free == EMBED_TURNSTILE_NO_LANE) && ((start_words[lane] & EMBED_TURNSTILE_BUSY_BIT) == 0u))
            {
                first_free = lane;
            }
        }
        PROVE_ASSERT(taken == first_free,
                     "spill_takes_first and spill_none_refused: the spill takes the first free lane");
        if (taken != EMBED_TURNSTILE_NO_LANE)
        {
            changed_place = taken;
            // entry_takes: busy set, and every other bit as it was
            changed_word = start_words[taken] + EMBED_TURNSTILE_BUSY_BIT;
        }
    }
    else if (operation == PROVE_LEAVE)
    {
        // A pass holds a lane of class 1: busy set and closed clear
        PROVE_ASSUME((place < EMBED_TURNSTILE_LANES) && (prove_class(start_words[place]) == EMBED_TURNSTILE_BUSY_BIT));
        const embed_atomic_word_value word = start_words[place];

        EMBED_CALL(embed_turnstile_lane_add, EmbedTurnstileLaneAddCfg, .lane = &s_set.lanes[place],
                   .addend = EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_BUSY_BIT);
        const embed_atomic_word_value left_word = prove_lane(place);

        // leave: busy clear, closed retained, and one count in the generation
        PROVE_ASSERT((left_word & EMBED_TURNSTILE_BUSY_BIT) == 0u, "leave: a leave clears busy");
        PROVE_ASSERT((left_word & EMBED_TURNSTILE_CLOSED_BIT) == (word & EMBED_TURNSTILE_CLOSED_BIT),
                     "leave: a leave retains closed");
        PROVE_ASSERT(prove_generation(left_word) == prove_counted(word), "leave: a leave counts one in the generation");
        // changes_futex_bits and end_over: the leave moves the low 32 bits, and the drain reads the pass over
        // Explicit casts retain the low 32 bits of each word, the bits the futex compares
        PROVE_ASSERT((uint32_t)left_word != (uint32_t)word, "changes_futex_bits: a leave changes the low 32 bits");
        PROVE_ASSERT(prove_generation(left_word) != prove_generation(word), "end_over: the drain reads the pass over");
        changed_place = place;
        changed_word = prove_added(word, EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_BUSY_BIT);
        // Explicit cast converts the test of the sleeper bit, an int of 0 or 1, to the count of wakes
        expected_wakes = (unsigned int)((word & EMBED_TURNSTILE_SLEEPER_BIT) != 0u);
    }
    else if (operation == PROVE_CONTRACT)
    {
        PROVE_ASSUME(open_lane_count > 1u);
        const unsigned int top_place = open_lane_count - 1u;
        const embed_atomic_word_value word = start_words[top_place];
        const embed_bool closed = EMBED_CALL(embed_turnstile_contract, EmbedTurnstileContractCfg, .set = &s_set,
                                             .open_lane_count = open_lane_count);

        // A lane that a pass holds refuses the claim. A free lane closes where the count is current, and else the
        // contraction releases it.
        PROVE_ASSERT(closed == (((word & EMBED_TURNSTILE_BUSY_BIT) == 0u) && count_current),
                     "close: a contraction closes a free top lane exactly where the count is current");
        if ((word & EMBED_TURNSTILE_BUSY_BIT) == 0u)
        {
            const embed_atomic_word_value claimed = word | EMBED_TURNSTILE_BUSY_BIT;

            changed_place = top_place;
            changed_word =
                prove_added(claimed, count_current ? (EMBED_TURNSTILE_GENERATION_UNIT + EMBED_TURNSTILE_CLOSED_BIT)
                                                   : (EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_BUSY_BIT));
            // Explicit cast converts the test of the sleeper bit, an int of 0 or 1, to the count of wakes
            expected_wakes = (unsigned int)((word & EMBED_TURNSTILE_SLEEPER_BIT) != 0u);
            end_top_lane = count_current ? (top_open_lane - 1u) : top_open_lane;
            PROVE_ASSERT(prove_class(changed_word) == (count_current ? 3u : 0u),
                         "close: a closed lane is busy and closed, and a released lane is free");
            PROVE_ASSERT(prove_generation(changed_word) == prove_counted(word),
                         "close: a close or a release counts one in the generation");
        }
    }
    else
    {
        embed_atomic_word_value held_word = 0u;
        const unsigned int grown = EMBED_CALL(embed_turnstile_grow, EmbedTurnstileGrowCfg, .set = &s_set,
                                              .open_lane_count = open_lane_count, .held_word = &held_word);

        if (open_lane_count >= EMBED_TURNSTILE_LANES)
        {
            PROVE_ASSERT((grown == EMBED_TURNSTILE_NO_LANE) && (held_word == 0u), "a full set has no lane to grow to");
        }
        else
        {
            const embed_atomic_word_value word = start_words[open_lane_count];
            const embed_atomic_word_value state = prove_class(word);

            if (state == EMBED_TURNSTILE_BUSY_BIT)
            {
                PROVE_ASSERT((grown == EMBED_TURNSTILE_NO_LANE) && (held_word == word),
                             "a held lane refuses the growth, and the grower sleeps on its word");
            }
            else
            {
                // takeover: a closed lane opens with busy kept, and a free lane takes busy
                const embed_atomic_word_value taken =
                    ((state == 0u) ? (word | EMBED_TURNSTILE_BUSY_BIT)
                                   : (word + EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_CLOSED_BIT)) &
                    ~EMBED_TURNSTILE_SLEEPER_BIT;
                const embed_atomic_word_value give_back =
                    (state == 0u) ? (EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_BUSY_BIT)
                                  : (EMBED_TURNSTILE_GENERATION_UNIT + EMBED_TURNSTILE_CLOSED_BIT);

                PROVE_ASSERT(grown == (count_current ? open_lane_count : EMBED_TURNSTILE_NO_LANE),
                             "a growth takes the lane exactly where the count is current");
                PROVE_ASSERT(held_word == 0u, "a growth that takes the lane sleeps on no word");
                changed_place = open_lane_count;
                changed_word = count_current ? taken : (taken + give_back);
                // Explicit cast converts the test of the sleeper bit, an int of 0 or 1, to the count of wakes
                expected_wakes = (unsigned int)((word & EMBED_TURNSTILE_SLEEPER_BIT) != 0u);
                end_top_lane = count_current ? (top_open_lane + 1u) : top_open_lane;
                if (count_current)
                {
                    PROVE_ASSERT(prove_class(changed_word) == EMBED_TURNSTILE_BUSY_BIT,
                                 "takeover: the lane is the grower's, busy and open");
                }
                else
                {
                    PROVE_ASSERT(prove_class(changed_word) == state, "a growth that fails gives the lane back");
                }
            }
        }
    }

    // Each lane but the one that the operation changed has its start word, and no word is in class 2
    for (unsigned int lane = 0u; lane < EMBED_TURNSTILE_LANES; lane += 1u)
    {
        const embed_atomic_word_value word = prove_lane(lane);

        PROVE_ASSERT(word == ((lane == changed_place) ? changed_word : start_words[lane]),
                     "each word has the value of the operation, and each other word is as it was");
        PROVE_ASSERT(prove_class(word) != EMBED_TURNSTILE_CLOSED_BIT, "class_two_never: no word is in class 2");
    }
    PROVE_ASSERT(EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_set.top_open_lane) ==
                     end_top_lane,
                 "the top lane moves exactly where a growth or a close lands");
    // sleeper_bit: the end of a hold on a word with the sleeper bit wakes it once
    PROVE_ASSERT(s_wake_calls == expected_wakes, "the end of a hold on a word with the sleeper bit wakes it once");
    PROVE_ASSERT(s_wait_calls == 0u, "no operation of the lane word sleeps");
    PROVE_DONE();
    return 0;
}
