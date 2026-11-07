/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_turnstile.h
 * @brief Turnstiles, and the sleep on the address of a word that their waits use.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-05
 *
 * A set of turnstiles is a bank of lanes, with one badge word for each lane. One lane mask shows the
 * open lanes, and a second shows the lanes with a pass under way. A reader badges in with a time stamp,
 * waits in the queue, and takes the next free lane. A writer swaps a published pointer, then drains the
 * lanes of a snapshot of the mask. The bank changes its size to match the pressure of the queue.
 *
 * @note Three words hold the state of the set. `open` has one bit for each open lane, with the low lanes
 *       from bit 0. `busy` has one bit for each lane with a pass under way. Its top bit is the flag that
 *       shows a thread asleep on the word.
 * @note The badge word of each lane holds four fields, from high to low. They are the id of the reader in
 *       the lane, the completion bit, the sleeper bit, and the time of the pass. The time is the nanosecond
 *       on the clock of the host, and it rolls over in the bits below the sleeper bit. The id is an
 *       immutable place, the owner place of MMgr, and the field holds it whole.
 * @note A badge-in adds one to the queue. It takes the lowest bit of `open & ~busy` with one fetch-or on
 *       `busy`. Where no lane is free, it opens the next lane of the bank and takes it. Then it writes the
 *       badge with the time from the clock, and it gives the lane. Where no lane is free and the bank is
 *       full, it does one backoff step on `busy` and gives EMBED_TURNSTILE_NONE. The reader then badges in
 *       again.
 * @note The passes of one id start one after the other, each with a later stamp. So the passes of an id
 *       come one after the other in time, across the lanes that they spill to. A pass ends with two
 *       fetch-and operations. One clears the state bits of the badge, and one clears the bit of the lane in
 *       `busy`.
 * @note The drain of a writer takes a snapshot of `busy`. It waits on each lane of the snapshot until the
 *       completion bit of the lane clears, or until its id or stamp moves. A pass that starts after the
 *       fence of the drain reads the swapped pointer, and the drain does not wait for it.
 * @note No branch occurs in a badge-in or at the end of a pass. Each choice is a mask, a select, or a row
 *       of a table of two entries, and the flag bit picks the row. A set at rest has one lane. When a pass
 *       ends with the queue empty, it trims `open` to the highest busy lane, and to one lane at least. A
 *       caller only enters and leaves, and the bank sets its own size.
 * @note A backoff step is one pause, then the wait of the host on the word for the milliseconds of the
 *       step. The first steps wait 0 milliseconds, and the host answers at once with no sleep. Each step
 *       doubles. The waits use embed_word_wait, which the host defines on its address wait: a futex,
 *       WaitOnAddress or __ulock_wait. Where the system has none of these, a timed sleep takes their place.
 * @note At its end, a pass wakes the sleepers on its lane and on `busy` through embed_word_wake. The clock
 *       is embed_clock_ns, which the host supplies. A build that defines none of the three fails at the
 *       link, and the error names them.
 * @warning The stamp rolls over after 2^EMBED_TURNSTILE_TIME_BITS nanoseconds. A drain that waits on a lane
 *          can see a pass end and a new pass of the same id start with the same stamp. Then the drain waits
 *          until the new pass ends too.
 */
#ifndef EMBED_TURNSTILE_H
#define EMBED_TURNSTILE_H

#include "embed_atomic.h"

EMBED_BEGIN_DECLS

/** @brief The lanes a set can open: every bit of the word but the sleeper flag's. */
#define EMBED_TURNSTILE_LANES_MOST (EMBED_WORD_BITS - 1u)
/** @brief The bits of `busy` that are lanes, and the flag above them that a thread sleeps on the word. */
#define EMBED_TURNSTILE_LANE_BITS ((((embed_atomic_word_value)1u << EMBED_TURNSTILE_LANES_MOST)) - 1u)
#define EMBED_TURNSTILE_SLEEPERS ((embed_atomic_word_value)1u << EMBED_TURNSTILE_LANES_MOST)
/** @brief No lane: a badge-in that found none free with the bank full, after its backoff step. */
#define EMBED_TURNSTILE_NONE 0xffffffffu

/** @brief A sleep with no end, for embed_word_wait. */
#define EMBED_WAIT_FOREVER 0xffffffffu

/**
 * @brief The badge word's fields, high to low: the id in EMBED_TURNSTILE_ID_BITS bits, the completion bit, the
 *        sleeper bit, and the stamp in the bits left.
 */
