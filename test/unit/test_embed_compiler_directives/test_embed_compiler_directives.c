/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_compiler_directives.c
 * @brief Exercises the feature gates, the static assertion, the counting and pasting macros, the
 *        call shape, and each attribute wrapper the header declares.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note Reaches <stdint.h> rather than embed_types.h. The header under test declares no width of
 *       its own, and a suite that borrowed embed_types.h could fail for a defect in either one.
 * @note An attribute wrapper expands to nothing where its attribute is unavailable, which costs
 *       speed or a diagnostic and never correctness. A case for one of those is guarded on
 *       EMBED_HAS_ATTRIBUTE and reports TEST_IGNORE on the other arm, so a build that cannot carry
 *       the attribute says so in the run rather than passing a case that measured nothing.
 * @note Unity's generator reads case names out of the source text and does not see a preprocessor
 *       conditional, so every #if here sits inside a case body. A case defined inside one would be
 *       called by the runner on the arm where its definition is gone, and the suite would fail to
 *       link.
 */
#include <stdint.h>
#include <string.h>

#include "embed_compiler_directives.h"

#include "unity.h"

/**
 * @brief Asserts EMBED_GNU_ATTRIBUTES carries a value rather than only a definition.
 *
 * @note The header defines it on both arms so that #if always has a number. An arm that failed to
 *       define it would make #if read 0 and switch off every attribute in a build that supports
 *       them, which nothing else diagnoses.
 */
EMBED_STATIC_ASSERT(EMBED_GNU_ATTRIBUTES == 0 || EMBED_GNU_ATTRIBUTES == 1, "EMBED_GNU_ATTRIBUTES must be 0 or 1");

/** @brief Asserts EMBED_BIG_ENDIAN carries a value, for the reason EMBED_GNU_ATTRIBUTES describes. */
EMBED_STATIC_ASSERT(EMBED_BIG_ENDIAN == 0 || EMBED_BIG_ENDIAN == 1, "EMBED_BIG_ENDIAN must be 0 or 1");

/** @brief Asserts EMBED_FAST_UNALIGNED_LOAD carries a value, for the same reason. */
EMBED_STATIC_ASSERT(EMBED_FAST_UNALIGNED_LOAD == 0 || EMBED_FAST_UNALIGNED_LOAD == 1,
                    "EMBED_FAST_UNALIGNED_LOAD must be 0 or 1");

/**
 * @brief What EMBED_HAS_BUILTIN answered for a name no compiler defines.
 *
 * @note Recorded here because __has_builtin is a preprocessor operator. Evaluating it in the
 *       preprocessor and carrying the answer down as a literal is what lets a case compare it.
 * @note Zero on both arms of the header: __has_builtin reports 0 for a name it does not know, and
 *       the fallback is 0 outright.
 */
#if EMBED_HAS_BUILTIN(embed_probe_not_a_builtin)
#define TEST_UNKNOWN_BUILTIN_ANSWER 1
#else

#define TEST_UNKNOWN_BUILTIN_ANSWER 0
#endif

/**
 * @brief What EMBED_HAS_ATTRIBUTE answered for a name no compiler defines.
 *
 * @note Recorded for the reason TEST_UNKNOWN_BUILTIN_ANSWER describes.
 * @warning Only the arm where __has_attribute exists can be checked. The header's fallback answers
 *          EMBED_GNU_ATTRIBUTES for every name asked about, so it cannot tell an unknown attribute
 *          from a known one, and the case that reads this is guarded to match.
 */
#if defined(__has_attribute)
#if EMBED_HAS_ATTRIBUTE(embed_probe_not_an_attribute)
#define TEST_UNKNOWN_ATTRIBUTE_ANSWER 1
#else
#define TEST_UNKNOWN_ATTRIBUTE_ANSWER 0
#endif
#else

#define TEST_UNKNOWN_ATTRIBUTE_ANSWER EMBED_GNU_ATTRIBUTES
#endif

/** @brief A value the paste case reaches only by building its name from two tokens. */
#define TEST_PASTED_NAME_7 7

/**
 * @brief A one-byte enum, declared to prove EMBED_ENUM_PACKED reaches the compiler.
 *
 * @note Its range needs a single byte, so a size of one is the attribute being honored and a size
 *       of int is the attribute being ignored. A compiler may accept it and then disregard it,
 *       which no #if can see.
 */
