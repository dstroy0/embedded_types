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

EMBED_BEGIN_DECLS

/**
 * @brief The lane's width in bits, derived from the target's widest vector register.
 *
 * @note EMBED_VECTOR_REGISTER_BITS comes from the compiler's own instruction set macros. A CUDA
 *       device takes 256, a thread's eight 32-bit registers. A target with no vector unit takes the
 *       word's width, and 32 where the word is narrower, since a lane holds whole 32-bit lanes.
 * @note A build that wants another width defines this macro ahead of this header, as for
 *       EMBED_WORD_BITS. The assertions below hold any width to whole 32-bit lanes, whole words, and
 *       at max 512 bits, the width that the byte mask can hold.
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
EMBED_STATIC_ASSERT(EMBED_LANE_BITS <= 512u, "EMBED_LANE_BITS must be at max 512: the byte mask holds 64 bytes");
EMBED_STATIC_ASSERT((EMBED_LANE_BYTES % sizeof(embed_word)) == 0u, "a lane must be whole words");

/**
 * @brief word_ expanded one time for each 32-bit lane of the lane, with the place of that lane. So an
 *        operation of the portable arm is one statement for each word, with no counted walk.
 *
 * @param[in] word_ A macro of one parameter, the word's place from 0.
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
 *       does not take it, because its compiler retains the lanes of a thread in registers from the struct.
 */
#if EMBED_HAS_VECTOR_SIZE && !EMBED_LANE_PORTABLE && !defined(__CUDA_ARCH__)
#define EMBED_LANE_VECTOR 1
#define EMBED_LANE_PLAIN 0
#else
#define EMBED_LANE_VECTOR 0
#define EMBED_LANE_PLAIN 1
#endif

/**
 * @brief The lane, EMBED_LANE_BYTES wide. On the vector arm it is EMBED_LANE_U32S 32-bit lanes in
 *        the vector type of the compiler. On the portable arm it is a struct with the alignment of
 *        the lane.
 *
 * @note On both arms lanes in a row arrange as the registers do, each lane at a multiple of its
 *       width.
 */
#if EMBED_LANE_VECTOR
typedef embed_u32 embed_lane EMBED_VECTOR(EMBED_LANE_BYTES);

/** @brief The view of the lane as its bytes, which the byte masks compare in. */
typedef embed_u8 embed_lane_bytes EMBED_VECTOR(EMBED_LANE_BYTES);

/** @brief The view of the lane as char, the shape that the byte mask builtin of the compiler reads. */
typedef char embed_lane_chars EMBED_VECTOR(EMBED_LANE_BYTES);
#else
typedef struct
{
    EMBED_ALIGN(EMBED_LANE_BYTES) embed_u32 at[EMBED_LANE_U32S]; /**< The 32-bit lanes, the lowest address first. */
} embed_lane;
#endif

EMBED_STATIC_ASSERT(sizeof(embed_lane) == EMBED_LANE_BYTES, "embed_lane must be exactly EMBED_LANE_BYTES wide");

#if EMBED_LANE_VECTOR
/**
 * @brief The lane again, readable and writable at an address that is not a multiple of its width,
 *        as embed_raw_word is the word.
 *
 * @note EMBED_RAW lowers the alignment to one and permits access through an alias. A load or a
 *       store through this type has a defined result at any address. It is one instruction.
 * @note The vector arm alone has this type. A type attribute on a typedef of a struct has no effect
 *       after the definition of the struct. The alignment of a struct cannot go below the alignment
 *       of its members. So the portable arm reads and writes a lane one embed_raw_word at a time.
 */
typedef embed_lane embed_lane_raw EMBED_RAW;
#endif

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
#define EMBED_LANE_MASK_ALL ((embed_lane_mask) ~(embed_u64)0u)
#else
#define EMBED_LANE_MASK_ALL ((embed_lane_mask)(((embed_u64)1u << EMBED_LANE_BYTES) - 1u))
#endif

/**
 * @brief The mask of the first `bytes_` bytes of a lane.
 *
 * @param[in] bytes_ The bytes, 0 through EMBED_LANE_BYTES.
 * @note A count of EMBED_LANE_BYTES or more gives EMBED_LANE_MASK_ALL. The shift then stays below the width of
 *       the mask. The cast widens the one to the mask before the shift.
 */
