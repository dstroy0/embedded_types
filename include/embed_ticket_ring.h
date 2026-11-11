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
 * The ring has 2^shift slots. A slot holds a stamp and a job pointer. The stamp is one atomic 64-bit word: the epoch
 * in bits 48 to 63, the flag bits in bits 32 to 47, and the ticket in bits 0 to 31. The ticket indexes the slot: the
 * low shift bits of the ticket are the slot index. The ticket is a 32-bit counter with wraparound, and the slot index
 * wraps with it, because 2^shift divides 2^32.
 *
 * @note Each transition of a slot is one compare-and-swap on the stamp word. The expected value is the stamp with
 *       the flag bits of the transitions before it. The new value adds the flag bit of the transition. So exactly
 *       one caller completes each transition, and a stamp that differs in any bit fails the compare.
 * @note The state sequence of a slot is grant, push, pop, replay, release. A replay is the read of a slot after its
 *       pop, which a rewind performs. A grant succeeds on a zero word or on a released word. A release succeeds
 *       after a pop. The next grant at the same index reuses the released word.
 * @note The pop writes the lane index of the pass that performed it into seven flag bits. The release keeps them.
 * @note The epoch is 16 bits. The grantor derives it from the host clock. The tick period covers the lifetime of a
 *       stamp, so the epoch does not wrap while the stamp is live. A compare succeeds only where the ticket and the
 *       epoch both match, and the ticket alone distinguishes any two live stamps.
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

/** @brief The width of the ticket field, and the positions of the flag field and the epoch field. */
#define EMBED_TICKET_BITS 32u
#define EMBED_TICKET_MASK ((embed_u64)0xffffffffu)
#define EMBED_TICKET_FLAG_SHIFT 32u
#define EMBED_TICKET_FLAG_BITS 16u
#define EMBED_TICKET_FLAG_MASK ((embed_u64)0xffffu << EMBED_TICKET_FLAG_SHIFT)
#define EMBED_TICKET_EPOCH_SHIFT 48u
#define EMBED_TICKET_EPOCH_BITS 16u

/** @brief The flag bits: one per transition of the slot state machine, and the lane index of the pop. */
#define EMBED_TICKET_GRANTED ((embed_u64)1u << 32)
#define EMBED_TICKET_PUSHED ((embed_u64)1u << 33)
#define EMBED_TICKET_POPPED ((embed_u64)1u << 34)
#define EMBED_TICKET_REPLAYED ((embed_u64)1u << 35)
#define EMBED_TICKET_RELEASED ((embed_u64)1u << 36)
#define EMBED_TICKET_LANE_SHIFT 40u
#define EMBED_TICKET_LANE_MASK ((embed_u64)0x7fu << EMBED_TICKET_LANE_SHIFT)
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_LANES <= 128u, "the lane index of a pop fits seven flag bits");

/** @brief The reserved bit. No transition sets it, so an expected value with it set always fails the compare. */
#define EMBED_TICKET_RESERVED ((embed_u64)1u << 47)

/** @brief The maximum slot count as a shift: 2^EMBED_TICKET_RING_SHIFT_MOST slots. */
#define EMBED_TICKET_RING_SHIFT_MOST 31u

/** @brief One slot: the stamp word, the state of the slot, and the job pointer. */
typedef struct
{
    embed_atomic_u64 stamp; /**< The stamp with the flag bits of the transitions that completed. */
    void *job;              /**< The job [BORROWS]. */
} EmbedTicketRingSlot;
EMBED_STATIC_ASSERT(sizeof(EmbedTicketRingSlot) == 16u, "a slot is two words");

/** @brief A ring: its index mask, 2^shift - 1, on a cache line of its own, and its slot array. */
typedef struct
{
    EMBED_ALIGN(64) embed_u64 mask; /**< 2^shift - 1. */
    EmbedTicketRingSlot *slots;     /**< 2^shift slots. */
} EmbedTicketRing;

/**
 * @brief The stamp of an epoch and a ticket, with the flag bits clear.
 *
 * @param[in] epoch  Bits 48 to 63.
 * @param[in] ticket Bits 0 to 31.
 * @return           The stamp.
 */
