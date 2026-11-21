/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_turnstile.h
 * @brief Turnstiles as a manifold of lanes, and the sleep on the address of a word that their waits use.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-07
 *
 * A set of turnstiles is a manifold of lanes. Each lane is one word on a cache line of its own. A worker enters at
 * the lane that its address selects, and where that lane is busy it spills to the next. A writer swaps a published
 * pointer, then drains: it waits until each pass that holds a lane at the drain ends. The manifold opens lanes under
 * pressure and closes them as the pressure goes.
 *
 * @note The lane word holds four fields, from bit 0 up: busy, closed, sleeper, and the generation. The busy bit shows
 *       that a pass or a contraction holds the lane. The closed bit shows a lane that a contraction closed. The
 *       sleeper bit shows that a thread sleeps on the word. The generation counts each pass end, each close and each
 *       reopen, and rolls in its bits.
 * @note Each change to a lane word is one atomic operation. An entry is a fetch-or of busy. It takes the lane where
 *       the old value has busy clear. Where the old value has busy, it changes nothing. A leave, a close, a release
 *       and a reopen are each one fetch-add, which changes the low 32 bits that the futex compares.
 * @note `top_open_lane` holds the count of open lanes less one, so a set in storage of zeros has one lane. Arrivals
 *       only read it. A growth and a contraction write it, each with one compare-and-swap. `queue_length` counts the
 *       workers that found every open lane busy. An entry on a free lane writes no word but the lane word.
 * @note The order of the passes is not the turnstile's. A consumer that needs an order keys it by the address of the
 *       worker and the worker's own sequence.
 * @note A backoff retains its start on the clock and its exponent in the caller's locals. No shared word holds a time.
 *       Inside the first unit, 2^EMBED_TURNSTILE_UNIT_SHIFT ns, a step is a pause and no call. From one unit on, a
 *       step waits on the word for 2^(EMBED_TURNSTILE_UNIT_SHIFT + exponent) ns at max, and the exponent goes up to
 *       EMBED_TURNSTILE_EXPONENT_MAX. A wake or a moved word ends the wait first.
 * @note The host defines embed_word_wait, embed_word_wake, embed_clock_ns and embed_thread_place. A build that
 *       defines none of the four fails at the link, and the error names them.
 * @note theory/embedded_types/EmbeddedTypes/Manifold proves the lane word, the backoff, the open count, the queue,
 *       the drain and the sleeps.
 * @warning The generation rolls after 2^(n - 3) counts, where n is the width of embed_atomic_word: 64 on a word of 64
 *          bits, and 32 below it. A drain that reads one lane twice, with exactly that count of passes between the two
 *          reads, waits for the next pass to end too.
 */
#ifndef EMBED_TURNSTILE_H
#define EMBED_TURNSTILE_H

#include "embed_atomic.h"

EMBED_BEGIN_DECLS

/**
 * @brief The lanes of a set: 64 for a CPU host and for a CUDA device, and 4 for a microcontroller. A build can set
 *        its own count before this header, from 1 to 64.
 *
 * @note EMBED_VECTOR_REGISTER_BITS is the boundary. A target with a vector register is a CPU host. A target with
 *       none is a microcontroller, except the device pass of CUDA. A lane takes one cache line, so 64 lanes take
 *       4 KiB, and a microcontroller cannot give that to each set.
 * @note The two passes of a CUDA build compile one set, so the device pass takes the count of the host pass.
 */
#if !defined(EMBED_TURNSTILE_LANES)
#if (EMBED_VECTOR_REGISTER_BITS > 0) || defined(__CUDA_ARCH__)
#define EMBED_TURNSTILE_LANES 64u
#else
#define EMBED_TURNSTILE_LANES 4u
#endif
#endif
EMBED_STATIC_ASSERT((EMBED_TURNSTILE_LANES >= 1u) && (EMBED_TURNSTILE_LANES <= 64u),
                    "EMBED_TURNSTILE_LANES must be from 1 to 64");

/**
 * @brief The shift of the backoff's unit, 2^shift ns: 10 for a CPU host, about 1.024 us, and 0 for a device. A build
 *        can set its own before this header.
 *
 * @note A CPU host is a target with a vector register, outside the device pass of CUDA. Each other target is a
 *       device. The value is a constant of the code and not of a layout, so the two passes of a CUDA build can
 *       differ.
 */
