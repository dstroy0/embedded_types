/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_lane.h
 * @brief The lane beside the machine word: the widest unit that the target moves and computes in one
 *        operation. Also the operations that each bulk pass over memory runs in.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note The word, embed_word, is the register, and each count, mask and carry runs in it. The lane is
 *       the widest vector register of the target, EMBED_LANE_BITS wide. This header derives its width,
 *       as embed_types.h derives the width of the word. It is 512 bits under AVX-512, and 256 under
 *       AVX2 and on a CUDA device. It is 128 under SSE2 or NEON, and the width of the word where the
 *       target has no vector unit.
 * @note A bulk operation steps one lane at a time on each target: a copy, a fill, a compare, a blend
 *       of pixels. At compile time, the lane takes the width that the target has.
 * @note There are two arms, and each defines every operation. The vector arm is the vector type of the
 *       compiler: EMBED_LANE_U32S lanes of 32 bits in its widest registers. The plain operators run
 *       over it, and the compiler lowers it to the instructions of the target. The portable arm is a
 *       struct of the same 32-bit lanes, with one statement for each word in each operation. A CUDA
 *       thread holds that struct in registers.
 * @note The 32-bit lanes are the unit of the arithmetic. A pixel is one, and the product of two bytes
 *       fits in one. The byte masks tell where bytes are equal or zero, with one bit for each byte.
 *       The register holds the mask whole, as one integer.
 * @note This header includes no header of the C library. A load or a store at any address goes
 *       through the raw view of the lane, as for the word.
 * @note Each entry has EMBED_HOST_DEVICE. Under nvcc one definition compiles into the host pass and
 *       the device pass. The device pass takes the portable arm at 256 bits. So a kernel calls the
 *       same functions that a host walk calls.
 * @warning Where the build sets EMBED_LANE_PORTABLE to 1, the header takes the portable arm on any
 *          target. This header defines both values of each arm flag. Test them with #if.
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
 * @note A build that wants another width defines this macro ahead of this header, as for
 *       EMBED_WORD_BITS. The assertions below hold any width to whole 32-bit lanes, whole words, and
 *       at most 512 bits, the width that the byte mask can hold.
 */
#ifndef EMBED_LANE_BITS
/**
 * @brief Set to 0 where this header derives the lane width, 1 where the build defines it.
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
 * @brief word_ expanded one time for each 32-bit lane of the lane, with the place of that lane. So an
 *        operation of the portable arm is one statement for each word, with no counted walk.
 *
 * @param word_ A macro of one parameter, the word's place from 0.
 */
#if EMBED_LANE_U32S == 1u
#define EMBED_LANE_WORDS(word_) word_(0)
#elif EMBED_LANE_U32S == 2u
#define EMBED_LANE_WORDS(word_) word_(0) word_(1)
#elif EMBED_LANE_U32S == 4u
#define EMBED_LANE_WORDS(word_) word_(0) word_(1) word_(2) word_(3)
#elif EMBED_LANE_U32S == 8u
#define EMBED_LANE_WORDS(word_) word_(0) word_(1) word_(2) word_(3) word_(4) word_(5) word_(6) word_(7)
#elif EMBED_LANE_U32S == 16u
#define EMBED_LANE_WORDS(word_)                                                                                        \
    word_(0) word_(1) word_(2) word_(3) word_(4) word_(5) word_(6) word_(7) word_(8) word_(9) word_(10) word_(11)      \
        word_(12) word_(13) word_(14) word_(15)
#else
#error "EMBED_LANE_U32S must be 1, 2, 4, 8 or 16"
#endif

/**
 * @brief Set to 1 by the build to take the portable arm on any target, else 0.
 *
 * @note To check the portable arm on a host with the vector type, build with this macro at 1. Then
 *       the host runs the arm that the device takes.
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
 * @note The vector arm needs the vector type of the compiler, EMBED_HAS_VECTOR_SIZE. A CUDA device
 *       does not take it, because its compiler keeps the lanes of a thread in registers from the struct.
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
    EMBED_ALIGN(EMBED_LANE_BYTES) embed_u32 at[EMBED_LANE_U32S]; /**< The 32-bit lanes, the lowest address first. */
} embed_lane;
#endif

EMBED_STATIC_ASSERT(sizeof(embed_lane) == EMBED_LANE_BYTES, "embed_lane must be exactly EMBED_LANE_BYTES wide");

/**
 * @brief The lane again, readable and writable at an address that is not a multiple of its width,
 *        as embed_raw_word is the word.
 *
 * @note EMBED_RAW lowers the alignment to one and permits access through an alias. A load or a
 *       store through this type has a defined result at any address. It is one instruction on either
 *       arm.
 */