EMBED_INLINE embed_u64 embed_ticket_stamp(embed_u16 epoch, embed_u32 ticket)
{
    return ((embed_u64)epoch << EMBED_TICKET_EPOCH_SHIFT) | (embed_u64)ticket;
}

/**
 * @brief The ticket field of a stamp.
 *
 * @param[in] stamp The stamp.
 * @return          Bits 0 to 31.
 */
EMBED_INLINE embed_u32 embed_ticket_of_stamp(embed_u64 stamp)
{
    // Explicit cast truncates to the 32 bits that the mask leaves
    return (embed_u32)(stamp & EMBED_TICKET_MASK);
}

/**
 * @brief The epoch field of a stamp.
 *
 * @param[in] stamp The stamp.
 * @return          Bits 48 to 63.
 */
EMBED_INLINE embed_u16 embed_epoch_of_stamp(embed_u64 stamp)
{
    // Explicit cast truncates to the 16 bits that the shift brings down
    return (embed_u16)(stamp >> EMBED_TICKET_EPOCH_SHIFT);
}

/**
 * @brief The flag field of a stamp, the lane index included.
 *
 * @param[in] stamp The stamp.
 * @return          Bits 32 to 47.
 */
EMBED_INLINE embed_u16 embed_ticket_flags_of_stamp(embed_u64 stamp)
{
    // Explicit cast truncates to the 16 flag bits that the shift brings down
    return (embed_u16)(stamp >> EMBED_TICKET_FLAG_SHIFT);
}

/**
 * @brief The lane index of the pass that performed the pop, from the stamp of the slot.
 *
 * @param[in] stamp The stamp of a slot after its pop.
 * @return          The lane index, seven bits.
 */
EMBED_INLINE unsigned int embed_ticket_lane_of_stamp(embed_u64 stamp)
{
    // Explicit cast truncates to the seven lane bits that the mask leaves and the shift brings down
    return (unsigned int)((stamp & EMBED_TICKET_LANE_MASK) >> EMBED_TICKET_LANE_SHIFT);
}

/**
 * @brief The epoch and ticket fields of a stamp, with the flag field cleared.
 *
 * @param[in] stamp The stamp.
 * @return          The stamp with bits 32 to 47 clear.
 */
EMBED_INLINE embed_u64 embed_ticket_fields_of_stamp(embed_u64 stamp)
{
    return stamp & ~EMBED_TICKET_FLAG_MASK;
}

/**
 * @brief Initializes the ring over a slot array of 2^shift slots.
 *
 * @param[out] ring  The ring [BORROWS].
 * @param[in]  slots The slot array [BORROWS], 2^shift slots, zero-initialized, which the caller owns.
 * @param[in]  shift The slot count as a power of two, at most EMBED_TICKET_RING_SHIFT_MOST.
 * @note The slot count is a power of two by construction, so the index is a mask and not a modulo.
 */
EMBED_INLINE void embed_ticket_ring_begin(EmbedTicketRing *ring, EmbedTicketRingSlot *slots, unsigned int shift)
{
    ring->mask = ((embed_u64)1u << shift) - 1u;
    ring->slots = slots;
}

/**
 * @brief The slot index of the ticket of a stamp.
 *
 * @param[in] ring  The ring [BORROWS].
 * @param[in] stamp The stamp.
 * @return          The low shift bits of the ticket.
 */
EMBED_INLINE embed_u64 embed_ticket_ring_place(const EmbedTicketRing *ring, embed_u64 stamp)
{
    return stamp & EMBED_TICKET_MASK & ring->mask;
}

/**
 * @brief The slot of a stamp.
 *
 * @param[in] ring  The ring [BORROWS].
 * @param[in] stamp The stamp.
 * @return          The slot at the index of the ticket [BORROWS].
 */
EMBED_INLINE EmbedTicketRingSlot *embed_ticket_ring_slot(const EmbedTicketRing *ring, embed_u64 stamp)
{
    return &ring->slots[embed_ticket_ring_place(ring, stamp)];
}