typedef enum EMBED_ENUM_PACKED
{
    TEST_PACKED_ENUM_MIN = 0,   /**< Low end of the probe range. */
    TEST_PACKED_ENUM_MAX = 255, /**< High end, the largest value one byte holds. */
} PackedEnumProbe;

/**
 * @brief A word whose alignment EMBED_ALIGN raised above its natural one.
 *
 * @note Sixteen rather than eight, so the answer cannot be the natural alignment of any type this
 *       file declares and a raise that vanished is visible.
 */
typedef uint32_t RaisedAlignmentProbe EMBED_ALIGN(16);

/**
 * @brief A word whose alignment EMBED_ALIGN lowered to one byte.
 *
 * @note The direction embed_types.h depends on. A lower that vanished leaves the type at its
 *       natural alignment while the code still reads it from any address.
 */
typedef uint32_t LoweredAlignmentProbe EMBED_ALIGN(1);

/**
 * @brief A word carrying both halves of EMBED_RAW, readable from any address.
 *
 * @note The alignment permits the address and the aliasing permits the bytes another type owns.
 *       Either alone leaves the read undefined, which is why the header spells the pair once.
 */
typedef uint32_t RawProbeWord EMBED_RAW;

/**
 * @brief Doubles its operand, so a caller can tell the body ran.
 *
 * @param[in] value Operand to double.
 * @return          Twice value.
 * @note Marked EMBED_INLINE, which requires inlining rather than asking for it. What a case can see
 *       is that the marked definition is still a definition and its body still runs; whether the
 *       call was inlined is not observable from the language.
 */
EMBED_INLINE uint32_t inline_probe_doubled(uint32_t value)
{
    return value * 2u;
}

/**
 * @brief Quadruples its operand by calling the inline helper twice.
 *
 * @param[in] value Operand to quadruple.
 * @return          Four times value.
 * @note Marked EMBED_FLATTEN, which asks that the bodies it calls be inlined into it. As with
 *       EMBED_INLINE the result is the only observable part, and it must not change.
 */
static EMBED_FLATTEN uint32_t flatten_probe_quadrupled(uint32_t value)
{
    return inline_probe_doubled(inline_probe_doubled(value));
}

/**
 * @brief Declares the weak default ahead of its definition.
 *
 * @return 7, which is what a build supplying no replacement reaches.
 * @note The attribute goes on the declaration rather than the definition. A weak declaration that
 *       follows its own definition is diagnosed by GCC and ignored by some releases, so the order
 *       here is the one that holds everywhere.
 */
EMBED_WEAK uint32_t weak_probe_default(void);

uint32_t weak_probe_default(void)
{
    return 7u;
}

EMBED_BEGIN_DECLS

/**
 * @brief Declared between the two linkage guards, to prove they leave a C declaration alone.
 *
 * @return 5, so a caller can tell the definition below is what it reached.
 * @note Both guards expand to nothing in C, which is what this checks. The extern "C" arm belongs
 *       to a C++ consumer's compiler and is not reachable from a C suite.
 */
static uint32_t guarded_probe_entry(void);

EMBED_END_DECLS

static uint32_t guarded_probe_entry(void)
{
    return 5u;
}

/**
 * @brief A table defined, marked EMBED_UNUSED, and deliberately never named again.
 *
 * @note The only shape that exercises the marker. An object a case reads is referenced, and the
 *       suppression it carries would then be doing nothing. A clean build of this file is the
 *       whole result.
 */
static const uint32_t unreferenced_probe_table[] EMBED_UNUSED = {1u, 2u, 4u};

/**
 * @brief A file-scope value the diagnostic case deliberately shadows.
 *
 * @note Declared for that case alone. Shadowing an object the suite already has would make the
 *       warning depend on what else the file happens to declare, and the case reads this one
 *       before shadowing it so the outer name is not itself unreferenced.
 */
static const uint32_t shadowed_probe_value = 11u;

/** @brief The operand block the compound-literal call builds at its call site. */
typedef struct
{
    uint32_t first;
    uint32_t second;
    uint32_t third;
} CallProbeArgs;

/**
 * @brief Sums the three members so a caller can see which of them arrived.
 *
 * @param[in] args Operand block built by the caller [BORROWS].
 * @return         first plus twice second plus four times third, which no two inputs share.
 * @note The weights make the answer name which members were set, rather than only their total.
 */
static uint32_t call_probe(const CallProbeArgs *args)
{
    return args->first + (2u * args->second) + (4u * args->third);
}

