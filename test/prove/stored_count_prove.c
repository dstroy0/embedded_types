/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file stored_count_prove.c
 * @brief The stored form of a 64-bit count of Widths.lean, on embed_stored_u64_get and embed_stored_u64_set. A get
 *        after a set gives the count that the set wrote, from a stored form of any value.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note A harness for CBMC. harness.py prove runs it on the host form and on the two halves of an embedded target.
 */
#include "prove.h"

int main(void)
{
    embed_stored_u64 stored;
    const embed_u64 count = nondet_u64();

#if EMBED_TARGET == EMBED_TARGET_EMBEDDED
    stored.lower = nondet_u32();
    stored.upper = nondet_u32();
    const embed_u32 old_upper = stored.upper;
#else
    stored = nondet_u64();
#endif
    EMBED_CALL(embed_stored_u64_table.set, EmbedStoredU64SetCfg, .stored = &stored, .count = count);

    // stored_set_get and stored_round_trip: a get after a set gives the count back
    PROVE_ASSERT(EMBED_CALL(embed_stored_u64_table.get, EmbedStoredU64GetCfg, .stored = &stored) == count,
                 "stored_set_get: a get after a set gives the count that the set wrote");
#if EMBED_TARGET == EMBED_TARGET_EMBEDDED
    // stored_set_is_split: the halves are the lower word and the upper word of the count
    // Explicit casts narrow the count to each half, which fits an embed_u32
    PROVE_ASSERT(stored.lower == (embed_u32)count, "stored_set_is_split: the lower half is the lower word");
    PROVE_ASSERT(stored.upper == (embed_u32)(count >> 32), "stored_set_is_split: the upper half is the upper word");
    // stored_upper_kept: a count with the stored upper word keeps that word
    if (old_upper == (embed_u32)(count >> 32))
    {
        PROVE_ASSERT(stored.upper == old_upper, "stored_upper_kept: the upper word stays where it does not change");
    }
#endif
    PROVE_DONE();
    return 0;
}