#if EMBED_WORD_BITS >= 64u
#define EMBED_TURNSTILE_ID_BITS 16u
#else
#define EMBED_TURNSTILE_ID_BITS 10u
#endif
#define EMBED_TURNSTILE_ID_SHIFT (EMBED_WORD_BITS - EMBED_TURNSTILE_ID_BITS)
#define EMBED_TURNSTILE_ID_MASK ((((embed_atomic_word_value)1u << EMBED_TURNSTILE_ID_BITS) - 1u) << EMBED_TURNSTILE_ID_SHIFT)
#define EMBED_TURNSTILE_COMPLETE_SHIFT (EMBED_TURNSTILE_ID_SHIFT - 1u)
#define EMBED_TURNSTILE_SLEEPER_SHIFT (EMBED_TURNSTILE_ID_SHIFT - 2u)
#define EMBED_TURNSTILE_COMPLETE ((embed_atomic_word_value)1u << EMBED_TURNSTILE_COMPLETE_SHIFT)
#define EMBED_TURNSTILE_SLEEPER ((embed_atomic_word_value)1u << EMBED_TURNSTILE_SLEEPER_SHIFT)
#define EMBED_TURNSTILE_TIME_BITS EMBED_TURNSTILE_SLEEPER_SHIFT
#define EMBED_TURNSTILE_TIME_MASK (((embed_atomic_word_value)1u << EMBED_TURNSTILE_TIME_BITS) - 1u)
/** @brief The bits that tell one pass from another, its id and stamp. Then the bits that a pass clears at its end. */
#define EMBED_TURNSTILE_PASS_MASK (EMBED_TURNSTILE_ID_MASK | EMBED_TURNSTILE_TIME_MASK)
#define EMBED_TURNSTILE_STATE_MASK (EMBED_TURNSTILE_COMPLETE | EMBED_TURNSTILE_SLEEPER)

/** @brief The shift that turns a backoff's step into its sleep in milliseconds, 0 for the first steps; and the
 *         longest step, whose sleep is EMBED_TURNSTILE_BACKOFF_MOST_MS. */
#define EMBED_TURNSTILE_SLEEP_SHIFT 7u
#define EMBED_TURNSTILE_BACKOFF_MOST_MS 64u
#define EMBED_TURNSTILE_STEP_MOST (EMBED_TURNSTILE_BACKOFF_MOST_MS << EMBED_TURNSTILE_SLEEP_SHIFT)

/**
 * @brief One lane: its badge word on a cache line of its own, so that no reader's pass shares another's line.
 */
typedef struct
{
    EMBED_ALIGN(64) embed_atomic_word badge; /**< The lane's badge word. */
} EmbedTurnstileLane;
EMBED_STATIC_ASSERT(sizeof(EmbedTurnstileLane) == 64u, "a lane takes one cache line");

/**
 * @brief A set of turnstiles, zeros at rest: one to each thing readers read while a writer releases its
 *        parts.
 *
 * @note `open` holds the open lanes. Each read of `open` takes bit 0 as set, because a set in storage of
 *       zeros has one lane. `busy` holds the lanes with a pass under way, and the sleeper flag above them.
 *       `waiting` is the pressure of the queue: the readers between their badge-in and their lane. `spare`
 *       is the word that gets the badge of a badge-in with no lane. So the write occurs on each path.
 */
typedef struct
{
    EmbedTurnstileLane lane[EMBED_TURNSTILE_LANES_MOST]; /**< The lanes, those of `open` open. */
    EmbedTurnstileLane spare;                            /**< The badge of a badge-in with no lane. */
    EMBED_ALIGN(64) embed_atomic_word open;              /**< One bit for each open lane, bit 0 read as set. */
    EMBED_ALIGN(64) embed_atomic_word busy;              /**< One bit for each busy lane, the sleeper flag above. */
    embed_atomic_u32 waiting;                            /**< Readers queued for a lane. */
} EmbedTurnstiles;

/**
 * @brief A sleep on word while it reads seen, at most milliseconds, EMBED_WAIT_FOREVER for no end, and none at
 *        all for 0.
 *
 * @param[in] word         The word slept on [BORROWS].
 * @param[in] seen         The value that the caller read.
 * @param[in] milliseconds The most the sleep lasts; 0 answers at once with no sleep and no system call.
 * @return                 1 where the word moved off seen, 0 where it did not. A sleep is free to end early,
 *                         so a sleeper tests what it waits for in a loop.
 * @note The host defines this function on its address wait. That is a futex on Linux, WaitOnAddress on
 *       Windows, and __ulock_wait on macOS. Where the system has none, it is a timed sleep.
 */