#if !defined(EMBED_TURNSTILE_UNIT_SHIFT)
#if (EMBED_VECTOR_REGISTER_BITS > 0) && !defined(__CUDA_ARCH__)
#define EMBED_TURNSTILE_UNIT_SHIFT 10u
#else
#define EMBED_TURNSTILE_UNIT_SHIFT 0u
#endif
#endif
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_UNIT_SHIFT <= 10u, "EMBED_TURNSTILE_UNIT_SHIFT must be 10 at max");

/**
 * @brief The max the backoff's exponent reaches on each word width, and on this word. The longest wait is
 *        2^(EMBED_TURNSTILE_UNIT_SHIFT + EMBED_TURNSTILE_EXPONENT_MAX) ns, and that fits the word below
 *        EMBED_WAIT_INFINITE.
 *
 * @note The ceiling is 2^30 ns, about 1.07 s, on a word of 32 or 64 bits, and 2^15 ns, about 32.8 us, on a word of 16
 *       bits.
 */
#define EMBED_TURNSTILE_EXPONENT_MAX_16 5u
#define EMBED_TURNSTILE_EXPONENT_MAX_32 20u
#define EMBED_TURNSTILE_EXPONENT_MAX_64 20u
#define EMBED_TURNSTILE_EXPONENT_MAX EMBED_CAT(EMBED_TURNSTILE_EXPONENT_MAX_, EMBED_WORD_BITS)
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_UNIT_SHIFT + EMBED_TURNSTILE_EXPONENT_MAX < EMBED_WORD_BITS,
                    "the longest wait of the backoff fits the word, below EMBED_WAIT_INFINITE");

/** @brief The fields of a lane word, and the unit of its generation. */
#define EMBED_TURNSTILE_BUSY_BIT ((embed_atomic_word_value)1u)
#define EMBED_TURNSTILE_CLOSED_BIT ((embed_atomic_word_value)2u)
#define EMBED_TURNSTILE_SLEEPER_BIT ((embed_atomic_word_value)4u)
#define EMBED_TURNSTILE_GENERATION_UNIT ((embed_atomic_word_value)8u)
#define EMBED_TURNSTILE_GENERATION_SHIFT 3u

/** @brief A sleep with no end, for embed_word_wait. */
#define EMBED_WAIT_INFINITE (~(embed_word)0u)

/** @brief One lane: its word on a cache line of its own, so that no pass shares the line of another. */
typedef struct
{
    EMBED_ALIGN(64u) embed_atomic_word word; /**< The lane word. */
} EmbedTurnstileLane;
EMBED_STATIC_ASSERT(sizeof(EmbedTurnstileLane) == 64u, "a lane takes one cache line");

/**
 * @brief A set of turnstiles, zeros at rest: one to each thing workers read while a writer releases its parts.
 *
 * @note `top_open_lane` is the place of the top open lane, the count of open lanes less one. `queue_length` is the
 *       count of workers that found every open lane busy.
 */
typedef struct
{
    EmbedTurnstileLane lanes[EMBED_TURNSTILE_LANES];  /**< The lanes, those below the open count open. */
    EMBED_ALIGN(64u) embed_atomic_word top_open_lane; /**< The place of the top open lane. */
    EMBED_ALIGN(64u) embed_atomic_u32 queue_length;   /**< The workers that found every open lane busy. */
} EmbedTurnstiles;

/** @brief A backoff's state, in the caller's locals: the clock at its start, and its exponent. */
typedef struct
{
    embed_word start_ns;   /**< The nanosecond that embed_clock_ns read at the start. */
    unsigned int exponent; /**< The exponent of the next wait, from 0 to EMBED_TURNSTILE_EXPONENT_MAX. */
} EmbedTurnstileBackoff;

