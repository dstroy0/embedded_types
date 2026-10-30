/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_lane.h
 * @brief The lane: the widest unit the target moves and computes in one operation, beside the
 *        machine word, and the operations every bulk pass over memory runs in.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note The word, embed_word, is the register, and every count, mask and carry runs in it. The
 *       lane is the target's widest vector register, EMBED_LANE_BITS wide, derived here as the
 *       word's width is derived in embed_types.h: 512 bits under AVX-512, 256 under AVX2 and on a
 *       CUDA device, 128 under SSE2 or NEON, and the word's own width where the target has no
 *       vector unit. A bulk operation, a copy, a fill, a compare, a blend of pixels, steps one lane
 *       at a time on every target, and the lane partitions at compile time to what the target has.
 * @note Two arms, every operation defined on both. The vector arm is the compiler's vector type,
 *       EMBED_LANE_U32S 32-bit lanes the compiler keeps in its widest registers and runs the plain
 *       operators over; the compiler lowers it to the target's own instructions. The portable arm is
 *       a struct of the same 32-bit lanes with the same operations written as loops, which a CUDA
 *       thread holds in registers.
 * @note The 32-bit lanes are the unit of the arithmetic, since a pixel is one and a byte's share of
 *       255 times a byte fits in one. The byte masks answer where bytes are equal or zero, one bit
 *       a byte, in an integer the register holds whole.
 * @note No header of the C library is reached: a load or a store at any address goes through the
 *       lane's own raw view, as the word's does.
 * @note Every entry carries EMBED_HOST_DEVICE. Under nvcc one definition compiles into the host pass
 *       and the device pass, the device pass taking the portable arm at 256 bits, and a kernel calls
 *       the same functions a host walk calls.
 * @warning EMBED_LANE_PORTABLE set to 1 by the build takes the portable arm on any target. Both
 *          values of every arm flag are defined here. Test them with #if.
 */
#ifndef EMBED_LANE_H
#define EMBED_LANE_H

#include "embed_types.h"

/**
 * @brief The lane's width in bits, derived from the target's widest vector register.
 *
 * @note EMBED_VECTOR_REGISTER_BITS comes from the compiler's own instruction set macros. A CUDA
 *       device takes 256, a thread's eight 32-bit registers. A target with no vector unit takes the
 *       word's width, and 32 where the word is narrower, since a lane holds whole 32-bit lanes.
 * @note A build wanting another width defines this ahead of this header, as EMBED_WORD_BITS is
 *       defined. The assertions below hold any width to whole 32-bit lanes, whole words, and at
 *       most 512 bits, the width the byte mask can hold.
 */
#ifndef EMBED_LANE_BITS
/**
 * @brief Set to 0 where the lane width was derived here, 1 where the build defined it.
 *
 * @note Both arms define this macro. Test it with #if.
 */
#define EMBED_LANE_BITS_WAS_OVERRIDDEN 0

#if defined(__CUDA_ARCH__)
#define EMBED_LANE_BITS 256
#elif EMBED_VECTOR_REGISTER_BITS != 0
#define EMBED_LANE_BITS EMBED_VECTOR_REGISTER_BITS
#elif EMBED_WORD_BITS > 32
#define EMBED_LANE_BITS EMBED_WORD_BITS
#else
#define EMBED_LANE_BITS 32
#endif
#else

/** @brief Set to 1 where the build defined EMBED_LANE_BITS ahead of this header. */
#define EMBED_LANE_BITS_WAS_OVERRIDDEN 1
#endif

/** @brief The lane's width in bytes. */
#define EMBED_LANE_BYTES (EMBED_LANE_BITS / 8u)

/** @brief The 32-bit lanes a lane holds. */
#define EMBED_LANE_U32S (EMBED_LANE_BYTES / 4u)

EMBED_STATIC_ASSERT((EMBED_LANE_BITS % 32u) == 0u, "EMBED_LANE_BITS must be whole 32-bit lanes");
EMBED_STATIC_ASSERT(EMBED_LANE_BITS >= 32u, "EMBED_LANE_BITS must hold at least one 32-bit lane");
EMBED_STATIC_ASSERT(EMBED_LANE_BITS <= 512u, "EMBED_LANE_BITS must be at most 512: the byte mask holds 64 bytes");
EMBED_STATIC_ASSERT((EMBED_LANE_BYTES % sizeof(embed_word)) == 0u, "a lane must be whole words");

