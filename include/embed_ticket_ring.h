/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_ticket_ring.h
 * @brief A ring of job slots that a ticket indexes, with a state machine per slot in the flag bits of its stamp.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-11
 *
 * The ring has 2^shift slots. A slot holds a stamp and a job pointer. The stamp is one atomic machine word. The
 * ticket is in its low EMBED_TICKET_BITS bits, the 16 flag bits are above it, and the epoch is in the high bits. A word
 * of 64 bits has a 32-bit ticket and a 16-bit epoch, and a word of 32 bits has a 12-bit ticket and a 4-bit epoch.
 *
 * The ticket indexes the slot: the low shift bits of the ticket are the slot index. The ticket is a counter with
 * wraparound, and the slot index wraps with it, because 2^shift divides 2^EMBED_TICKET_BITS.
 *
 * @note Each transition of a slot is one compare-and-swap on the stamp word. The expected value is the stamp with
 *       the flag bits of the transitions before it. The new value adds the flag bit of the transition. So exactly
 *       one caller completes each transition, and a stamp that differs in any bit fails the compare.
 * @note The state sequence of a slot is grant, push, pop, replay, release. A replay is the read of a slot after its
 *       pop, which a rewind performs. A grant succeeds on a zero word or on a released word. A release succeeds
 *       after a pop. The next grant at the same index reuses the released word.
 * @note The pop writes the lane index of the pass that performed it into seven flag bits. The release retains them.
 * @note The push stores a three-bit cyclic redundancy check of the job pointer in three flag bits. The pop and the
 *       replay check the pointer they read against it. Every single-bit error in the job pointer fails the check.
 *       Every double-bit error fails it too, except one whose two bits are a multiple of seven places apart. The
 *       pop then returns no job. EMBED_TICKET_JOB_CHECK turns the check off.
 * @note The epoch is EMBED_TICKET_EPOCH_BITS wide: 16 bits on a word of 64 bits, and 4 bits on a word of 32 bits.
 *       The grantor derives it from the host clock. The tick period covers the lifetime of a stamp, so the epoch does
 *       not wrap while the stamp is live. A compare succeeds only where the ticket and the epoch both match, and the
 *       ticket alone distinguishes any two live stamps.
 * @note The turnstile serializes the transitions with respect to the writer. A transition executes inside a pass,
 *       and the drain of a writer waits for every pass that started before it. The functions at the end of this
 *       header wrap each transition in a pass.
 * @note theory/embedded_types/EmbeddedTypes/TicketRing.lean proves the stamp fields and the slot index. Flags.lean
 *       proves the flag matrix: each transition preserves every other field, succeeds for exactly one caller, and
 *       fails on a corrupted word. Claim.lean proves the collision of two transitions and the drain after one.
 * @warning A caller that accesses a slot without a successful compare-and-swap is outside every proof here.
 */
#ifndef EMBED_TICKET_RING_H
#define EMBED_TICKET_RING_H

#include "embed_turnstile.h"

EMBED_BEGIN_DECLS

/**
 * @brief The stamp layout of each word width that the ring builds on: the bits of the ticket and of the epoch. The
 *        flag field takes the 16 bits between them on each width.
 *
 * @note The epoch outlasts the longest hold of a stamp, and that time does not grow with the word. So the epoch
 *       needs no more than 16 bits, and a wider word gives its bits to the ticket, which bounds the slot count.
 * @warning At 32 bits the epoch covers 16 ticks. The tick of the grantor is then 1/16 of the longest hold at least.
 */
#define EMBED_TICKET_BITS_32 12u
#define EMBED_TICKET_EPOCH_BITS_32 4u
#define EMBED_TICKET_BITS_64 32u
#define EMBED_TICKET_EPOCH_BITS_64 16u

EMBED_STATIC_ASSERT(EMBED_TICKET_BITS_32 + 16u + EMBED_TICKET_EPOCH_BITS_32 == 32u,
                    "the stamp layout of a 32-bit word fills the word");
EMBED_STATIC_ASSERT(EMBED_TICKET_BITS_64 + 16u + EMBED_TICKET_EPOCH_BITS_64 == 64u,
                    "the stamp layout of a 64-bit word fills the word");
EMBED_STATIC_ASSERT(EMBED_WORD_BITS >= 32u,
                    "the ticket ring needs a word of 32 bits at least: a 16-bit word has no room for three fields");

/** @brief The width of the ticket field, and the positions of the flag field and the epoch field, of this word. */
#define EMBED_TICKET_BITS EMBED_CAT(EMBED_TICKET_BITS_, EMBED_WORD_BITS)
#define EMBED_TICKET_MASK ((((embed_word)1u) << EMBED_TICKET_BITS) - 1u)
#define EMBED_TICKET_FLAG_SHIFT EMBED_TICKET_BITS
#define EMBED_TICKET_FLAG_BITS 16u
#define EMBED_TICKET_FLAG_MASK ((embed_word)0xffffu << EMBED_TICKET_FLAG_SHIFT)
#define EMBED_TICKET_EPOCH_SHIFT (EMBED_TICKET_FLAG_SHIFT + EMBED_TICKET_FLAG_BITS)
#define EMBED_TICKET_EPOCH_BITS EMBED_CAT(EMBED_TICKET_EPOCH_BITS_, EMBED_WORD_BITS)
#define EMBED_TICKET_EPOCH_MASK ((((embed_word)1u) << EMBED_TICKET_EPOCH_BITS) - 1u)