#define EMBED_LANE_MASK_OF(bytes_)                                                                                     \
    (((bytes_) >= EMBED_LANE_BYTES) ? EMBED_LANE_MASK_ALL : (embed_lane_mask)(((embed_lane_mask)1u << (bytes_)) - 1u))

/** @brief The parameters of embed_lane_mask_first. */
typedef struct
{
    const embed_lane_mask mask; /**< A mask with at least one byte set. */
} EmbedLaneMaskFirstCfg;

/**
 * @brief The index of the first byte set in a mask.
 *
 * @param[in] args The parameters, as EmbedLaneMaskFirstCfg holds them [BORROWS].
 * @return         The byte, counted from the first.
 * @note The trailing zero count where the compiler has it, else the lowest bit isolated and placed by six masked
 *       steps.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_lane_mask_first(const EmbedLaneMaskFirstCfg *args)
{
#if EMBED_HAS_BUILTIN(__builtin_ctzll)
    // Explicit casts widen the mask to the builtin's operand and read its count, below 64, as unsigned
    return (embed_u32)__builtin_ctzll((unsigned long long)args->mask);
#else
    // Explicit casts widen the mask to 64 bits, so the negation keeps the lowest set bit alone at each mask width
    const unsigned long long low = (unsigned long long)args->mask & (0ull - (unsigned long long)args->mask);
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

/** @brief The parameters of embed_lane_mask_before. */
typedef struct
{
    const embed_lane_mask mask; /**< Bytes that end a walk. */
} EmbedLaneMaskBeforeCfg;

/**
 * @brief The bytes before the first set byte of a mask, or every byte where the mask has no set byte.
 *
 * @param[in] args The parameters, as EmbedLaneMaskBeforeCfg holds them [BORROWS].
 * @return         The bytes ahead of the first one that ends it.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane_mask embed_lane_mask_before(const EmbedLaneMaskBeforeCfg *args)
{
    return (args->mask == 0u) ? EMBED_LANE_MASK_ALL : (embed_lane_mask)((args->mask & (0u - args->mask)) - 1u);
}

#if EMBED_LANE_PLAIN
/**
 * @brief The lane and the same bytes as words, for the load and the put of the portable arm.
 *
 * @note The portable lane is a struct, and a struct cannot take the raw attributes. So this arm reads the lane one
 *       raw word at a time, as for the word. The raw word has both attributes on every compiler that has them. The
 *       union gives the lane a view as words, which C defines, so no pointer casts between the two types.
 */
typedef union
{
    embed_lane lane;                                         /**< The lane. */
    embed_word words[EMBED_LANE_BYTES / sizeof(embed_word)]; /**< The same bytes as words. */
} embed_lane_words;
#endif

/** @brief The parameters of embed_lane_load. */
typedef struct
{
    const void *const at; /**< The address of the EMBED_LANE_BYTES bytes to read, at any alignment. */
} EmbedLaneLoadCfg;

/**
 * @brief A lane loaded from any address.
 *
 * @param[in] args The parameters, as EmbedLaneLoadCfg holds them [BORROWS].
 * @return         The bytes as a lane.
 */
#if EMBED_LANE_PLAIN
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_load(const EmbedLaneLoadCfg *args)
{
    embed_lane_words view;
    const embed_raw_word *const from = (const embed_raw_word *)args->at;

    for (size_t place = 0u; place < (EMBED_LANE_BYTES / sizeof(embed_word)); place += 1u)
    {
        view.words[place] = from[place];
    }
    return view.lane;
}
#else
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_load(const EmbedLaneLoadCfg *args)
{
    return *(const embed_lane_raw *)args->at;
}
#endif

/** @brief The parameters of embed_lane_load_aligned. */
typedef struct
{
    const void *const at; /**< The address of the EMBED_LANE_BYTES bytes to read, a multiple of EMBED_LANE_BYTES. */
} EmbedLaneLoadAlignedCfg;

