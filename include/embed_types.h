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
 * @note Every name here is defined once. A consumer that wants its own vocabulary aliases these
 *       rather than declaring its own, so two libraries in one build agree on what a word is
 *       instead of each deciding separately.
 * @note uint8_t is optional in C11, and a part whose byte is not eight bits has none. Naming the
 *       widths here makes that one file to port.
 */
#ifndef EMBED_TYPES_H
#define EMBED_TYPES_H

#include <stdint.h>

#include "embed_compiler_directives.h"

EMBED_BEGIN_DECLS

/**
 * @brief The eight-bit unsigned integer, which is the byte every scan and copy walks.
 *
 * @note Pinned below, and the only one of the four whose assertion can fail on a real target. A
 *       part with no eight-bit type has no byte to walk.
 */
typedef uint8_t embed_u8;

/** @brief The sixteen-bit unsigned integer. Pinned below. */
typedef uint16_t embed_u16;

/** @brief The thirty-two-bit unsigned integer. Pinned below. */
typedef uint32_t embed_u32;

/** @brief The sixty-four-bit unsigned integer. Pinned below. */
typedef uint64_t embed_u64;

/**
 * @brief The eight-bit signed integer.
 *
 * @note The four signed aliases carry no assertion of their own. Each stdint type they alias is
 *       already exact by the standard, and the unsigned four are pinned because the word and the
 *       index are built out of them.
 */
typedef int8_t embed_i8;

/** @brief The sixteen-bit signed integer. Carries no assertion, as embed_i8 describes. */
typedef int16_t embed_i16;

/** @brief The thirty-two-bit signed integer. Carries no assertion, as embed_i8 describes. */
typedef int32_t embed_i32;

/** @brief The sixty-four-bit signed integer. Carries no assertion, as embed_i8 describes. */
typedef int64_t embed_i64;

/**
 * @brief The truth value.
 *
 * @note Each dialect's own boolean, so this needs no header and cannot collide with a vendor bool
 *       macro, which several embedded SDKs define. That is why <stdbool.h> is not reached here.
 * @note Both spellings normalize any nonzero to 1 on assignment, so a value crossing between a
 *       library and its caller compares the same on either side. A plain eight-bit alias would not,
 *       and two true values would then differ.
 */
#ifdef __cplusplus
typedef bool embed_bool;
#else

typedef _Bool embed_bool;
#endif

/**
 * @brief True, cast so it carries embed_bool rather than int.
 *
 * @note The cast is what keeps a comparison against it in the boolean type. An unparenthesized 1
 *       would promote and compare as int, which reads the same and is a different expression.
 */
#define EMBED_TRUE ((embed_bool)1)

/**
 * @brief False, cast so it carries embed_bool rather than int.
 *
 * @note The counterpart to EMBED_TRUE, cast for the same reason.
 */
#define EMBED_FALSE ((embed_bool)0)

/**
 * @brief The width of the machine word in bits, derived from the target's pointer width.
 *
 * @note UINTPTR_MAX is the compiler's own statement about the target, so a build states nothing and
 *       cannot state it wrong. A knob that has to be set is a knob that gets set to the wrong
 *       number on a new part.
 * @note A build whose register width differs from its pointer width defines this on the command
 *       line ahead of this header. That is the x32 and ILP32-on-64 case, and the large-model
 *       eight-bit case, where a pointer is wider than the register that indexes with it.
 * @warning #error where UINTPTR_MAX is absent, which C11 permits, and where it names a width other
 *          than 16, 32 or 64. Neither is guessed at: a silent fall to the narrowest width would
 *          halve every lane on a machine that has more.
 */
#ifndef EMBED_WORD_BITS
/**
 * @brief Set to 0 where the word width was derived here, 1 where the build stated it.
 *
 * @note This header is the only place that knows which of the two happened, so it answers rather
 *       than asking a build to state it a second time. A build that overrode the width and had to
 *       set this as well would be carrying two copies of one fact, and the copy nobody remembers to
 *       set is the one that reports the override as a defect.
 * @note What reads it is any check that the word matches the pointer it would have been derived
 *       from. That holds only where the derivation ran, and an override says the two differ on
 *       purpose.
 * @note Defined on both arms, so #ifdef EMBED_WORD_BITS_WAS_OVERRIDDEN is always true. Test it
 *       with #if.
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

/** @brief Set to 1 where the build stated EMBED_WORD_BITS ahead of this header. */
#define EMBED_WORD_BITS_WAS_OVERRIDDEN 1
#endif

/**
 * @brief The width of an offset or a length in bits.
 *
 * @note Stated apart from the word because an offset does not need the register's full width. A
 *       64-bit host carrying 32-bit offsets halves what a descriptor costs.
 * @note Defaults to the word width at 16 bits and to 32 above it, so the index never outgrows the
 *       register it is carried in and the default needs no assertion to be safe.
 * @warning A build wanting 16-bit offsets on a wider register defines this ahead of this header.
 */
#ifndef EMBED_INDEX_BITS
#if EMBED_WORD_BITS == 16
#define EMBED_INDEX_BITS 16
#else
#define EMBED_INDEX_BITS 32
#endif
#endif

