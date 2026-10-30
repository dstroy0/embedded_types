/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_lane.c
 * @brief Holds every lane operation to the scalar answer on the arm the build takes.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note The arm is the build's: the compiler's vector type where it has one, the portable struct
 *       where it does not or where EMBED_LANE_PORTABLE is 1. The same cases hold on each, one
 *       source native on every target.
 * @note The cases reach no header of the C library: bytes are read and written through the lane's
 *       own raw views, as the library's code does.
 */
#include "embed_lane.h"

#include "unity.h"

/** @brief A 32-bit value readable at any address, the test's view into a byte array. */
typedef embed_u32 raw_u32 EMBED_RAW;

/** @brief The bytes the cases read lanes from: the index times seven, which repeats nowhere in 64. */
static embed_u8 s_bytes[64];

void setUp(void)
{
    for (unsigned int at = 0u; at < sizeof(s_bytes); at += 1u)
    {
        s_bytes[at] = (embed_u8)(at * 7u); // the low byte of the product is the pattern
    }
}

void tearDown(void)
{
}

/** @brief Every byte of `bytes`, `count` of them, set to `val`. */
static void bytes_set(embed_u8 *bytes, unsigned int count, embed_u8 val)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        bytes[at] = val;
    }
}

/** @brief The first `count` bytes of `from` written to `to`. */
static void bytes_copy(embed_u8 *to, const embed_u8 *from, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        to[at] = from[at];
    }
}

/** @brief The 32-bit value at `bytes`, at any alignment. */
static embed_u32 u32_at(const embed_u8 *bytes)
{
    return *(const raw_u32 *)bytes;
}

/** @brief `value` written at `bytes`, at any alignment. */
static void u32_put(embed_u8 *bytes, embed_u32 value)
{
    *(raw_u32 *)bytes = value;
}

/** @brief The 32-bit lane `index` of `lane`. */
static embed_u32 u32_of(embed_lane lane, unsigned int index)
{
    embed_u8 bytes[EMBED_LANE_BYTES];
    embed_lane_put(bytes, lane);
    return u32_at(bytes + (4u * index));
}

void test_the_lane_is_256_bits_of_eight_32_bit_lanes_on_one_arm(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(32u, sizeof(embed_lane), "a lane is 32 bytes");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(8u, EMBED_LANE_U32S, "a lane holds eight 32-bit lanes");
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, EMBED_LANE_VECTOR + EMBED_LANE_PLAIN, "exactly one arm is taken");
}

void test_a_lane_loaded_at_an_odd_address_puts_back_the_same_bytes(void)
{
    const embed_lane lane = embed_lane_load(s_bytes + 1);
    embed_u8 out[EMBED_LANE_BYTES + 8u];
    bytes_set(out, sizeof(out), 0xeeu);
    embed_lane_put(out + 3, lane);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(s_bytes + 1, out + 3, EMBED_LANE_BYTES, "the bytes come back as loaded");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xeeu, out[2], "the byte before the put stands");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xeeu, out[3u + EMBED_LANE_BYTES], "the byte after the put stands");
}

void test_a_lane_loaded_aligned_puts_back_aligned(void)
{
    static embed_u8 aligned[EMBED_LANE_BYTES] EMBED_ALIGN(32);
    static embed_u8 out[EMBED_LANE_BYTES] EMBED_ALIGN(32);
    bytes_copy(aligned, s_bytes, EMBED_LANE_BYTES);
    embed_lane_put_aligned(out, embed_lane_load_aligned(aligned));
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(aligned, out, EMBED_LANE_BYTES, "the aligned bytes come back");
}

void test_a_fill_by_byte_sets_every_byte_and_by_u32_every_lane(void)
{
    embed_u8 bytes[EMBED_LANE_BYTES];
    embed_lane_put(bytes, embed_lane_fill_byte(0xa5u));
    for (unsigned int at = 0u; at < EMBED_LANE_BYTES; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xa5u, bytes[at], "every byte is the fill");
    }
    const embed_lane lane = embed_lane_fill_u32(0x12345678u);
    for (unsigned int at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0x12345678u, u32_of(lane, at), "every 32-bit lane is the fill");
    }
}