/**
 * @brief A sleep on word while it holds old_value, at max nanoseconds, EMBED_WAIT_INFINITE for no end, and none at
 *        all for 0.
 *
 * @param[in] word        The word slept on [BORROWS].
 * @param[in] old_value   The value of the word that the caller read last.
 * @param[in] nanoseconds The max the sleep lasts. 0 answers at once with no sleep and no system call.
 * @return                1 where the word moved off old_value, 0 where it did not. A sleep is free to end early, so
 *                        a sleeper tests what it waits for in a loop.
 * @note The host defines this function on its address wait. That is a futex on Linux, WaitOnAddress on Windows, and
 *       __ulock_wait on macOS. Where the system has none, it is a timed sleep.
 */
int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value old_value, embed_word nanoseconds);

/**
 * @brief Every thread asleep on word woken.
 *
 * @param[in] word The word [BORROWS].
 * @note The host defines this beside embed_word_wait.
 */
void embed_word_wake(embed_atomic_word *word);

/**
 * @brief The host's clock in nanoseconds, which the backoff measures its time on.
 *
 * @return The nanoseconds modulo 2^EMBED_WORD_BITS, from a start that the host selects.
 * @note The host defines this beside embed_word_wait.
 * @warning The clock rolls at the width of the word: after about 4.29 s on a word of 32 bits, and 65.5 us on a word
 *          of 16 bits. The backoff takes the difference of two reads, and that difference is correct across the roll.
 *          A step one roll or more after its start reads the time modulo the roll. Where that is below one unit, the
 *          step pauses and makes no call.
 */
embed_word embed_clock_ns(void);

/**
 * @brief The place of the thread that calls: a count that names it among the threads of the program.
 *
 * @return The place, from 0. The first thread that calls answers 0, the next answers 1, and each thread answers its
 *         own place for its life. No two live threads answer one place.
 * @note The host defines this beside embed_word_wait. A host with one thread answers 0.
 * @note A consumer that holds a store for each thread indexes the stores by this place, modulo the count of its stores.
 *       Two threads whose places meet modulo that count then share one store, which is correct and slower.
 */
embed_u32 embed_thread_place(void);

/**
 * @brief The processor's pause in a spin, a compiler barrier alone where the processor has none the compiler reaches.
 * @note A build that defines it before this header retains its own, as a model checker's build does with no processor.
 */
#if !defined(EMBED_TURNSTILE_SPIN_PAUSE)
#if EMBED_HAS_BUILTIN(__builtin_ia32_pause)
#define EMBED_TURNSTILE_SPIN_PAUSE() __builtin_ia32_pause()
#elif EMBED_HAS_BUILTIN(__builtin_arm_yield)
#define EMBED_TURNSTILE_SPIN_PAUSE() __builtin_arm_yield()
#else
#define EMBED_TURNSTILE_SPIN_PAUSE() EMBED_ATOMIC_ORDERED()
#endif
#endif

/**
 * @brief The start of a backoff: the clock read, and the exponent at 0.
 *
 * @param[out] backoff The backoff [BORROWS].
 */
EMBED_INLINE void embed_turnstile_backoff_begin(EmbedTurnstileBackoff *backoff)
{
    backoff->start_ns = embed_clock_ns();
    backoff->exponent = 0u;
}

/**
 * @brief One step of a backoff on word, which the caller last read as old_value.
 *
 * The step does one pause. Where the time since the start is less than one unit, or where `sleep_allowed` is false,
 * that is all. Otherwise the step sets `sleeper_bit` on the word, and the host waits on the word for the limit of the
 * exponent. Then the exponent goes up by one, to EMBED_TURNSTILE_EXPONENT_MAX at max.
 *
 * @param[in]     word          The word [BORROWS].
 * @param[in]     old_value     The value of the word that the caller read last.
 * @param[in]     sleeper_bit   The sleeper bit of the word.
 * @param[in,out] backoff       The backoff [BORROWS].
 * @param[in]     sleep_allowed True where old_value lets the caller sleep, false for a pause alone.
 * @note The sleep needs a value that a later change ends. A caller sleeps only on a value whose change comes with a
 *       wake, such as a lane word that has the busy bit.
 */