/**
 * @brief The expected value of a transition: the input stamp with the flag bits of the transitions before it.
 *
 * @param[in] stamp The input stamp, as the caller holds it.
 * @param[in] flags The flag bits of the transitions before this one.
 * @return          The expected value.
 * @note An input stamp with any flag bit set is invalid. The expected value then has the reserved bit set, and no
 *       slot word has it, so the compare fails. An input stamp with a corrupted ticket or epoch expects a word of
 *       another stamp, so the compare fails too.
 */
EMBED_INLINE embed_u64 embed_ticket_ring_expected(embed_u64 stamp, embed_u64 flags)
{
    // Explicit cast converts the invalid-input condition to 1 or 0, for the shift to the reserved bit
    const embed_u64 invalid = (embed_u64)((stamp & EMBED_TICKET_FLAG_MASK) != 0u);

    return stamp | flags | (invalid << 47u);
}

/**
 * @brief One transition of a slot: a compare-and-swap from the expected value to the new value.
 *
 * @param[in,out] slot     The slot [BORROWS].
 * @param[in]     expected The expected value.
 * @param[in]     value    The new value: the expected value with the flag bit of the transition.
 * @param[out]    seen     The value found [BORROWS]: the expected value on success, the current value on failure.
 * @return                 EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_transition(EmbedTicketRingSlot *slot, embed_u64 expected, embed_u64 value,
                                                     embed_u64 *seen)
{
    embed_u64 found = expected;
    const embed_bool success = embed_atomic_u64_swap_if(&slot->stamp, &found, value);

    *seen = found;
    return success;
}

/**
 * @brief The grant transition: the stamp with the granted bit, from a zero word or from a released word.
 *
 * @param[in,out] ring  The ring [BORROWS].
 * @param[in]     stamp The input stamp, flag bits clear.
 * @param[out]    seen  The value found [BORROWS].
 * @return              EMBED_TRUE on success.
 * @note The expected value is the current value where it has the released bit, and zero where it does not. A
 *       word in any other state fails the compare. The next grant at an index reuses the released word.
 * @note An input stamp with a flag bit set adds the reserved bit to the expected value, so the compare fails.
 */
EMBED_INLINE embed_bool embed_ticket_ring_grant(EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *seen)
{
    EmbedTicketRingSlot *const slot = embed_ticket_ring_slot(ring, stamp);
    const embed_u64 current = embed_atomic_u64_load(&slot->stamp);
    // Explicit cast converts the released bit to an all-ones or all-zeros mask over the current value
    const embed_u64 released = (embed_u64)0u - ((current & EMBED_TICKET_RELEASED) >> 36u);
    const embed_u64 invalid = embed_ticket_ring_expected(stamp, 0u) & EMBED_TICKET_RESERVED;

    return embed_ticket_ring_transition(slot, (current & released) | invalid, stamp | EMBED_TICKET_GRANTED, seen);
}

/**
 * @brief The push transition: the job pointer stored, then the pushed bit added to the granted word.
 *
 * @param[in,out] ring  The ring [BORROWS].
 * @param[in]     stamp The input stamp of a granted slot.
 * @param[in]     job   The job [BORROWS].
 * @param[out]    seen  The value found [BORROWS].
 * @return              EMBED_TRUE on success.
 * @note The store of the job pointer precedes the compare-and-swap, which has release semantics. A pop that succeeds
 *       after it has acquire semantics, so the pop reads the job pointer.
 */
EMBED_INLINE embed_bool embed_ticket_ring_push(EmbedTicketRing *ring, embed_u64 stamp, void *job, embed_u64 *seen)
{
    EmbedTicketRingSlot *const slot = embed_ticket_ring_slot(ring, stamp);
    const embed_u64 expected = embed_ticket_ring_expected(stamp, EMBED_TICKET_GRANTED);

    slot->job = job;
    return embed_ticket_ring_transition(slot, expected, expected | EMBED_TICKET_PUSHED, seen);
}