/** @brief The flag bits: one per transition of the slot state machine, and the lane index of the pop. */
#define EMBED_TICKET_GRANTED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 0u))
#define EMBED_TICKET_PUSHED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 1u))
#define EMBED_TICKET_POPPED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 2u))
#define EMBED_TICKET_REPLAYED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 3u))
#define EMBED_TICKET_RELEASED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 4u))
#define EMBED_TICKET_LANE_SHIFT (EMBED_TICKET_FLAG_SHIFT + 8u)
#define EMBED_TICKET_LANE_MASK ((embed_word)0x7fu << EMBED_TICKET_LANE_SHIFT)
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_LANES <= 128u, "the lane index of a pop fits seven flag bits");

/** @brief The three check bits of the job pointer, which the push sets and the pop and the replay check. */
#define EMBED_TICKET_CHECK_SHIFT (EMBED_TICKET_FLAG_SHIFT + 5u)
#define EMBED_TICKET_CHECK_MASK ((embed_word)0x7u << EMBED_TICKET_CHECK_SHIFT)

/** @brief The reserved bit. No transition sets it, so an expected value with it set always fails the compare. */
#define EMBED_TICKET_RESERVED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 15u))

/**
 * @brief 1 where the push stores the check of the job pointer in the stamp and the pop and the replay verify it,
 *        0 where they do not. A build sets it before this header. This header defines both values. Test it with #if.
 */
#if !defined(EMBED_TICKET_JOB_CHECK)
#define EMBED_TICKET_JOB_CHECK 1
#endif
#if (EMBED_TICKET_JOB_CHECK != 0) && (EMBED_TICKET_JOB_CHECK != 1)
#error "EMBED_TICKET_JOB_CHECK must be 0 or 1"
#endif

/** @brief The max slot count as a shift: 2^EMBED_TICKET_RING_SHIFT_MAX slots, below the ticket width. */
#define EMBED_TICKET_RING_SHIFT_MAX (EMBED_TICKET_BITS - 1u)

/** @brief One slot: the stamp word, the state of the slot, and the job pointer. */
typedef struct
{
    embed_atomic_word stamp; /**< The stamp with the flag bits of the transitions that completed. */
    void *job;               /**< The job [BORROWS]. */
} EmbedTicketRingSlot;
EMBED_STATIC_ASSERT(sizeof(EmbedTicketRingSlot) ==
                        2u *
                            ((sizeof(embed_atomic_word) > sizeof(void *)) ? sizeof(embed_atomic_word) : sizeof(void *)),
                    "a slot is two of its wider member: the stamp word and the job pointer");

/** @brief A ring: its index mask, 2^shift - 1, on a cache line of its own, and its slot array. */
typedef struct
{
    EMBED_ALIGN(64u) embed_word mask; /**< 2^shift - 1. */
    EmbedTicketRingSlot *slots;       /**< 2^shift slots. */
} EmbedTicketRing;

/** @brief The parameters of embed_ticket_stamp. */
typedef struct
{
    const embed_word epoch;  /**< The epoch, of which the stamp keeps the low EMBED_TICKET_EPOCH_BITS bits. */
    const embed_word ticket; /**< The ticket, of which the stamp keeps the low EMBED_TICKET_BITS bits. */
} EmbedTicketStampCfg;

/**
 * @brief The stamp of an epoch and a ticket, with the flag bits clear.
 *
 * @param[in] args The parameters, as EmbedTicketStampCfg holds them [BORROWS].
 * @return         The stamp.
 * @note Each field keeps only its own bits, so a wide epoch or ticket sets no flag bit.
 */
EMBED_INLINE embed_word embed_ticket_stamp(const EmbedTicketStampCfg *args)
{
    return ((args->epoch & EMBED_TICKET_EPOCH_MASK) << EMBED_TICKET_EPOCH_SHIFT) | (args->ticket & EMBED_TICKET_MASK);
}

/** @brief The parameters of embed_ticket_of_stamp. */
typedef struct
{
    const embed_word stamp; /**< The stamp. */
} EmbedTicketOfStampCfg;

/**
 * @brief The ticket field of a stamp.
 *
 * @param[in] args The parameters, as EmbedTicketOfStampCfg holds them [BORROWS].
 * @return         The low EMBED_TICKET_BITS bits.
 */
EMBED_INLINE embed_word embed_ticket_of_stamp(const EmbedTicketOfStampCfg *args)
{
    return args->stamp & EMBED_TICKET_MASK;
}

/** @brief The parameters of embed_epoch_of_stamp. */
typedef struct
{
    const embed_word stamp; /**< The stamp. */
} EmbedEpochOfStampCfg;

/**
 * @brief The epoch field of a stamp.
 *
 * @param[in] args The parameters, as EmbedEpochOfStampCfg holds them [BORROWS].
 * @return         The high EMBED_TICKET_EPOCH_BITS bits, shifted down.
 */
EMBED_INLINE embed_word embed_epoch_of_stamp(const EmbedEpochOfStampCfg *args)
{
    return args->stamp >> EMBED_TICKET_EPOCH_SHIFT;
}

/** @brief The parameters of embed_ticket_flags_of_stamp. */
typedef struct
{
    const embed_word stamp; /**< The stamp. */
} EmbedTicketFlagsOfStampCfg;

/**
 * @brief The flag field of a stamp, the lane index included.
 *
 * @param[in] args The parameters, as EmbedTicketFlagsOfStampCfg holds them [BORROWS].
 * @return         The 16 bits from EMBED_TICKET_FLAG_SHIFT up.
 */
EMBED_INLINE embed_u16 embed_ticket_flags_of_stamp(const EmbedTicketFlagsOfStampCfg *args)
{
    // Explicit cast truncates to the 16 flag bits that the shift brings down
    return (embed_u16)(args->stamp >> EMBED_TICKET_FLAG_SHIFT);
}

/** @brief The parameters of embed_ticket_lane_of_stamp. */
typedef struct
{
    const embed_word stamp; /**< The stamp of a slot after its pop. */
} EmbedTicketLaneOfStampCfg;