/**
 * @brief Set to 1 by the build to take the portable arm on any target, else 0.
 *
 * @note A check of the portable arm on a host with the vector type builds with this on, which holds
 *       the arm the device takes on the host.
 */
#ifndef EMBED_LANE_PORTABLE
#define EMBED_LANE_PORTABLE 0
#endif
#if (EMBED_LANE_PORTABLE != 0) && (EMBED_LANE_PORTABLE != 1)
#error "EMBED_LANE_PORTABLE must be 0 or 1"
#endif

/**
 * @brief The arm the lane takes: EMBED_LANE_VECTOR is 1 and EMBED_LANE_PLAIN 0, or the other way.
 *
 * @note The vector arm needs the compiler's vector type, EMBED_HAS_VECTOR_SIZE, and is left on a
 *       CUDA device, whose compiler keeps a thread's lanes in registers from the struct.
 */
#if EMBED_HAS_VECTOR_SIZE && !EMBED_LANE_PORTABLE && !defined(__CUDA_ARCH__)
#define EMBED_LANE_VECTOR 1
#define EMBED_LANE_PLAIN 0
#else
#define EMBED_LANE_VECTOR 0
#define EMBED_LANE_PLAIN 1
#endif

/**
 * @brief The lane, EMBED_LANE_BYTES wide: EMBED_LANE_U32S 32-bit lanes in the compiler's vector
 *        type on the vector arm, in a struct aligned to the lane on the portable arm.
 *
 * @note On both arms lanes in a row lay out as the registers do, each lane at a multiple of its
 *       width.
 */
#if EMBED_LANE_VECTOR
typedef embed_u32 embed_lane EMBED_VECTOR(EMBED_LANE_BYTES);

/** @brief The lane seen as its bytes, which the byte masks compare in. */
typedef embed_u8 embed_lane_bytes EMBED_VECTOR(EMBED_LANE_BYTES);

/** @brief The lane seen as char, the shape the compiler's byte mask builtin reads. */
typedef char embed_lane_chars EMBED_VECTOR(EMBED_LANE_BYTES);
#else
typedef struct
{
    embed_u32 at[EMBED_LANE_U32S]; /**< The 32-bit lanes, the lowest address first. */
} embed_lane EMBED_ALIGN(EMBED_LANE_BYTES);
#endif

EMBED_STATIC_ASSERT(sizeof(embed_lane) == EMBED_LANE_BYTES, "embed_lane must be exactly EMBED_LANE_BYTES wide");

/**
 * @brief The lane again, readable and writable at an address that is not a multiple of its width,
 *        as embed_raw_word is the word.
 *
 * @note EMBED_RAW lowers the alignment to one and permits the aliasing. A load or a store through
 *       this type is defined at any address, and one instruction on either arm.
 */
typedef embed_lane embed_lane_raw EMBED_RAW;

/**
 * @brief The byte mask a lane's byte operations answer in, one bit a byte, the lowest byte the
 *        lowest bit: 32 bits wide up to a 32-byte lane, 64 above it.
 */
#if EMBED_LANE_BYTES <= 32u
typedef embed_u32 embed_lane_mask;
#else
typedef embed_u64 embed_lane_mask;
#endif

/** @brief The mask with every byte of the lane set. */
#if EMBED_LANE_BYTES >= 64u
#define EMBED_LANE_MASK_ALL ((embed_lane_mask) ~(embed_u64)0)
#else
#define EMBED_LANE_MASK_ALL ((embed_lane_mask)(((embed_u64)1 << EMBED_LANE_BYTES) - 1u))
#endif

/**
 * @brief The index of the first byte set in a mask.
 *
 * @param mask  A mask with at least one byte set.
 * @return      The byte, counted from the first.
 * @note The trailing zero count where the compiler has it, else a walk of the bits.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_lane_mask_first(embed_lane_mask mask)
{
#if EMBED_HAS_BUILTIN(__builtin_ctzll)
    // Explicit casts widen the mask to the builtin's operand and read its count, below 64, as unsigned
    return (embed_u32)__builtin_ctzll((unsigned long long)mask);
#else
    embed_u32 byte = 0u;
    while (((mask >> byte) & 1u) == 0u)
    {
        byte += 1u;
    }
    return byte;
#endif
}

/**
 * @brief A lane loaded from any address.
 *
 * @param at  Where EMBED_LANE_BYTES bytes are read from, at any alignment.
 * @return    The bytes as a lane.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_load(const void *at)
{
    return *(const embed_lane_raw *)at;
}

/**
 * @brief A lane loaded from an address aligned to the lane.
 *
 * @param at  Where EMBED_LANE_BYTES bytes are read from, a multiple of EMBED_LANE_BYTES.
 * @return    The bytes as a lane.
 * @warning An address not aligned to the lane is undefined here; embed_lane_load reads any.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_load_aligned(const void *at)
{
    return *(const embed_lane *)at;
}

/**
 * @brief A lane put at any address.
 *
 * @param dst   Where EMBED_LANE_BYTES bytes are written, at any alignment.
 * @param lane  The bytes.
 */
