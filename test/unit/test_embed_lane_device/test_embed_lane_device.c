/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_lane_device.c
 * @brief Holds the lane's device arm to the scalar answer: every operation run on the device over a
 *        span, read back, and compared byte for byte with a loop on the host.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note The kernels are in test_embed_lane_device.cu. This unit is C and never names the lane
 *       itself: the span is bytes and 32-bit values, the device decides how many lanes they are,
 *       and the host computes what each byte must come back as.
 * @note Where the build has the device arm and the host has no part to run it on, every case that
 *       needs the part is ignored and says so.
 */
#include "test_embed_lane_device.h"

#include "unity.h"

/** @brief The span the cases run over, a multiple of every lane width up to the widest. */
#define SPAN_BYTES 256u

/** @brief The 32-bit values the span holds. */
#define SPAN_U32S (SPAN_BYTES / 4u)

/** @brief A 32-bit value readable at any address, the test's view into a byte array. */
typedef embed_u32 raw_u32 EMBED_RAW;

static embed_u8 s_one[SPAN_BYTES];
static embed_u8 s_other[SPAN_BYTES];

void setUp(void)
{
    for (unsigned int at = 0u; at < SPAN_BYTES; at += 1u)
    {
        // the low bytes of the products are the patterns, neither ever zero below 256
        s_one[at] = (embed_u8)((at * 11u) + 1u);
        s_other[at] = (embed_u8)((at * 7u) + 3u);
    }
}

void tearDown(void)
{
}

/** @brief The case is ignored where no part is present to run it. */
#define PART_OR_IGNORED()                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (embed_lane_device_present() == 0)                                                                          \
        {                                                                                                              \
            TEST_IGNORE_MESSAGE("no device present to run the lane's device arm on");                                  \
        }                                                                                                              \
    } while (0)

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

void test_the_device_lane_is_a_threads_eight_registers(void)
{
    PART_OR_IGNORED();
    embed_u32 bits = 0u;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, embed_lane_device_bits(&bits), "the kernel ran");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(256u, bits, "the device pass derives a 256-bit lane");
}

void test_the_device_copies_a_span_byte_for_byte(void)
{
    PART_OR_IGNORED();
    embed_u8 out[SPAN_BYTES];
    for (unsigned int at = 0u; at < SPAN_BYTES; at += 1u)
    {
        out[at] = 0xeeu;
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, embed_lane_device_copy(s_one, out, SPAN_BYTES), "the kernel ran");
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(s_one, out, SPAN_BYTES, "every byte comes back as loaded");
}

void test_the_device_fills_every_byte_of_a_span(void)
{
    PART_OR_IGNORED();
    embed_u8 out[SPAN_BYTES];
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, embed_lane_device_fill(0xa5u, out, SPAN_BYTES), "the kernel ran");
    for (unsigned int at = 0u; at < SPAN_BYTES; at += 1u)
    {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xa5u, out[at], "every byte is the fill");
    }
}

void test_the_device_divides_by_255_exactly_in_every_lane(void)
{
    PART_OR_IGNORED();
    // the blend's identity over sampled products: (x + (x >> 8)) >> 8 is (a * b + 127) / 255, x being a * b + 128
    embed_u8 products[SPAN_BYTES];
    embed_u8 divided[SPAN_BYTES];
    for (unsigned int a = 0u; a < 256u; a += 5u)
    {
        for (unsigned int at = 0u; at < SPAN_U32S; at += 1u)
        {
            const unsigned int b = (at * 4u) % 256u;
            u32_put(products + (4u * at), (a * b) + 128u);
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, embed_lane_device_divide((const embed_u32 *)(const void *)products,
                                                                  (embed_u32 *)(void *)divided, SPAN_BYTES),
                                      "the kernel ran");
        for (unsigned int at = 0u; at < SPAN_U32S; at += 1u)
        {
            const unsigned int b = (at * 4u) % 256u;
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(((a * b) + 127u) / 255u, u32_at(divided + (4u * at)), "the divide is exact");
        }
    }
}

