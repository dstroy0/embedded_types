/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file ticket_flags_prove.c
 * @brief The flag matrix of Flags.lean, on the C transitions. One transition runs on a slot word of any value with
 *        the reserved bit clear, with an input stamp of any value. It lands exactly where the slot is in the state
 *        before it, and then it retains the fields and adds its own bits. A repeat fails, and a transition that
 *        fails changes no bit.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note A harness for CBMC, which harness.py prove runs on each word width of its bound. The check of the job
 *       pointer of job_flip_changes_check and job_double_flip runs on a pointer of any value.
 */
#include "prove.h"

#include "embed_ticket_ring.h"

/** @brief The transitions of a slot, in the order of the state machine. */
enum
{
    PROVE_GRANT,
    PROVE_PUSH,
    PROVE_POP,
    PROVE_REPLAY,
    PROVE_RELEASE,
    PROVE_TRANSITION_COUNT
};

/** @brief One slot, and the ring of one slot over it. */
static EmbedTicketRingSlot s_slots[1];
static EmbedTicketRing s_ring;

/** @brief The answer of one transition, and the job and the found stamp that it gave. */
typedef struct
{
    embed_bool landed;      /**< The answer of the transition. */
    void *job;              /**< The job that a pop or a replay gave. */
    embed_word found_stamp; /**< The stamp that the compare found. */
} ProveAnswer;

/** @brief One transition of the input stamp, with the job of a push and the lane of a pop. */
static ProveAnswer prove_transition(unsigned int transition, embed_word input, void *push_job, unsigned int lane)
{
    ProveAnswer answer = {EMBED_FALSE, NULL, 0u};

    switch (transition)
    {
    case PROVE_GRANT:
        answer.landed = EMBED_CALL(embed_ticket_ring_table.grant, EmbedTicketRingGrantCfg, .ring = &s_ring,
                                   .stamp = input, .found_stamp = &answer.found_stamp);
        break;
    case PROVE_PUSH:
        answer.landed = EMBED_CALL(embed_ticket_ring_table.push, EmbedTicketRingPushCfg, .ring = &s_ring,
                                   .stamp = input, .job = push_job, .found_stamp = &answer.found_stamp);
        break;
    case PROVE_POP:
        answer.landed = EMBED_CALL(embed_ticket_ring_table.pop, EmbedTicketRingPopCfg, .ring = &s_ring, .stamp = input,
                                   .lane_place = lane, .job = &answer.job, .found_stamp = &answer.found_stamp);
        break;
    case PROVE_REPLAY:
        answer.landed = EMBED_CALL(embed_ticket_ring_table.replay, EmbedTicketRingReplayCfg, .ring = &s_ring,
                                   .stamp = input, .job = &answer.job, .found_stamp = &answer.found_stamp);
        break;
    default:
        answer.landed = EMBED_CALL(embed_ticket_ring_table.release, EmbedTicketRingReleaseCfg, .ring = &s_ring,
                                   .stamp = input, .found_stamp = &answer.found_stamp);
        break;
    }
    return answer;
}

/** @brief The check of a job pointer, from its bits. */
static embed_word prove_check(uintptr_t bits)
{
    // Explicit cast carries the bits as the pointer that the check takes. No step reads through it.
    return EMBED_CALL(embed_ticket_ring_table.ticket_job_check, EmbedTicketJobCheckCfg, .job = (const void *)bits);
}