typedef embed_lane embed_lane_raw EMBED_RAW;

/**
 * @brief The byte mask that the byte operations of a lane give, one bit for each byte, the lowest
 *        byte at the lowest bit. It is 32 bits wide up to a 32-byte lane, and 64 above it.
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
 * @note The trailing zero count where the compiler has it, else the lowest bit isolated and placed by six masked
 *       steps.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_lane_mask_first(embed_lane_mask mask)
{
#if EMBED_HAS_BUILTIN(__builtin_ctzll)
    // Explicit casts widen the mask to the builtin's operand and read its count, below 64, as unsigned
    return (embed_u32)__builtin_ctzll((unsigned long long)mask);
#else
    const unsigned long long low = (unsigned long long)mask & (0ull - (unsigned long long)mask);
    embed_u32 byte = 0u;

    byte += ((low & 0xFFFFFFFF00000000ull) != 0u) ? 32u : 0u;
    byte += ((low & 0xFFFF0000FFFF0000ull) != 0u) ? 16u : 0u;
    byte += ((low & 0xFF00FF00FF00FF00ull) != 0u) ? 8u : 0u;
    byte += ((low & 0xF0F0F0F0F0F0F0F0ull) != 0u) ? 4u : 0u;
    byte += ((low & 0xCCCCCCCCCCCCCCCCull) != 0u) ? 2u : 0u;
    byte += ((low & 0xAAAAAAAAAAAAAAAAull) != 0u) ? 1u : 0u;
    return byte;
#endif
}

/**
 * @brief A lane loaded from any address.
 *
 * @param at  The address of the EMBED_LANE_BYTES bytes to read, at any alignment.
 * @return    The bytes as a lane.
 */
#if EMBED_LANE_PLAIN && !EMBED_HAS_ATTRIBUTE(aligned)
// Where EMBED_RAW cannot lower an alignment, embed_lane_raw keeps the alignment of the lane. A load through it is an
// aligned load, and the compiler can emit one. So this arm reads the lane one raw word at a time, as for the word.
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_load(const void *at)
{
    embed_lane lane;
    const embed_raw_word *const from = (const embed_raw_word *)at;
    embed_word *const to = (embed_word *)(void *)lane.at;

    for (size_t place = 0u; place < (EMBED_LANE_BYTES / sizeof(embed_word)); place += 1u)
    {
        to[place] = from[place];
    }
    return lane;
}
#else
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_load(const void *at)
{
    return *(const embed_lane_raw *)at;
}
#endif

/**
 * @brief A lane loaded from an address aligned to the lane.
 *
 * @param at  The address of the EMBED_LANE_BYTES bytes to read, a multiple of EMBED_LANE_BYTES.
 * @return    The bytes as a lane.
 * @warning C leaves a load from an address not aligned to the lane undefined here. embed_lane_load
 *          reads from any address.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_load_aligned(const void *at)
{
    return *(const embed_lane *)at;
}

/**
 * @brief A lane put at any address.
 *
 * @param dst   The address of the EMBED_LANE_BYTES bytes to write, at any alignment.
 * @param lane  The bytes.
 */
#if EMBED_LANE_PLAIN && !EMBED_HAS_ATTRIBUTE(aligned)
// The store that matches the load arm above, one raw word at a time, for the same reason.
EMBED_HOST_DEVICE EMBED_INLINE void embed_lane_put(void *dst, embed_lane lane)
{
    embed_raw_word *const to = (embed_raw_word *)dst;
    const embed_word *const from = (const embed_word *)(const void *)lane.at;

    for (size_t place = 0u; place < (EMBED_LANE_BYTES / sizeof(embed_word)); place += 1u)
    {
        to[place] = from[place];
    }
}
#else
EMBED_HOST_DEVICE EMBED_INLINE void embed_lane_put(void *dst, embed_lane lane)
{
    *(embed_lane_raw *)dst = lane;
}
#endif

