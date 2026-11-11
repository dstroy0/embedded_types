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
 * @note `open` holds the count of open lanes less one, so a set in storage of zeros has one lane. Arrivals only read
 *       it. A growth and a contraction write it, each with one compare-and-swap. `queue` counts the workers that
 *       found every open lane busy. An entry on a free lane writes no word but the lane word.
 * @note The order of the passes is not the turnstile's. A consumer that needs an order keys it by the address of the
 *       worker and the worker's own sequence.
 * @note A backoff keeps its start on the clock and its exponent in the caller's locals. No shared word holds a time.
 *       Inside the first unit, 2^EMBED_TURNSTILE_UNIT_SHIFT ns, a step is a pause and no call. From one unit on, a
 *       step waits on the word for 2^(EMBED_TURNSTILE_UNIT_SHIFT + k) ns at most, and k goes up to
 *       EMBED_TURNSTILE_K_MOST. A wake or a moved word ends the wait first.
 * @note The host defines embed_word_wait, embed_word_wake and embed_clock_ns. A build that defines none of the three
 *       fails at the link, and the error names them.
 * @note theory/embedded_types/EmbeddedTypes/Manifold proves the lane word, the backoff, the open count, the queue,
 *       the drain and the sleeps.
 * @warning The generation rolls after 2^(EMBED_WORD_BITS - 3) counts. A drain that reads one lane twice, with exactly
 *          that count of passes between the two reads, waits for the next pass to end too.
 */
#ifndef EMBED_TURNSTILE_H
#define EMBED_TURNSTILE_H

#include "embed_atomic.h"

EMBED_BEGIN_DECLS

/** @brief The lanes of a set. A build can set its own count before this header, from 1 to 64. */
#if !defined(EMBED_TURNSTILE_LANES)
#define EMBED_TURNSTILE_LANES 64u
#endif
EMBED_STATIC_ASSERT((EMBED_TURNSTILE_LANES >= 1u) && (EMBED_TURNSTILE_LANES <= 64u),
                    "EMBED_TURNSTILE_LANES must be from 1 to 64");

/**
 * @brief The shift of the backoff's unit, 2^shift ns: 10 for a CPU host, about 1.024 us, and 0 for a device. A build
 *        can set its own before this header.
 */
#if !defined(EMBED_TURNSTILE_UNIT_SHIFT)
#define EMBED_TURNSTILE_UNIT_SHIFT 10u
#endif
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_UNIT_SHIFT <= 10u, "EMBED_TURNSTILE_UNIT_SHIFT must be 10 at most");

/** @brief The most the backoff's exponent reaches: a wait of 2^(EMBED_TURNSTILE_UNIT_SHIFT + 20) ns at most. */
#define EMBED_TURNSTILE_K_MOST 20u

/** @brief The fields of a lane word, and the unit of its generation. */
#define EMBED_TURNSTILE_BUSY ((embed_atomic_word_value)1u)
#define EMBED_TURNSTILE_CLOSED ((embed_atomic_word_value)2u)
#define EMBED_TURNSTILE_SLEEPER ((embed_atomic_word_value)4u)
#define EMBED_TURNSTILE_GENERATION ((embed_atomic_word_value)8u)
#define EMBED_TURNSTILE_GENERATION_SHIFT 3u

/** @brief A sleep with no end, for embed_word_wait. */
#define EMBED_WAIT_FOREVER (~(embed_u64)0u)

/** @brief One lane: its word on a cache line of its own, so that no pass shares the line of another. */
typedef struct
{
    EMBED_ALIGN(64) embed_atomic_word word; /**< The lane word. */
} EmbedTurnstileLane;
EMBED_STATIC_ASSERT(sizeof(EmbedTurnstileLane) == 64u, "a lane takes one cache line");

/**
 * @brief A set of turnstiles, zeros at rest: one to each thing workers read while a writer releases its parts.
 *
 * @note `open` is the count of open lanes less one. `queue` is the count of workers that found every open lane busy.
 */
