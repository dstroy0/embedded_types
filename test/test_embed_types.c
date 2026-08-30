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
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "embed_types.h"

/** @brief Counts the checks that did not hold, which becomes the exit status. */
static int failure_count;

/**
 * @brief Reports one check and counts it when it does not hold.
 *
 * @param[in] holds_ Condition that is expected to be true.
 * @param[in] what_  String literal naming what was checked, printed on failure.
 * @note A macro rather than a function so the failing text appears at the line that failed.
 */
#define CHECK(holds_, what_)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(holds_))                                                                                                 \
        {                                                                                                              \
            (void)printf("FAIL %s:%d %s\n", __FILE__, __LINE__, what_);                                                \
            failure_count++;                                                                                           \
        }                                                                                                              \
    } while (0)

/**
 * @brief Checks each exact-width alias against a byte count written as a literal.
 *
 * @note The literals are the independent side. Deriving them from the alias would restate the
 *       typedef rather than check it.
 */
static void the_widths_are_the_widths_they_are_named_for(void)
{
    CHECK(sizeof(embed_u8) == 1u, "embed_u8 is one byte");
    CHECK(sizeof(embed_u16) == 2u, "embed_u16 is two bytes");
    CHECK(sizeof(embed_u32) == 4u, "embed_u32 is four bytes");
    CHECK(sizeof(embed_u64) == 8u, "embed_u64 is eight bytes");
    CHECK(sizeof(embed_i8) == 1u, "embed_i8 is one byte");
    CHECK(sizeof(embed_i16) == 2u, "embed_i16 is two bytes");
    CHECK(sizeof(embed_i32) == 4u, "embed_i32 is four bytes");
    CHECK(sizeof(embed_i64) == 8u, "embed_i64 is eight bytes");
}

/**
 * @brief Checks that each unsigned alias is unsigned and each signed one holds a negative value.
 *
 * @note A typedef pointing at the wrong signedness has the right size, so the size checks above
 *       cannot see it. Round-tripping a negative through the signed type is what does.
 */
static void the_signed_aliases_carry_a_sign_and_the_unsigned_ones_do_not(void)
{
    CHECK((embed_u8)-1 > 0, "embed_u8 is unsigned");
    CHECK((embed_u16)-1 > 0, "embed_u16 is unsigned");
    CHECK((embed_u32)-1 > 0, "embed_u32 is unsigned");
    CHECK((embed_u64)-1 > 0, "embed_u64 is unsigned");
    CHECK((embed_i8)-1 < 0, "embed_i8 is signed");
    CHECK((embed_i16)-1 < 0, "embed_i16 is signed");
    CHECK((embed_i32)-1 < 0, "embed_i32 is signed");
    CHECK((embed_i64)-1 < 0, "embed_i64 is signed");
}

/**
 * @brief Checks that the derived word is the width of the pointer it was derived from.
 *
 * @note uintptr_t is the compiler's, not the header's, so this checks the derivation rather than
 *       repeating it. A build that overrides EMBED_WORD_BITS has said the two differ on purpose,
 *       and this case does not run there.
 */
static void the_word_matches_the_pointer_it_was_derived_from(void)
{
#if !defined(EMBED_WORD_BITS_WAS_OVERRIDDEN)
    CHECK(sizeof(embed_word) == sizeof(uintptr_t), "embed_word is the width of a pointer");
#endif
    CHECK(sizeof(embed_iword) == sizeof(embed_word), "the signed word is the same register");
    CHECK((embed_iword)-1 < 0, "embed_iword is signed");
    CHECK((embed_word)-1 > 0, "embed_word is unsigned");
}

/**
 * @brief Checks that an index fits the register it is carried in and is one of the two legal widths.
 *
 * @note The two byte counts are literals. Comparing against EMBED_INDEX_BITS would divide the macro
 *       by eight and compare it with itself.
 */
static void the_index_fits_the_register_and_is_two_or_four_bytes(void)
{
    CHECK(sizeof(embed_index) <= sizeof(embed_word), "an index fits the word");
    CHECK(sizeof(embed_index) == 2u || sizeof(embed_index) == 4u, "an index is two or four bytes");
    CHECK((embed_index)-1 > 0, "embed_index is unsigned");
}

/**
 * @brief Checks that the boolean normalizes any nonzero to one.
 *
 * @note The property a plain eight-bit alias would not have. Two values that are both true have to
 *       compare equal, or a caller and a library disagree about a flag they both set.
 */
static void the_boolean_normalizes_every_nonzero_to_one(void)
{
    const embed_bool from_two = (embed_bool)2;
    const embed_bool from_large = (embed_bool)255;

    CHECK(from_two == EMBED_TRUE, "two normalizes to true");
    CHECK(from_large == EMBED_TRUE, "255 normalizes to true");
    CHECK(from_two == from_large, "two true values compare equal");
    CHECK((embed_bool)0 == EMBED_FALSE, "zero is false");
    CHECK(EMBED_TRUE != EMBED_FALSE, "true and false differ");
}

/**
 * @brief Checks that a word reads correctly from an address that is not a multiple of its width.
 *
 * @note The expectation is assembled with memcpy over the same bytes, which is defined at any
 *       address and does not depend on byte order. Building it with shifts would need the header's
 *       own endian answer, and the read would then be checked against the library.
 * @note An unaligned read through the plain embed_word would be undefined, so this case is the only
 *       proof that EMBED_RAW reaches the compiler.
 */
static void a_raw_word_reads_from_an_odd_address(void)
{
    static const embed_u8 bytes[sizeof(embed_word) * 2u] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u,
                                                            0x77u, 0x88u, 0x99u, 0xAAu, 0xBBu, 0xCCu,
                                                            0xDDu, 0xEEu, 0xFFu, 0x01u};
    const embed_raw_word *const at_one = (const embed_raw_word *)(const void *)(&bytes[1]);
    embed_word expected = 0u;

    (void)memcpy(&expected, &bytes[1], sizeof(expected));
    CHECK(*at_one == expected, "a raw word at offset one matches the bytes it covers");
}

/**
 * @brief Runs every case and reports the count that did not hold.
 *
 * @return 0 where every check held, 1 otherwise.
 */
int main(void)
{
    the_widths_are_the_widths_they_are_named_for();
    the_signed_aliases_carry_a_sign_and_the_unsigned_ones_do_not();
    the_word_matches_the_pointer_it_was_derived_from();
    the_index_fits_the_register_and_is_two_or_four_bytes();
    the_boolean_normalizes_every_nonzero_to_one();
    a_raw_word_reads_from_an_odd_address();

    if (failure_count != 0)
    {
        (void)printf("%d check(s) failed\n", failure_count);
        return 1;
    }
    (void)printf("embed_types: all checks held\n");
    return 0;
}
