/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_ticket_ring.h
 * @brief A ring of job slots that a ticket indexes, written and read inside a pass through a turnstile.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-11
 *
 * The ring has 2^shift slots. A slot holds a stamp and a job. The stamp is one 64-bit word: the epoch in the high 32
 * bits and the ticket in the low 32 bits. The ticket selects the slot: the low shift bits of the ticket are the slot's
 * place. The ticket counts up without end and rolls at 32 bits, and the place rolls with it, because 2^shift divides
 * 2^32.
 *
 * @note The ring has no compare-and-swap and no guard of its own. The turnstile is the guard. The grantor of a ticket
 *       holds the turnstile while it grants, so each ticket has one grant. A push and a pop each run inside a pass:
 *       an entry, one write or one read, and a leave. The grantor also holds the depth of the ring below 2^shift, so
 *       a push never writes a slot whose job a pop has not read.
 * @note A push and a pop have no branch. Each is one shift, one mask and two moves.
 * @note theory/embedded_types/EmbeddedTypes/TicketRing.lean proves the ring. It proves the stamp's fields, the place
 *       of a ticket, and the roll at 2^shift and at 2^32. It proves that the tickets of one window each have a place
 *       of their own. It proves that a pop reads what the push of its ticket wrote.
 * @warning A stamp built from a ticket that no turnstile granted indexes a slot that another ticket owns. The ring
 *          does not detect that. The pop's stamp shows which ticket wrote the slot, which a replay log reads.
 */
#ifndef EMBED_TICKET_RING_H
#define EMBED_TICKET_RING_H

#include "embed_turnstile.h"

EMBED_BEGIN_DECLS

/** @brief The bits of the ticket in a stamp, and where the epoch starts. */
#define EMBED_TICKET_BITS 32u
#define EMBED_TICKET_MASK ((embed_u64)0xffffffffu)
#define EMBED_TICKET_EPOCH_SHIFT 32u

/** @brief The most slots a ring has, as a shift: 2^EMBED_TICKET_RING_SHIFT_MOST slots. */
#define EMBED_TICKET_RING_SHIFT_MOST 31u

/** @brief One slot: the stamp of the push that wrote it, and the job. */
typedef struct
{
    embed_u64 stamp; /**< The stamp of the push, for a replay log. */
    void *job;       /**< The job. */
} EmbedTicketRingSlot;
EMBED_STATIC_ASSERT(sizeof(EmbedTicketRingSlot) == 16u, "a slot is two words");

/** @brief A ring: its mask, 2^shift - 1, on a cache line of its own, and its slots. */
typedef struct
{
    EMBED_ALIGN(64) embed_u64 mask;  /**< 2^shift - 1. */
    EmbedTicketRingSlot *slots;      /**< 2^shift slots. */
} EmbedTicketRing;

/**
 * @brief The stamp of epoch and ticket.
 *
 * @param[in] epoch  The high 32 bits.
 * @param[in] ticket The low 32 bits.
 * @return           The stamp.
 */
EMBED_INLINE embed_u64 embed_ticket_stamp(embed_u32 epoch, embed_u32 ticket)
{
    return ((embed_u64)epoch << EMBED_TICKET_EPOCH_SHIFT) | (embed_u64)ticket;
}

/**
 * @brief The ticket of a stamp.
 *
 * @param[in] stamp The stamp.
 * @return          Its low 32 bits.
 */
EMBED_INLINE embed_u32 embed_ticket_of_stamp(embed_u64 stamp)
{
    // Explicit cast takes the low 32 bits, which the mask leaves
    return (embed_u32)(stamp & EMBED_TICKET_MASK);
}

/**
 * @brief The epoch of a stamp.
 *
 * @param[in] stamp The stamp.
 * @return          Its high 32 bits.
 */