/**
 * @brief The pop transition: the popped bit and the lane index added to the pushed word, and the job pointer read.
 *
 * @param[in,out] ring  The ring [BORROWS].
 * @param[in]     stamp The input stamp of a pushed slot.
 * @param[in]     lane  The lane index of the pass of the caller, as embed_turnstile_enter returned it.
 * @param[out]    job   The job on success [BORROWS], and NULL on failure.
 * @param[out]    seen  The value found [BORROWS].
 * @return              EMBED_TRUE on success.
 * @note Two pops of one stamp contend here. One succeeds, and the other finds the popped bit in seen.
 */
EMBED_INLINE embed_bool embed_ticket_ring_pop(EmbedTicketRing *ring, embed_u64 stamp, unsigned int lane, void **job,
                                              embed_u64 *seen)
{
    EmbedTicketRingSlot *const slot = embed_ticket_ring_slot(ring, stamp);
    const embed_u64 expected = embed_ticket_ring_expected(stamp, EMBED_TICKET_GRANTED | EMBED_TICKET_PUSHED);
    const embed_u64 value = expected | EMBED_TICKET_POPPED | ((embed_u64)lane << EMBED_TICKET_LANE_SHIFT);
    const embed_bool success = embed_ticket_ring_transition(slot, expected, value, seen);

    *job = success ? slot->job : NULL;
    return success;
}

/**
 * @brief The replay transition: the replayed bit added to the popped word, and the job pointer read again.
 *
 * @param[in,out] ring  The ring [BORROWS].
 * @param[in]     stamp The input stamp of a popped slot.
 * @param[out]    job   The job on success [BORROWS], and NULL on failure.
 * @param[out]    seen  The value found [BORROWS].
 * @return              EMBED_TRUE on success.
 * @note A rewind of k tickets is a replay at the ticket k back. The expected value is the current value where the
 *       current value is the popped word of the stamp, lane bits aside. Any other current value fails the compare.
 */
EMBED_INLINE embed_bool embed_ticket_ring_replay(EmbedTicketRing *ring, embed_u64 stamp, void **job, embed_u64 *seen)
{
    EmbedTicketRingSlot *const slot = embed_ticket_ring_slot(ring, stamp);
    const embed_u64 current = embed_atomic_u64_load(&slot->stamp);
    const embed_u64 popped =
        embed_ticket_ring_expected(stamp, EMBED_TICKET_GRANTED | EMBED_TICKET_PUSHED | EMBED_TICKET_POPPED);
    // Explicit cast converts the state test to an all-ones or all-zeros mask over the current value
    const embed_u64 valid = (embed_u64)0u - (embed_u64)((current & ~EMBED_TICKET_LANE_MASK) == popped);
    const embed_bool success =
        embed_ticket_ring_transition(slot, current & valid, current | EMBED_TICKET_REPLAYED, seen);

    *job = success ? slot->job : NULL;
    return success;
}

/**
 * @brief The release transition: the released bit added to the popped word, replayed or not.
 *
 * @param[in,out] ring  The ring [BORROWS].
 * @param[in]     stamp The input stamp of a popped slot.
 * @param[out]    seen  The value found [BORROWS].
 * @return              EMBED_TRUE on success.
 * @note The writer releases after its drain, so no pass that started before the release reads the slot after it.
 */
EMBED_INLINE embed_bool embed_ticket_ring_release(EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *seen)
{
    EmbedTicketRingSlot *const slot = embed_ticket_ring_slot(ring, stamp);
    const embed_u64 current = embed_atomic_u64_load(&slot->stamp);
    const embed_u64 popped =
        embed_ticket_ring_expected(stamp, EMBED_TICKET_GRANTED | EMBED_TICKET_PUSHED | EMBED_TICKET_POPPED);
    // Explicit cast converts the state test, lane and replayed bits aside, to an all-ones or all-zeros mask
    const embed_u64 valid =
        (embed_u64)0u - (embed_u64)((current & ~(EMBED_TICKET_LANE_MASK | EMBED_TICKET_REPLAYED)) == popped);

    return embed_ticket_ring_transition(slot, current & valid, current | EMBED_TICKET_RELEASED, seen);
}