/**
 * @brief The lane index of the pass that performed the pop, from the stamp of the slot.
 *
 * @param[in] args The parameters, as EmbedTicketLaneOfStampCfg holds them [BORROWS].
 * @return         The lane index, seven bits.
 */
EMBED_INLINE unsigned int embed_ticket_lane_of_stamp(const EmbedTicketLaneOfStampCfg *args)
{
    // Explicit cast truncates to the seven lane bits that the mask leaves and the shift brings down
    return (unsigned int)((args->stamp & EMBED_TICKET_LANE_MASK) >> EMBED_TICKET_LANE_SHIFT);
}

/** @brief The parameters of embed_ticket_job_check. */
typedef struct
{
    const void *const job; /**< The job pointer. */
} EmbedTicketJobCheckCfg;

/**
 * @brief The three-bit cyclic redundancy check of a job pointer: its bits as a polynomial, modulo x^3 + x + 1.
 *
 * @param[in] args The parameters, as EmbedTicketJobCheckCfg holds them [BORROWS].
 * @return         The remainder, three bits.
 * @note The polynomial has period 7, so x^7 is 1 modulo it. The fold shifts by 7 times each power of two, from the
 *       byte count of the pointer down to 1. That sums the bits of each residue class modulo 7 into the low seven
 *       bits. The shifts are 56, 28, 14 and 7 places for a pointer of 64 bits, and 28, 14 and 7 for 32 bits. The
 *       remainder of the seven bits reads off the table x^3 = x + 1, x^4 = x^2 + x, x^5 = x^2 + x + 1, x^6 = x^2 + 1.
 * @note A single-bit error at any place changes the remainder, because no power of x is zero modulo the
 *       polynomial. A double-bit error changes it unless the two places are a multiple of seven apart.
 */
EMBED_INLINE embed_word embed_ticket_job_check(const EmbedTicketJobCheckCfg *args)
{
    // Explicit cast carries the pointer as an integer for the fold
    uintptr_t bits = (uintptr_t)args->job;

    for (uintptr_t fold_shift = 7u * sizeof(uintptr_t); fold_shift >= 7u; fold_shift /= 2u)
    {
        bits ^= bits >> fold_shift;
    }

    const uintptr_t residue_0 = bits & 1u;
    const uintptr_t residue_1 = (bits >> 1u) & 1u;
    const uintptr_t residue_2 = (bits >> 2u) & 1u;
    const uintptr_t residue_3 = (bits >> 3u) & 1u;
    const uintptr_t residue_4 = (bits >> 4u) & 1u;
    const uintptr_t residue_5 = (bits >> 5u) & 1u;
    const uintptr_t residue_6 = (bits >> 6u) & 1u;

    // Explicit cast narrows the remainder, three bits, to the word
    return (embed_word)((residue_0 ^ residue_3 ^ residue_5 ^ residue_6) |
                        ((residue_1 ^ residue_3 ^ residue_4 ^ residue_5) << 1u) |
                        ((residue_2 ^ residue_4 ^ residue_5 ^ residue_6) << 2u));
}

/** @brief The parameters of embed_ticket_job_check_bits. */
typedef struct
{
    const void *const job; /**< The job pointer. */
} EmbedTicketJobCheckBitsCfg;

/**
 * @brief The check bits that the push stores for a job pointer: the check where the build verifies it, 0 where not.
 *
 * @param[in] args The parameters, as EmbedTicketJobCheckBitsCfg holds them [BORROWS].
 * @return         The check at EMBED_TICKET_CHECK_SHIFT, or 0.
 */
EMBED_INLINE embed_word embed_ticket_job_check_bits(const EmbedTicketJobCheckBitsCfg *args)
{
#if EMBED_TICKET_JOB_CHECK
    return EMBED_CALL(embed_ticket_job_check, EmbedTicketJobCheckCfg, args->job) << EMBED_TICKET_CHECK_SHIFT;
#else
    (void)args->job;
    return 0u;
#endif
}

/** @brief The parameters of embed_ticket_fields_of_stamp. */
typedef struct
{
    const embed_word stamp; /**< The stamp. */
} EmbedTicketFieldsOfStampCfg;

/**
 * @brief The epoch and ticket fields of a stamp, with the flag field cleared.
 *
 * @param[in] args The parameters, as EmbedTicketFieldsOfStampCfg holds them [BORROWS].
 * @return         The stamp with the 16 flag bits clear.
 */
EMBED_INLINE embed_word embed_ticket_fields_of_stamp(const EmbedTicketFieldsOfStampCfg *args)
{
    return args->stamp & ~EMBED_TICKET_FLAG_MASK;
}

/** @brief The parameters of embed_ticket_ring_begin. */
typedef struct
{
    EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    EmbedTicketRingSlot *const
        slots;                /**< The slot array [BORROWS], 2^shift slots, zero-initialized, which the caller owns. */
    const unsigned int shift; /**< The slot count as a power of two, at max EMBED_TICKET_RING_SHIFT_MAX. */
} EmbedTicketRingBeginCfg;

/**
 * @brief Initializes the ring over a slot array of 2^shift slots.
 *
 * @param[in] args The parameters, as EmbedTicketRingBeginCfg holds them [BORROWS].
 * @note The slot count is a power of two by construction, so the index is a mask and not a modulo.
 */
EMBED_INLINE void embed_ticket_ring_begin(const EmbedTicketRingBeginCfg *args)
{
    // Explicit cast widens the one to the word before the shift, and EMBED_TICKET_RING_SHIFT_MAX holds the shift below
    // the width
    args->ring->mask = ((embed_word)1u << args->shift) - 1u;
    args->ring->slots = args->slots;
}