EMBED_INLINE void embed_turnstile_backoff_step(embed_atomic_word *word, embed_atomic_word_value old_value,
                                               embed_atomic_word_value sleeper_bit, EmbedTurnstileBackoff *backoff,
                                               embed_bool sleep_allowed)
{
    EMBED_TURNSTILE_SPIN_PAUSE();
    // Explicit cast takes the difference modulo the word, because a word narrower than int promotes to int
    const embed_word elapsed_ns = (embed_word)(embed_clock_ns() - backoff->start_ns);

    if (sleep_allowed && (elapsed_ns >= ((embed_word)1u << EMBED_TURNSTILE_UNIT_SHIFT)))
    {
        (void)embed_atomic_word_fetch_or(word, sleeper_bit);
        // Explicit cast takes the limit as the word, and EMBED_TURNSTILE_EXPONENT_MAX holds it below the width
        (void)embed_word_wait(word, old_value | sleeper_bit,
                              (embed_word)((embed_word)1u << (EMBED_TURNSTILE_UNIT_SHIFT + backoff->exponent)));
        // Explicit cast converts the comparison, an int of 0 or 1, to the unsigned type of the exponent
        backoff->exponent += (unsigned int)(backoff->exponent < EMBED_TURNSTILE_EXPONENT_MAX);
    }
}

/**
 * @brief The lanes a set has open.
 *
 * @param[in] set The set [BORROWS].
 * @return        The lanes open, from 1 to EMBED_TURNSTILE_LANES.
 */
EMBED_INLINE unsigned int embed_turnstile_open_lane_count(const EmbedTurnstiles *set)
{
    // Explicit cast takes the place of the top open lane from the word, which EMBED_TURNSTILE_LANES bounds
    return (unsigned int)embed_atomic_word_load(&set->top_open_lane) + 1u;
}

/**
 * @brief A fetch-add on a lane word that the caller holds, and the wake of its sleepers where the old value had the
 *        sleeper bit.
 *
 * @param[in,out] lane   The lane [BORROWS].
 * @param[in]     addend The constant of the fetch-add.
 */
EMBED_INLINE void embed_turnstile_lane_add(EmbedTurnstileLane *lane, embed_atomic_word_value addend)
{
    const embed_atomic_word_value previous_word = embed_atomic_word_fetch_add(&lane->word, addend);

    if ((previous_word & EMBED_TURNSTILE_SLEEPER_BIT) != 0u)
    {
        (void)embed_atomic_word_fetch_and(&lane->word, ~EMBED_TURNSTILE_SLEEPER_BIT);
        embed_word_wake(&lane->word);
    }
}

/** @brief The place of no lane: the answer of a growth or a spill that took none. */
#define EMBED_TURNSTILE_NO_LANE 0xffffffffu

/**
 * @brief The growth from open_lane_count lanes to one more, which gives the caller lane open_lane_count as its pass.
 *
 * The growth takes lane open_lane_count with one compare-and-swap on the lane word. From a free word it sets busy.
 * From a closed word it clears closed and counts the takeover, and busy stays set. Then one compare-and-swap moves
 * `top_open_lane` up by one.
 *
 * Where that swap lands, the lane is the caller's pass. Where it fails, the growth gives the lane back as it was. A
 * lane that had the closed bit gets a close, and a lane that was free gets a release.
 *
 * @param[in,out] set             The set [BORROWS].
 * @param[in]     open_lane_count The lanes the caller read open.
 * @param[out]    held_word       The word of lane open_lane_count where another thread holds it, and 0 in each other
 *                                case [BORROWS].
 * @return                        open_lane_count, the lane the caller now holds, or EMBED_TURNSTILE_NO_LANE where it
 *                                holds none.
 * @note A lane that a pass, a contraction or another growth holds refuses the first compare-and-swap, and nothing
 *       changes. A contraction holds its lane from its claim to its close, so no growth takes a lane between a
 *       contraction's swap and its close. The caller sleeps on `held_word`: the end of that hold changes the word
 *       and wakes it.
 */