/**
 * @brief The grant transition inside a pass: enter at address, grant, leave.
 *
 * @param[in,out] set     The turnstiles over the ring [BORROWS].
 * @param[in]     address The address of the worker, as embed_turnstile_enter takes it.
 * @param[in,out] ring    The ring [BORROWS].
 * @param[in]     stamp   The input stamp, flag bits clear.
 * @param[out]    seen    The value found [BORROWS].
 * @return                EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_granted(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                  EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *seen)
{
    const unsigned int lane = embed_turnstile_enter(set, address);
    const embed_bool success = embed_ticket_ring_grant(ring, stamp, seen);

    embed_turnstile_leave(set, lane);
    return success;
}

/**
 * @brief The push transition inside a pass: enter at address, push, leave.
 *
 * @param[in,out] set     The turnstiles over the ring [BORROWS].
 * @param[in]     address The address of the worker, as embed_turnstile_enter takes it.
 * @param[in,out] ring    The ring [BORROWS].
 * @param[in]     stamp   The input stamp of a granted slot.
 * @param[in]     job     The job [BORROWS].
 * @param[out]    seen    The value found [BORROWS].
 * @return                EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_pushed(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                 EmbedTicketRing *ring, embed_u64 stamp, void *job, embed_u64 *seen)
{
    const unsigned int lane = embed_turnstile_enter(set, address);
    const embed_bool success = embed_ticket_ring_push(ring, stamp, job, seen);

    embed_turnstile_leave(set, lane);
    return success;
}

/**
 * @brief The pop transition inside a pass: enter at address, pop with the lane index of that pass, leave.
 *
 * @param[in,out] set     The turnstiles over the ring [BORROWS].
 * @param[in]     address The address of the worker, as embed_turnstile_enter takes it.
 * @param[in,out] ring    The ring [BORROWS].
 * @param[in]     stamp   The input stamp of a pushed slot.
 * @param[out]    job     The job on success [BORROWS], and NULL on failure.
 * @param[out]    seen    The value found [BORROWS].
 * @return                EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_popped(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                 EmbedTicketRing *ring, embed_u64 stamp, void **job, embed_u64 *seen)
{
    const unsigned int lane = embed_turnstile_enter(set, address);
    const embed_bool success = embed_ticket_ring_pop(ring, stamp, lane, job, seen);

    embed_turnstile_leave(set, lane);
    return success;
}

/**
 * @brief The replay transition inside a pass: enter at address, replay, leave.
 *
 * @param[in,out] set     The turnstiles over the ring [BORROWS].
 * @param[in]     address The address of the worker, as embed_turnstile_enter takes it.
 * @param[in,out] ring    The ring [BORROWS].
 * @param[in]     stamp   The input stamp of a popped slot.
 * @param[out]    job     The job on success [BORROWS], and NULL on failure.
 * @param[out]    seen    The value found [BORROWS].
 * @return                EMBED_TRUE on success.
 */
EMBED_INLINE embed_bool embed_ticket_ring_replayed(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                   EmbedTicketRing *ring, embed_u64 stamp, void **job,
                                                   embed_u64 *seen)
{
    const unsigned int lane = embed_turnstile_enter(set, address);
    const embed_bool success = embed_ticket_ring_replay(ring, stamp, job, seen);

    embed_turnstile_leave(set, lane);
    return success;
}

/**
 * @brief The release transition after a drain: drain set, then release the stamp.
 *
 * @param[in,out] set   The turnstiles over the ring [BORROWS].
 * @param[in,out] ring  The ring [BORROWS].
 * @param[in]     stamp The input stamp of a popped slot.
 * @param[out]    seen  The value found [BORROWS].
 * @return              EMBED_TRUE on success.
 * @note The writer calls this after it observed the popped bit and replaced what the slot referenced. The pass of
 *       that pop started before the drain, so the drain waits for it, and the release follows.
 */
EMBED_INLINE embed_bool embed_ticket_ring_released(EmbedTurnstiles *set, EmbedTicketRing *ring, embed_u64 stamp,
                                                   embed_u64 *seen)
{
    embed_turnstiles_drain(set);
    return embed_ticket_ring_release(ring, stamp, seen);
}

EMBED_END_DECLS

#endif