/** @brief The parameters of embed_ticket_ring_place. */
typedef struct
{
    const EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    const embed_word stamp;            /**< The stamp. */
} EmbedTicketRingPlaceCfg;

/**
 * @brief The slot index of the ticket of a stamp.
 *
 * @param[in] args The parameters, as EmbedTicketRingPlaceCfg holds them [BORROWS].
 * @return         The low shift bits of the ticket.
 */
EMBED_INLINE embed_word embed_ticket_ring_place(const EmbedTicketRingPlaceCfg *args)
{
    return args->stamp & EMBED_TICKET_MASK & args->ring->mask;
}

/** @brief The parameters of embed_ticket_ring_slot. */
typedef struct
{
    const EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    const embed_word stamp;            /**< The stamp. */
} EmbedTicketRingSlotCfg;

/**
 * @brief The slot of a stamp.
 *
 * @param[in] args The parameters, as EmbedTicketRingSlotCfg holds them [BORROWS].
 * @return         The slot at the index of the ticket [BORROWS].
 */
EMBED_INLINE EmbedTicketRingSlot *embed_ticket_ring_slot(const EmbedTicketRingSlotCfg *args)
{
    return &args->ring->slots[EMBED_CALL(embed_ticket_ring_place, EmbedTicketRingPlaceCfg, args->ring, args->stamp)];
}

/** @brief The parameters of embed_ticket_ring_expected_stamp. */
typedef struct
{
    const embed_word stamp; /**< The input stamp, as the caller holds it. */
    const embed_word flags; /**< The flag bits of the transitions before this one. */
} EmbedTicketRingExpectedStampCfg;

/**
 * @brief The expected stamp of a transition: the input stamp with the flag bits of the transitions before it.
 *
 * @param[in] args The parameters, as EmbedTicketRingExpectedStampCfg holds them [BORROWS].
 * @return         The expected stamp.
 * @note An input stamp with any flag bit set is invalid. The expected stamp then has the reserved bit set, and no
 *       slot word has it, so the compare fails. An input stamp with a corrupted ticket or epoch expects a word of
 *       another stamp, so the compare fails too.
 */
EMBED_INLINE embed_word embed_ticket_ring_expected_stamp(const EmbedTicketRingExpectedStampCfg *args)
{
    // Explicit cast converts the invalid-input condition to 1 or 0, for the shift to the reserved bit
    const embed_word invalid_input = (embed_word)((args->stamp & EMBED_TICKET_FLAG_MASK) != 0u);

    return args->stamp | args->flags | (invalid_input << (EMBED_TICKET_FLAG_SHIFT + 15u));
}

/** @brief The parameters of embed_ticket_ring_guarded_stamp. */
typedef struct
{
    const embed_word current_stamp; /**< The current value of the slot word. */
    const embed_word valid_mask;    /**< All ones where the state test passed, all zeros where it did not. */
} EmbedTicketRingGuardedStampCfg;

/**
 * @brief The expected stamp of a transition that read the current stamp first. It is the current stamp where the
 *        state test passed, and the current stamp with the reserved bit inverted where it did not.
 *
 * @param[in] args The parameters, as EmbedTicketRingGuardedStampCfg holds them [BORROWS].
 * @return         The expected stamp.
 * @note A word with the reserved bit inverted differs from the current stamp in one bit, so the compare fails. An
 *       expected stamp of zero does not serve: a zero current stamp would match it.
 */
EMBED_INLINE embed_word embed_ticket_ring_guarded_stamp(const EmbedTicketRingGuardedStampCfg *args)
{
    return args->current_stamp ^ (~args->valid_mask & EMBED_TICKET_RESERVED_BIT);
}

/** @brief The parameters of embed_ticket_ring_transition. */
typedef struct
{
    EmbedTicketRingSlot *const slot; /**< The slot [BORROWS]. */
    const embed_word expected_stamp; /**< The expected stamp. */
    const embed_word new_stamp;      /**< The new stamp: the expected stamp with the flag bit of the transition. */
    embed_word *const found_stamp;   /**< The stamp that the compare found in the slot [BORROWS]: the expected stamp on
                                        success, the current stamp on failure. */
} EmbedTicketRingTransitionCfg;

