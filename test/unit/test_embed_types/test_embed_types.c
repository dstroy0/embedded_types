/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_types.c
 * @brief Exercises the widths, the boolean, the derived word and index, and the raw-access word.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note Every expectation here comes from a literal or from the compiler, never from the header
 *       under test. A width checked against the macro that produced it would agree with itself and
 *       report green for any value.
 * @note The eight static assertions inside embed_types.h are proved by this file compiling at all.
 *       What runs below is only what a static assertion cannot reach.
 * @note Unity's generator collects the file-scope cases and writes unity_runner.c beside this file.
 *       It reads the case names out of the source text, so no case below sits inside a #if - a
 *       conditional goes inside a case, where every arm still compiles into one definition.
 */
#include <stdint.h>
#include <string.h>

#include "embed_types.h"

#include "unity.h"

/**
 * @brief Runs before each case, and has nothing to prepare.
 *
 * @note Unity calls this whether or not it does anything, and unity_internals.h declares it. Every
 *       case here reads types and literals rather than shared state, so there is nothing to reset.
 */
void setUp(void)
{
}

/** @brief Runs after each case, and has nothing to release. */
void tearDown(void)
{
}

/**
 * @brief Checks each exact-width alias against a byte count written as a literal.
 *
 * @note The literals are the independent side. Deriving them from the alias would restate the
 *       typedef rather than check it.
 */
void test_the_widths_are_the_widths_they_are_named_for(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, sizeof(embed_u8), "embed_u8 is one byte");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, sizeof(embed_u16), "embed_u16 is two bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, sizeof(embed_u32), "embed_u32 is four bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, sizeof(embed_u64), "embed_u64 is eight bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, sizeof(embed_i8), "embed_i8 is one byte");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(2u, sizeof(embed_i16), "embed_i16 is two bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, sizeof(embed_i32), "embed_i32 is four bytes");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(8u, sizeof(embed_i64), "embed_i64 is eight bytes");
}

/**
 * @brief Checks that each unsigned alias is unsigned and each signed one holds a negative value.
 *
 * @note A typedef pointing at the wrong signedness has the right size, so the size checks above
 *       cannot see it. Round-tripping a negative through the signed type is what does.
 */
void test_the_signed_aliases_carry_a_sign_and_the_unsigned_ones_do_not(void)
{
    TEST_ASSERT_TRUE_MESSAGE((embed_u8)-1 > 0, "embed_u8 is unsigned");
    TEST_ASSERT_TRUE_MESSAGE((embed_u16)-1 > 0, "embed_u16 is unsigned");
    TEST_ASSERT_TRUE_MESSAGE((embed_u32)-1 > 0, "embed_u32 is unsigned");
    TEST_ASSERT_TRUE_MESSAGE((embed_u64)-1 > 0, "embed_u64 is unsigned");
    TEST_ASSERT_TRUE_MESSAGE((embed_i8)-1 < 0, "embed_i8 is signed");
    TEST_ASSERT_TRUE_MESSAGE((embed_i16)-1 < 0, "embed_i16 is signed");
    TEST_ASSERT_TRUE_MESSAGE((embed_i32)-1 < 0, "embed_i32 is signed");
    TEST_ASSERT_TRUE_MESSAGE((embed_i64)-1 < 0, "embed_i64 is signed");
}

/**
 * @brief Checks that the derived word is the width of the pointer it was derived from.
 *
 * @note uintptr_t is the compiler's, not the header's, so this checks the derivation rather than
 *       repeating it. A build that overrides EMBED_WORD_BITS has said the two differ on purpose,
 *       and that one assertion does not run there.
 */
void test_the_word_matches_the_pointer_it_was_derived_from(void)
{
#if !EMBED_WORD_BITS_WAS_OVERRIDDEN
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(uintptr_t), sizeof(embed_word), "embed_word is the width of a pointer");
#endif
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(embed_word), sizeof(embed_iword), "the signed word is the same register");
    TEST_ASSERT_TRUE_MESSAGE((embed_iword)-1 < 0, "embed_iword is signed");
    TEST_ASSERT_TRUE_MESSAGE((embed_word)-1 > 0, "embed_word is unsigned");
}

/**
 * @brief Checks that an index fits the register it is carried in and is one of the two legal widths.
 *
 * @note The two byte counts are literals. Comparing against EMBED_INDEX_BITS would divide the macro
 *       by eight and compare it with itself.
 */
void test_the_index_fits_the_register_and_is_two_or_four_bytes(void)
{
    TEST_ASSERT_TRUE_MESSAGE(sizeof(embed_index) <= sizeof(embed_word), "an index fits the word");
    TEST_ASSERT_TRUE_MESSAGE(sizeof(embed_index) == 2u || sizeof(embed_index) == 4u, "an index is two or four bytes");
    TEST_ASSERT_TRUE_MESSAGE((embed_index)-1 > 0, "embed_index is unsigned");
}

/**
 * @brief Checks that the boolean normalizes any nonzero to one.
 *
 * @note The property a plain eight-bit alias would not have. Two values that are both true have to
 *       compare equal, or a caller and a library disagree about a flag they both set.
 */
void test_the_boolean_normalizes_every_nonzero_to_one(void)
{
    const embed_bool from_two = (embed_bool)2;
    const embed_bool from_large = (embed_bool)255;

    TEST_ASSERT_TRUE_MESSAGE(from_two == EMBED_TRUE, "two normalizes to true");
    TEST_ASSERT_TRUE_MESSAGE(from_large == EMBED_TRUE, "255 normalizes to true");
    TEST_ASSERT_TRUE_MESSAGE(from_two == from_large, "two true values compare equal");
    TEST_ASSERT_TRUE_MESSAGE((embed_bool)0 == EMBED_FALSE, "zero is false");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_TRUE != EMBED_FALSE, "true and false differ");
}

/**
 * @brief Checks that a word reads correctly from an address that is not a multiple of its width.
 *
 * @note The expectation is assembled with memcpy over the same bytes, which is defined at any
 *       address and does not depend on byte order. Building it with shifts would need the header's
 *       own endian answer, and the read would then be checked against the library.
 * @note An unaligned read through the plain embed_word would be undefined, so this case is the only
 *       proof that EMBED_RAW reaches the compiler.
 * @note Sixteen bytes, written as a literal rather than derived from embed_word. Sixty-four bits is
 *       the widest word this library admits, so sixteen is twice the widest and the read at offset
 *       one is in bounds at every width. A bound of sizeof(embed_word) * 2u shrinks with the word
 *       while the initializer list does not, so a build that overrode the width to 32 or 16 had
 *       more initializers than array and failed under -Werror.
 */
void test_a_raw_word_reads_from_an_odd_address(void)
{
    static const embed_u8 bytes[16u] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
                                        0x99u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu, 0x01u};
    const embed_raw_word *const at_one = (const embed_raw_word *)(const void *)(&bytes[1]);
    embed_word expected = 0u;

    (void)memcpy(&expected, &bytes[1], sizeof(expected));
    TEST_ASSERT_TRUE_MESSAGE(*at_one == expected, "a raw word at offset one matches the bytes it covers");
}