EMBED_INLINE unsigned int embed_turnstile_grow(EmbedTurnstiles *set, unsigned int open_lane_count,
                                               embed_atomic_word_value *held_word)
{
    *held_word = 0u;
    if (open_lane_count >= EMBED_TURNSTILE_LANES)
    {
        return EMBED_TURNSTILE_NO_LANE;
    }
    EmbedTurnstileLane *const lane = &set->lanes[open_lane_count];
    embed_atomic_word_value expected_word = embed_atomic_word_load(&lane->word);
    const embed_atomic_word_value state = expected_word & (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT);
    // Explicit cast converts the comparison, an int of 0 or 1, to the project boolean
    const embed_bool lane_closed = (embed_bool)(state == (EMBED_TURNSTILE_BUSY_BIT | EMBED_TURNSTILE_CLOSED_BIT));

    if ((state != 0u) && !lane_closed)
    {
        *held_word = expected_word;
        return EMBED_TURNSTILE_NO_LANE;
    }
    const embed_atomic_word_value taken_word =
        lane_closed ? (expected_word + EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_CLOSED_BIT)
                    : (expected_word | EMBED_TURNSTILE_BUSY_BIT);

    if (!embed_atomic_word_swap_if(&lane->word, &expected_word, taken_word))
    {
        return EMBED_TURNSTILE_NO_LANE;
    }
    if ((taken_word & EMBED_TURNSTILE_SLEEPER_BIT) != 0u)
    {
        (void)embed_atomic_word_fetch_and(&lane->word, ~EMBED_TURNSTILE_SLEEPER_BIT);
        embed_word_wake(&lane->word);
    }
    // Explicit casts widen the lane count to the word, and EMBED_TURNSTILE_LANES bounds it at 64
    embed_atomic_word_value expected_top_lane = (embed_atomic_word_value)open_lane_count - 1u;

    if (embed_atomic_word_swap_if(&set->top_open_lane, &expected_top_lane, (embed_atomic_word_value)open_lane_count))
    {
        return open_lane_count;
    }
    embed_turnstile_lane_add(lane, lane_closed ? (EMBED_TURNSTILE_GENERATION_UNIT + EMBED_TURNSTILE_CLOSED_BIT)
                                               : (EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_BUSY_BIT));
    return EMBED_TURNSTILE_NO_LANE;
}

/**
 * @brief The contraction from open_lane_count lanes to one less.
 *
 * The contraction claims the top open lane as an entry does. Then one compare-and-swap moves `top_open_lane` down by
 * one. Where the swap lands, the contraction closes the lane. Where the swap fails, the contraction releases the lane.
 *
 * @param[in,out] set             The set [BORROWS].
 * @param[in]     open_lane_count The lanes the caller read open, above 1.
 * @return                        EMBED_TRUE where the lane closed, EMBED_FALSE where a pass held it or the swap
 *                                failed.
 * @note A lane that a pass holds refuses the claim, and nothing changes. So each pass stays below the open count.
 */
EMBED_INLINE embed_bool embed_turnstile_contract(EmbedTurnstiles *set, unsigned int open_lane_count)
{
    EmbedTurnstileLane *const lane = &set->lanes[open_lane_count - 1u];
    const embed_atomic_word_value previous_word = embed_atomic_word_fetch_or(&lane->word, EMBED_TURNSTILE_BUSY_BIT);
    embed_bool lane_closed = EMBED_FALSE;

    if ((previous_word & EMBED_TURNSTILE_BUSY_BIT) == 0u)
    {
        // Explicit casts widen the lane count to the word, and EMBED_TURNSTILE_LANES bounds it at 64
        embed_atomic_word_value expected_top_lane = (embed_atomic_word_value)open_lane_count - 1u;

        lane_closed = embed_atomic_word_swap_if(&set->top_open_lane, &expected_top_lane,
                                                (embed_atomic_word_value)open_lane_count - 2u);
        embed_turnstile_lane_add(lane, lane_closed ? (EMBED_TURNSTILE_GENERATION_UNIT + EMBED_TURNSTILE_CLOSED_BIT)
                                                   : (EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_BUSY_BIT));
    }
    return lane_closed;
}

/**
 * @brief The spill of an entry over the open lanes: a fetch-or of busy on each lane from start_lane, mod
 *        open_lane_count.
 *
 * @param[in,out] set             The set [BORROWS].
 * @param[in]     open_lane_count The lanes the caller read open.
 * @param[in]     start_lane      The start lane, below open_lane_count.
 * @return                        The lane the caller now holds, or EMBED_TURNSTILE_NO_LANE where every lane was
 *                                busy.
 * @note A lane with busy in the old value refuses the fetch-or, and nothing changes. The spill writes no word but
 *       the lane words.
 */