/**
 * @brief Runs before each case, and has nothing to prepare.
 *
 * @note Unity calls this whether or not it does anything, and unity_internals.h declares it. Every
 *       case here reads types, macros and const objects, so there is nothing to reset.
 */
void setUp(void)
{
}

/** @brief Runs after each case, and has nothing to release. */
void tearDown(void)
{
}

/**
 * @brief Checks that each feature gate holds one of the two values it is documented to hold.
 *
 * @note The same claim the file-scope assertions above make, reached without going through
 *       EMBED_STATIC_ASSERT. A build where that macro was compiled out still reports the gates
 *       here, which is the reason test_embed_dispatch_layout re-measures its table size at run
 *       time.
 */
void test_every_feature_gate_carries_a_value_of_zero_or_one(void)
{
    TEST_ASSERT_TRUE_MESSAGE(EMBED_GNU_ATTRIBUTES == 0 || EMBED_GNU_ATTRIBUTES == 1, "EMBED_GNU_ATTRIBUTES is 0 or 1");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_BIG_ENDIAN == 0 || EMBED_BIG_ENDIAN == 1, "EMBED_BIG_ENDIAN is 0 or 1");
    TEST_ASSERT_TRUE_MESSAGE(EMBED_FAST_UNALIGNED_LOAD == 0 || EMBED_FAST_UNALIGNED_LOAD == 1,
                             "EMBED_FAST_UNALIGNED_LOAD is 0 or 1");
}

/**
 * @brief Checks that the static assertion is a declaration at block scope and that the dialect is C11.
 *
 * @note The file-scope form is proved by the assertions above compiling. The block-scope form is
 *       the one a consumer writes inside a function, and it has to take its trailing semicolon and
 *       leave one declaration rather than two.
 * @note The header raises #error below C11, so a build that reached this line is C11 or later and
 *       whichever spelling it selected exists.
 */
void test_the_static_assertion_compiles_at_file_and_block_scope(void)
{
    EMBED_STATIC_ASSERT(sizeof(uint32_t) == 4u, "a block-scope assertion is a declaration like any other");

    TEST_ASSERT_TRUE_MESSAGE(__STDC_VERSION__ >= 201112L, "the dialect is C11 or later");
}

/**
 * @brief Checks that the feature tests answer zero for a name no compiler defines.
 *
 * @note What makes the wrappers a question rather than a guess. A test that answered non-zero for
 *       an invented name would answer non-zero for every name, and every attribute below would be
 *       emitted on a compiler that rejects it.
 * @note The attribute half runs only where __has_attribute exists. The header's fallback answers
 *       EMBED_GNU_ATTRIBUTES whatever it is asked about, which is the warning on the macro rather
 *       than a defect a case can find.
 */
void test_the_feature_tests_answer_zero_for_a_name_nothing_defines(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, TEST_UNKNOWN_BUILTIN_ANSWER, "an unknown builtin answers 0");
#if defined(__has_attribute)
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, TEST_UNKNOWN_ATTRIBUTE_ANSWER, "an unknown attribute answers 0");
#endif
}

/**
 * @brief Checks that the endian answer agrees with how the target lays a word out in memory.
 *
 * @note The bytes are the independent side. EMBED_BIG_ENDIAN is derived from __BYTE_ORDER__, and
 *       reading the same macro back would restate the derivation rather than check it.
 * @note The first two assertions hold on every target. Answering 0 where the compiler states no
 *       order is the header's documented safe direction, so a big-endian part whose compiler is
 *       silent is not a failure and only the guarded assertion below can ask for agreement.
 */
void test_the_endian_answer_agrees_with_the_bytes_of_a_word(void)
{
    const uint32_t probe_value = 1u;
    uint8_t as_bytes[sizeof(uint32_t)];

    (void)memcpy(as_bytes, &probe_value, sizeof(as_bytes));

    const int bytes_are_big_endian = (as_bytes[sizeof(as_bytes) - 1u] == 1u);
    const int bytes_are_little_endian = (as_bytes[0] == 1u);

    TEST_ASSERT_TRUE_MESSAGE(bytes_are_big_endian || bytes_are_little_endian,
                             "the target lays a word out in one of the two orders");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_BIG_ENDIAN && bytes_are_little_endian,
                              "EMBED_BIG_ENDIAN is never 1 where the low byte comes first");
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
    TEST_ASSERT_EQUAL_INT_MESSAGE(EMBED_BIG_ENDIAN, bytes_are_big_endian,
                                  "where the compiler states an order, the macro is that order");