int main(void)
{
    const embed_word start_word = nondet_word();
    const embed_word input = nondet_word();
    const unsigned int transition = nondet_uint();
    const unsigned int lane = nondet_uint();
    // Explicit casts carry any bits as the job pointers. No step reads through them.
    void *const push_job = (void *)nondet_uptr();
    void *const slot_job = (void *)nondet_uptr();

    // states_reserved_clear: no state word has the reserved bit, so no slot word holds it
    PROVE_ASSUME((start_word & EMBED_TICKET_RESERVED_BIT) == 0u);
    PROVE_ASSUME(transition < PROVE_TRANSITION_COUNT);
    PROVE_ASSUME(lane < EMBED_TURNSTILE_LANES);
    EMBED_CALL(embed_ticket_ring_table.begin, EmbedTicketRingBeginCfg, .ring = &s_ring, .slots = s_slots, .shift = 0u);
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_slots[0].stamp, .value = start_word);
    s_slots[0].job = slot_job;

    // invalid_input_refuses and valid_input_expects: an input with a flag bit set expects the reserved bit
    // Explicit cast converts the test of the flag field, an int of 0 or 1, to the project boolean
    const embed_bool valid_input = (embed_bool)((input & EMBED_TICKET_FLAG_MASK) == 0u);
    const embed_word fields = start_word & ~EMBED_TICKET_FLAG_MASK;
    const embed_word lane_bits = (embed_word)lane << EMBED_TICKET_LANE_SHIFT;
    const embed_word granted = input | EMBED_TICKET_GRANTED_BIT;
    const embed_word pushed = granted | EMBED_TICKET_PUSHED_BIT;
    const embed_word popped = pushed | EMBED_TICKET_POPPED_BIT;
    // Explicit casts carry the job pointer as an integer for the check, and convert the comparison to the boolean
    const embed_bool slot_check_matches =
        (embed_bool)(prove_check((uintptr_t)slot_job) ==
                     ((start_word & EMBED_TICKET_CHECK_MASK) >> EMBED_TICKET_CHECK_SHIFT));
    embed_bool state_matches = EMBED_FALSE;
    embed_word landed_word = 0u;

    // The state before each transition, and the word that the transition leaves. Explicit casts convert each
    // comparison, an int of 0 or 1, to the project boolean.
    switch (transition)
    {
    case PROVE_GRANT:
        state_matches = (embed_bool)((start_word == 0u) || ((start_word & EMBED_TICKET_RELEASED_BIT) != 0u));
        landed_word = granted;
        break;
    case PROVE_PUSH:
        state_matches = (embed_bool)(start_word == granted);
        // Explicit cast carries the job pointer as an integer for the check
        landed_word = pushed | (prove_check((uintptr_t)push_job) << EMBED_TICKET_CHECK_SHIFT);
        break;
    case PROVE_POP:
        state_matches = (embed_bool)((start_word & ~EMBED_TICKET_CHECK_MASK) == pushed);
        landed_word = start_word | EMBED_TICKET_POPPED_BIT | lane_bits;
        break;
    case PROVE_REPLAY:
        state_matches = (embed_bool)((start_word & ~(EMBED_TICKET_LANE_MASK | EMBED_TICKET_CHECK_MASK)) == popped);
        landed_word = start_word | EMBED_TICKET_REPLAYED_BIT;
        break;
    default:
        state_matches = (embed_bool)((start_word & ~(EMBED_TICKET_LANE_MASK | EMBED_TICKET_REPLAYED_BIT |
                                                     EMBED_TICKET_CHECK_MASK)) == popped);
        landed_word = start_word | EMBED_TICKET_RELEASED_BIT;
        break;
    }
    // Explicit cast converts the conjunction, an int of 0 or 1, to the project boolean
    const embed_bool swapped = (embed_bool)(valid_input && state_matches);
    // Explicit cast converts the disjunction, an int of 0 or 1, to the project boolean
    const embed_bool reads_job = (embed_bool)((transition == PROVE_POP) || (transition == PROVE_REPLAY));
    const ProveAnswer answer = prove_transition(transition, input, push_job, lane);
    const embed_word end_word =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_slots[0].stamp);

    // The compare lands exactly where the input is valid and the slot is in the state before the transition
    PROVE_ASSERT(end_word == (swapped ? landed_word : start_word),
                 "set_once and corruption_refuses: the compare lands exactly on the state before it");
    // A pop and a replay answer success exactly where the job pointer passes its check too
    PROVE_ASSERT(answer.landed == (swapped && (!reads_job || slot_check_matches)),
                 "guarded_refuses and job_flip_changes_check: the answer is the swap and the check of the job");
    PROVE_ASSERT((end_word & EMBED_TICKET_RESERVED_BIT) == 0u,
                 "states_reserved_clear: no transition sets the reserved bit");
    if (swapped)
    {
        PROVE_ASSERT(end_word != start_word, "set_once: the word after a transition differs from the word before");
        PROVE_ASSERT((end_word & ~EMBED_TICKET_FLAG_MASK) ==
                         ((transition == PROVE_GRANT) ? (input & ~EMBED_TICKET_FLAG_MASK) : fields),
                     "set_retains_fields: a transition retains the ticket and the epoch");
        if (transition == PROVE_POP)
        {
            PROVE_ASSERT(EMBED_CALL(embed_ticket_ring_table.ticket_lane_of_stamp, EmbedTicketLaneOfStampCfg,
                                    .stamp = end_word) == lane,
                         "lane_recorded: the lane of the pop comes back out of the popped word");
        }
        // A repeat of the transition with the same input fails, and the word stays
        const ProveAnswer repeat = prove_transition(transition, input, push_job, lane);

        PROVE_ASSERT(!repeat.landed, "claim_once: a repeat of the transition fails");
        PROVE_ASSERT(EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_slots[0].stamp) ==
                         end_word,
                     "claim_once: a repeat changes no bit of the word");
    }
    else
    {
        PROVE_ASSERT(answer.found_stamp == start_word, "cas_contract: a failed compare answers the word it found");
    }
    if (answer.landed && reads_job)
    {
        PROVE_ASSERT(answer.job == slot_job, "a pop or a replay that lands reads the job of the slot");
    }
    if (reads_job && !answer.landed)
    {
        PROVE_ASSERT(answer.job == NULL, "a pop that fails gives no job");
    }

    // job_flip_changes_check: a single-bit error at any place of the pointer changes its check
    const uintptr_t job_bits = nondet_uptr();
    const unsigned int flip_place = nondet_uint();
    const unsigned int flip_distance = nondet_uint();
    const unsigned int pointer_bits = 8u * (unsigned int)sizeof(uintptr_t);

    PROVE_ASSUME(flip_place < pointer_bits);
    PROVE_ASSERT(prove_check(job_bits ^ ((uintptr_t)1u << flip_place)) != prove_check(job_bits),
                 "job_flip_changes_check: a single-bit error changes the check");
    // job_double_flip: a double-bit error changes the check exactly where the places are not seven apart
    // The bound on the distance alone holds the sum below the width, with no wrap of the unsigned int
    PROVE_ASSUME((flip_distance != 0u) && (flip_distance < pointer_bits) &&
                 (flip_place + flip_distance < pointer_bits));
    const uintptr_t double_flip = ((uintptr_t)1u << flip_place) | ((uintptr_t)1u << (flip_place + flip_distance));

    PROVE_ASSERT((prove_check(job_bits ^ double_flip) != prove_check(job_bits)) == ((flip_distance % 7u) != 0u),
                 "job_double_flip: a double-bit error changes the check unless the places are seven apart");
    PROVE_DONE();
    return 0;
}