typedef struct
{
    EmbedTurnstileLane lane[EMBED_TURNSTILE_LANES]; /**< The lanes, those below the open count open. */
    EMBED_ALIGN(64) embed_atomic_word open;         /**< The count of open lanes less one. */
    EMBED_ALIGN(64) embed_atomic_u32 queue;         /**< The workers that found every open lane busy. */
} EmbedTurnstiles;

/** @brief A backoff's state, in the caller's locals: the clock at its start, and its exponent. */
typedef struct
{
    embed_u64 start;  /**< The nanosecond that embed_clock_ns read at the start. */
    unsigned int exponent; /**< The exponent of the next wait, from 0 to EMBED_TURNSTILE_K_MOST. */
} EmbedTurnstileBackoff;

/**
 * @brief A sleep on word while it reads seen, at most nanoseconds, EMBED_WAIT_FOREVER for no end, and none at all
 *        for 0.
 *
 * @param[in] word        The word slept on [BORROWS].
 * @param[in] seen        The value that the caller read.
 * @param[in] nanoseconds The most the sleep lasts. 0 answers at once with no sleep and no system call.
 * @return                1 where the word moved off seen, 0 where it did not. A sleep is free to end early, so a
 *                        sleeper tests what it waits for in a loop.
 * @note The host defines this function on its address wait. That is a futex on Linux, WaitOnAddress on Windows, and
 *       __ulock_wait on macOS. Where the system has none, it is a timed sleep.
 */
int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u64 nanoseconds);

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
 * @return The nanoseconds, from a start that the host selects.
 * @note The host defines this beside embed_word_wait.
 */
embed_u64 embed_clock_ns(void);

/**
 * @brief The processor's pause in a spin, a compiler barrier alone where the processor has none the compiler reaches.
 * @note A build that defines it before this header keeps its own, as a model checker's build does with no processor.
 */
#if !defined(EMBED_TURNSTILE_PAUSE) && defined(__has_builtin)
#if __has_builtin(__builtin_ia32_pause)
#define EMBED_TURNSTILE_PAUSE() __builtin_ia32_pause()
#elif __has_builtin(__builtin_arm_yield)
#define EMBED_TURNSTILE_PAUSE() __builtin_arm_yield()
#endif
#endif
#if !defined(EMBED_TURNSTILE_PAUSE)
#define EMBED_TURNSTILE_PAUSE() EMBED_ATOMIC_ORDERED()
#endif

/**
 * @brief The start of a backoff: the clock read, and the exponent at 0.
 *
 * @param[out] backoff The backoff [BORROWS].
 */
EMBED_INLINE void embed_turnstile_backoff_begin(EmbedTurnstileBackoff *backoff)
{
    backoff->start = embed_clock_ns();
    backoff->exponent = 0u;
}

/**
 * @brief One step of a backoff on word, which the caller last read as read.
 *
 * The step does one pause. Where the time since the start is less than one unit, or where `on` is 0, that is all.
 * Otherwise the step sets the sleeper bit `flag` on the word, and the host waits on the word for the limit of the
 * exponent. Then the exponent goes up by one, to EMBED_TURNSTILE_K_MOST at most.
 *
 * @param[in]     word    The word [BORROWS].
 * @param[in]     read    What it was last read as.
 * @param[in]     flag    The word's sleeper bit.
 * @param[in,out] backoff The backoff [BORROWS].
 * @param[in]     on      1 where the value read lets the caller sleep, 0 for a pause alone.
 * @note The sleep needs a value that a later change ends. A caller sleeps only on a value whose change comes with a
 *       wake, such as a lane word that has the busy bit.
 */
EMBED_INLINE void embed_turnstile_backed_off(embed_atomic_word *word, embed_atomic_word_value read,
                                             embed_atomic_word_value flag, EmbedTurnstileBackoff *backoff,
                                             unsigned int on)
{
    EMBED_TURNSTILE_PAUSE();
    const embed_u64 elapsed = embed_clock_ns() - backoff->start;

    if ((on != 0u) && (elapsed >= ((embed_u64)1u << EMBED_TURNSTILE_UNIT_SHIFT)))
    {
        (void)embed_atomic_word_fetch_or(word, flag);
        (void)embed_word_wait(word, read | flag, (embed_u64)1u << (EMBED_TURNSTILE_UNIT_SHIFT + backoff->exponent));
        backoff->exponent += (unsigned int)(backoff->exponent < EMBED_TURNSTILE_K_MOST);
    }
}