/**
 * @brief A lane loaded from an address aligned to the lane.
 *
 * @param[in] args The parameters, as EmbedLaneLoadAlignedCfg holds them [BORROWS].
 * @return         The bytes as a lane.
 * @warning C leaves a load from an address not aligned to the lane undefined here. embed_lane_load
 *          reads from any address.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_load_aligned(const EmbedLaneLoadAlignedCfg *args)
{
    return *(const embed_lane *)args->at;
}

/** @brief The parameters of embed_lane_put. */
typedef struct
{
    void *const destination; /**< The address of the EMBED_LANE_BYTES bytes to write, at any alignment. */
    const embed_lane lane;   /**< The bytes. */
} EmbedLanePutCfg;

/**
 * @brief A lane put at any address.
 *
 * @param[in] args The parameters, as EmbedLanePutCfg holds them [BORROWS].
 */
#if EMBED_LANE_PLAIN
// The store that matches the load arm above, one raw word at a time, for the same reason.
EMBED_HOST_DEVICE EMBED_INLINE void embed_lane_put(const EmbedLanePutCfg *args)
{
    embed_lane_words view;
    embed_raw_word *const to = (embed_raw_word *)args->destination;

    view.lane = args->lane;
    for (size_t place = 0u; place < (EMBED_LANE_BYTES / sizeof(embed_word)); place += 1u)
    {
        to[place] = view.words[place];
    }
}
#else
EMBED_HOST_DEVICE EMBED_INLINE void embed_lane_put(const EmbedLanePutCfg *args)
{
    *(embed_lane_raw *)args->destination = args->lane;
}
#endif

/** @brief The parameters of embed_lane_put_aligned. */
typedef struct
{
    void *const destination; /**< The address of the EMBED_LANE_BYTES bytes to write, a multiple of EMBED_LANE_BYTES. */
    const embed_lane lane;   /**< The bytes. */
} EmbedLanePutAlignedCfg;

/**
 * @brief A lane put at an address aligned to the lane.
 *
 * @param[in] args The parameters, as EmbedLanePutAlignedCfg holds them [BORROWS].
 * @warning C leaves a store to an address not aligned to the lane undefined here. embed_lane_put
 *          writes to any address.
 */
EMBED_HOST_DEVICE EMBED_INLINE void embed_lane_put_aligned(const EmbedLanePutAlignedCfg *args)
{
    *(embed_lane *)args->destination = args->lane;
}

/** @brief The parameters of embed_lane_fill_u32. */
typedef struct
{
    const embed_u32 value; /**< The 32-bit value. */
} EmbedLaneFillU32Cfg;

/**
 * @brief Every 32-bit lane of a lane set to `value`.
 *
 * @param[in] args The parameters, as EmbedLaneFillU32Cfg holds them [BORROWS].
 * @return         The lane.
 * @note On the vector arm a scalar added to the zero vector is the scalar in every lane.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_fill_u32(const EmbedLaneFillU32Cfg *args)
{
#if EMBED_LANE_VECTOR
    const embed_lane zero = {0u};
    return zero + args->value;
#else
    embed_lane lane;
#define EMBED_LANE_FILL_WORD(at_) lane.at[at_] = args->value;
    EMBED_LANE_WORDS(EMBED_LANE_FILL_WORD)
#undef EMBED_LANE_FILL_WORD
    return lane;
#endif
}

/** @brief The parameters of embed_lane_fill_byte. */
typedef struct
{
    const embed_u8 byte; /**< The byte. */
} EmbedLaneFillByteCfg;

/**
 * @brief Every byte of a lane set to `byte`.
 *
 * @param[in] args The parameters, as EmbedLaneFillByteCfg holds them [BORROWS].
 * @return         The lane.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_fill_byte(const EmbedLaneFillByteCfg *args)
{
    // Explicit cast widens the byte to 32 bits, so the product puts a copy of it in each of the four bytes
    return EMBED_CALL(embed_lane_fill_u32, EmbedLaneFillU32Cfg, (embed_u32)args->byte * 0x01010101u);
}

/** @brief The parameters of embed_lane_and. */
typedef struct
{
    const embed_lane one;   /**< A lane. */
    const embed_lane other; /**< A lane. */
} EmbedLaneAndCfg;