int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u32 milliseconds);

/**
 * @brief Every thread asleep on word woken.
 *
 * @param[in] word The word [BORROWS].
 * @note The host defines this beside embed_word_wait.
 */
void embed_word_wake(embed_atomic_word *word);

/**
 * @brief The host's clock in nanoseconds, which a badge carries the low EMBED_TURNSTILE_TIME_BITS bits of.
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
 * @brief The place of the lowest bit set in mask.
 *
 * @param[in] mask A word with a bit set.
 * @return         The bit's place from 0.
 * @note The trailing zero count where the compiler has it, else the lowest bit isolated and placed by six
 *       masked steps.
 */
EMBED_INLINE unsigned int embed_turnstile_lowest(embed_atomic_word_value mask)
{
#if EMBED_HAS_BUILTIN(__builtin_ctzll)
    // The cast widens the mask to the operand of the builtin. The count stays below the bits of the word.
    return (unsigned int)__builtin_ctzll((unsigned long long)mask);
#else
    const unsigned long long low = (unsigned long long)mask & (0ull - (unsigned long long)mask);
    unsigned int place = 0u;

    place += ((low & 0xFFFFFFFF00000000ull) != 0u) ? 32u : 0u;
    place += ((low & 0xFFFF0000FFFF0000ull) != 0u) ? 16u : 0u;
    place += ((low & 0xFF00FF00FF00FF00ull) != 0u) ? 8u : 0u;
    place += ((low & 0xF0F0F0F0F0F0F0F0ull) != 0u) ? 4u : 0u;
    place += ((low & 0xCCCCCCCCCCCCCCCCull) != 0u) ? 2u : 0u;
    place += ((low & 0xAAAAAAAAAAAAAAAAull) != 0u) ? 1u : 0u;
    return place;
#endif
}

/**
 * @brief The bits of mask set, counted.
 *
 * @param[in] mask A word.
 * @return         The count of its set bits.
 * @note The population count where the compiler has it, else the SWAR count.
 */
EMBED_INLINE unsigned int embed_turnstile_count(embed_atomic_word_value mask)
{
#if EMBED_HAS_BUILTIN(__builtin_popcountll)
    // The cast widens the mask to the operand of the builtin.
    return (unsigned int)__builtin_popcountll((unsigned long long)mask);
#else
    unsigned long long bits = (unsigned long long)mask;

    bits -= (bits >> 1) & 0x5555555555555555ull;
    bits = (bits & 0x3333333333333333ull) + ((bits >> 2) & 0x3333333333333333ull);
    bits = (bits + (bits >> 4)) & 0x0F0F0F0F0F0F0F0Full;
    return (unsigned int)((bits * 0x0101010101010101ull) >> 56);
#endif
}

/**
 * @brief The lanes a set has open: the bits of `open`, bit 0 read as set.
 *
 * @param[in] set The set [BORROWS].
 * @return        The lanes open, from 1 to EMBED_TURNSTILE_LANES_MOST.
 */
EMBED_INLINE unsigned int embed_turnstile_lanes(const EmbedTurnstiles *set)
{
    return embed_turnstile_count(embed_atomic_word_load(&set->open) | 1u);
}

/**
 * @brief The badge of id at the current time of the clock: the id in its field, the completion bit, and the stamp.
 *
 * @param[in] id The reader's id, nonzero, its low EMBED_TURNSTILE_ID_BITS bits the badge.
 * @return       The badge word of a pass that starts now.
 */
EMBED_INLINE embed_atomic_word_value embed_turnstile_badge(embed_atomic_word_value id)
{
    return ((id << EMBED_TURNSTILE_ID_SHIFT) & EMBED_TURNSTILE_ID_MASK) | EMBED_TURNSTILE_COMPLETE |
           ((embed_atomic_word_value)embed_clock_ns() & EMBED_TURNSTILE_TIME_MASK);
}

