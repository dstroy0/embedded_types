/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file ticket_stamp_prove.c
 * @brief The stamp and the place of TicketRing.lean, on the C functions, for each epoch, ticket and shift.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note A harness for CBMC, which harness.py prove runs on each word width of its bound.
 */
#include "prove.h"

#include "embed_ticket_ring.h"

/** @brief The slot array of the ring. The place reads only the mask, so one slot serves each shift. */
static EmbedTicketRingSlot s_slots[1];

int main(void)
{
    const embed_word epoch = nondet_word();
    const embed_word ticket = nondet_word();
    const embed_word other_epoch = nondet_word();
    const embed_word other_ticket = nondet_word();
    const embed_word stamp =
        EMBED_CALL(embed_ticket_ring_table.ticket_stamp, EmbedTicketStampCfg, .epoch = epoch, .ticket = ticket);
    const embed_word other_stamp = EMBED_CALL(embed_ticket_ring_table.ticket_stamp, EmbedTicketStampCfg,
                                              .epoch = other_epoch, .ticket = other_ticket);

    // stamp_ticket and stamp_epoch: each field comes back out of the stamp
    PROVE_ASSERT(EMBED_CALL(embed_ticket_ring_table.ticket_of_stamp, EmbedTicketOfStampCfg, .stamp = stamp) ==
                     (ticket & EMBED_TICKET_MASK),
                 "stamp_ticket: the ticket comes back out of the stamp");
    PROVE_ASSERT(EMBED_CALL(embed_ticket_ring_table.epoch_of_stamp, EmbedEpochOfStampCfg, .stamp = stamp) ==
                     (epoch & EMBED_TICKET_EPOCH_MASK),
                 "stamp_epoch: the epoch comes back out of the stamp");
    // stamp_flags_clear: a wide epoch or ticket sets no flag bit
    PROVE_ASSERT(
        EMBED_CALL(embed_ticket_ring_table.ticket_flags_of_stamp, EmbedTicketFlagsOfStampCfg, .stamp = stamp) == 0u,
        "stamp_flags_clear: the flag field of a stamp is clear");
    PROVE_ASSERT(EMBED_CALL(embed_ticket_ring_table.ticket_fields_of_stamp, EmbedTicketFieldsOfStampCfg,
                            .stamp = stamp) == stamp,
                 "stamp_flags_clear: a stamp is its own fields");
    // stamp_injective: two stamps that agree have the same fields
    if (stamp == other_stamp)
    {
        PROVE_ASSERT((epoch & EMBED_TICKET_EPOCH_MASK) == (other_epoch & EMBED_TICKET_EPOCH_MASK),
                     "stamp_injective: two equal stamps have one epoch");
        PROVE_ASSERT((ticket & EMBED_TICKET_MASK) == (other_ticket & EMBED_TICKET_MASK),
                     "stamp_injective: two equal stamps have one ticket");
    }

    // A ring of 2^shift slots, for each shift up to EMBED_TICKET_RING_SHIFT_MAX
    const unsigned int shift = nondet_uint();
    static EmbedTicketRing ring;

    PROVE_ASSUME(shift <= EMBED_TICKET_RING_SHIFT_MAX);
    EMBED_CALL(embed_ticket_ring_table.begin, EmbedTicketRingBeginCfg, .ring = &ring, .slots = s_slots, .shift = shift);
    // Explicit cast widens the one to the word before the shift, and the assumption holds the shift below the width
    const embed_word slot_count = (embed_word)1u << shift;
    const embed_word place =
        EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &ring, .stamp = stamp);

    // place_lt and place_mask: the place is the ticket modulo the slot count
    PROVE_ASSERT(place < slot_count, "place_lt: the place is below the slot count");
    PROVE_ASSERT(place == ((ticket & EMBED_TICKET_MASK) % slot_count), "place_mask: the mask is the modulus");
    // place_wrap and place_roll: a ticket one ring later, and a ticket past its width, take the same place
    const embed_word later_stamp = EMBED_CALL(embed_ticket_ring_table.ticket_stamp, EmbedTicketStampCfg, .epoch = epoch,
                                              .ticket = ticket + slot_count);

    PROVE_ASSERT(EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &ring,
                            .stamp = later_stamp) == place,
                 "place_wrap: a ticket one ring later takes the same place");
    // window_distinct: a ticket less than one ring later takes a place of its own
    const embed_word distance = nondet_word();

    PROVE_ASSUME((distance != 0u) && (distance < slot_count));
    const embed_word near_stamp = EMBED_CALL(embed_ticket_ring_table.ticket_stamp, EmbedTicketStampCfg, .epoch = epoch,
                                             .ticket = ticket + distance);

    PROVE_ASSERT(
        EMBED_CALL(embed_ticket_ring_table.place, EmbedTicketRingPlaceCfg, .ring = &ring, .stamp = near_stamp) != place,
        "window_distinct: a ticket less than one ring later takes another place");
    PROVE_DONE();
    return 0;
}