/**
 * @brief The bitwise and of two lanes.
 *
 * @param[in] args The parameters, as EmbedLaneAndCfg holds them [BORROWS].
 * @return         The and.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_and(const EmbedLaneAndCfg *args)
{
#if EMBED_LANE_VECTOR
    return args->one & args->other;
#else
    embed_lane lane;
#define EMBED_LANE_AND_WORD(at_) lane.at[at_] = args->one.at[at_] & args->other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_AND_WORD)
#undef EMBED_LANE_AND_WORD
    return lane;
#endif
}

/** @brief The parameters of embed_lane_or. */
typedef struct
{
    const embed_lane one;   /**< A lane. */
    const embed_lane other; /**< A lane. */
} EmbedLaneOrCfg;

/**
 * @brief The bitwise or of two lanes.
 *
 * @param[in] args The parameters, as EmbedLaneOrCfg holds them [BORROWS].
 * @return         The or.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_or(const EmbedLaneOrCfg *args)
{
#if EMBED_LANE_VECTOR
    return args->one | args->other;
#else
    embed_lane lane;
#define EMBED_LANE_OR_WORD(at_) lane.at[at_] = args->one.at[at_] | args->other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_OR_WORD)
#undef EMBED_LANE_OR_WORD
    return lane;
#endif
}

/** @brief The parameters of embed_lane_xor. */
typedef struct
{
    const embed_lane one;   /**< A lane. */
    const embed_lane other; /**< A lane. */
} EmbedLaneXorCfg;

/**
 * @brief The bitwise exclusive or of two lanes.
 *
 * @param[in] args The parameters, as EmbedLaneXorCfg holds them [BORROWS].
 * @return         The exclusive or.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_xor(const EmbedLaneXorCfg *args)
{
#if EMBED_LANE_VECTOR
    return args->one ^ args->other;
#else
    embed_lane lane;
#define EMBED_LANE_XOR_WORD(at_) lane.at[at_] = args->one.at[at_] ^ args->other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_XOR_WORD)
#undef EMBED_LANE_XOR_WORD
    return lane;
#endif
}

/** @brief The parameters of embed_lane_add_u32. */
typedef struct
{
    const embed_lane one;   /**< A lane. */
    const embed_lane other; /**< A lane. */
} EmbedLaneAddU32Cfg;

/**
 * @brief The 32-bit lanes of two lanes added, each with wrap around at 32 bits.
 *
 * @param[in] args The parameters, as EmbedLaneAddU32Cfg holds them [BORROWS].
 * @return         The sums.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_add_u32(const EmbedLaneAddU32Cfg *args)
{
#if EMBED_LANE_VECTOR
    return args->one + args->other;
#else
    embed_lane lane;
#define EMBED_LANE_ADD_U32_WORD(at_) lane.at[at_] = args->one.at[at_] + args->other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_ADD_U32_WORD)
#undef EMBED_LANE_ADD_U32_WORD
    return lane;
#endif
}

/** @brief The parameters of embed_lane_sub_u32. */
typedef struct
{
    const embed_lane one;   /**< A lane. */
    const embed_lane other; /**< A lane. */
} EmbedLaneSubU32Cfg;

/**
 * @brief The 32-bit lanes of `other` taken from those of `one`, each with wrap around at 32 bits.
 *
 * @param[in] args The parameters, as EmbedLaneSubU32Cfg holds them [BORROWS].
 * @return         The differences.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_sub_u32(const EmbedLaneSubU32Cfg *args)
{
#if EMBED_LANE_VECTOR
    return args->one - args->other;
#else
    embed_lane lane;
#define EMBED_LANE_SUB_U32_WORD(at_) lane.at[at_] = args->one.at[at_] - args->other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_SUB_U32_WORD)
#undef EMBED_LANE_SUB_U32_WORD
    return lane;
#endif
}

/** @brief The parameters of embed_lane_mul_u32. */
typedef struct
{
    const embed_lane one;   /**< A lane. */
    const embed_lane other; /**< A lane. */
} EmbedLaneMulU32Cfg;