EMBED_HOST_DEVICE EMBED_INLINE void embed_lane_put(void *dst, embed_lane lane)
{
    *(embed_lane_raw *)dst = lane;
}

/**
 * @brief A lane put at an address aligned to the lane.
 *
 * @param dst   Where EMBED_LANE_BYTES bytes are written, a multiple of EMBED_LANE_BYTES.
 * @param lane  The bytes.
 * @warning An address not aligned to the lane is undefined here; embed_lane_put writes any.
 */
EMBED_HOST_DEVICE EMBED_INLINE void embed_lane_put_aligned(void *dst, embed_lane lane)
{
    *(embed_lane *)dst = lane;
}

/**
 * @brief Every 32-bit lane of a lane set to `val`.
 *
 * @param val  The 32-bit value.
 * @return     The lane.
 * @note On the vector arm a scalar added to the zero vector is the scalar in every lane.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_fill_u32(embed_u32 val)
{
#if EMBED_LANE_VECTOR
    const embed_lane zero = {0u};
    return zero + val;
#else
    embed_lane lane;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        lane.at[at] = val;
    }
    return lane;
#endif
}

/**
 * @brief Every byte of a lane set to `val`.
 *
 * @param val  The byte.
 * @return     The lane.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_fill_byte(embed_u8 val)
{
    return embed_lane_fill_u32((embed_u32)val * 0x01010101u);
}

/**
 * @brief The bitwise and of two lanes.
 *
 * @param one    A lane.
 * @param other  A lane.
 * @return       The and.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_and(embed_lane one, embed_lane other)
{
#if EMBED_LANE_VECTOR
    return one & other;
#else
    embed_lane lane;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        lane.at[at] = one.at[at] & other.at[at];
    }
    return lane;
#endif
}

/**
 * @brief The bitwise or of two lanes.
 *
 * @param one    A lane.
 * @param other  A lane.
 * @return       The or.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_or(embed_lane one, embed_lane other)
{
#if EMBED_LANE_VECTOR
    return one | other;
#else
    embed_lane lane;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        lane.at[at] = one.at[at] | other.at[at];
    }
    return lane;
#endif
}

/**
 * @brief The bitwise exclusive or of two lanes.
 *
 * @param one    A lane.
 * @param other  A lane.
 * @return       The exclusive or.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_xor(embed_lane one, embed_lane other)
{
#if EMBED_LANE_VECTOR
    return one ^ other;
#else
    embed_lane lane;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        lane.at[at] = one.at[at] ^ other.at[at];
    }
    return lane;
#endif
}

/**
 * @brief The 32-bit lanes of two lanes added, each wrapping at 32 bits.
 *
 * @param one    A lane.
 * @param other  A lane.
 * @return       The sums.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_add_u32(embed_lane one, embed_lane other)
{
#if EMBED_LANE_VECTOR
    return one + other;
#else
    embed_lane lane;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        lane.at[at] = one.at[at] + other.at[at];
    }
    return lane;
#endif
}

/**
 * @brief The 32-bit lanes of `other` taken from those of `one`, each wrapping at 32 bits.
 *
 * @param one    A lane.
 * @param other  A lane.
 * @return       The differences.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_sub_u32(embed_lane one, embed_lane other)
{
#if EMBED_LANE_VECTOR
    return one - other;
#else
    embed_lane lane;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        lane.at[at] = one.at[at] - other.at[at];
    }
    return lane;
#endif
}

/**
 * @brief The 32-bit lanes of two lanes multiplied, the low 32 bits of each product kept.
 *
 * @param one    A lane.
 * @param other  A lane.
 * @return       The products' low halves.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_mul_u32(embed_lane one, embed_lane other)
{
#if EMBED_LANE_VECTOR
    return one * other;
#else
    embed_lane lane;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        lane.at[at] = one.at[at] * other.at[at];
    }
    return lane;
#endif
}

/**
 * @brief The 32-bit lanes of a lane shifted right by `count` bits, zeros shifted in.
 *
 * @param lane   A lane.
 * @param count  The bits, below 32.
 * @return       The shifted lanes.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_shift_right_u32(embed_lane lane, embed_u32 count)
{
#if EMBED_LANE_VECTOR
    return lane >> count;
#else
    embed_lane shifted;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        shifted.at[at] = lane.at[at] >> count;
    }
    return shifted;
#endif
}

/**
 * @brief The 32-bit lanes of a lane shifted left by `count` bits, zeros shifted in.
 *
 * @param lane   A lane.
 * @param count  The bits, below 32.
 * @return       The shifted lanes.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_shift_left_u32(embed_lane lane, embed_u32 count)
{
#if EMBED_LANE_VECTOR
    return lane << count;
#else
    embed_lane shifted;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        shifted.at[at] = lane.at[at] << count;
    }
    return shifted;
#endif
}

/**
 * @brief The four bytes of a 32-bit lane, each holding 1 or 0 in its lowest bit, gathered into the
 *        lane's low nibble, the lowest byte the lowest bit.
 *
 * @note A multiply places the four bits at bits 24 to 27 alone: the multiplier's four powers of two
 *       set each bit of the product once, with no carry between them. The product's top byte is the
 *       nibble.
 */
