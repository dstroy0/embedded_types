/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_types.h
 * @brief The exact-width integers, the boolean, and the machine word and index types a consumer
 *        builds on.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note This header defines each name one time. A consumer that wants its own vocabulary makes
 *       aliases of these names. Then two libraries in one build agree on what a word is.
 * @note C11 makes uint8_t optional, and a part whose byte is not eight bits has none. To port to
 *       such a part, edit this header.
 */
#ifndef EMBED_TYPES_H
#define EMBED_TYPES_H

#include <stdint.h>

#include "embed_compiler_directives.h"

EMBED_BEGIN_DECLS

/**
 * @brief The eight-bit unsigned integer that every scan and copy walks.
 *
 * @note The assertion below pins the width. On a part whose byte is not eight bits, stdint omits
 *       uint8_t, and this typedef fails before the compiler reaches the assertion.
 */
typedef uint8_t embed_u8;

/** @brief The sixteen-bit unsigned integer, pinned by the assertion below. */
typedef uint16_t embed_u16;

/** @brief The thirty-two-bit unsigned integer, pinned by the assertion below. */
typedef uint32_t embed_u32;

/** @brief The sixty-four-bit unsigned integer, pinned by the assertion below. */
typedef uint64_t embed_u64;

/**
 * @brief The eight-bit signed integer.
 *
 * @note The four signed aliases carry no assertion of their own. int8_t through int64_t are
 *       exact-width by definition in stdint. The assertion on embed_iword below holds the signed
 *       word to embed_word's size.
 */
typedef int8_t embed_i8;

/** @brief The sixteen-bit signed integer. It has no assertion, for the reason on embed_i8. */
typedef int16_t embed_i16;

/** @brief The thirty-two-bit signed integer. It has no assertion, for the reason on embed_i8. */
typedef int32_t embed_i32;

/** @brief The sixty-four-bit signed integer. It has no assertion, for the reason on embed_i8. */
typedef int64_t embed_i64;

/**
 * @brief The truth value.
 *
 * @note C++ gets bool and C gets _Bool. Both are keywords, so this typedef needs no header.
 * @note This header does not include <stdbool.h>. Its bool macro collides with the bool that some
 *       embedded SDKs define for themselves.
 * @note Both types change any nonzero value to 1 on conversion. Then a value that crosses between a
 *       library and its caller compares equal on either side. A plain eight-bit alias retains the
 *       value that it gets, and two true values can differ.
 */
#ifdef __cplusplus
typedef bool embed_bool;
#else

typedef _Bool embed_bool;
#endif

/**
 * @brief True, cast to embed_bool.
 *
 * @note The cast gives the macro embed_bool type. A bare 1 would have type int, and sizeof would
 *       measure it as int.
 */
#define EMBED_TRUE ((embed_bool)1)

/**
 * @brief False, cast to embed_bool.
 *
 * @note The cast gives the macro embed_bool type, for the reason given on EMBED_TRUE.
 */
#define EMBED_FALSE ((embed_bool)0)

/**
 * @brief The width of the machine word in bits, derived from the target's pointer width.
 *
 * @note UINTPTR_MAX comes from the compiler. The build supplies no width of its own here, and it
 *       cannot supply a wrong one.
 * @note A build whose register width differs from its pointer width defines this on the command
 *       line ahead of this header. The x32 and ILP32-on-64 ABIs are examples. A large-model eight-bit
 *       target is another: its pointer is wider than the register that indexes with it.
 * @warning The #error arms below fire where UINTPTR_MAX is absent, and where it is not 16, 32 or 64
 *          bits wide. C11 makes uintptr_t optional, so its absence is legal. This header guesses
 *          neither case. A silent fall to the narrowest width would halve every lane on a machine
 *          that has more.
 */
#ifndef EMBED_WORD_BITS
/**
 * @brief Set to 0 where this header derives the word width, 1 where the build defines it.
 *
 * @note This header sets the flag itself. Otherwise a build that overrides the width must set a
 *       second macro to match, and the two can drift apart.
 * @note A check that the word width matches the pointer width holds only where the derivation ran.
 *       A build that overrides the width declares that the two differ.
 * @note Both arms define this macro, so #ifdef EMBED_WORD_BITS_WAS_OVERRIDDEN is always true. Test
 *       it with #if.
 */
#define EMBED_WORD_BITS_WAS_OVERRIDDEN 0