/**
 * @brief One step of a backoff on word, which the caller last read as read.
 *
 * The step does one pause. Where the step is at its sleeps, it sets the sleeper bit `flag` on the word. Then
 * the host waits on the word for the milliseconds of the step. That time is 0 below the sleeps, and 0 where
 * `on` is 0, and the host then answers at once.
 *
 * @param[in] word The word [BORROWS].
 * @param[in] read What it was last read as.
 * @param[in] flag The word's sleeper bit.
 * @param[in] step The step.
 * @param[in] on   1 for a step, 0 for none: the pause alone, the flag and the sleep masked to nothing.
 * @return         The step after it, doubled up to EMBED_TURNSTILE_STEP_MOST, or the step itself where `on` is 0.
 */
EMBED_INLINE unsigned int embed_turnstile_backed_off(embed_atomic_word *word, embed_atomic_word_value read,
                                                     embed_atomic_word_value flag, unsigned int step, unsigned int on)
{
    const unsigned int milliseconds = (step >> EMBED_TURNSTILE_SLEEP_SHIFT) & (0u - on);
    const embed_atomic_word_value set = flag & (0u - (embed_atomic_word_value)(milliseconds != 0u));
    const unsigned int doubled = step << on;

    EMBED_TURNSTILE_PAUSE();
    (void)embed_atomic_word_fetch_or(word, set);
    (void)embed_word_wait(word, read | set, milliseconds);
    return doubled ^ ((doubled ^ EMBED_TURNSTILE_STEP_MOST) & (0u - (unsigned int)(doubled > EMBED_TURNSTILE_STEP_MOST)));
}

/**
 * @brief One claim on set: the lowest free lane's bit set in `busy` with one fetch-or, where `permit` allows it.
 *
 * @param[in,out] set    The set [BORROWS].
 * @param[in]     permit Every bit set for a claim, none for a claim that takes nothing.
 * @return               The lane's place, or EMBED_TURNSTILE_NONE. That is where no lane was free, where
 *                       `permit` stops the claim, or where the fetch-or of another reader set the same bit first.
 * @note The code has no branch. It does two loads, makes the free mask, and takes the trailing zero count.
 *       The bit of the sleeper flag is the sentinel, where the count of a full bank lands. Then it does the
 *       fetch-or, and on the sentinel a mask takes its bit to nothing.
 */
EMBED_INLINE unsigned int embed_turnstile_claim(EmbedTurnstiles *set, embed_atomic_word_value permit)
{
    const embed_atomic_word_value busy = embed_atomic_word_load(&set->busy);
    const embed_atomic_word_value open = embed_atomic_word_load(&set->open) | 1u;
    const unsigned int at = embed_turnstile_lowest((open & ~busy & EMBED_TURNSTILE_LANE_BITS) | EMBED_TURNSTILE_SLEEPERS);
    const embed_atomic_word_value bit = ((embed_atomic_word_value)1u << at) & EMBED_TURNSTILE_LANE_BITS & permit;
    const embed_atomic_word_value before = embed_atomic_word_fetch_or(&set->busy, bit);

    // The lane belongs to this reader where its bit was clear before the fetch-or. Otherwise each bit is on: NONE.
    return at | (0u - (unsigned int)((bit & ~before) == 0u));
}

/**
 * @brief A badge-in by the reader id, which gives a lane or EMBED_TURNSTILE_NONE.
 *
 * The badge-in adds one to the queue and claims the lowest free lane. Where no lane is free, it opens the next
 * lane of the bank and claims it. It writes the badge with the time from the clock, and gives the lane. Where it
 * gets no lane and the bank is full, it does one backoff step on `busy` and gives EMBED_TURNSTILE_NONE. The
 * reader then badges in again.
 *
 * @param[in,out] set The set [BORROWS].
 * @param[in]     id  The reader's id, nonzero: an immutable place, MMgr's owner place.
 * @return            The lane's place, which the end of the pass takes, or EMBED_TURNSTILE_NONE.
 * @note The code has no branch. The miss of the first claim permits the second claim. The lane that it opens is
 *       the next bit of the bank, or nothing where the bank is full. The badge goes to the lane, or to the spare
 *       word of the set. The miss also turns the backoff step on or off.
 * @note The pass reads the data of a published pointer only after the pass starts, and none of it after the
 *       pass ends.
 */