EMBED_INLINE unsigned int embed_turnstile_spill(EmbedTurnstiles *set, unsigned int open_lane_count,
                                                unsigned int start_lane)
{
    unsigned int place = start_lane;

    for (unsigned int tries = 0u; tries < open_lane_count; tries += 1u)
    {
        const embed_atomic_word_value previous_word =
            embed_atomic_word_fetch_or(&set->lanes[place].word, EMBED_TURNSTILE_BUSY_BIT);

        if ((previous_word & EMBED_TURNSTILE_BUSY_BIT) == 0u)
        {
            return place;
        }
        place = (place + 1u == open_lane_count) ? 0u : (place + 1u);
    }
    return EMBED_TURNSTILE_NO_LANE;
}

/**
 * @brief An entry by the worker at address: a lane taken, which the end of the pass takes back.
 *
 * The entry reads the open count and starts at the lane of the address's cache line. A fetch-or of busy takes the
 * lane where the old value has busy clear. Where the old value has busy, the fetch-or changes nothing, and the entry
 * spills to the next lane.
 * Where every open lane was busy, the worker joins the queue and grows the manifold, which gives it the new lane as its
 * pass. Where it can not grow the manifold, it backs off on its start lane. It leaves the queue when it takes a lane.
 *
 * @param[in,out] set     The set [BORROWS].
 * @param[in]     address The worker's address: an immutable place, such as the worker's own state.
 * @return                The lane's place, which the end of the pass takes.
 * @note The pass reads the data of a published pointer only after the entry returns, and none of it after the leave.
 */
EMBED_INLINE unsigned int embed_turnstile_enter(EmbedTurnstiles *set, embed_atomic_uptr_value address)
{
    EmbedTurnstileBackoff backoff;
    embed_bool queue_member = EMBED_FALSE;

    for (;;)
    {
        const unsigned int open_lane_count = embed_turnstile_open_lane_count(set);
        // Explicit cast takes the address's cache line mod the count, which is below EMBED_TURNSTILE_LANES
        const unsigned int start_lane = (unsigned int)((address >> 6u) % (embed_atomic_uptr_value)open_lane_count);
        const unsigned int place = embed_turnstile_spill(set, open_lane_count, start_lane);

        if (place != EMBED_TURNSTILE_NO_LANE)
        {
            // Only a worker in the queue writes the queue, so the path of an entry on a free lane writes the lane
            // word alone
            if (queue_member)
            {
                (void)embed_atomic_u32_fetch_sub(&set->queue_length, 1u);
            }
            // The pass starts before it reads anything. The fence of a writer's drain pairs with this fence.
            EMBED_ATOMIC_ORDERED();
            return place;
        }
        if (!queue_member)
        {
            (void)embed_atomic_u32_fetch_add(&set->queue_length, 1u);
            queue_member = EMBED_TRUE;
            embed_turnstile_backoff_begin(&backoff);
        }
        embed_atomic_word_value held_word = 0u;
        const unsigned int grown_place = embed_turnstile_grow(set, open_lane_count, &held_word);

        if (grown_place != EMBED_TURNSTILE_NO_LANE)
        {
            (void)embed_atomic_u32_fetch_sub(&set->queue_length, 1u);
            // The pass starts before it reads anything. The fence of a writer's drain pairs with this fence.
            EMBED_ATOMIC_ORDERED();
            return grown_place;
        }
        // Each sleep needs busy set in the value read: the end of the hold on that lane changes the word and wakes it.
        // A lane above the count that another thread holds blocks the growth, and the worker sleeps on it. A full
        // manifold has no lane to grow to, and the worker sleeps on its start lane. In each other case the growth
        // lost a race to a change, and the worker tries again.
        if (held_word != 0u)
        {
            embed_turnstile_backoff_step(&set->lanes[open_lane_count].word, held_word, EMBED_TURNSTILE_SLEEPER_BIT,
                                         &backoff, EMBED_TRUE);
        }
        else if (open_lane_count == EMBED_TURNSTILE_LANES)
        {
            const embed_atomic_word_value start_word = embed_atomic_word_load(&set->lanes[start_lane].word);
            // Explicit cast converts the test of the busy bit, an int of 0 or 1, to the project boolean
            const embed_bool start_busy = (embed_bool)((start_word & EMBED_TURNSTILE_BUSY_BIT) != 0u);

            embed_turnstile_backoff_step(&set->lanes[start_lane].word, start_word, EMBED_TURNSTILE_SLEEPER_BIT,
                                         &backoff, start_busy);
        }
    }
}