/**
 * @brief One transition of a slot: a compare-and-swap from the expected stamp to the new stamp.
 *
 * @param[in] args The parameters, as EmbedTicketRingTransitionCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_transition(const EmbedTicketRingTransitionCfg *args)
{
    embed_word compared_stamp = args->expected_stamp;
    const embed_bool success = EMBED_CALL(embed_atomic_word_swap_if, EmbedAtomicWordSwapIfCfg, &args->slot->stamp,
                                          &compared_stamp, args->new_stamp);

    *args->found_stamp = compared_stamp;
    return success;
}

/** @brief The parameters of embed_ticket_ring_grant. */
typedef struct
{
    const EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    const embed_word stamp;            /**< The input stamp, flag bits clear. */
    embed_word *const found_stamp;     /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingGrantCfg;

/**
 * @brief The grant transition: the stamp with the granted bit, from a zero word or from a released word.
 *
 * @param[in] args The parameters, as EmbedTicketRingGrantCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 * @note The expected stamp is the current stamp where it has the released bit, and zero where it does not. A
 *       word in any other state fails the compare. The next grant at an index reuses the released word.
 * @note An input stamp with a flag bit set adds the reserved bit to the expected stamp, so the compare fails.
 */
EMBED_INLINE embed_bool embed_ticket_ring_grant(const EmbedTicketRingGrantCfg *args)
{
    EmbedTicketRingSlot *const slot =
        EMBED_CALL(embed_ticket_ring_slot, EmbedTicketRingSlotCfg, args->ring, args->stamp);
    const embed_word current_stamp = EMBED_CALL(embed_atomic_word_load, EmbedAtomicWordLoadCfg, &slot->stamp);
    // Explicit cast converts the released bit to an all-ones or all-zeros mask over the current stamp
    const embed_word released_mask =
        (embed_word)0u - ((current_stamp & EMBED_TICKET_RELEASED_BIT) >> (EMBED_TICKET_FLAG_SHIFT + 4u));
    const embed_word reserved_bit =
        EMBED_CALL(embed_ticket_ring_expected_stamp, EmbedTicketRingExpectedStampCfg, args->stamp, 0u) &
        EMBED_TICKET_RESERVED_BIT;

    return EMBED_CALL(embed_ticket_ring_transition, EmbedTicketRingTransitionCfg, slot,
                      (current_stamp & released_mask) | reserved_bit, args->stamp | EMBED_TICKET_GRANTED_BIT,
                      args->found_stamp);
}

/** @brief The parameters of embed_ticket_ring_push. */
typedef struct
{
    const EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    const embed_word stamp;            /**< The input stamp of a granted slot. */
    void *const job;                   /**< The job [BORROWS]. */
    embed_word *const found_stamp;     /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingPushCfg;

/**
 * @brief The push transition: the job pointer stored, then the pushed bit added to the granted word.
 *
 * @param[in] args The parameters, as EmbedTicketRingPushCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 * @note The store of the job pointer precedes the compare-and-swap, which has release semantics. A pop that succeeds
 *       after it has acquire semantics, so the pop reads the job pointer.
 * @note The new stamp carries the check bits of the job pointer where the build verifies them.
 */
EMBED_INLINE embed_bool embed_ticket_ring_push(const EmbedTicketRingPushCfg *args)
{
    EmbedTicketRingSlot *const slot =
        EMBED_CALL(embed_ticket_ring_slot, EmbedTicketRingSlotCfg, args->ring, args->stamp);
    const embed_word expected_stamp = EMBED_CALL(embed_ticket_ring_expected_stamp, EmbedTicketRingExpectedStampCfg,
                                                 args->stamp, EMBED_TICKET_GRANTED_BIT);
    const embed_word new_stamp = expected_stamp | EMBED_TICKET_PUSHED_BIT |
                                 EMBED_CALL(embed_ticket_job_check_bits, EmbedTicketJobCheckBitsCfg, args->job);

    slot->job = args->job;
    return EMBED_CALL(embed_ticket_ring_transition, EmbedTicketRingTransitionCfg, slot, expected_stamp, new_stamp,
                      args->found_stamp);
}

/** @brief The parameters of embed_ticket_ring_job_verify. */
typedef struct
{
    const EmbedTicketRingSlot *const slot; /**< The slot [BORROWS]. */
    const embed_word current_stamp;        /**< The stamp of the slot before the transition. */
    void **const job; /**< The job where the check matches [BORROWS], and NULL where it does not. */
    embed_word *const
        found_stamp; /**< The stamp, with the reserved bit set where the check does not match [BORROWS]. */
} EmbedTicketRingJobVerifyCfg;

/**
 * @brief The job pointer of a slot after a successful read transition, verified against the check bits of the word.
 *
 * @param[in] args The parameters, as EmbedTicketRingJobVerifyCfg holds them [BORROWS].
 * @return         EMBED_TRUE where the check matches, or where the build does not verify it.
 * @note The reserved bit in found_stamp marks a corrupted job pointer, which no contention produces, since no slot
 *       word ever holds that bit.
 */
EMBED_INLINE embed_bool embed_ticket_ring_job_verify(const EmbedTicketRingJobVerifyCfg *args)
{
    void *const read_job = args->slot->job;
#if EMBED_TICKET_JOB_CHECK
    const embed_word stored_check = (args->current_stamp & EMBED_TICKET_CHECK_MASK) >> EMBED_TICKET_CHECK_SHIFT;
    // Explicit cast converts the comparison, an int of 0 or 1, to the project boolean
    const embed_bool match =
        (embed_bool)(EMBED_CALL(embed_ticket_job_check, EmbedTicketJobCheckCfg, read_job) == stored_check);
#else
    const embed_bool match = EMBED_TRUE;
#endif

    *args->job = match ? read_job : NULL;
    *args->found_stamp = match ? args->current_stamp : (args->current_stamp | EMBED_TICKET_RESERVED_BIT);
    return match;
}

/** @brief The parameters of embed_ticket_ring_pop. */
typedef struct
{
    const EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    const embed_word stamp;            /**< The input stamp of a pushed slot. */
    const unsigned int
        lane_place;   /**< The place of the lane of the pass of the caller, as embed_turnstile_enter returned it. */
    void **const job; /**< The job on success [BORROWS], and NULL on failure. */
    embed_word *const found_stamp; /**< The stamp that the compare found in the slot [BORROWS], with the reserved bit
                                      set where the job pointer failed its check. */
} EmbedTicketRingPopCfg;

/**
 * @brief The pop transition: the popped bit and the lane index added to the pushed word, and the job pointer read.
 *
 * @param[in] args The parameters, as EmbedTicketRingPopCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 * @note Two pops of one stamp contend here. One succeeds, and the other finds the popped bit in found_stamp.
 * @note The expected stamp is the current stamp where the current stamp is the pushed word of the stamp, check
 *       bits aside. A pop whose compare succeeds but whose job pointer fails its check returns failure, with the
 *       reserved bit in found_stamp. The slot stays popped, so no caller runs the corrupted job.
 */
EMBED_INLINE embed_bool embed_ticket_ring_pop(const EmbedTicketRingPopCfg *args)
{
    EmbedTicketRingSlot *const slot =
        EMBED_CALL(embed_ticket_ring_slot, EmbedTicketRingSlotCfg, args->ring, args->stamp);
    const embed_word current_stamp = EMBED_CALL(embed_atomic_word_load, EmbedAtomicWordLoadCfg, &slot->stamp);
    const embed_word pushed_stamp = EMBED_CALL(embed_ticket_ring_expected_stamp, EmbedTicketRingExpectedStampCfg,
                                               args->stamp, EMBED_TICKET_GRANTED_BIT | EMBED_TICKET_PUSHED_BIT);
    // Explicit cast converts the state test, check bits aside, to an all-ones or all-zeros mask over the current stamp
    const embed_word valid_mask =
        (embed_word)0u - (embed_word)((current_stamp & ~EMBED_TICKET_CHECK_MASK) == pushed_stamp);
    // Explicit cast converts the lane place to the stamp, and EMBED_TURNSTILE_LANES bounds it at 64
    const embed_word new_stamp =
        current_stamp | EMBED_TICKET_POPPED_BIT | ((embed_word)args->lane_place << EMBED_TICKET_LANE_SHIFT);
    const embed_bool success = EMBED_CALL(
        embed_ticket_ring_transition, EmbedTicketRingTransitionCfg, slot,
        EMBED_CALL(embed_ticket_ring_guarded_stamp, EmbedTicketRingGuardedStampCfg, current_stamp, valid_mask),
        new_stamp, args->found_stamp);

    if (!success)
    {
        *args->job = NULL;
        return EMBED_FALSE;
    }
    return EMBED_CALL(embed_ticket_ring_job_verify, EmbedTicketRingJobVerifyCfg, slot, current_stamp, args->job,
                      args->found_stamp);
}

/** @brief The parameters of embed_ticket_ring_replay. */
typedef struct
{
    const EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    const embed_word stamp;            /**< The input stamp of a popped slot. */
    void **const job;                  /**< The job on success [BORROWS], and NULL on failure. */
    embed_word *const found_stamp;     /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingReplayCfg;

/**
 * @brief The replay transition: the replayed bit added to the popped word, and the job pointer read again.
 *
 * @param[in] args The parameters, as EmbedTicketRingReplayCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 * @note A rewind of a count of tickets is a replay at the ticket that count back. The expected stamp is the current
 *       stamp where the current stamp is the popped word of the stamp, lane and check bits aside. Any other current
 *       stamp fails the compare. The job pointer passes the same check as at the pop.
 */
EMBED_INLINE embed_bool embed_ticket_ring_replay(const EmbedTicketRingReplayCfg *args)
{
    EmbedTicketRingSlot *const slot =
        EMBED_CALL(embed_ticket_ring_slot, EmbedTicketRingSlotCfg, args->ring, args->stamp);
    const embed_word current_stamp = EMBED_CALL(embed_atomic_word_load, EmbedAtomicWordLoadCfg, &slot->stamp);
    const embed_word popped_stamp =
        EMBED_CALL(embed_ticket_ring_expected_stamp, EmbedTicketRingExpectedStampCfg, args->stamp,
                   EMBED_TICKET_GRANTED_BIT | EMBED_TICKET_PUSHED_BIT | EMBED_TICKET_POPPED_BIT);
    // Explicit cast converts the state test, lane and check bits aside, to an all-ones or all-zeros mask
    const embed_word valid_mask =
        (embed_word)0u -
        (embed_word)((current_stamp & ~(EMBED_TICKET_LANE_MASK | EMBED_TICKET_CHECK_MASK)) == popped_stamp);
    const embed_bool success = EMBED_CALL(
        embed_ticket_ring_transition, EmbedTicketRingTransitionCfg, slot,
        EMBED_CALL(embed_ticket_ring_guarded_stamp, EmbedTicketRingGuardedStampCfg, current_stamp, valid_mask),
        current_stamp | EMBED_TICKET_REPLAYED_BIT, args->found_stamp);

    if (!success)
    {
        *args->job = NULL;
        return EMBED_FALSE;
    }
    return EMBED_CALL(embed_ticket_ring_job_verify, EmbedTicketRingJobVerifyCfg, slot, current_stamp, args->job,
                      args->found_stamp);
}

/** @brief The parameters of embed_ticket_ring_release. */
typedef struct
{
    const EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    const embed_word stamp;            /**< The input stamp of a popped slot. */
    embed_word *const found_stamp;     /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingReleaseCfg;

/**
 * @brief The release transition: the released bit added to the popped word, replayed or not.
 *
 * @param[in] args The parameters, as EmbedTicketRingReleaseCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 * @note The writer releases after its drain, so no pass that started before the release reads the slot after it.
 */
EMBED_INLINE embed_bool embed_ticket_ring_release(const EmbedTicketRingReleaseCfg *args)
{
    EmbedTicketRingSlot *const slot =
        EMBED_CALL(embed_ticket_ring_slot, EmbedTicketRingSlotCfg, args->ring, args->stamp);
    const embed_word current_stamp = EMBED_CALL(embed_atomic_word_load, EmbedAtomicWordLoadCfg, &slot->stamp);
    const embed_word popped_stamp =
        EMBED_CALL(embed_ticket_ring_expected_stamp, EmbedTicketRingExpectedStampCfg, args->stamp,
                   EMBED_TICKET_GRANTED_BIT | EMBED_TICKET_PUSHED_BIT | EMBED_TICKET_POPPED_BIT);
    const embed_word ignored_bits = EMBED_TICKET_LANE_MASK | EMBED_TICKET_REPLAYED_BIT | EMBED_TICKET_CHECK_MASK;
    // Explicit cast converts the state test, lane, replayed and check bits aside, to an all-ones or all-zeros mask
    const embed_word valid_mask = (embed_word)0u - (embed_word)((current_stamp & ~ignored_bits) == popped_stamp);

    return EMBED_CALL(
        embed_ticket_ring_transition, EmbedTicketRingTransitionCfg, slot,
        EMBED_CALL(embed_ticket_ring_guarded_stamp, EmbedTicketRingGuardedStampCfg, current_stamp, valid_mask),
        current_stamp | EMBED_TICKET_RELEASED_BIT, args->found_stamp);
}

/** @brief The parameters of embed_ticket_ring_pass_grant. */
typedef struct
{
    EmbedTurnstiles *const set;            /**< The turnstiles over the ring [BORROWS]. */
    const embed_atomic_uptr_value address; /**< The address of the worker, as embed_turnstile_enter takes it. */
    const EmbedTicketRing *const ring;     /**< The ring [BORROWS]. */
    const embed_word stamp;                /**< The input stamp, flag bits clear. */
    embed_word *const found_stamp;         /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingPassGrantCfg;

/**
 * @brief The grant transition inside a pass: enter at address, grant, leave.
 *
 * @param[in] args The parameters, as EmbedTicketRingPassGrantCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_pass_grant(const EmbedTicketRingPassGrantCfg *args)
{
    const unsigned int place = EMBED_CALL(embed_turnstile_enter, EmbedTurnstileEnterCfg, args->set, args->address);
    const embed_bool success =
        EMBED_CALL(embed_ticket_ring_grant, EmbedTicketRingGrantCfg, args->ring, args->stamp, args->found_stamp);

    EMBED_CALL(embed_turnstile_leave, EmbedTurnstileLeaveCfg, args->set, place);
    return success;
}

/** @brief The parameters of embed_ticket_ring_pass_push. */
typedef struct
{
    EmbedTurnstiles *const set;            /**< The turnstiles over the ring [BORROWS]. */
    const embed_atomic_uptr_value address; /**< The address of the worker, as embed_turnstile_enter takes it. */
    const EmbedTicketRing *const ring;     /**< The ring [BORROWS]. */
    const embed_word stamp;                /**< The input stamp of a granted slot. */
    void *const job;                       /**< The job [BORROWS]. */
    embed_word *const found_stamp;         /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingPassPushCfg;

/**
 * @brief The push transition inside a pass: enter at address, push, leave.
 *
 * @param[in] args The parameters, as EmbedTicketRingPassPushCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_pass_push(const EmbedTicketRingPassPushCfg *args)
{
    const unsigned int place = EMBED_CALL(embed_turnstile_enter, EmbedTurnstileEnterCfg, args->set, args->address);
    const embed_bool success = EMBED_CALL(embed_ticket_ring_push, EmbedTicketRingPushCfg, args->ring, args->stamp,
                                          args->job, args->found_stamp);

    EMBED_CALL(embed_turnstile_leave, EmbedTurnstileLeaveCfg, args->set, place);
    return success;
}

/** @brief The parameters of embed_ticket_ring_pass_pop. */
typedef struct
{
    EmbedTurnstiles *const set;            /**< The turnstiles over the ring [BORROWS]. */
    const embed_atomic_uptr_value address; /**< The address of the worker, as embed_turnstile_enter takes it. */
    const EmbedTicketRing *const ring;     /**< The ring [BORROWS]. */
    const embed_word stamp;                /**< The input stamp of a pushed slot. */
    void **const job;                      /**< The job on success [BORROWS], and NULL on failure. */
    embed_word *const found_stamp;         /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingPassPopCfg;

/**
 * @brief The pop transition inside a pass: enter at address, pop with the lane index of that pass, leave.
 *
 * @param[in] args The parameters, as EmbedTicketRingPassPopCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_pass_pop(const EmbedTicketRingPassPopCfg *args)
{
    const unsigned int place = EMBED_CALL(embed_turnstile_enter, EmbedTurnstileEnterCfg, args->set, args->address);
    const embed_bool success = EMBED_CALL(embed_ticket_ring_pop, EmbedTicketRingPopCfg, args->ring, args->stamp, place,
                                          args->job, args->found_stamp);

    EMBED_CALL(embed_turnstile_leave, EmbedTurnstileLeaveCfg, args->set, place);
    return success;
}

/** @brief The parameters of embed_ticket_ring_pass_replay. */
typedef struct
{
    EmbedTurnstiles *const set;            /**< The turnstiles over the ring [BORROWS]. */
    const embed_atomic_uptr_value address; /**< The address of the worker, as embed_turnstile_enter takes it. */
    const EmbedTicketRing *const ring;     /**< The ring [BORROWS]. */
    const embed_word stamp;                /**< The input stamp of a popped slot. */
    void **const job;                      /**< The job on success [BORROWS], and NULL on failure. */
    embed_word *const found_stamp;         /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingPassReplayCfg;

/**
 * @brief The replay transition inside a pass: enter at address, replay, leave.
 *
 * @param[in] args The parameters, as EmbedTicketRingPassReplayCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_pass_replay(const EmbedTicketRingPassReplayCfg *args)
{
    const unsigned int place = EMBED_CALL(embed_turnstile_enter, EmbedTurnstileEnterCfg, args->set, args->address);
    const embed_bool success = EMBED_CALL(embed_ticket_ring_replay, EmbedTicketRingReplayCfg, args->ring, args->stamp,
                                          args->job, args->found_stamp);

    EMBED_CALL(embed_turnstile_leave, EmbedTurnstileLeaveCfg, args->set, place);
    return success;
}

/** @brief The parameters of embed_ticket_ring_drain_release. */
typedef struct
{
    EmbedTurnstiles *const set;        /**< The turnstiles over the ring [BORROWS]. */
    const EmbedTicketRing *const ring; /**< The ring [BORROWS]. */
    const embed_word stamp;            /**< The input stamp of a popped slot. */
    embed_word *const found_stamp;     /**< The stamp that the compare found in the slot [BORROWS]. */
} EmbedTicketRingDrainReleaseCfg;

/**
 * @brief The release transition after a drain: drain set, then release the stamp.
 *
 * @param[in] args The parameters, as EmbedTicketRingDrainReleaseCfg holds them [BORROWS].
 * @return         EMBED_TRUE on success.
 * @note The writer calls this after it observed the popped bit and replaced what the slot referenced. The pass of
 *       that pop started before the drain, so the drain waits for it, and the release follows.
 */
EMBED_INLINE embed_bool embed_ticket_ring_drain_release(const EmbedTicketRingDrainReleaseCfg *args)
{
    EMBED_CALL(embed_turnstiles_drain, EmbedTurnstilesDrainCfg, args->set);
    return EMBED_CALL(embed_ticket_ring_release, EmbedTicketRingReleaseCfg, args->ring, args->stamp, args->found_stamp);
}

/**
 * @brief The dispatch table of embed_ticket_ring.h: one member for each public function, in the order of the header.
 *
 * @note EMBED_TABLE_LAYOUT asserts that the members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets.
 */
typedef struct
{
    embed_word (*ticket_stamp)(const EmbedTicketStampCfg *args);                   /**< embed_ticket_stamp. */
    embed_word (*ticket_of_stamp)(const EmbedTicketOfStampCfg *args);              /**< embed_ticket_of_stamp. */
    embed_word (*epoch_of_stamp)(const EmbedEpochOfStampCfg *args);                /**< embed_epoch_of_stamp. */
    embed_u16 (*ticket_flags_of_stamp)(const EmbedTicketFlagsOfStampCfg *args);    /**< embed_ticket_flags_of_stamp. */
    unsigned int (*ticket_lane_of_stamp)(const EmbedTicketLaneOfStampCfg *args);   /**< embed_ticket_lane_of_stamp. */
    embed_word (*ticket_job_check)(const EmbedTicketJobCheckCfg *args);            /**< embed_ticket_job_check. */
    embed_word (*ticket_job_check_bits)(const EmbedTicketJobCheckBitsCfg *args);   /**< embed_ticket_job_check_bits. */
    embed_word (*ticket_fields_of_stamp)(const EmbedTicketFieldsOfStampCfg *args); /**< embed_ticket_fields_of_stamp. */
    void (*begin)(const EmbedTicketRingBeginCfg *args);                            /**< embed_ticket_ring_begin. */
    embed_word (*place)(const EmbedTicketRingPlaceCfg *args);                      /**< embed_ticket_ring_place. */
    embed_bool (*grant)(const EmbedTicketRingGrantCfg *args);                      /**< embed_ticket_ring_grant. */
    embed_bool (*push)(const EmbedTicketRingPushCfg *args);                        /**< embed_ticket_ring_push. */
    embed_bool (*pop)(const EmbedTicketRingPopCfg *args);                          /**< embed_ticket_ring_pop. */
    embed_bool (*replay)(const EmbedTicketRingReplayCfg *args);                    /**< embed_ticket_ring_replay. */
    embed_bool (*release)(const EmbedTicketRingReleaseCfg *args);                  /**< embed_ticket_ring_release. */
    embed_bool (*pass_grant)(const EmbedTicketRingPassGrantCfg *args);             /**< embed_ticket_ring_pass_grant. */
    embed_bool (*pass_push)(const EmbedTicketRingPassPushCfg *args);               /**< embed_ticket_ring_pass_push. */
    embed_bool (*pass_pop)(const EmbedTicketRingPassPopCfg *args);                 /**< embed_ticket_ring_pass_pop. */
    embed_bool (*pass_replay)(const EmbedTicketRingPassReplayCfg *args);     /**< embed_ticket_ring_pass_replay. */
    embed_bool (*drain_release)(const EmbedTicketRingDrainReleaseCfg *args); /**< embed_ticket_ring_drain_release. */
} EmbedTicketRingNs;
EMBED_TABLE_LAYOUT(EmbedTicketRingNs, ticket_stamp, ticket_of_stamp, epoch_of_stamp, ticket_flags_of_stamp,
                   ticket_lane_of_stamp, ticket_job_check, ticket_job_check_bits, ticket_fields_of_stamp, begin, place,
                   grant, push, pop, replay, release, pass_grant, pass_push, pass_pop, pass_replay, drain_release);

#if !defined(__CUDA_ARCH__)
/**
 * @brief The table of embed_ticket_ring.h. A call through it folds to a direct call under the optimizer and LTO.
 *
 * @note The device pass of CUDA has no table, because a kernel cannot read the address of a host function. A
 *       kernel calls each function by its name.
 */
static const EmbedTicketRingNs embed_ticket_ring_table = {embed_ticket_stamp,
                                                          embed_ticket_of_stamp,
                                                          embed_epoch_of_stamp,
                                                          embed_ticket_flags_of_stamp,
                                                          embed_ticket_lane_of_stamp,
                                                          embed_ticket_job_check,
                                                          embed_ticket_job_check_bits,
                                                          embed_ticket_fields_of_stamp,
                                                          embed_ticket_ring_begin,
                                                          embed_ticket_ring_place,
                                                          embed_ticket_ring_grant,
                                                          embed_ticket_ring_push,
                                                          embed_ticket_ring_pop,
                                                          embed_ticket_ring_replay,
                                                          embed_ticket_ring_release,
                                                          embed_ticket_ring_pass_grant,
                                                          embed_ticket_ring_pass_push,
                                                          embed_ticket_ring_pass_pop,
                                                          embed_ticket_ring_pass_replay,
                                                          embed_ticket_ring_drain_release};
#endif

EMBED_END_DECLS

#endif