EMBED_INLINE embed_u32 embed_epoch_of_stamp(embed_u64 stamp)
{
    // Explicit cast takes the high 32 bits, which the shift brings down
    return (embed_u32)(stamp >> EMBED_TICKET_EPOCH_SHIFT);
}

/**
 * @brief The ring begun over slots, 2^shift of them.
 *
 * @param[out] ring  The ring [BORROWS].
 * @param[in]  slots The slots [BORROWS], 2^shift of them, which the caller owns.
 * @param[in]  shift The count of slots as a power of two, at most EMBED_TICKET_RING_SHIFT_MOST.
 * @note The count is a power of two by construction, so the mask wraps the ticket with no division.
 */
EMBED_INLINE void embed_ticket_ring_begin(EmbedTicketRing *ring, EmbedTicketRingSlot *slots, unsigned int shift)
{
    ring->mask = ((embed_u64)1u << shift) - 1u;
    ring->slots = slots;
}

/**
 * @brief The place of a stamp's ticket in a ring.
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
 * @brief A push: the stamp and the job written at the ticket's place.
 *
 * @param[in,out] ring  The ring [BORROWS].
 * @param[in]     stamp The stamp of a ticket the turnstile granted.
 * @param[in]     job   The job [BORROWS].
 * @note The caller holds a pass through the turnstile over the ring. The push has no branch.
 */
EMBED_INLINE void embed_ticket_ring_push(EmbedTicketRing *ring, embed_u64 stamp, void *job)
{
    EmbedTicketRingSlot *const slot = &ring->slots[embed_ticket_ring_place(ring, stamp)];

    slot->stamp = stamp;
    slot->job = job;
}

/**
 * @brief A pop: the job at the ticket's place, and the stamp of the push that wrote it.
 *
 * @param[in]  ring      The ring [BORROWS].
 * @param[in]  stamp     The stamp of a ticket the turnstile granted.
 * @param[out] stamp_out The stamp of the push [BORROWS].
 * @return               The job.
 * @note The caller holds a pass through the turnstile over the ring. The pop has no branch.
 */
EMBED_INLINE void *embed_ticket_ring_pop(const EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *stamp_out)
{
    const EmbedTicketRingSlot *const slot = &ring->slots[embed_ticket_ring_place(ring, stamp)];

    *stamp_out = slot->stamp;
    return slot->job;
}

/**
 * @brief A push inside a pass: an entry at address, the push, and the leave.
 *
 * @param[in,out] set     The turnstiles over the ring [BORROWS].
 * @param[in]     address The worker's address, as embed_turnstile_enter takes it.
 * @param[in,out] ring    The ring [BORROWS].
 * @param[in]     stamp   The stamp of a ticket the turnstile granted.
 * @param[in]     job     The job [BORROWS].
 */
EMBED_INLINE void embed_ticket_ring_pushed(EmbedTurnstiles *set, embed_atomic_uptr_value address, EmbedTicketRing *ring,
                                           embed_u64 stamp, void *job)
{
    const unsigned int lane = embed_turnstile_enter(set, address);

    embed_ticket_ring_push(ring, stamp, job);
    embed_turnstile_leave(set, lane);
}

/**
 * @brief A pop inside a pass: an entry at address, the pop, and the leave.
 *
 * @param[in,out] set       The turnstiles over the ring [BORROWS].
 * @param[in]     address   The worker's address, as embed_turnstile_enter takes it.
 * @param[in]     ring      The ring [BORROWS].
 * @param[in]     stamp     The stamp of a ticket the turnstile granted.
 * @param[out]    stamp_out The stamp of the push [BORROWS].
 * @return                  The job.
 */
EMBED_INLINE void *embed_ticket_ring_popped(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                            const EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *stamp_out)
{
    const unsigned int lane = embed_turnstile_enter(set, address);
    void *const job = embed_ticket_ring_pop(ring, stamp, stamp_out);

    embed_turnstile_leave(set, lane);
    return job;
}

EMBED_END_DECLS

#endif