#endif
}

/**
 * @brief Checks that the argument count answers the number of arguments it was handed.
 *
 * @note Each expectation is the literal number of items written on the same line, counted by hand.
 *       Deriving it from EMBED_NARG would compare the macro with itself.
 * @note One, two, twenty-three and twenty-four are the cases that matter: the floor, the step, and
 *       both sides of the ceiling the family stops at.
 */
void test_the_argument_count_answers_the_length_of_the_list(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, EMBED_NARG(a), "one argument counts as one");
    TEST_ASSERT_EQUAL_INT_MESSAGE(2, EMBED_NARG(a, b), "two arguments count as two");
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, EMBED_NARG(a, b, c, d, e, f, g, h), "eight arguments count as eight");
    TEST_ASSERT_EQUAL_INT_MESSAGE(23, EMBED_NARG(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w),
                                  "twenty-three arguments count as twenty-three");
    TEST_ASSERT_EQUAL_INT_MESSAGE(24,
                                  EMBED_NARG(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x),
                                  "twenty-four arguments count as twenty-four");
}

/**
 * @brief Checks that the two-step paste joins a name to an expanded count.
 *
 * @note The point of the outer step. ## suppresses expansion of its own operands, so a caller
 *       pasting a macro's value rather than its name has to go through EMBED_CAT, and that is what
 *       the dispatch layout family selects its arity with.
 */
void test_the_paste_joins_a_name_to_an_expanded_count(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, EMBED_CAT(TEST_PASTED_NAME_, 7), "a literal suffix pastes");
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, EMBED_CAT(TEST_PASTED_NAME_, EMBED_NARG(a, b, c, d, e, f, g)),
                                  "an expanded count pastes");
}

/**
 * @brief Checks that the call shape passes named members and zeroes the ones left out.
 *
 * @note The zeroed member is the part worth proving. It is a property of the standard rather than
 *       of a compiler, and a consumer relies on it every time it omits a default.
 */
void test_the_call_passes_named_members_and_zeroes_the_rest(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(7u, EMBED_CALL(call_probe, CallProbeArgs, .first = 1u, .second = 1u, .third = 1u),
                                   "all three members arrive");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, EMBED_CALL(call_probe, CallProbeArgs, .second = 1u),
                                   "an omitted member is zero");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(4u, EMBED_CALL(call_probe, CallProbeArgs, .third = 1u),
                                   "only the named member is set");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(7u, EMBED_CALL(call_probe, CallProbeArgs, 1u, 1u, 1u),
                                   "positional initializers arrive");
}

/**
 * @brief Checks that the inline and flattened helpers still compute what their bodies say.
 *
 * @note Both attributes cost speed and never correctness, so the result is the whole contract. A
 *       wrapper that expanded to something the compiler misread would show up here as an answer
 *       rather than as a build failure.
 */
void test_the_inline_and_flattened_helpers_run_their_bodies(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(10u, inline_probe_doubled(5u), "the inline helper doubles");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(20u, flatten_probe_quadrupled(5u), "the flattened helper quadruples");
}

/**
 * @brief Checks that a packed enum is the one byte its range needs.
 *
 * @note The assertion embed_types.h makes about its own probe enum, made here against the wrapper
 *       rather than against a type built on it. A failure means every enum a consumer declares
 *       widens to int and any offset computed from a struct holding one is wrong.
 */
void test_a_packed_enum_takes_the_width_its_range_needs(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(255, TEST_PACKED_ENUM_MAX, "the range survives the attribute");
#if EMBED_HAS_ATTRIBUTE(packed)
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, sizeof(PackedEnumProbe), "a packed enum is one byte");
#else
    TEST_IGNORE_MESSAGE("EMBED_HAS_ATTRIBUTE(packed) is 0 here, so EMBED_ENUM_PACKED expands to nothing");
#endif
}

/**
 * @brief Checks that the alignment attribute raises alignment and lowers it.
 *
 * @note Both directions, because the header uses both. The raise is what an object needs before a
 *       use that assumes it; the lower to one is half of EMBED_RAW and is what makes a read from an
 *       odd address defined.
 */