#if !defined(UINTPTR_MAX)
#error "embed_types.h needs UINTPTR_MAX to derive the word width - define EMBED_WORD_BITS instead"
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define EMBED_WORD_BITS 64
#elif UINTPTR_MAX == 0xFFFFFFFFu
#define EMBED_WORD_BITS 32
#elif UINTPTR_MAX == 0xFFFFu
#define EMBED_WORD_BITS 16
#else
#error "UINTPTR_MAX is not 16, 32 or 64 bits wide - define EMBED_WORD_BITS for this target"
#endif
#else

/** @brief Set to 1 where the build defined EMBED_WORD_BITS ahead of this header. */
#define EMBED_WORD_BITS_WAS_OVERRIDDEN 1
#endif

/**
 * @brief The width of an offset or a length in bits.
 *
 * @note This width is separate from the word width, because an offset does not need the full width
 *       of the register. A 64-bit host with 32-bit offsets halves the cost of a descriptor.
 * @note The default is the word width at 16 bits and 32 above it. That gives 16 on a 16-bit word,
 *       32 on a 32-bit word, and 32 on a 64-bit word, so the index fits the register in all three.
 * @warning A build that wants 16-bit offsets on a wider register defines this ahead of this header.
 *          The assertion below stops an override wider than the word.
 */
#ifndef EMBED_INDEX_BITS
#if EMBED_WORD_BITS == 16
#define EMBED_INDEX_BITS 16
#else
#define EMBED_INDEX_BITS 32
#endif
#endif

#if (EMBED_WORD_BITS != 16) && (EMBED_WORD_BITS != 32) && (EMBED_WORD_BITS != 64)
#error "EMBED_WORD_BITS must be 16, 32 or 64"
#endif
#if (EMBED_INDEX_BITS != 16) && (EMBED_INDEX_BITS != 32)
#error "EMBED_INDEX_BITS must be 16 or 32"
#endif

/**
 * @brief The unsigned word that every lane operation runs in, EMBED_WORD_BITS wide: the exact-width
 *        type above whose name ends in that width.
 *
 * @note A consumer reasons in this register. A lane mask, a zero test and a byte count all have this
 *       width, so a change to EMBED_WORD_BITS changes what one operation covers.
 * @note EMBED_CAT pastes the width onto the stem of the name, so one line serves each width. A build
 *       that defines EMBED_WORD_BITS gives a plain decimal, as 32, and not 32u.
 */
typedef EMBED_CAT(embed_u, EMBED_WORD_BITS) embed_word;

/**
 * @brief The signed word, the same register as embed_word.
 *
 * @note This type holds a result that goes below zero. An unsigned word would wrap it. The
 *       assertion below holds this type to the size of embed_word.
 */
typedef EMBED_CAT(embed_i, EMBED_WORD_BITS) embed_iword;

/**
 * @brief The index type, EMBED_INDEX_BITS wide, for every offset and length.
 *
 * @note This type is not size_t. size_t is as wide as the target's pointer. Then one expression
 *       would be 32-bit index arithmetic on a device and 64-bit on the host that tests it. One
 *       source would then emit different code on the two.
 * @note EMBED_CAT pastes the width onto the stem of the name, as for embed_word.
 */
typedef EMBED_CAT(embed_u, EMBED_INDEX_BITS) embed_index;

/**
 * @brief The word again, readable from an address that is not a multiple of its width.
 *
 * @note EMBED_RAW lowers the alignment to one and permits access through an alias. A load through
 *       this type has a defined result at any address only with both. A load through the plain
 *       embed_word at an unaligned address stays undefined.
 * @warning Where either attribute is not available, this type is the plain word, and C leaves a read
 *          through it undefined. Nothing diagnoses that. The warnings on EMBED_ALIGN and EMBED_ALIAS
 *          in embed_compiler_directives.h cover the same failure.
 */
typedef embed_word embed_raw_word EMBED_RAW;

/**
 * @brief The stored form of a 64-bit count: one embed_u64 on a host and on a device, and two embed_u32 halves on an
 *        embedded target.
 *
 * @note A value is an embed_u64 in each arm, and embed_stored_u64_get and embed_stored_u64_set convert it. The
 *       stored form is the only difference between the arms.
 * @note On an embedded target the halves are the lower word and the upper word of the count. Each is one plain load
 *       or store, and the stored form takes the alignment of an embed_u32. A count that stays below 2^32 changes only its
 *       lower word.
 */
#if EMBED_TARGET == EMBED_TARGET_EMBEDDED
typedef struct
{
    embed_u32 lower; /**< Bits 0 to 31 of the count. */
    embed_u32 upper; /**< Bits 32 to 63 of the count. */
} embed_stored_u64;
#else
typedef embed_u64 embed_stored_u64;
#endif

