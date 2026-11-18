/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_clock.c
 * @brief Holds the clock selection to the answers of the host build: one family, the host's name, no counter,
 *        the register width, the caller's clock, and the scale of its ticks.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-18
 *
 * @note The suite states a frequency of 48 MHz and the caller's clock before the header. The header then
 *       takes no default and gives no warning.
 */
#define EMBED_CLOCK_HZ 48000000u
#define EMBED_CLOCK_SOURCE EMBED_CLOCK_CALLER

#include "embed_clock.h"

#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_the_build_is_one_family_and_a_host(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, EMBED_ARCH_ARM + EMBED_ARCH_RISCV + EMBED_ARCH_XTENSA + EMBED_ARCH_HOST,
                                  "one family");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, EMBED_ARCH_HOST, "the suites run on a host");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("this host", EMBED_ARCH_NAME, "the host's name");
}

void test_a_host_has_no_counter_to_pin(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, EMBED_ARCH_CYCLE_COUNTER, "a host build takes the caller's clock");
    TEST_ASSERT_EQUAL_INT_MESSAGE(EMBED_CLOCK_CALLER, EMBED_CLOCK_SOURCE, "the source the suite stated");
}

void test_the_register_is_the_pointer_width_and_holds_the_word(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(sizeof(void *) * 8u, EMBED_REGISTER_BITS, "the width of a pointer");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_WORD_BITS <= EMBED_REGISTER_BITS, "the word fits the register");
}

void test_the_ticks_scale_to_whole_microseconds_rounded_down(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(48u, EMBED_CLOCK_TICKS_PER_MICRO, "48 ticks in a microsecond");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(10u, (unsigned int)EMBED_CLOCK_MICROS((embed_word)480u), "480 ticks");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(9u, (unsigned int)EMBED_CLOCK_MICROS((embed_word)479u), "rounded down");
}

void test_a_stated_clock_takes_no_default(void)
{
#if defined(EMBED_UNSET_CLOCK_HZ) || defined(EMBED_UNSET_CLOCK_SOURCE) || defined(EMBED_UNSET_CLOCK_CORE)
    TEST_FAIL_MESSAGE("a value the suite stated took a default");
#else
    TEST_PASS();
#endif
}
