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
 * @note An expected width below is a literal or comes from the compiler. None comes from
 *       EMBED_WORD_BITS or EMBED_INDEX_BITS. That would compare those macros with the types that
 *       they made, and agree at any value.
 * @note This file compiles, and that proves the static assertions inside embed_types.h.
 * @note The generator of Unity collects the file-scope cases and writes unity_runner.c beside this
 *       file. It reads the case names out of the source text, so no case below sits inside an #if.
 *       A conditional goes inside a case, where every arm still compiles into one definition.
 */
#include <stdint.h>

#include "embed_types.h"

#include "unity.h"

/**
 * @brief Runs before each case, and has nothing to prepare.
 *
 * @note Unity calls this before every case, and a suite has to define it. Every case here reads
 *       types and literals, and none of them share state that needs a reset.
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
 * @note The byte counts are literals. A count derived from the alias would restate the typedef and
 *       check nothing.
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
 * @brief Checks that each unsigned alias has no sign and each signed one holds a negative value.
 *
 * @note A typedef of the wrong signedness has the right size, and the size checks above pass
 *       either way. Each assertion below casts -1 to the alias and tests the sign of the result.
 */
void test_the_signed_aliases_carry_a_sign_and_the_unsigned_ones_do_not(void)
{
    // Each cast below converts the int -1 to the alias under test, and the assertion reads the
    // converted value. For an unsigned alias, C defines the conversion, and it gives the maximum of
    // that type, above 0. For a signed alias the value fits and stays -1.
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
 * @brief Checks that the derived word is the width of the pointer that it comes from.
 *
 * @note uintptr_t comes from the compiler, and embed_types.h derives EMBED_WORD_BITS from it. A
 *       comparison of the two checks that derivation. The case skips the comparison where the build
 *       defines EMBED_WORD_BITS itself, because such a build declares that the two widths differ.
 */
void test_the_word_matches_the_pointer_it_was_derived_from(void)
{
#if !EMBED_WORD_BITS_WAS_OVERRIDDEN
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(uintptr_t), sizeof(embed_word), "embed_word is the width of a pointer");
#endif
    TEST_ASSERT_EQUAL_size_t_MESSAGE(sizeof(embed_word), sizeof(embed_iword), "the signed word is the same register");
    // The cast of the int -1 to each word type is the sign check. The unsigned word gives its
    // maximum, which is above 0. The signed word holds -1.
    TEST_ASSERT_TRUE_MESSAGE((embed_iword)-1 < 0, "embed_iword is signed");
    TEST_ASSERT_TRUE_MESSAGE((embed_word)-1 > 0, "embed_word is unsigned");
}

/**
 * @brief Checks that an index fits the register that holds it and is one of the two legal widths.
 *
 * @note The two byte counts are literals. A comparison against EMBED_INDEX_BITS divides the macro
 *       by eight and compares it with itself.
 */
void test_the_index_fits_the_register_and_is_two_or_four_bytes(void)
{
    TEST_ASSERT_TRUE_MESSAGE(sizeof(embed_index) <= sizeof(embed_word), "an index fits the word");
    TEST_ASSERT_TRUE_MESSAGE(sizeof(embed_index) == 2u || sizeof(embed_index) == 4u, "an index is two or four bytes");
    // The cast of the int -1 to embed_index is the sign check. C defines the conversion, and it gives
    // the maximum of that type, which is above 0.
    TEST_ASSERT_TRUE_MESSAGE((embed_index)-1 > 0, "embed_index is unsigned");
}

/**
 * @brief Checks that the boolean normalizes any nonzero to one.
 *
 * @note A plain eight-bit alias would not normalize. Two values that are both true have to compare
 *       equal, or a caller and a library disagree about a flag they both set.
 */
void test_the_boolean_normalizes_every_nonzero_to_one(void)
{
    // The conversion of an int to embed_bool is the normalization under test. C gives 1 for any
    // operand that compares unequal to 0, and 0 otherwise. Both casts below start from more than one
    // bit.
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
 * @note The expected value is a shift of the literal bytes at offset one, one byte for each byte of
 *       the word. EMBED_BIG_ENDIAN names the order. That is exact arithmetic over the same literal,
 *       and no call.
 * @note C does not define an unaligned read through the plain embed_word. embed_raw_word has
 *       EMBED_RAW, and this case reads through it at an odd address.
 * @note The array is sixteen bytes, as a literal. Sixty-four bits is the widest word that this
 *       library admits, and sixteen bytes covers a read of that word at offset one. A bound of
 *       sizeof(embed_word) * 2u shrinks with the word while the initializer list stays at sixteen.
 *       A build that sets the width to 32 or 16 then fails under -Werror.
 */
void test_a_raw_word_reads_from_an_odd_address(void)
{
    static const embed_u8 bytes[16u] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u,
                                        0x99u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu, 0xFFu, 0x01u};
    // The cast goes through const void * to convert from embed_u8 with no alignment warning.
    // EMBED_RAW on embed_raw_word lowers its alignment to one and permits the alias.
    const embed_raw_word *const at_one = (const embed_raw_word *)(const void *)(&bytes[1]);
    embed_word expected = 0u;
    embed_index at;

    for (at = 0u; at < (embed_index)sizeof(embed_word); at += 1u)
    {
#if EMBED_BIG_ENDIAN
        expected = (embed_word)((expected << 8u) | (embed_word)bytes[1u + at]);
#else
        expected = (embed_word)(expected | ((embed_word)bytes[1u + at] << (8u * at)));
#endif
    }
    TEST_ASSERT_TRUE_MESSAGE(*at_one == expected, "a raw word at offset one matches the bytes it covers");
}
