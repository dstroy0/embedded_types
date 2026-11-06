/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_turnstile.h
 * @brief Turnstiles: a bank of lanes, one badge word each, a lane mask of the lanes open and one of the
 *        lanes with a pass under way; a reader badges in timestamped, queues, and takes the next free lane
 *        where one is, a writer drains the lanes of a mask snapshot after swapping a published pointer, and
 *        the bank fits itself to the queue's pressure; and the sleep on a word's address the waits rest on.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-05
 *
 * @note Three words carry the set: `open`, one bit a lane open, the low lanes from bit 0; `busy`, one bit a
 *       lane with a pass under way, its top bit the flag that a thread sleeps on the word; and each lane's
 *       badge word, high to low: the id of the reader passing, the completion bit, the sleeper bit, and the
 *       nanosecond of the pass on the host's clock, rolling in the bits left. The id is an immutable place,
 *       MMgr's owner place, which fits the field whole. A badge-in bumps the queue, takes the lowest bit of
 *       `open & ~busy` with one fetch-or on `busy`, spills to the next lane of the bank where none was free,
 *       writes the badge with the clock's reading, and answers the lane; where no lane was free and the bank
 *       is full, it backs off one step on `busy` and answers EMBED_TURNSTILE_NONE, and the reader badges in
 *       again. Passes of one id are admitted one after another, each with a later stamp, so the passes of an
 *       id are serial in time across the lanes they spill to. The pass ends with the fetch-and that clears the
 *       badge's state bits and the one that clears the lane's `busy` bit. A writer's drain snapshots `busy`
 *       and waits on each lane of the snapshot until its completion bit clears or its id or stamp moves: a
 *       pass begun after the drain's fence reads the swapped pointer and is not waited on.
 * @note No branch stands in a badge-in or a pass's end: every choice is a mask, a select, or a row of a
 *       two-row table of entries, the row the flag bit. A set is one lane at rest. A pass ending with the
 *       queue empty trims `open` to the highest busy lane, one lane at the least. A caller only enters and
 *       leaves; the bank sizes itself.
 * @note A backoff step is one pause and the host's wait on the word for the step's milliseconds, 0 below the
 *       sleeps, which the host answers at once with no sleep; the steps double. The waits are embed_word_wait,
 *       which the host defines on its address wait, a futex, WaitOnAddress or __ulock_wait, with a timed
 *       sleep where the system has none. A pass ending wakes the sleepers on its lane and on `busy` through
 *       embed_word_wake. The clock is embed_clock_ns, the host's. A build with none of the three defined
 *       fails at the link, naming them.
 * @warning The stamp rolls over after 2^EMBED_TURNSTILE_TIME_BITS nanoseconds; a writer asleep on a lane
 *          through a pass that ends and another of the same id that begins on the same stamp waits on until
 *          that one ends.
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
/** @brief The bits a pass is told by: its id and its stamp; and the bits the end of a pass clears. */
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
 * @note open is the lanes open, read with bit 0 set, since a set in zeroed storage is one lane. busy is the
 *       lanes with a pass under way and the sleeper flag above them. waiting is the queue's pressure: the
 *       readers between their badge-in and their lane. spare is the word a badge-in that found no lane writes
 *       its badge to, so the write stands on every path.
 */
typedef struct
{
    EmbedTurnstileLane lane[EMBED_TURNSTILE_LANES_MOST]; /**< The lanes, those of `open` open. */
    EmbedTurnstileLane spare;                            /**< The badge of a badge-in with no lane. */
    EMBED_ALIGN(64) embed_atomic_word open;              /**< One bit a lane open, bit 0 read as set. */
    EMBED_ALIGN(64) embed_atomic_word busy;              /**< One bit a lane passing, the sleeper flag above. */
    embed_atomic_u32 waiting;                            /**< Readers queued for a lane. */
} EmbedTurnstiles;

/**
 * @brief A sleep on word while it reads seen, at most milliseconds, EMBED_WAIT_FOREVER for no end, and none at
 *        all for 0.
 *
 * @param[in] word         The word slept on [BORROWS].
 * @param[in] seen         The value it was read at.
 * @param[in] milliseconds The most the sleep lasts; 0 answers at once with no sleep and no system call.
 * @return                 1 where the word moved off seen, 0 where it did not. A sleep is free to end early,
 *                         so a sleeper tests what it waits for in a loop.
 * @note The host defines this on its address wait: a futex on Linux, WaitOnAddress on Windows,
 *       __ulock_wait on macOS, and a timed sleep where the system has none.
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
 * @return The nanoseconds, from a start of the host's choosing.
 * @note The host defines this beside embed_word_wait.
 */