/**
 * @brief The 32-bit lanes of two lanes multiplied, the low 32 bits of each product retained.
 *
 * @param[in] args The parameters, as EmbedLaneMulU32Cfg holds them [BORROWS].
 * @return         The products' low halves.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_mul_u32(const EmbedLaneMulU32Cfg *args)
{
#if EMBED_LANE_VECTOR
    return args->one * args->other;
#else
    embed_lane lane;
#define EMBED_LANE_MUL_U32_WORD(at_) lane.at[at_] = args->one.at[at_] * args->other.at[at_];
    EMBED_LANE_WORDS(EMBED_LANE_MUL_U32_WORD)
#undef EMBED_LANE_MUL_U32_WORD
    return lane;
#endif
}

/** @brief The parameters of embed_lane_shift_right_u32. */
typedef struct
{
    const embed_lane lane; /**< A lane. */
    const embed_u32 count; /**< The bits, below 32. */
} EmbedLaneShiftRightU32Cfg;

/**
 * @brief The 32-bit lanes of a lane shifted right by `count` bits, zeros shifted in.
 *
 * @param[in] args The parameters, as EmbedLaneShiftRightU32Cfg holds them [BORROWS].
 * @return         The shifted lanes.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_shift_right_u32(const EmbedLaneShiftRightU32Cfg *args)
{
#if EMBED_LANE_VECTOR
    return args->lane >> args->count;
#else
    embed_lane shifted;
#define EMBED_LANE_SHIFT_RIGHT_U32_WORD(at_) shifted.at[at_] = args->lane.at[at_] >> args->count;
    EMBED_LANE_WORDS(EMBED_LANE_SHIFT_RIGHT_U32_WORD)
#undef EMBED_LANE_SHIFT_RIGHT_U32_WORD
    return shifted;
#endif
}

/** @brief The parameters of embed_lane_shift_left_u32. */
typedef struct
{
    const embed_lane lane; /**< A lane. */
    const embed_u32 count; /**< The bits, below 32. */
} EmbedLaneShiftLeftU32Cfg;

/**
 * @brief The 32-bit lanes of a lane shifted left by `count` bits, zeros shifted in.
 *
 * @param[in] args The parameters, as EmbedLaneShiftLeftU32Cfg holds them [BORROWS].
 * @return         The shifted lanes.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane embed_lane_shift_left_u32(const EmbedLaneShiftLeftU32Cfg *args)
{
#if EMBED_LANE_VECTOR
    return args->lane << args->count;
#else
    embed_lane shifted;
#define EMBED_LANE_SHIFT_LEFT_U32_WORD(at_) shifted.at[at_] = args->lane.at[at_] << args->count;
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

/** @brief The parameters of embed_lane_bytes_equal. */
typedef struct
{
    const embed_lane one;   /**< A lane. */
    const embed_lane other; /**< A lane. */
} EmbedLaneBytesEqualCfg;