/**
 * @brief The lanes a set has open.
 *
 * @param[in] set The set [BORROWS].
 * @return        The lanes open, from 1 to EMBED_TURNSTILE_LANES.
 */
EMBED_INLINE unsigned int embed_turnstile_lanes(const EmbedTurnstiles *set)
{
    // Explicit cast takes the count less one from the word, which EMBED_TURNSTILE_LANES bounds
    return (unsigned int)embed_atomic_word_load(&set->open) + 1u;
}

/**
 * @brief A fetch-add on a lane word that the caller holds, and the wake of its sleepers where the old value had the
 *        sleeper bit.
 *
 * @param[in,out] lane   The lane [BORROWS].
 * @param[in]     change The constant of the fetch-add.
 */
EMBED_INLINE void embed_turnstile_changed(EmbedTurnstileLane *lane, embed_atomic_word_value change)
{
    const embed_atomic_word_value was = embed_atomic_word_fetch_add(&lane->word, change);

    if ((was & EMBED_TURNSTILE_SLEEPER) != 0u)
    {
        (void)embed_atomic_word_fetch_and(&lane->word, ~EMBED_TURNSTILE_SLEEPER);
        embed_word_wake(&lane->word);
    }
}

/** @brief No lane: a growth that took none. */
#define EMBED_TURNSTILE_NONE 0xffffffffu

/**
 * @brief The growth from count lanes to count + 1, which gives the caller lane count as its pass.
 *
 * The growth takes lane count with one compare-and-swap on the lane word. From a free word it sets busy. From a closed
 * word it clears closed and counts the takeover, and busy stays set. Then one compare-and-swap moves `open` up by one.
 *
 * Where that swap lands, the lane is the caller's pass. Where it fails, the growth gives the lane back as it was. A
 * lane that had the closed bit gets a close, and a lane that was free gets a release.
 *
 * @param[in,out] set   The set [BORROWS].
 * @param[in]     count The lanes the caller read open.
 * @param[out]    held  The value of lane count where another thread holds it, and 0 in each other case [BORROWS].
 * @return              count, the lane the caller now holds, or EMBED_TURNSTILE_NONE where it holds none.
 * @note A lane that a pass, a contraction or another growth holds refuses the first compare-and-swap, and nothing
 *       changes. A contraction holds its lane from its claim to its close, so no growth takes a lane between a
 *       contraction's swap and its close. The caller sleeps on `held`: the end of that hold changes the word and
 *       wakes it.
 */
EMBED_INLINE unsigned int embed_turnstile_grow(EmbedTurnstiles *set, unsigned int count, embed_atomic_word_value *held)
{
    *held = 0u;
    if (count >= EMBED_TURNSTILE_LANES)
    {
        return EMBED_TURNSTILE_NONE;
    }
    EmbedTurnstileLane *const lane = &set->lane[count];
    embed_atomic_word_value seen = embed_atomic_word_load(&lane->word);
    const embed_atomic_word_value state = seen & (EMBED_TURNSTILE_BUSY | EMBED_TURNSTILE_CLOSED);
    const embed_bool was_closed = (embed_bool)(state == (EMBED_TURNSTILE_BUSY | EMBED_TURNSTILE_CLOSED));

    if ((state != 0u) && !was_closed)
    {
        *held = seen;
        return EMBED_TURNSTILE_NONE;
    }
    const embed_atomic_word_value taken =
        was_closed ? (seen + EMBED_TURNSTILE_GENERATION - EMBED_TURNSTILE_CLOSED) : (seen | EMBED_TURNSTILE_BUSY);

    if (!embed_atomic_word_swap_if(&lane->word, &seen, taken))
    {
        return EMBED_TURNSTILE_NONE;
    }
    if ((taken & EMBED_TURNSTILE_SLEEPER) != 0u)
    {
        (void)embed_atomic_word_fetch_and(&lane->word, ~EMBED_TURNSTILE_SLEEPER);
        embed_word_wake(&lane->word);
    }
    embed_atomic_word_value expected = (embed_atomic_word_value)count - 1u;

    if (embed_atomic_word_swap_if(&set->open, &expected, (embed_atomic_word_value)count))
    {
        return count;
    }
    embed_turnstile_changed(lane, was_closed ? (EMBED_TURNSTILE_GENERATION + EMBED_TURNSTILE_CLOSED)
                                             : (EMBED_TURNSTILE_GENERATION - EMBED_TURNSTILE_BUSY));
    return EMBED_TURNSTILE_NONE;
}