EMBED_INLINE unsigned int embed_turnstile_enter(EmbedTurnstiles *set, embed_atomic_word_value id)
{
    (void)embed_atomic_u32_fetch_add(&set->waiting, 1u);
    const unsigned int first = embed_turnstile_claim(set, (embed_atomic_word_value)0u - 1u);
    const embed_atomic_word_value missed = 0u - (embed_atomic_word_value)(first == EMBED_TURNSTILE_NONE);
    // The next lane of the bank is the bit above the highest open lane. Where each lane is open, it is nothing.
    const embed_atomic_word_value open = embed_atomic_word_load(&set->open) | 1u;
    const embed_atomic_word_value next = (open + 1u) & EMBED_TURNSTILE_LANE_BITS & missed;

    (void)embed_atomic_word_fetch_or(&set->open, next);
    const unsigned int at = first & embed_turnstile_claim(set, missed);
    const unsigned int none = (unsigned int)(at == EMBED_TURNSTILE_NONE);
    EmbedTurnstileLane *const lane = (none != 0u) ? &set->spare : &set->lane[at & (none - 1u)];

    embed_atomic_word_store(&lane->badge, embed_turnstile_badge(id));
    // The pass starts before it reads anything. The fence of the drain of a writer pairs with this fence.
    EMBED_ATOMIC_ORDERED();
    // The sleep needs each open lane busy in the value that this code reads. The end of a pass wakes only a bank
    // that it frees. A value with a free lane has no end of a pass to wake it.
    const embed_atomic_word_value seen = embed_atomic_word_load(&set->busy);
    const unsigned int full =
        (unsigned int)(((embed_atomic_word_load(&set->open) | 1u) & ~seen & EMBED_TURNSTILE_LANE_BITS) == 0u);
    (void)embed_turnstile_backed_off(&set->busy, seen, EMBED_TURNSTILE_SLEEPERS,
                                     (unsigned int)1u << EMBED_TURNSTILE_SLEEP_SHIFT, none & full);
    (void)embed_atomic_u32_fetch_sub(&set->waiting, 1u);
    return at;
}

/** @brief A wake of nothing, the row of the wake table a clear flag takes. */
EMBED_INLINE void embed_turnstile_wake_none(embed_atomic_word *word)
{
    (void)word;
}

/** @brief A trim of nothing, the row of the trim table a trim with nothing to take takes. */
EMBED_INLINE void embed_turnstile_trim_none(EmbedTurnstiles *set, embed_atomic_word_value open,
                                            embed_atomic_word_value trimmed)
{
    (void)set;
    (void)open;
    (void)trimmed;
}

/**
 * @brief The trim: one fetch-and cuts `open` to `trimmed`.
 *
 * The fetch-and keeps what the end of another pass does to the word at the same time. Two ends at the same time
 * each cut the lanes above their own highest busy lane. The word then stays at the lower of the two.
 */
EMBED_INLINE void embed_turnstile_trim(EmbedTurnstiles *set, embed_atomic_word_value open,
                                       embed_atomic_word_value trimmed)
{
    (void)open;
    (void)embed_atomic_word_fetch_and(&set->open, trimmed);
}

/**
 * @brief The end of the pass through the lane at `at` of set.
 *
 * The end clears the state bits of the badge and the bit of the lane in `busy`. It wakes the sleepers on the lane
 * and on `busy`. Where the queue is empty, it retires the free lanes above the highest busy lane.
 *
 * @param[in,out] set The set [BORROWS].
 * @param[in]     at  The lane's place, as embed_turnstile_enter answered it.
 * @note The code has no branch. Each wake is a row of a table of two entries, and the flag bit of the word picks
 *       the row. The trim is a row of another table. Its row shows whether `open` has bits above the highest busy
 *       lane while the queue is empty.
 * @note The id and the stamp stay in the badge until the next pass takes the lane. So a drain uses them to tell
 *       this pass from the next.
 */