/**
 * @brief Which bytes of two lanes are equal, one bit a byte, the lowest byte the lowest bit.
 *
 * @param[in] args The parameters, as EmbedLaneBytesEqualCfg holds them [BORROWS].
 * @return         The mask.
 * @note On the vector arm, the code compares the bytes as a byte vector, and each equal byte becomes
 *       all ones. Where the compiler has the byte mask builtin for the width of the lane, it reads the
 *       mask in one instruction. That builtin is EMBED_BYTE_MASK_256 or EMBED_BYTE_MASK_128. Otherwise
 *       one multiply over the full lane gathers the four bits of each 32-bit lane into a nibble, and the
 *       code reads out the nibbles. On the portable arm, each 32-bit lane finds its zero bytes by the
 *       carry test of the word, and gathers them the same way.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane_mask embed_lane_bytes_equal(const EmbedLaneBytesEqualCfg *args)
{
    embed_lane_mask mask = 0u;
#if EMBED_LANE_VECTOR
    // Explicit casts view each lane as its bytes, and read the signed bytes of the compare, 0 or -1, as 0 or 0xFF
    const embed_lane_bytes equal = (embed_lane_bytes)((embed_lane_bytes)args->one == (embed_lane_bytes)args->other);
#if EMBED_HAS_BYTE_MASK_256 && (EMBED_LANE_BITS == 256)
    // Explicit casts read the builtin's int as the 32 unsigned bits it holds
    mask = (embed_lane_mask)(embed_u32)EMBED_BYTE_MASK_256((embed_lane_chars)equal);
#elif EMBED_HAS_BYTE_MASK_128 && (EMBED_LANE_BITS == 128)
    // Explicit casts read the builtin's int as the 16 unsigned bits it holds
    mask = (embed_lane_mask)(embed_u32)EMBED_BYTE_MASK_128((embed_lane_chars)equal);
#else
    // Explicit cast views the bytes as the 32-bit lanes again, and the and keeps one bit of each byte
    const embed_lane bits = (embed_lane)equal & EMBED_CALL(embed_lane_fill_u32, EmbedLaneFillU32Cfg, 0x01010101u);
    const embed_lane nibbles =
        (bits * EMBED_CALL(embed_lane_fill_u32, EmbedLaneFillU32Cfg, EMBED_LANE_NIBBLE_GATHER)) >> 24u;
    // Explicit cast widens the four bits of each 32-bit lane to the mask, before the shift to their place
#define EMBED_LANE_NIBBLE_WORD(at_) mask |= (embed_lane_mask)nibbles[at_] << (4u * (at_));
    EMBED_LANE_WORDS(EMBED_LANE_NIBBLE_WORD)
#undef EMBED_LANE_NIBBLE_WORD
#endif
#else
    // A byte of the difference of a word is zero where its high bit is clear after the carry test. The sum
    // carries out of each byte that holds a bit, and the complement retains the other bytes.
#define EMBED_LANE_ZERO_BYTES(at_)                                                                                     \
    ((~((((args->one.at[at_] ^ args->other.at[at_]) & 0x7f7f7f7fu) + 0x7f7f7f7fu) |                                    \
        (args->one.at[at_] ^ args->other.at[at_]) | 0x7f7f7f7fu)) >>                                                   \
     7u)
#define EMBED_LANE_EQUAL_WORD(at_)                                                                                     \
    mask |= (embed_lane_mask)((EMBED_LANE_ZERO_BYTES(at_) * EMBED_LANE_NIBBLE_GATHER) >> 24u) << (4u * (at_));
    EMBED_LANE_WORDS(EMBED_LANE_EQUAL_WORD)
#undef EMBED_LANE_EQUAL_WORD
#undef EMBED_LANE_ZERO_BYTES
#endif
    return mask;
}

/** @brief The parameters of embed_lane_bytes_zero. */
typedef struct
{
    const embed_lane lane; /**< A lane. */
} EmbedLaneBytesZeroCfg;

/**
 * @brief Which bytes of a lane are zero, one bit a byte, the lowest byte the lowest bit.
 *
 * @param[in] args The parameters, as EmbedLaneBytesZeroCfg holds them [BORROWS].
 * @return         The mask.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane_mask embed_lane_bytes_zero(const EmbedLaneBytesZeroCfg *args)
{
    return EMBED_CALL(embed_lane_bytes_equal, EmbedLaneBytesEqualCfg, args->lane,
                      EMBED_CALL(embed_lane_fill_u32, EmbedLaneFillU32Cfg, 0u));
}

/** @brief The parameters of embed_lane_bytes_differ. */
typedef struct
{
    const embed_lane one;   /**< A lane. */
    const embed_lane other; /**< The lane to compare it with. */
} EmbedLaneBytesDifferCfg;

/**
 * @brief Which bytes two lanes differ in, one bit a byte, the lowest byte the lowest bit.
 *
 * @param[in] args The parameters, as EmbedLaneBytesDifferCfg holds them [BORROWS].
 * @return         The mask.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_lane_mask embed_lane_bytes_differ(const EmbedLaneBytesDifferCfg *args)
{
    return EMBED_LANE_MASK_ALL & ~EMBED_CALL(embed_lane_bytes_equal, EmbedLaneBytesEqualCfg, args->one, args->other);
}

/**
 * @brief The dispatch table of embed_lane.h: one member for each public function, in the order of the header.
 *
 * @note EMBED_TABLE_LAYOUT asserts that the members sit at consecutive EMBED_FUNCTION_POINTER_BYTES offsets.
 */