/**
 * @brief The contraction from count lanes to count - 1.
 *
 * The contraction claims lane count - 1 as an entry does. Then one compare-and-swap moves `open` down by one. Where
 * the swap lands, the contraction closes the lane. Where the swap fails, the contraction releases the lane.
 *
 * @param[in,out] set   The set [BORROWS].
 * @param[in]     count The lanes the caller read open, above 1.
 * @return              1 where the lane closed, 0 where a pass held it or the swap failed.
 * @note A lane that a pass holds refuses the claim, and nothing changes. So each pass stays below the open count.
 */
EMBED_INLINE embed_bool embed_turnstile_contract(EmbedTurnstiles *set, unsigned int count)
{
    EmbedTurnstileLane *const lane = &set->lane[count - 1u];
    const embed_atomic_word_value was = embed_atomic_word_fetch_or(&lane->word, EMBED_TURNSTILE_BUSY);
    embed_bool shut = 0;

    if ((was & EMBED_TURNSTILE_BUSY) == 0u)
    {
        embed_atomic_word_value expected = (embed_atomic_word_value)count - 1u;

        shut = embed_atomic_word_swap_if(&set->open, &expected, (embed_atomic_word_value)count - 2u);
        embed_turnstile_changed(lane, shut ? (EMBED_TURNSTILE_GENERATION + EMBED_TURNSTILE_CLOSED)
                                           : (EMBED_TURNSTILE_GENERATION - EMBED_TURNSTILE_BUSY));
    }
    return shut;
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
    unsigned int queued = 0u;

    for (;;)
    {
        const unsigned int count = embed_turnstile_lanes(set);
        // Explicit cast takes the address's cache line mod the count, which is below EMBED_TURNSTILE_LANES
        const unsigned int first = (unsigned int)((address >> 6) % (embed_atomic_uptr_value)count);
        unsigned int at = first;

        for (unsigned int tried = 0u; tried < count; tried += 1u)
        {
            const embed_atomic_word_value was = embed_atomic_word_fetch_or(&set->lane[at].word, EMBED_TURNSTILE_BUSY);

            if ((was & EMBED_TURNSTILE_BUSY) == 0u)
            {
                // Only a worker in the queue writes the queue, so the path of an entry on a free lane writes the
                // lane word alone
                if (queued != 0u)
                {
                    (void)embed_atomic_u32_fetch_sub(&set->queue, 1u);
                }
                // The pass starts before it reads anything. The fence of a writer's drain pairs with this fence.
                EMBED_ATOMIC_ORDERED();
                return at;
            }
            at = (at + 1u == count) ? 0u : (at + 1u);
        }
        if (queued == 0u)
        {
            (void)embed_atomic_u32_fetch_add(&set->queue, 1u);
            queued = 1u;
            embed_turnstile_backoff_begin(&backoff);
        }
        embed_atomic_word_value held = 0u;
        const unsigned int grown = embed_turnstile_grow(set, count, &held);

        if (grown != EMBED_TURNSTILE_NONE)
        {
            (void)embed_atomic_u32_fetch_sub(&set->queue, 1u);
            // The pass starts before it reads anything. The fence of a writer's drain pairs with this fence.
            EMBED_ATOMIC_ORDERED();
            return grown;
        }
        // Each sleep needs busy set in the value read: the end of the hold on that lane changes the word and wakes it.
        // A lane above the count that another thread holds blocks the growth, and the worker sleeps on it. A full
        // manifold has no lane to grow to, and the worker sleeps on its start lane. In each other case the growth
        // lost a race to a change, and the worker tries again.
        if (held != 0u)
        {
            embed_turnstile_backed_off(&set->lane[count].word, held, EMBED_TURNSTILE_SLEEPER, &backoff, 1u);
        }
        else if (count == EMBED_TURNSTILE_LANES)
        {
            const embed_atomic_word_value read = embed_atomic_word_load(&set->lane[first].word);

            embed_turnstile_backed_off(&set->lane[first].word, read, EMBED_TURNSTILE_SLEEPER, &backoff,
                                       (unsigned int)((read & EMBED_TURNSTILE_BUSY) != 0u));
        }
    }
}