EMBED_INLINE void embed_turnstile_leave(EmbedTurnstiles *set, unsigned int at)
{
    static void (*const wakes[2])(embed_atomic_word *) = {embed_turnstile_wake_none, embed_word_wake};
    static void (*const trims[2])(EmbedTurnstiles *, embed_atomic_word_value, embed_atomic_word_value) = {
        embed_turnstile_trim_none, embed_turnstile_trim};
    const embed_atomic_word_value bit = (embed_atomic_word_value)1u << at;
    const embed_atomic_word_value read =
        embed_atomic_word_fetch_and(&set->lane[at].badge, (embed_atomic_word_value) ~EMBED_TURNSTILE_STATE_MASK);
    const embed_atomic_word_value busy = embed_atomic_word_fetch_and(&set->busy, ~(bit | EMBED_TURNSTILE_SLEEPERS));

    wakes[(read >> EMBED_TURNSTILE_SLEEPER_SHIFT) & 1u](&set->lane[at].badge);
    wakes[(busy >> EMBED_TURNSTILE_LANES_MOST) & 1u](&set->busy);
    // Spread the bits down from the highest busy lane. Where the queue is not empty, set each bit.
    embed_atomic_word_value spread = ((busy & ~bit) & EMBED_TURNSTILE_LANE_BITS) | 1u;

    spread |= spread >> 1;
    spread |= spread >> 2;
    spread |= spread >> 4;
    spread |= spread >> 8;
    spread |= spread >> 16;
#if EMBED_WORD_BITS > 32u
    spread |= spread >> 32;
#endif
    spread |= 0u - (embed_atomic_word_value)(embed_atomic_u32_load(&set->waiting) != 0u);
    const embed_atomic_word_value open = embed_atomic_word_load(&set->open);

    trims[(unsigned int)((open & ~spread) != 0u)](set, open, open & spread);
}

/**
 * @brief Every pass through set begun before this call waited out.
 *
 * @param[in,out] set The set [BORROWS].
 * @note A writer swaps a published pointer and calls this function. Then it releases the data that the old
 *       pointer led to. A pass that starts after the swap reads the new pointer, and the drain does not wait
 *       for it.
 * @note The drain waits on the lanes in the snapshot of `busy`. Each round reads the lowest lane in the
 *       snapshot. Where the pass of that lane is over, the round takes the lane off the snapshot. Where it is
 *       not over, the round does one backoff step.
 * @note A pass is over where the bit of its lane leaves `busy`. It is also over where the id or stamp of the
 *       badge moves from the first live badge that the drain read on the lane. That move is the next pass
 *       through the lane.
 * @note A lane whose badge is not live holds a pass between its claim and its badge, or a pass at its end. The
 *       drain waits on that lane until its bit leaves `busy`.
 * @note The backoff sleeps on `busy` with the flag EMBED_TURNSTILE_SLEEPERS, at the value of `busy` that the
 *       round read. A pass is over only after the fetch-and of its end on `busy`. That fetch-and changes the
 *       word, clears the flag, and wakes each sleeper where the flag was on.
 */
EMBED_INLINE void embed_turnstiles_drain(EmbedTurnstiles *set)
{
    // This fence orders the swap of the writer before the snapshot. It pairs with the fence at the start of a
    // pass. A pass that the snapshot misses reads the swapped pointer.
    EMBED_ATOMIC_ORDERED();
    embed_atomic_word_value pending = embed_atomic_word_load(&set->busy) & EMBED_TURNSTILE_LANE_BITS;
    embed_atomic_word_value first = 0u;
    embed_atomic_word_value held = 0u;
    embed_atomic_word_value seen = 0u;
    unsigned int step = 1u;

    while (pending != 0u)
    {
        const unsigned int at = embed_turnstile_lowest(pending);
        const embed_atomic_word_value bit = (embed_atomic_word_value)1u << at;
        embed_atomic_word *const word = &set->lane[at].badge;
        const embed_atomic_word_value read = embed_atomic_word_load(word);
        const embed_atomic_word_value busy = embed_atomic_word_load(&set->busy);
        const embed_atomic_word_value gone = 0u - (embed_atomic_word_value)((busy & bit) == 0u);
        const embed_atomic_word_value live = 0u - (embed_atomic_word_value)((read & EMBED_TURNSTILE_COMPLETE) != 0u);
        // The first live badge on this lane. This read gives it where the lane is new, or where the drain holds none.
        const embed_atomic_word_value take = (0u - (embed_atomic_word_value)(seen != bit)) | ~held;

        first = (read & take) | (first & ~take);
        held = (live & take) | (held & ~take);
        seen = bit;
        const embed_atomic_word_value moved =
            held & (0u - (embed_atomic_word_value)(((read ^ first) & EMBED_TURNSTILE_PASS_MASK) != 0u));
        const unsigned int over = (unsigned int)((gone | moved) != 0u);

        pending &= ~(bit & (0u - (embed_atomic_word_value)over));
        // The sleep is on `busy` at the value this round read. Each event that ends the wait starts with the
        // fetch-and of a pass's end on `busy`, which clears the flag and wakes the word.
        step = embed_turnstile_backed_off(&set->busy, busy, EMBED_TURNSTILE_SLEEPERS, step, over ^ 1u);
    }
}

EMBED_END_DECLS

#endif