typedef struct
{
    embed_u32 (*mask_first)(const EmbedLaneMaskFirstCfg *args);           /**< embed_lane_mask_first. */
    embed_lane_mask (*mask_before)(const EmbedLaneMaskBeforeCfg *args);   /**< embed_lane_mask_before. */
    embed_lane (*load)(const EmbedLaneLoadCfg *args);                     /**< embed_lane_load. */
    embed_lane (*load_aligned)(const EmbedLaneLoadAlignedCfg *args);      /**< embed_lane_load_aligned. */
    void (*put)(const EmbedLanePutCfg *args);                             /**< embed_lane_put. */
    void (*put_aligned)(const EmbedLanePutAlignedCfg *args);              /**< embed_lane_put_aligned. */
    embed_lane (*fill_u32)(const EmbedLaneFillU32Cfg *args);              /**< embed_lane_fill_u32. */
    embed_lane (*fill_byte)(const EmbedLaneFillByteCfg *args);            /**< embed_lane_fill_byte. */
    embed_lane (*bitwise_and)(const EmbedLaneAndCfg *args);               /**< embed_lane_and. */
    embed_lane (*bitwise_or)(const EmbedLaneOrCfg *args);                 /**< embed_lane_or. */
    embed_lane (*bitwise_xor)(const EmbedLaneXorCfg *args);               /**< embed_lane_xor. */
    embed_lane (*add_u32)(const EmbedLaneAddU32Cfg *args);                /**< embed_lane_add_u32. */
    embed_lane (*sub_u32)(const EmbedLaneSubU32Cfg *args);                /**< embed_lane_sub_u32. */
    embed_lane (*mul_u32)(const EmbedLaneMulU32Cfg *args);                /**< embed_lane_mul_u32. */
    embed_lane (*shift_right_u32)(const EmbedLaneShiftRightU32Cfg *args); /**< embed_lane_shift_right_u32. */
    embed_lane (*shift_left_u32)(const EmbedLaneShiftLeftU32Cfg *args);   /**< embed_lane_shift_left_u32. */
    embed_lane_mask (*bytes_equal)(const EmbedLaneBytesEqualCfg *args);   /**< embed_lane_bytes_equal. */
    embed_lane_mask (*bytes_zero)(const EmbedLaneBytesZeroCfg *args);     /**< embed_lane_bytes_zero. */
    embed_lane_mask (*bytes_differ)(const EmbedLaneBytesDifferCfg *args); /**< embed_lane_bytes_differ. */
} EmbedLaneNs;
EMBED_TABLE_LAYOUT(EmbedLaneNs, mask_first, mask_before, load, load_aligned, put, put_aligned, fill_u32, fill_byte,
                   bitwise_and, bitwise_or, bitwise_xor, add_u32, sub_u32, mul_u32, shift_right_u32, shift_left_u32,
                   bytes_equal, bytes_zero, bytes_differ);

/**
 * @brief The table of embed_lane.h. A call through it folds to a direct call under the optimizer and LTO.
 *
 * @note EMBED_TABLE_STORAGE holds the table. Under nvcc each pass gets its own copy, and a kernel calls through it.
 */
EMBED_TABLE_STORAGE EmbedLaneNs embed_lane_table EMBED_UNUSED = {embed_lane_mask_first,
                                                                 embed_lane_mask_before,
                                                                 embed_lane_load,
                                                                 embed_lane_load_aligned,
                                                                 embed_lane_put,
                                                                 embed_lane_put_aligned,
                                                                 embed_lane_fill_u32,
                                                                 embed_lane_fill_byte,
                                                                 embed_lane_and,
                                                                 embed_lane_or,
                                                                 embed_lane_xor,
                                                                 embed_lane_add_u32,
                                                                 embed_lane_sub_u32,
                                                                 embed_lane_mul_u32,
                                                                 embed_lane_shift_right_u32,
                                                                 embed_lane_shift_left_u32,
                                                                 embed_lane_bytes_equal,
                                                                 embed_lane_bytes_zero,
                                                                 embed_lane_bytes_differ};

EMBED_END_DECLS

#endif