/**
 * @brief The unsigned word, EMBED_WORD_BITS wide, which every lane operation runs in.
 *
 * @note The register a consumer reasons in. A lane mask, a zero test and a byte count are all built
 *       to this width, so changing EMBED_WORD_BITS changes what a single operation covers.
 * @note Declared on each of the three arms below. The 64-bit arm carries this block, being the one
 *       the compiler meets first.
 */
#if EMBED_WORD_BITS == 64
typedef embed_u64 embed_word;

/**
 * @brief The signed word, the same register as embed_word.
 *
 * @note Carries a result that has gone negative, which an unsigned word would wrap instead. The
 *       assertion below holds it to embed_word's size, so the two are interchangeable as storage.
 */
typedef embed_i64 embed_iword;
#elif EMBED_WORD_BITS == 32
typedef embed_u32 embed_word;
typedef embed_i32 embed_iword;
#elif EMBED_WORD_BITS == 16
typedef embed_u16 embed_word;
typedef embed_i16 embed_iword;
#else
#error "EMBED_WORD_BITS must be 16, 32 or 64"
#endif

/**
 * @brief The index type, EMBED_INDEX_BITS wide, carrying every offset and length.
 *
 * @note Never size_t, whose width is whatever the target's pointer happens to be. The same
 *       expression would then be 32-bit index arithmetic on a device and 64-bit on the host that
 *       tests it, which is different emitted code from one source.
 * @note Declared on both arms below. The 32-bit arm carries this block, being the one the compiler
 *       meets first.
 */
#if EMBED_INDEX_BITS == 32
typedef embed_u32 embed_index;
#elif EMBED_INDEX_BITS == 16
typedef embed_u16 embed_index;
#else
#error "EMBED_INDEX_BITS must be 16 or 32"
#endif

/**
 * @brief The word again, readable from an address that is not a multiple of its width.
 *
 * @note EMBED_RAW lowers the alignment to one and permits the aliasing, which together are what
 *       make a load through this type defined at any address. The plain embed_word is not.
 * @warning Where either attribute is unavailable this type is the plain word and the reads through
 *          it are undefined. Nothing diagnoses that, which is what the two warnings on EMBED_ALIGN
 *          and EMBED_ALIAS describe.
 */
typedef embed_word embed_raw_word EMBED_RAW;

/**
 * @brief Asserts embed_u8 is exactly eight bits.
 *
 * @note The one assertion here that can fail on a real target. A part whose byte is wider has no
 *       eight-bit type, and stdint omits uint8_t rather than approximating it.
 */
EMBED_STATIC_ASSERT(sizeof(embed_u8) == 1, "embed_u8 must be exactly 8 bits: this target has no 8-bit type");

/** @brief Asserts embed_u16 is exactly sixteen bits. */
EMBED_STATIC_ASSERT(sizeof(embed_u16) * 8u == 16u, "embed_u16 must be exactly 16 bits");

/** @brief Asserts embed_u32 is exactly thirty-two bits. */
EMBED_STATIC_ASSERT(sizeof(embed_u32) * 8u == 32u, "embed_u32 must be exactly 32 bits");

/** @brief Asserts embed_u64 is exactly sixty-four bits. */
EMBED_STATIC_ASSERT(sizeof(embed_u64) * 8u == 64u, "embed_u64 must be exactly 64 bits");

/**
 * @brief Asserts embed_word is the width EMBED_WORD_BITS names.
 *
 * @note Every offset a consumer computes is derived from this width at compile time rather than
 *       measured at run time, so a width that is not what the code assumed has to fail the build.
 */
EMBED_STATIC_ASSERT(sizeof(embed_word) * 8u == EMBED_WORD_BITS, "embed_word must be exactly EMBED_WORD_BITS wide");

/** @brief Asserts the signed word occupies the same register as the unsigned one. */
EMBED_STATIC_ASSERT(sizeof(embed_iword) == sizeof(embed_word), "the signed word must be the same register as the word");

/** @brief Asserts embed_index is the width EMBED_INDEX_BITS names. */
EMBED_STATIC_ASSERT(sizeof(embed_index) * 8u == EMBED_INDEX_BITS, "embed_index must be exactly EMBED_INDEX_BITS wide");

/** @brief Asserts an index fits the register it is carried in. */
EMBED_STATIC_ASSERT(sizeof(embed_index) <= sizeof(embed_word), "an index must fit the register it is carried in");

/**
 * @brief A one-byte enum used only to prove EMBED_ENUM_PACKED reaches the compiler.
 *
 * @note Its range needs a single byte, so the assertion below fails exactly when packing is
 *       ignored. A compiler may accept the attribute and then disregard it, which no #if can see.
 * @warning A failure means EMBED_ENUM_PACKED expanded to nothing or was ignored. Every enum a
 *          consumer declares then widens to int, and any offset computed from a struct holding one
 *          is wrong.
 */
typedef enum EMBED_ENUM_PACKED
{
    EMBED_ENUM_PROBE_MIN = 0,   /**< Low end of the probe range. */
    EMBED_ENUM_PROBE_MAX = 255, /**< High end, the largest value one byte holds. */
} EmbedEnumProbe;

/** @brief Asserts the packed-enum attribute is honored, which proves it for every enum carrying it. */
EMBED_STATIC_ASSERT(sizeof(EmbedEnumProbe) == 1,
                    "EMBED_ENUM_PACKED is not honored here, so no enum keeps its declared width "
                    "(TI: pass --small_enum)");

EMBED_END_DECLS

#endif
