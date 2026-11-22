/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file backoff_prove.c
 * @brief The backoff of Backoff.lean, on embed_turnstile_backoff_begin and embed_turnstile_backoff_step. One step
 *        runs at any two reads of the clock, at each exponent up to the max, on a word of any value. Inside the first
 *        unit, the step makes no call. From one unit on, it sets the sleeper bit and waits once with the limit of
 *        its exponent, and the exponent goes up to the max.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note A harness for CBMC, which harness.py prove runs on each word width of its bound.
 */
#include "prove.h"

#include "embed_turnstile.h"

/** @brief The value of the next read of the clock, which the harness sets. */
static embed_word s_clock_now;
/** @brief The calls of the host's wait, and the parameters of the last one. */
static unsigned int s_wait_calls;
static embed_atomic_word_value s_wait_old_value;
static embed_word s_wait_nanoseconds;

int embed_word_wait(const EmbedWordWaitCfg *args)
{
    s_wait_calls += 1u;
    s_wait_old_value = args->old_value;
    s_wait_nanoseconds = args->nanoseconds;
    return 0;
}

void embed_word_wake(const EmbedWordWakeCfg *args)
{
    (void)args;
}

embed_word embed_clock_ns(void)
{
    return s_clock_now;
}

embed_u32 embed_thread_place(void)
{
    return 0u;
}

/** @brief The word that the step sleeps on. */
static embed_atomic_word s_word;

int main(void)
{
    const embed_word start_ns = nondet_word();
    const embed_word elapsed_ns = nondet_word();
    const unsigned int exponent = nondet_uint();
    const embed_atomic_word_value start_word = (embed_atomic_word_value)nondet_word();
    const embed_atomic_word_value old_value = (embed_atomic_word_value)nondet_word();
    const embed_bool sleep_allowed = (embed_bool)nondet_bool();
    EmbedTurnstileBackoff backoff;

    PROVE_ASSUME(exponent <= EMBED_TURNSTILE_EXPONENT_MAX);
    EMBED_CALL(embed_atomic_word_table.store, EmbedAtomicWordStoreCfg, .word = &s_word, .value = start_word);
    s_clock_now = start_ns;
    EMBED_CALL(embed_turnstile_table.backoff_begin, EmbedTurnstileBackoffBeginCfg, .backoff = &backoff);
    PROVE_ASSERT((backoff.start_ns == start_ns) && (backoff.exponent == 0u),
                 "the begin reads the clock, and the exponent starts at 0");
    backoff.exponent = exponent;
    // elapsed_rolls: the clock reads the start plus the elapsed time, modulo the word
    // Explicit cast takes the sum modulo the word, which is the contract of the clock
    s_clock_now = (embed_word)(start_ns + elapsed_ns);
    EMBED_CALL(embed_turnstile_table.backoff_step, EmbedTurnstileBackoffStepCfg, .word = &s_word,
               .old_value = old_value, .sleeper_bit = EMBED_TURNSTILE_SLEEPER_BIT, .backoff = &backoff,
               .sleep_allowed = sleep_allowed);
    // Explicit cast takes the unit as the word, and the assertion of the header holds the unit shift below the width
    const embed_word unit_ns = (embed_word)1u << EMBED_TURNSTILE_UNIT_SHIFT;
    const embed_atomic_word_value end_word =
        EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_word);

    if (!sleep_allowed || (elapsed_ns < unit_ns))
    {
        // no_call_in_unit: a step inside the first unit, or with no sleep allowed, is a pause and no call
        PROVE_ASSERT(s_wait_calls == 0u, "no_call_in_unit: the step makes no call");
        PROVE_ASSERT(end_word == start_word, "no_call_in_unit: the step changes no bit of the word");
        PROVE_ASSERT(backoff.exponent == exponent, "no_call_in_unit: the exponent stays");
    }
    else
    {
        // Explicit cast takes the limit as the word, and EMBED_TURNSTILE_EXPONENT_MAX holds it below the width
        const embed_word limit_ns = (embed_word)1u << (EMBED_TURNSTILE_UNIT_SHIFT + exponent);

        // wait_from_unit: from one unit on, the step sets the sleeper bit and waits once with the limit
        PROVE_ASSERT(s_wait_calls == 1u, "wait_from_unit: the step waits once");
        PROVE_ASSERT(end_word == (start_word | EMBED_TURNSTILE_SLEEPER_BIT), "the step sets the sleeper bit");
        PROVE_ASSERT(s_wait_old_value == (old_value | EMBED_TURNSTILE_SLEEPER_BIT),
                     "the wait expects the value with the sleeper bit");
        PROVE_ASSERT(s_wait_nanoseconds == limit_ns, "wait_from_unit: the wait has the limit of the exponent");
        PROVE_ASSERT((exponent != 0u) || (s_wait_nanoseconds == unit_ns), "first_wait: the first wait is one unit");
        PROVE_ASSERT(s_wait_nanoseconds < EMBED_WAIT_INFINITE, "limit_below_infinite: no limit reads as no end");
        // Explicit cast widens the limit to 64 bits, which hold 2^30 on each word width
        PROVE_ASSERT((embed_u64)s_wait_nanoseconds <= ((embed_u64)1u << 30u), "limit_fits: each limit fits 2^30 ns");
        // update_is_next and next_at_max: one more, up to the max
        PROVE_ASSERT(backoff.exponent ==
                         ((exponent < EMBED_TURNSTILE_EXPONENT_MAX) ? (exponent + 1u) : EMBED_TURNSTILE_EXPONENT_MAX),
                     "update_is_next: the exponent goes up by one, to the max");
    }
    PROVE_DONE();
    return 0;
}