/**
 * @brief The end of the pass through the lane at `at` of set.
 *
 * One fetch-add clears busy and counts the pass in the generation. Where the old value had the sleeper bit, the end
 * clears it and wakes the word. Where the lane is the top open lane, the end contracts the manifold. Each step closes
 * the top lane while the queue is empty, more than one lane is open, and no pass holds the top lane.
 *
 * @param[in,out] set The set [BORROWS].
 * @param[in]     at  The lane's place, as embed_turnstile_enter answered it.
 * @note Only the end of a pass on the top lane contracts. So the end of a pass on a lower lane reads and writes no
 *       word but its own lane word and `open`.
 */
EMBED_INLINE void embed_turnstile_leave(EmbedTurnstiles *set, unsigned int at)
{
    embed_turnstile_changed(&set->lane[at], EMBED_TURNSTILE_GENERATION - EMBED_TURNSTILE_BUSY);
    unsigned int count = embed_turnstile_lanes(set);

    if (at + 1u == count)
    {
        while ((count > 1u) && (embed_atomic_u32_load(&set->queue) == 0u) && embed_turnstile_contract(set, count))
        {
            count -= 1u;
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
 *       these are each lane that a pass can hold. The drain waits on each lane whose first read has busy, and keeps
 *       the generation of that read. Its wait on a lane ends where busy reads clear or where the generation moved.
 * @note A round that finds the lane still held backs off on that lane word, with the sleeper bit. The end of the pass
 *       changes the word and wakes it.
 */
EMBED_INLINE void embed_turnstiles_drain(EmbedTurnstiles *set)
{
    embed_atomic_word_value snapshot[EMBED_TURNSTILE_LANES];

    // This fence orders the swap of the writer before the snapshot. It pairs with the fence after an entry's claim.
    EMBED_ATOMIC_ORDERED();
    const unsigned int count = embed_turnstile_lanes(set);

    for (unsigned int at = 0u; at < count; at += 1u)
    {
        snapshot[at] = embed_atomic_word_load(&set->lane[at].word);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const embed_atomic_word_value generation = snapshot[at] >> EMBED_TURNSTILE_GENERATION_SHIFT;
        EmbedTurnstileBackoff backoff;
        embed_atomic_word_value now = snapshot[at];

        // A lane whose first read has the closed bit holds no pass, and the drain does not wait on it
        const embed_atomic_word_value closed = snapshot[at] & EMBED_TURNSTILE_CLOSED;

        embed_turnstile_backoff_begin(&backoff);
        while ((closed == 0u) && ((now & EMBED_TURNSTILE_BUSY) != 0u) &&
               ((now >> EMBED_TURNSTILE_GENERATION_SHIFT) == generation))
        {
            embed_turnstile_backed_off(&set->lane[at].word, now, EMBED_TURNSTILE_SLEEPER, &backoff, 1u);
            now = embed_atomic_word_load(&set->lane[at].word);
        }
    }
}

EMBED_END_DECLS

#endif