embed_u64 embed_clock_ns(void);

/** @brief The processor's pause in a spin, a compiler barrier alone where the processor has none the compiler reaches. */
#if defined(__has_builtin)
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
    // the mask widened to the builtin's operand, its count below the word's bits
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
 * @return         How many of its bits are set.
 * @note The population count where the compiler has it, else the SWAR count.
 */
EMBED_INLINE unsigned int embed_turnstile_count(embed_atomic_word_value mask)
{
#if EMBED_HAS_BUILTIN(__builtin_popcountll)
    // the mask widened to the builtin's operand
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
 * @brief The badge of id at the clock's reading now: the id in its field, the completion bit, and the stamp.
 *
 * @param[in] id The reader's id, nonzero, its low EMBED_TURNSTILE_ID_BITS bits the badge.
 * @return       The badge word of a pass beginning now.
 */
EMBED_INLINE embed_atomic_word_value embed_turnstile_badge(embed_atomic_word_value id)
{
    return ((id << EMBED_TURNSTILE_ID_SHIFT) & EMBED_TURNSTILE_ID_MASK) | EMBED_TURNSTILE_COMPLETE |
           ((embed_atomic_word_value)embed_clock_ns() & EMBED_TURNSTILE_TIME_MASK);
}

/**
 * @brief One step of a backoff on word, last read as read: one pause; the sleeper bit `flag` set on the word
 *        where the step has reached its sleeps; and the host's wait on the word for the step's milliseconds, 0
 *        below the sleeps and 0 where `on` is 0, which the host answers at once.
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
 * @return               The lane's place, or EMBED_TURNSTILE_NONE where no lane was free, the claim was not
 *                       permitted, or another reader's fetch-or set the same bit first.
 * @note Straight-line: two loads, the free mask, the trailing zero count with the sleeper flag's bit as the
 *       sentinel a full bank lands on, and the fetch-or, whose bit is masked to nothing on the sentinel.
 */
EMBED_INLINE unsigned int embed_turnstile_claim(EmbedTurnstiles *set, embed_atomic_word_value permit)
{
    const embed_atomic_word_value busy = embed_atomic_word_load(&set->busy);
    const embed_atomic_word_value open = embed_atomic_word_load(&set->open) | 1u;
    const unsigned int at = embed_turnstile_lowest((open & ~busy & EMBED_TURNSTILE_LANE_BITS) | EMBED_TURNSTILE_SLEEPERS);
    const embed_atomic_word_value bit = ((embed_atomic_word_value)1u << at) & EMBED_TURNSTILE_LANE_BITS & permit;
    const embed_atomic_word_value before = embed_atomic_word_fetch_or(&set->busy, bit);

    // the lane is this reader's where its bit was clear before the fetch-or; else every bit set, EMBED_TURNSTILE_NONE
    return at | (0u - (unsigned int)((bit & ~before) == 0u));
}

/**
 * @brief A badge-in by the reader id: the queue bumped, the lowest free lane claimed, the bank's next lane opened
 *        and claimed where none was free, the badge written with the clock's reading, and the lane answered; or,
 *        with no lane and the bank full, one backoff step on `busy` and EMBED_TURNSTILE_NONE, for the reader to
 *        badge in again.
 *
 * @param[in,out] set The set [BORROWS].
 * @param[in]     id  The reader's id, nonzero: an immutable place, MMgr's owner place.
 * @return            The lane's place, which the end of the pass takes, or EMBED_TURNSTILE_NONE.
 * @note No branch: the second claim is permitted by the first's miss, the lane opened is the bank's next bit or
 *       nothing where the bank is full, the badge is written to the lane or to the set's spare word, and the
 *       backoff step is on or off by the miss. What the pass reads of a published pointer is read after the pass
 *       begins, and no part of it is read after the pass ends.
 */
EMBED_INLINE unsigned int embed_turnstile_enter(EmbedTurnstiles *set, embed_atomic_word_value id)
{
    (void)embed_atomic_u32_fetch_add(&set->waiting, 1u);
    const unsigned int first = embed_turnstile_claim(set, (embed_atomic_word_value)0u - 1u);
    const embed_atomic_word_value missed = 0u - (embed_atomic_word_value)(first == EMBED_TURNSTILE_NONE);
    // the next lane of the bank: the bit above the highest open, nothing where every lane is open
    const embed_atomic_word_value open = embed_atomic_word_load(&set->open) | 1u;
    const embed_atomic_word_value next = (open + 1u) & EMBED_TURNSTILE_LANE_BITS & missed;

    (void)embed_atomic_word_fetch_or(&set->open, next);
    const unsigned int at = first & embed_turnstile_claim(set, missed);
    const unsigned int none = (unsigned int)(at == EMBED_TURNSTILE_NONE);
    EmbedTurnstileLane *const lane = (none != 0u) ? &set->spare : &set->lane[at & (none - 1u)];

    embed_atomic_word_store(&lane->badge, embed_turnstile_badge(id));
    // the pass begun before anything it reads is read, which the fence of a writer's drain pairs with
    EMBED_ATOMIC_ORDERED();
    (void)embed_turnstile_backed_off(&set->busy, embed_atomic_word_load(&set->busy), EMBED_TURNSTILE_SLEEPERS,
                                     (unsigned int)1u << EMBED_TURNSTILE_SLEEP_SHIFT, none);
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
 * @brief The trim: `open` cut to `trimmed` by one fetch-and, which lands whatever another pass's end does to the
 *        word meanwhile: two ends at once each cut the lanes above their own highest busy lane, and the word is
 *        left at the lower of the two.
 */
EMBED_INLINE void embed_turnstile_trim(EmbedTurnstiles *set, embed_atomic_word_value open,
                                       embed_atomic_word_value trimmed)
{
    (void)open;
    (void)embed_atomic_word_fetch_and(&set->open, trimmed);
}

/**
 * @brief The pass through the lane at `at` of set ended: the badge's state bits and the lane's `busy` bit cleared,
 *        the sleepers on the lane and on `busy` woken, and the free lanes above the highest busy one retired
 *        where the queue is empty.
 *
 * @param[in,out] set The set [BORROWS].
 * @param[in]     at  The lane's place, as embed_turnstile_enter answered it.
 * @note No branch: each wake is a row of a two-row table of entries, the row the flag bit read off the word; the
 *       trim is a row of another, the row whether `open` has bits above the highest busy lane while the queue is
 *       empty. The id and the stamp stay in the badge until the next pass takes the lane, so a drain tells this
 *       pass from the next by them.
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
    // the bits up to the highest busy lane spread down from it, every bit where the queue is not empty
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
 * @note A writer swaps a published pointer, calls this, and then releases what the old pointer led to. A
 *       pass begun after the swap reads the new pointer, and is not waited on. The lanes waited on are the
 *       snapshot of `busy`; each round reads the lowest lane left, takes it off the snapshot where its pass
 *       is over, and backs off one step where it is not. A pass is over where its lane's bit has left `busy`,
 *       or where the lane's badge has moved in id or stamp from the live badge the drain first read on it: the
 *       next pass through the lane. A lane whose badge is not live is a pass between its claim and its badge,
 *       or one ending, and is waited on until its bit leaves `busy`.
 */
EMBED_INLINE void embed_turnstiles_drain(EmbedTurnstiles *set)
{
    // the writer's swap ordered before the snapshot, which the fence of a pass's beginning pairs with: a pass
    // this misses reads the swapped pointer
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
        const embed_atomic_word_value gone =
            0u - (embed_atomic_word_value)((embed_atomic_word_load(&set->busy) & bit) == 0u);
        const embed_atomic_word_value live = 0u - (embed_atomic_word_value)((read & EMBED_TURNSTILE_COMPLETE) != 0u);
        // the live badge first read on this lane: taken from this read where the lane is new or none is held yet
        const embed_atomic_word_value take = (0u - (embed_atomic_word_value)(seen != bit)) | ~held;

        first = (read & take) | (first & ~take);
        held = (live & take) | (held & ~take);
        seen = bit;
        const embed_atomic_word_value moved =
            held & (0u - (embed_atomic_word_value)(((read ^ first) & EMBED_TURNSTILE_PASS_MASK) != 0u));
        const unsigned int over = (unsigned int)((gone | moved) != 0u);

        pending &= ~(bit & (0u - (embed_atomic_word_value)over));
        step = embed_turnstile_backed_off(word, read, EMBED_TURNSTILE_SLEEPER, step, over ^ 1u);
    }
}

EMBED_END_DECLS

#endif