/**
 * @brief The end of the pass through the lane at `place` of set.
 *
 * One fetch-add clears busy and counts the pass in the generation. Where the old value had the sleeper bit, the end
 * clears it and wakes the word. Where the lane is the top open lane, the end contracts the manifold. Each step closes
 * the top lane while the queue is empty, more than one lane is open, and no pass holds the top lane.
 *
 * @param[in,out] set   The set [BORROWS].
 * @param[in]     place The lane's place, as embed_turnstile_enter answered it.
 * @note Only the end of a pass on the top lane contracts. So the end of a pass on a lower lane reads and writes no
 *       word but its own lane word and `top_open_lane`.
 */
EMBED_INLINE void embed_turnstile_leave(EmbedTurnstiles *set, unsigned int place)
{
    embed_turnstile_lane_add(&set->lanes[place], EMBED_TURNSTILE_GENERATION_UNIT - EMBED_TURNSTILE_BUSY_BIT);
    unsigned int open_lane_count = embed_turnstile_open_lane_count(set);

    if (place + 1u == open_lane_count)
    {
        while ((open_lane_count > 1u) && (embed_atomic_u32_load(&set->queue_length) == 0u))
        {
            if (!embed_turnstile_contract(set, open_lane_count))
            {
                break;
            }
            open_lane_count -= 1u;
        }
    }
}

/**
 * @brief Every pass through set begun before this call waited out.
 *
 * @param[in,out] set The set [BORROWS].
 * @note A writer swaps a published pointer and calls this function. Then it releases the data that the old pointer
 *       led to. A pass that starts after the fence of the drain reads the new pointer, and the drain does not wait
 *       for it.
 * @note The drain reads the open count after its fence, then each open lane. Each pass is below the open count, so
 *       these are each lane that a pass can hold. The drain waits on each lane whose first read has busy, and retains
 *       the generation of that read. Its wait on a lane ends where busy reads clear or where the generation moved.
 * @note A round that finds the lane still held backs off on that lane word, with the sleeper bit. The end of the pass
 *       changes the word and wakes it.
 */
EMBED_INLINE void embed_turnstiles_drain(EmbedTurnstiles *set)
{
    embed_atomic_word_value snapshot[EMBED_TURNSTILE_LANES];

    // This fence orders the swap of the writer before the snapshot. It pairs with the fence after an entry's claim.
    EMBED_ATOMIC_ORDERED();
    const unsigned int open_lane_count = embed_turnstile_open_lane_count(set);

    for (unsigned int place = 0u; place < open_lane_count; place += 1u)
    {
        snapshot[place] = embed_atomic_word_load(&set->lanes[place].word);
    }
    for (unsigned int place = 0u; place < open_lane_count; place += 1u)
    {
        const embed_atomic_word_value generation = snapshot[place] >> EMBED_TURNSTILE_GENERATION_SHIFT;
        EmbedTurnstileBackoff backoff;
        embed_atomic_word_value current_word = snapshot[place];

        // A lane whose first read has the closed bit holds no pass, and the drain does not wait on it
        const embed_atomic_word_value closed_bit = snapshot[place] & EMBED_TURNSTILE_CLOSED_BIT;

        embed_turnstile_backoff_begin(&backoff);
        while ((closed_bit == 0u) && ((current_word & EMBED_TURNSTILE_BUSY_BIT) != 0u) &&
               ((current_word >> EMBED_TURNSTILE_GENERATION_SHIFT) == generation))
        {
            embed_turnstile_backoff_step(&set->lanes[place].word, current_word, EMBED_TURNSTILE_SLEEPER_BIT, &backoff,
                                         EMBED_TRUE);
            current_word = embed_atomic_word_load(&set->lanes[place].word);
        }
    }
}

EMBED_END_DECLS

#endif