void test_and_or_and_xor_run_in_every_bit(void)
{
    const embed_lane one = embed_lane_load(s_bytes);
    const embed_lane other = embed_lane_load(s_bytes + 16);
    for (unsigned int at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        const embed_u32 a = u32_at(s_bytes + (4u * at));
        const embed_u32 b = u32_at(s_bytes + 16u + (4u * at));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(a & b, u32_of(embed_lane_and(one, other), at), "and");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(a | b, u32_of(embed_lane_or(one, other), at), "or");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(a ^ b, u32_of(embed_lane_xor(one, other), at), "xor");
    }
}

void test_add_sub_and_mul_of_32_bit_lanes_wrap_as_the_scalar_does(void)
{
    const embed_lane one = embed_lane_fill_u32(0xfffffff0u);
    embed_u8 steps[EMBED_LANE_BYTES];
    for (unsigned int at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        u32_put(steps + (4u * at), (at + 1u) * 0x01000003u);
    }
    const embed_lane other = embed_lane_load(steps);
    for (unsigned int at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        const embed_u32 step = (at + 1u) * 0x01000003u;
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0xfffffff0u + step, u32_of(embed_lane_add_u32(one, other), at), "add wraps");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0xfffffff0u - step, u32_of(embed_lane_sub_u32(one, other), at), "sub wraps");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0xfffffff0u * step, u32_of(embed_lane_mul_u32(one, other), at), "mul keeps the low 32 bits");
    }
}

void test_shifts_move_each_32_bit_lane_alone(void)
{
    const embed_lane lane = embed_lane_fill_u32(0x80ff00ffu);
    for (unsigned int at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0x80ff00ffu >> 8u, u32_of(embed_lane_shift_right_u32(lane, 8u), at),
                                         "a right shift stays in its lane");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE((embed_u32)(0x80ff00ffu << 8u), u32_of(embed_lane_shift_left_u32(lane, 8u), at),
                                         "a left shift stays in its lane");
    }
}

void test_the_byte_masks_answer_one_bit_a_byte_lowest_first(void)
{
    embed_u8 changed[EMBED_LANE_BYTES];
    bytes_copy(changed, s_bytes, EMBED_LANE_BYTES);
    changed[0] ^= 1u;
    changed[5] ^= 0x80u;
    changed[31] ^= 0x10u;
    const embed_lane_mask equal = embed_lane_bytes_equal(embed_lane_load(s_bytes), embed_lane_load(changed));
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(0xffffffffu & ~((1u << 0u) | (1u << 5u) | (1u << 31u)), equal,
                                    "the bytes changed are the bits clear");
    embed_u8 zeros[EMBED_LANE_BYTES];
    bytes_set(zeros, sizeof(zeros), 1u);
    zeros[2] = 0u;
    zeros[17] = 0u;
    TEST_ASSERT_EQUAL_HEX32_MESSAGE((1u << 2u) | (1u << 17u), embed_lane_bytes_zero(embed_lane_load(zeros)),
                                    "the zero bytes are the bits set");
}

void test_the_exact_divide_by_255_holds_in_every_lane_over_every_product(void)
{
    // the blend's identity: (x + (x >> 8)) >> 8 is (a * b + 127) / 255 for every a and b of a byte, x being
    // a * b + 128; eight products a lane, every pair covered
    for (unsigned int a = 0u; a < 256u; a += 1u)
    {
        for (unsigned int b = 0u; b < 256u; b += 8u)
        {
            embed_u8 products[EMBED_LANE_BYTES];
            for (unsigned int at = 0u; at < EMBED_LANE_U32S; at += 1u)
            {
                u32_put(products + (4u * at), (a * (b + at)) + 128u);
            }
            const embed_lane x = embed_lane_load(products);
            const embed_lane divided = embed_lane_shift_right_u32(embed_lane_add_u32(x, embed_lane_shift_right_u32(x, 8u)), 8u);
            for (unsigned int at = 0u; at < EMBED_LANE_U32S; at += 1u)
            {
                TEST_ASSERT_EQUAL_UINT32_MESSAGE(((a * (b + at)) + 127u) / 255u, u32_of(divided, at), "the divide is exact");
            }
        }
    }
}