/**
 * @brief A lane put at an address aligned to the lane.
 *
 * @param dst   The address of the EMBED_LANE_BYTES bytes to write, a multiple of EMBED_LANE_BYTES.
 * @param lane  The bytes.
 * @warning C leaves a store to an address not aligned to the lane undefined here. embed_lane_put
 *          writes to any address.
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
#define EMBED_LANE_FILL_WORD(at_) lane.at[at_] = val;
    EMBED_LANE_WORDS(EMBED_LANE_FILL_WORD)
#undef EMBED_LANE_FILL_WORD
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
#define EMBED_LANE_AND_WORD(at_) lane.at[at_] = one.at[at_] & other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_AND_WORD)
#undef EMBED_LANE_AND_WORD
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
#define EMBED_LANE_OR_WORD(at_) lane.at[at_] = one.at[at_] | other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_OR_WORD)
#undef EMBED_LANE_OR_WORD
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
#define EMBED_LANE_XOR_WORD(at_) lane.at[at_] = one.at[at_] ^ other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_XOR_WORD)
#undef EMBED_LANE_XOR_WORD
    return lane;
#endif
}

/**
 * @brief The 32-bit lanes of two lanes added, each with wrap around at 32 bits.
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
#define EMBED_LANE_ADD_U32_WORD(at_) lane.at[at_] = one.at[at_] + other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_ADD_U32_WORD)
#undef EMBED_LANE_ADD_U32_WORD
    return lane;
#endif
}

/**
 * @brief The 32-bit lanes of `other` taken from those of `one`, each with wrap around at 32 bits.
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
#define EMBED_LANE_SUB_U32_WORD(at_) lane.at[at_] = one.at[at_] - other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_SUB_U32_WORD)
#undef EMBED_LANE_SUB_U32_WORD
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
#define EMBED_LANE_MUL_U32_WORD(at_) lane.at[at_] = one.at[at_] * other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_MUL_U32_WORD)
#undef EMBED_LANE_MUL_U32_WORD
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
#define EMBED_LANE_SHIFT_RIGHT_U32_WORD(at_) shifted.at[at_] = lane.at[at_] >> count;
    EMBED_LANE_WORDS(EMBED_LANE_SHIFT_RIGHT_U32_WORD)
#undef EMBED_LANE_SHIFT_RIGHT_U32_WORD
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
#define EMBED_LANE_SHIFT_LEFT_U32_WORD(at_) shifted.at[at_] = lane.at[at_] << count;
    EMBED_LANE_WORDS(EMBED_LANE_SHIFT_LEFT_U32_WORD)
#undef EMBED_LANE_SHIFT_LEFT_U32_WORD
    return shifted;
#endif
}

/**
 * @brief The multiplier that gathers the four bytes of a 32-bit lane into one nibble. Each byte holds
 *        1 or 0 in its lowest bit, and the lowest byte goes to the lowest bit.
 *
 * @note A multiply puts the four bits at bits 24 to 27 alone. The four powers of two in the multiplier
 *       set each bit of the product one time, with no carry between them. The top byte of the product
 *       is the nibble.
 */
#define EMBED_LANE_NIBBLE_GATHER 0x01020408u

/**
 * @brief Which bytes of two lanes are equal, one bit a byte, the lowest byte the lowest bit.
 *
 * @param one    A lane.
 * @param other  A lane.
 * @return       The mask.
 * @note On the vector arm, the code compares the bytes as a byte vector, and each equal byte becomes
 *       all ones. Where the compiler has the byte mask builtin for the width of the lane, it reads the
 *       mask in one instruction. That builtin is EMBED_BYTE_MASK_256 or EMBED_BYTE_MASK_128. Otherwise
 *       one multiply over the full lane gathers the four bits of each 32-bit lane into a nibble, and the
 *       code reads out the nibbles. On the portable arm, each 32-bit lane finds its zero bytes by the
 *       carry test of the word, and gathers them the same way.
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
#define EMBED_LANE_NIBBLE_WORD(at_) mask |= (embed_lane_mask)nibbles[at_] << (4u * (at_));
    EMBED_LANE_WORDS(EMBED_LANE_NIBBLE_WORD)
#undef EMBED_LANE_NIBBLE_WORD
#endif
#else
    // A byte of the difference of a word is zero where its high bit is clear after the carry test. The sum
    // carries out of each byte that holds a bit, and the complement keeps the other bytes.
#define EMBED_LANE_ZERO_BYTES(at_)                                                                                    \
    ((~((((one.at[at_] ^ other.at[at_]) & 0x7f7f7f7fu) + 0x7f7f7f7fu) | (one.at[at_] ^ other.at[at_]) | 0x7f7f7f7fu)) >> 7u)
#define EMBED_LANE_EQUAL_WORD(at_)                                                                                    \
    mask |= (embed_lane_mask)((EMBED_LANE_ZERO_BYTES(at_) * EMBED_LANE_NIBBLE_GATHER) >> 24u) << (4u * (at_));
    EMBED_LANE_WORDS(EMBED_LANE_EQUAL_WORD)
#undef EMBED_LANE_EQUAL_WORD
#undef EMBED_LANE_ZERO_BYTES
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