#define EMBED_LANE_NIBBLE_GATHER 0x01020408u

/**
 * @brief Which bytes of two lanes are equal, one bit a byte, the lowest byte the lowest bit.
 *
 * @param one    A lane.
 * @param other  A lane.
 * @return       The mask.
 * @note On the vector arm the bytes are compared as a byte vector, every equal byte all ones. Where
 *       the compiler has the byte mask builtin for the lane's width, EMBED_BYTE_MASK_256 or
 *       EMBED_BYTE_MASK_128, it reads the mask in one instruction. Otherwise each 32-bit lane's four
 *       bits are gathered into a nibble by one multiply over the whole lane, and the nibbles are
 *       read out. On the portable arm each 32-bit lane finds its zero bytes by the word's own carry
 *       test and gathers them the same way.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane_mask embed_lane_bytes_equal(embed_lane one, embed_lane other)
{
    embed_lane_mask mask = 0u;
#if EMBED_LANE_VECTOR
    const embed_lane_bytes equal = (embed_lane_bytes)((embed_lane_bytes)one == (embed_lane_bytes)other);
#if EMBED_HAS_BYTE_MASK_256 && (EMBED_LANE_BITS == 256)
    // Explicit casts read the builtin's int as the 32 unsigned bits it holds
    mask = (embed_lane_mask)(embed_u32)EMBED_BYTE_MASK_256((embed_lane_chars)equal);
#elif EMBED_HAS_BYTE_MASK_128 && (EMBED_LANE_BITS == 128)
    // Explicit casts read the builtin's int as the 16 unsigned bits it holds
    mask = (embed_lane_mask)(embed_u32)EMBED_BYTE_MASK_128((embed_lane_chars)equal);
#else
    const embed_lane bits = (embed_lane)equal & embed_lane_fill_u32(0x01010101u);
    const embed_lane nibbles = (bits * embed_lane_fill_u32(EMBED_LANE_NIBBLE_GATHER)) >> 24;
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        mask |= (embed_lane_mask)nibbles[at] << (4u * at);
    }
#endif
#else
    for (embed_u32 at = 0u; at < EMBED_LANE_U32S; at += 1u)
    {
        // a byte of the difference is zero where its high bit is clear after the carry test: the
        // sum carries out of every byte that holds any bit, and the complement leaves the rest
        const embed_u32 difference = one.at[at] ^ other.at[at];
        const embed_u32 carried = (difference & 0x7f7f7f7fu) + 0x7f7f7f7fu;
        const embed_u32 zero_bytes = (~(carried | difference | 0x7f7f7f7fu)) >> 7u;
        mask |= (embed_lane_mask)((zero_bytes * EMBED_LANE_NIBBLE_GATHER) >> 24u) << (4u * at);
    }
#endif
    return mask;
}

/**
 * @brief Which bytes of a lane are zero, one bit a byte, the lowest byte the lowest bit.
 *
 * @param lane  A lane.
 * @return      The mask.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane_mask embed_lane_bytes_zero(embed_lane lane)
{
    return embed_lane_bytes_equal(lane, embed_lane_fill_u32(0u));
}

#endif