/**
 * @brief The count that a stored form holds.
 *
 * @param[in] stored The stored form [BORROWS].
 * @return           The count.
 */
EMBED_INLINE embed_u64 embed_stored_u64_get(const embed_stored_u64 *stored)
{
#if EMBED_TARGET == EMBED_TARGET_EMBEDDED
    // Explicit casts widen each half to the count before the upper half moves to bits 32 to 63
    return ((embed_u64)stored->upper << 32) | (embed_u64)stored->lower;
#else
    return *stored;
#endif
}

/**
 * @brief Writes a count into a stored form.
 *
 * @param[out] stored The stored form [BORROWS].
 * @param[in]  count  The count.
 * @note On an embedded target the write stores the upper word only where it changes.
 */
EMBED_INLINE void embed_stored_u64_set(embed_stored_u64 *stored, embed_u64 count)
{
#if EMBED_TARGET == EMBED_TARGET_EMBEDDED
    // Explicit casts narrow the count to its two halves, each of which fits an embed_u32
    const embed_u32 upper = (embed_u32)(count >> 32);
    stored->lower = (embed_u32)count;
    if (stored->upper != upper)
    {
        stored->upper = upper;
    }
#else
    *stored = count;
#endif
}

/** @brief Asserts the stored form of a 64-bit count takes 8 bytes in each arm. */
EMBED_STATIC_ASSERT(sizeof(embed_stored_u64) == 8u, "embed_stored_u64 must take 8 bytes");

/**
 * @brief Asserts embed_u8 is exactly eight bits.
 *
 * @note A stdint that conforms to C defines uint8_t at exactly eight bits, so this assertion guards
 *       against a stdint that does not conform.
 */
EMBED_STATIC_ASSERT(sizeof(embed_u8) == 1u, "embed_u8 must be exactly 8 bits: this target has no 8-bit type");

/** @brief Asserts embed_u16 is exactly sixteen bits. */
EMBED_STATIC_ASSERT(sizeof(embed_u16) == 2u, "embed_u16 must be exactly 16 bits");

/** @brief Asserts embed_u32 is exactly thirty-two bits. */
EMBED_STATIC_ASSERT(sizeof(embed_u32) == 4u, "embed_u32 must be exactly 32 bits");

/** @brief Asserts embed_u64 is exactly sixty-four bits. */
EMBED_STATIC_ASSERT(sizeof(embed_u64) == 8u, "embed_u64 must be exactly 64 bits");

/**
 * @brief Asserts embed_word is EMBED_WORD_BITS wide.
 *
 * @note A consumer derives every offset from this width at compile time. Nothing measures it at run
 *       time. A width that differs from what the code assumed has to fail the build.
 */
EMBED_STATIC_ASSERT(sizeof(embed_word) == EMBED_WORD_BITS / 8u, "embed_word must be exactly EMBED_WORD_BITS wide");

/** @brief Asserts embed_iword is the same size as embed_word. */
EMBED_STATIC_ASSERT(sizeof(embed_iword) == sizeof(embed_word), "the signed word must be the same register as the word");

/** @brief Asserts embed_index is EMBED_INDEX_BITS wide. */
EMBED_STATIC_ASSERT(sizeof(embed_index) == EMBED_INDEX_BITS / 8u, "embed_index must be exactly EMBED_INDEX_BITS wide");

/** @brief Asserts embed_index is no larger than embed_word. */
EMBED_STATIC_ASSERT(sizeof(embed_index) <= sizeof(embed_word), "an index must fit the register it is carried in");

/**
 * @brief A one-byte enum used only to prove EMBED_ENUM_BYTE gives one byte on this compiler.
 *
 * @note Its range needs a single byte, so the assertion below fails only where the compiler accepted
 *       the packed attribute and disregarded it.
 * @warning A failure means that every enum a consumer declares through EMBED_ENUM_BYTE widens. Then
 *          any offset that comes from a struct with such an enum is wrong.
 */
EMBED_ENUM_BYTE(EmbedEnumProbe, EMBED_ENUM_PROBE_MIN = 0, /**< Low end of the probe range. */
                EMBED_ENUM_PROBE_MAX = 255                /**< High end, the largest value eight bits hold. */
);

/** @brief Asserts EMBED_ENUM_BYTE gave the probe one byte. */
EMBED_STATIC_ASSERT(sizeof(EmbedEnumProbe) == 1,
                    "EMBED_ENUM_BYTE gave an enum more than one byte here, so no enum retains its declared width "
                    "(TI: pass --small_enum)");

EMBED_END_DECLS

#endif