void test_the_device_logic_and_arithmetic_match_the_scalar(void)
{
    PART_OR_IGNORED();
    embed_u8 out_and[SPAN_BYTES];
    embed_u8 out_or[SPAN_BYTES];
    embed_u8 out_xor[SPAN_BYTES];
    embed_u8 out_add[SPAN_BYTES];
    embed_u8 out_sub[SPAN_BYTES];
    embed_u8 out_mul[SPAN_BYTES];
    embed_u8 out_left[SPAN_BYTES];
    embed_u8 out_right[SPAN_BYTES];
    TEST_ASSERT_EQUAL_INT_MESSAGE(
        0,
        embed_lane_device_arithmetic((const embed_u32 *)(const void *)s_one, (const embed_u32 *)(const void *)s_other, 7u,
                                     (embed_u32 *)(void *)out_and, (embed_u32 *)(void *)out_or, (embed_u32 *)(void *)out_xor,
                                     (embed_u32 *)(void *)out_add, (embed_u32 *)(void *)out_sub, (embed_u32 *)(void *)out_mul,
                                     (embed_u32 *)(void *)out_left, (embed_u32 *)(void *)out_right, SPAN_BYTES),
        "the kernel ran");
    for (unsigned int at = 0u; at < SPAN_U32S; at += 1u)
    {
        const embed_u32 a = u32_at(s_one + (4u * at));
        const embed_u32 b = u32_at(s_other + (4u * at));
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(a & b, u32_at(out_and + (4u * at)), "and");
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(a | b, u32_at(out_or + (4u * at)), "or");
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(a ^ b, u32_at(out_xor + (4u * at)), "xor");
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(a + b, u32_at(out_add + (4u * at)), "add wraps");
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(a - b, u32_at(out_sub + (4u * at)), "sub wraps");
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(a * b, u32_at(out_mul + (4u * at)), "mul keeps the low 32 bits");
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(a << 7u, u32_at(out_left + (4u * at)), "a left shift stays in its lane");
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(a >> 7u, u32_at(out_right + (4u * at)), "a right shift stays in its lane");
    }
}

void test_the_device_byte_masks_match_the_byte_loop(void)
{
    PART_OR_IGNORED();
    embed_u32 bits = 0u;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, embed_lane_device_bits(&bits), "the kernel ran");
    const unsigned int lane_bytes = bits / 8u;
    const unsigned int lanes = SPAN_BYTES / lane_bytes;
    // the other span agrees with the first in a few bytes and holds a zero in each lane but the last
    embed_u8 other[SPAN_BYTES];
    embed_u8 one[SPAN_BYTES];
    for (unsigned int at = 0u; at < SPAN_BYTES; at += 1u)
    {
        one[at] = s_one[at];
        other[at] = ((at % 9u) == 0u) ? s_one[at] : s_other[at];
    }
    for (unsigned int lane = 0u; (lane + 1u) < lanes; lane += 1u)
    {
        one[(lane * lane_bytes) + (lane % lane_bytes) + 1u] = 0u;
    }
    embed_u64 equal[SPAN_BYTES / 4u];
    embed_u64 zero[SPAN_BYTES / 4u];
    embed_u32 first_zero[SPAN_BYTES / 4u];
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, embed_lane_device_masks(one, other, equal, zero, first_zero, SPAN_BYTES),
                                  "the kernel ran");
    for (unsigned int lane = 0u; lane < lanes; lane += 1u)
    {
        embed_u64 want_equal = 0u;
        embed_u64 want_zero = 0u;
        embed_u32 want_first = 0xffffffffu;
        for (unsigned int byte = 0u; byte < lane_bytes; byte += 1u)
        {
            const unsigned int at = (lane * lane_bytes) + byte;
            if (one[at] == other[at])
            {
                want_equal |= (embed_u64)1 << byte;
            }
            if (one[at] == 0u)
            {
                want_zero |= (embed_u64)1 << byte;
                want_first = (want_first == 0xffffffffu) ? byte : want_first;
            }
        }
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(want_equal, equal[lane], "the equal bytes are the bits set");
        TEST_ASSERT_EQUAL_HEX64_MESSAGE(want_zero, zero[lane], "the zero bytes are the bits set");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(want_first, first_zero[lane], "the first zero byte is the lowest bit");
    }
}