void test_the_alignment_attribute_raises_and_lowers(void)
{
#if EMBED_HAS_ATTRIBUTE(aligned)
    TEST_ASSERT_EQUAL_size_t_MESSAGE(16u, _Alignof(RaisedAlignmentProbe), "a raise to sixteen reaches the type");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, _Alignof(LoweredAlignmentProbe), "a lower to one reaches the type");
#else
    TEST_IGNORE_MESSAGE("EMBED_HAS_ATTRIBUTE(aligned) is 0 here, so EMBED_ALIGN expands to nothing");
#endif
}

/**
 * @brief Checks that a word carrying EMBED_RAW reads correctly from an odd address.
 *
 * @note The expectation is assembled with memcpy over the same bytes, which is defined at any
 *       address and does not depend on byte order.
 * @note Guarded on the alignment half of EMBED_RAW. Where that attribute is absent the type is the
 *       plain word and the read below would be undefined rather than wrong, which is not something
 *       a case can report.
 */
void test_a_raw_word_reads_from_an_odd_address(void)
{
#if EMBED_HAS_ATTRIBUTE(aligned)
    static const uint8_t bytes[sizeof(uint32_t) * 2u] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u};
    const RawProbeWord *const at_one = (const RawProbeWord *)(const void *)(&bytes[1]);
    uint32_t expected = 0u;

    (void)memcpy(&expected, &bytes[1], sizeof(expected));
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(expected, *at_one, "a raw word at offset one matches the bytes it covers");
#else
    TEST_IGNORE_MESSAGE("EMBED_HAS_ATTRIBUTE(aligned) is 0 here, so a read at an odd address is undefined");
#endif
}

/**
 * @brief Checks that the diagnostic bracket suppresses a warning and stringizes its pragma text.
 *
 * @note The inner declaration shadows shadowed_probe_value, which -Wshadow reports and the suite is
 *       compiled with. The bracket is what lets it compile, so a bracket that reached the compiler
 *       wrongly fails this build rather than passing a case that measured nothing.
 * @note EMBED_DIAGNOSTIC_STRING is the only part a case can read back. _Pragma takes a string
 *       literal, so the text the ignore macro builds has to come out with the warning name still
 *       quoted inside it.
 * @note The pop is proved by the outer scope still diagnosing. Nothing below the bracket shadows
 *       anything, so a suppression left in force would not be visible here; what it would silence
 *       is the rest of the translation unit, which is the reason the bracket is written as a pair.
 */
void test_the_diagnostic_bracket_bounds_a_suppression(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(11u, shadowed_probe_value, "the file-scope value is what an outer read reaches");

    {
        EMBED_DIAGNOSTIC_PUSH
        EMBED_DIAGNOSTIC_IGNORE("-Wshadow")
        const uint32_t shadowed_probe_value = 3u;
        EMBED_DIAGNOSTIC_POP

        TEST_ASSERT_EQUAL_UINT_MESSAGE(3u, shadowed_probe_value, "the inner declaration is the one in scope");
    }

    TEST_ASSERT_EQUAL_STRING_MESSAGE("GCC diagnostic ignored \"-Wpadded\"",
                                     EMBED_DIAGNOSTIC_STRING(GCC diagnostic ignored "-Wpadded"),
                                     "the whole pragma text stringizes, quotes included");
}

/**
 * @brief Checks that the weak default links as itself where nothing replaces it.
 *
 * @note Half of what the attribute promises. The other half is a strong definition of the same
 *       name overriding this one, which needs a second translation unit that a one-file suite does
 *       not have.
 */
void test_the_weak_default_links_as_itself(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(7u, weak_probe_default(), "the weak definition is what the call reached");
}

/**
 * @brief Checks that the linkage guards leave a C declaration reachable.
 *
 * @note Both expand to nothing in C, so what could go wrong is a stray brace rather than a wrong
 *       answer. The call is what proves the declaration between them survived to name this
 *       definition.
 */
void test_the_declaration_guards_leave_a_c_declaration_intact(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(5u, guarded_probe_entry(), "the declaration between the guards is reachable");
}

/**
 * @brief Records that a definition marked EMBED_UNUSED and never referenced compiled quietly.
 *
 * @note Passes on the strength of the build. The marker suppresses a diagnostic, so the result is
 *       an absence, and naming unreferenced_probe_table here to check it would reference it and
 *       remove the condition the marker exists for.
 */
void test_an_unreferenced_marked_definition_compiles_quietly(void)
{
    TEST_PASS_MESSAGE("unreferenced_probe_table is defined, marked EMBED_UNUSED, and never named again");
}
