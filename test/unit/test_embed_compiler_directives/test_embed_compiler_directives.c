/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_compiler_directives.c
 * @brief Exercises the feature gates, the static assertion, the linkage guards, the diagnostic
 *        bracket, and each attribute wrapper the header declares.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note This reaches <stdint.h> and not embed_types.h. The header under test declares no width of
 *       its own, and a suite that borrowed embed_types.h could fail for a defect in either one.
 * @note This suite does not cover EMBED_NARG, EMBED_CAT and EMBED_CALL. The dispatch header declares
 *       them, this suite does not include it, and test_embed_dispatch_layout.c exercises all three
 *       against that header.
 * @note An attribute wrapper expands to nothing where the compiler has no such attribute. That costs
 *       speed or a diagnostic, never correctness. A case for one of those has an #if on
 *       EMBED_HAS_ATTRIBUTE and calls TEST_IGNORE_MESSAGE on the other arm. Unity marks the case
 *       ignored, and a case that measured nothing does not count as a pass.
 * @note The generator of Unity reads case names out of the source text and does not evaluate a
 *       preprocessor conditional. No case below sits inside an #if. The runner calls a case inside
 *       one on each arm, and on the arm with no definition the suite fails to link. The
 *       conditionals at file scope here define macros, and a case that needs a conditional puts it
 *       inside its own body.
 */
#include <stdint.h>

#include "embed_compiler_directives.h"

#include "unity.h"

/**
 * @brief Asserts EMBED_GNU_ATTRIBUTES holds a value and not only a definition.
 *
 * @note The header defines it on both arms so that #if always has a number. An arm that failed to
 *       define it would make #if evaluate 0 and switch off every attribute in a build that
 *       supports them. Nothing else diagnoses that.
 */
EMBED_STATIC_ASSERT(EMBED_GNU_ATTRIBUTES == 0 || EMBED_GNU_ATTRIBUTES == 1, "EMBED_GNU_ATTRIBUTES must be 0 or 1");

/** @brief Asserts EMBED_BIG_ENDIAN holds a value, for the reason given on EMBED_GNU_ATTRIBUTES. */
EMBED_STATIC_ASSERT(EMBED_BIG_ENDIAN == 0 || EMBED_BIG_ENDIAN == 1, "EMBED_BIG_ENDIAN must be 0 or 1");

/** @brief Asserts EMBED_FAST_UNALIGNED_LOAD holds a value, for the reason given on EMBED_GNU_ATTRIBUTES. */
EMBED_STATIC_ASSERT(EMBED_FAST_UNALIGNED_LOAD == 0 || EMBED_FAST_UNALIGNED_LOAD == 1,
                    "EMBED_FAST_UNALIGNED_LOAD must be 0 or 1");

/**
 * @brief Records what EMBED_HAS_BUILTIN evaluated to for a name no compiler defines, as 1 or 0.
 *
 * @note __has_builtin is a preprocessor operator and a case body cannot evaluate one. The #if below
 *       evaluates it and leaves a literal a case can compare against.
 * @note EMBED_HAS_BUILTIN is 0 on both of its arms for this name. The operator expands to 0 for a
 *       name that is not a builtin. The fallback arm expands to 0 for each name.
 */
#if EMBED_HAS_BUILTIN(embed_probe_not_a_builtin)
#define TEST_UNKNOWN_BUILTIN_ANSWER 1
#else

#define TEST_UNKNOWN_BUILTIN_ANSWER 0
#endif

/**
 * @brief Records what EMBED_HAS_ATTRIBUTE evaluated to for a name no compiler defines, as 1 or 0.
 *
 * @note Recorded for the reason given on TEST_UNKNOWN_BUILTIN_ANSWER.
 * @warning Only the arm with __has_attribute carries a value that a case can check. The fallback
 *          arm expands to EMBED_GNU_ATTRIBUTES for each name. An unknown attribute and a known one
 *          give the same answer there. The case that reads this has an #if to match.
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

/**
 * @brief A one-byte enum, declared to prove EMBED_ENUM_BYTE gives one byte on this compiler.
 *
 * @note Its range needs a single byte, and test_a_byte_enum_takes_one_byte checks for that width on
 *       every compiler, whichever arm of EMBED_ENUM_BYTE it takes.
 */
EMBED_ENUM_BYTE(ByteEnumProbe, TEST_BYTE_ENUM_MIN = 0, /**< Low end of the probe range. */
                TEST_BYTE_ENUM_MAX = 255               /**< High end, the largest value eight bits hold. */
);

/**
 * @brief A word whose alignment EMBED_ALIGN raised above its natural one.
 *
 * @note Sixteen bytes. The widest type this file declares is four bytes wide and aligns to four,
 *       and no other declaration here aligns to sixteen. A reported alignment of sixteen came from
 *       the attribute.
 */
#if EMBED_HAS_ATTRIBUTE(aligned)
typedef uint32_t RaisedAlignmentProbe EMBED_ALIGN(16);

/**
 * @brief A word whose alignment EMBED_ALIGN lowered to one byte.
 *
 * @note embed_types.h depends on this direction. EMBED_RAW lowers a word to alignment one there.
 *       Where the attribute expands to nothing the type keeps its natural alignment, and the code
 *       still reads it from any address.
 */
typedef uint32_t LoweredAlignmentProbe EMBED_ALIGN(1);
#endif

/**
 * @brief A word with both halves of EMBED_RAW, which a read at any address can reach.
 *
 * @note The alignment attribute permits the address. The alias attribute permits a read of bytes
 *       that another type owns. Either one alone leaves the read undefined, and EMBED_RAW has both.
 */
typedef uint32_t RawProbeWord EMBED_RAW;

/**
 * @brief Doubles its operand.
 *
 * @param[in] operand Value to double.
 * @return            Twice operand.
 * @note Marked EMBED_INLINE. That macro is static inline, plus the always_inline attribute where
 *       the compiler has it. A case can check that the marked definition is still a definition and
 *       that its body still runs. The language does not show whether the compiler inlined the call.
 */
EMBED_INLINE uint32_t inline_probe_doubled(uint32_t operand)
{
    return operand * 2u;
}

/**
 * @brief Quadruples its operand with two calls to inline_probe_doubled.
 *
 * @param[in] operand Value to quadruple.
 * @return            Four times operand.
 * @note Marked EMBED_FLATTEN. GCC and clang inline the bodies this function calls into it. A case
 *       checks the result here, as it does for EMBED_INLINE, and the result must not change.
 */
static EMBED_FLATTEN uint32_t flatten_probe_quadrupled(uint32_t operand)
{
    return inline_probe_doubled(inline_probe_doubled(operand));
}

/**
 * @brief Declares the weak default ahead of its definition.
 *
 * @return 7, the value a build that supplies no replacement reaches.
 * @note The attribute sits on the declaration. GCC diagnoses a weak declaration that follows its
 *       own definition, and the attribute has no effect there. The attribute in first place works on
 *       GCC and clang alike.
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
 * @return 5, the value a case compares against to confirm the call reached this definition.
 * @note Both guards expand to nothing in C. This suite compiles as C, so the extern "C" arm is
 *       never selected here.
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
 * @note A case that read this table would reference it, and a referenced object raises no
 *       unused-variable warning for the marker to suppress. Nothing here checks the marker at run
 *       time. The suite compiles without a diagnostic for this definition.
 */
static const uint32_t unreferenced_probe_table[] EMBED_UNUSED = {1u, 2u, 4u};

/**
 * @brief A file-scope value that test_the_diagnostic_bracket_bounds_a_suppression shadows.
 *
 * @note This object exists for that case. A shadow of an object that the suite also uses would make
 *       the warning depend on what else the file declares. The case reads this value before it
 *       shadows it, and that read keeps a reference to the outer name.
 */
static const uint32_t shadowed_probe_value = 11u;

/**
 * @brief Runs before each case, and has nothing to prepare.
 *
 * @note Unity calls this before every case, and a suite has to define it. Every case here reads
 *       types, macros and const objects, and none of those carries state to reset.
 */
void setUp(void)
{
}

/** @brief Runs after each case, and has nothing to release. */
void tearDown(void)
{
}

/**
 * @brief Checks that each feature gate holds one of the two values that its block names.
 *
 * @note The file-scope assertions above check the same three gates at compile time. This case
 *       checks them again at run time. The suite's report then lists them alongside every other case.
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
 * @note The assertions above prove the file-scope form, because they compile. A consumer writes the
 *       block-scope form inside a function. There the expansion plus the semicolon at the call site
 *       has to form one valid declaration.
 * @note The header raises #error below C11. A build that reached this line is therefore C11 or
 *       later, and whichever keyword EMBED_STATIC_ASSERT expands to is available.
 */
void test_the_static_assertion_compiles_at_file_and_block_scope(void)
{
    EMBED_STATIC_ASSERT(sizeof(uint32_t) == 4u, "a block-scope assertion is a declaration like any other");

    TEST_ASSERT_TRUE_MESSAGE(__STDC_VERSION__ >= 201112L, "the dialect is C11 or later");
}

/**
 * @brief Checks that the feature tests evaluate to zero for a name no compiler defines.
 *
 * @note A feature test that gives nonzero for an invented name gives nonzero for every name. Each
 *       attribute below then expands on a compiler that does not support it.
 * @note The attribute half runs only where the compiler has __has_attribute. The fallback arm
 *       expands to EMBED_GNU_ATTRIBUTES for each name. The header documents that as a warning on
 *       EMBED_HAS_ATTRIBUTE, and no case can find a defect there.
 */
void test_the_feature_tests_evaluate_to_zero_for_an_unknown_name(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, TEST_UNKNOWN_BUILTIN_ANSWER, "an unknown builtin evaluates to 0");
#if defined(__has_attribute)
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, TEST_UNKNOWN_ATTRIBUTE_ANSWER, "an unknown attribute evaluates to 0");
#endif
}

/**
 * @brief Checks that EMBED_BIG_ENDIAN agrees with how the target lays a word out in memory.
 *
 * @note This case reads the bytes of a word through a byte pointer, which C defines for any object.
 *       EMBED_BIG_ENDIAN comes from __BYTE_ORDER__, and a read of that macro restates the
 *       derivation and checks nothing.
 * @note The first two assertions hold on every target. The header falls to 0 where the compiler
 *       defines neither __BYTE_ORDER__ nor __ORDER_BIG_ENDIAN__, and its block names that as the
 *       safe direction. A big-endian part with such a compiler is not a failure here. For that
 *       reason, the third assertion has an #if on those two macros.
 */
void test_the_endian_flag_agrees_with_the_bytes_of_a_word(void)
{
    const uint32_t probe_value = 1u;
    const uint8_t *const as_bytes = (const uint8_t *)&probe_value;

    const int bytes_are_big_endian = (as_bytes[sizeof(probe_value) - 1u] == 1u);
    const int bytes_are_little_endian = (as_bytes[0] == 1u);

    TEST_ASSERT_TRUE_MESSAGE(bytes_are_big_endian || bytes_are_little_endian,
                             "the target lays a word out in one of the two orders");
    TEST_ASSERT_FALSE_MESSAGE(EMBED_BIG_ENDIAN && bytes_are_little_endian,
                              "EMBED_BIG_ENDIAN is never 1 where the low byte comes first");
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
    TEST_ASSERT_EQUAL_INT_MESSAGE(EMBED_BIG_ENDIAN, bytes_are_big_endian, "EMBED_BIG_ENDIAN matches the byte layout");
#endif
}

/**
 * @brief Checks that inline_probe_doubled and flatten_probe_quadrupled compute what their bodies
 *        define.
 *
 * @note Neither attribute changes what the bodies compute. Both reach the inliner and leave the
 *       arithmetic alone, and this case checks the two results, ten and twenty.
 */
void test_the_inline_and_flattened_helpers_run_their_bodies(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(10u, inline_probe_doubled(5u), "the inline helper doubles");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(20u, flatten_probe_quadrupled(5u), "the flattened helper quadruples");
}

/**
 * @brief Checks that a byte enum takes one byte and keeps its enumerators' values.
 *
 * @note embed_types.h asserts the same width on its own probe enum. This case checks EMBED_ENUM_BYTE
 *       directly on a type declared here, on every compiler, with no arm left out.
 */
void test_a_byte_enum_takes_one_byte(void)
{
    const ByteEnumProbe low = TEST_BYTE_ENUM_MIN;
    const ByteEnumProbe high = TEST_BYTE_ENUM_MAX;

    TEST_ASSERT_EQUAL_INT_MESSAGE(0, low, "the low enumerator survives the declaration");
    TEST_ASSERT_EQUAL_INT_MESSAGE(255, high, "the high enumerator survives the declaration");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, sizeof(ByteEnumProbe), "a byte enum is one byte");
}

/**
 * @brief Checks that the alignment attribute raises alignment and lowers it.
 *
 * @note This library uses EMBED_ALIGN in both directions. A higher alignment on an object matters
 *       where later code assumes it. An alignment of one is half of EMBED_RAW, and embed_types.h
 *       needs that for a read at an odd address.
 */
void test_the_alignment_attribute_raises_and_lowers(void)
{
#if EMBED_HAS_ATTRIBUTE(aligned)
    TEST_ASSERT_EQUAL_size_t_MESSAGE(16u, _Alignof(RaisedAlignmentProbe), "the raised type aligns to sixteen");
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, _Alignof(LoweredAlignmentProbe), "the lowered type aligns to one");
#elif defined(_MSC_VER)
    // MSVC's form stands before an object and raises its alignment only, as the header states
    static EMBED_ALIGN(16) uint32_t raised;
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, (size_t)((uintptr_t)&raised % 16u), "the raised object aligns to sixteen");
#else
    TEST_IGNORE_MESSAGE("EMBED_HAS_ATTRIBUTE(aligned) is 0 here, so EMBED_ALIGN expands to nothing");
#endif
}

/**
 * @brief Checks that a word with EMBED_RAW reads correctly from an odd address.
 *
 * @note The expected value is a shift of the four literal bytes at offset one, in the order that
 *       EMBED_BIG_ENDIAN names. That is exact arithmetic over the same literal, and no call.
 * @note This case has an #if on EMBED_HAS_ATTRIBUTE(aligned). Without that attribute the type is
 *       the plain word, and C does not define the read below. Undefined behavior gives a case
 *       nothing to assert against.
 */
void test_a_raw_word_reads_from_an_odd_address(void)
{
#if EMBED_HAS_ATTRIBUTE(aligned)
    static const uint8_t bytes[sizeof(uint32_t) * 2u] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u};
    const RawProbeWord *const at_one = (const RawProbeWord *)(const void *)(&bytes[1]);
#if EMBED_BIG_ENDIAN
    const uint32_t expected = 0x22334455u;
#else
    const uint32_t expected = 0x55443322u;
#endif

    TEST_ASSERT_EQUAL_HEX32_MESSAGE(expected, *at_one, "a raw word at offset one matches the bytes it covers");
#else
    TEST_IGNORE_MESSAGE("EMBED_HAS_ATTRIBUTE(aligned) is 0 here, the read would be undefined");
#endif
}

/**
 * @brief Checks that the diagnostic bracket suppresses a warning and stringizes its pragma text.
 *
 * @note The inner declaration shadows shadowed_probe_value. EMBED_DIAGNOSTIC_IGNORE("-Wshadow")
 *       suppresses the warning that a compiler raises for that shadow, and the bracket keeps the
 *       suppression to those lines.
 * @note EMBED_DIAGNOSTIC_STRING is the part that a case can read back. _Pragma takes a string
 *       literal, and the text that EMBED_DIAGNOSTIC_IGNORE builds has to keep the warning name in
 *       quotes inside it.
 * @note Nothing below the bracket shadows a name, so a suppression that stays in force does not
 *       show in this case. It silences the rest of the translation unit instead. For that reason,
 *       EMBED_DIAGNOSTIC_PUSH and EMBED_DIAGNOSTIC_POP stand as a pair.
 */
void test_the_diagnostic_bracket_bounds_a_suppression(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(11u, shadowed_probe_value, "the outer read reaches the file-scope value");

    {
        EMBED_DIAGNOSTIC_PUSH
        EMBED_DIAGNOSTIC_IGNORE("-Wshadow")
        const uint32_t shadowed_probe_value = 3u;
        EMBED_DIAGNOSTIC_POP

        TEST_ASSERT_EQUAL_UINT_MESSAGE(3u, shadowed_probe_value, "the inner declaration shadows the outer one");
    }

    TEST_ASSERT_EQUAL_STRING_MESSAGE("GCC diagnostic ignored \"-Wpadded\"",
                                     EMBED_DIAGNOSTIC_STRING(GCC diagnostic ignored "-Wpadded"),
                                     "the whole pragma text stringizes, quotes included");
}

/**
 * @brief Checks that the weak default links as itself where nothing replaces it.
 *
 * @note This case covers one half of EMBED_WEAK. The other half is a strong definition of the same
 *       name that replaces the weak one, and that needs a definition in another translation unit.
 */
void test_the_weak_default_links_as_itself(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(7u, weak_probe_default(), "the call reached the weak definition");
}

/**
 * @brief Checks that the linkage guards leave a C declaration reachable.
 *
 * @note The call below reaches this definition through the declaration between the guards. A
 *       defect in either guard would leave a stray brace, and the compiler catches that.
 */
void test_the_declaration_guards_leave_a_c_declaration_intact(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(5u, guarded_probe_entry(), "the declaration between the guards is reachable");
}

/**
 * @brief Records that a definition marked EMBED_UNUSED and never referenced compiled quietly.
 *
 * @note This case passes where the suite builds. The marker suppresses a diagnostic, and a
 *       suppressed diagnostic leaves nothing for a case to read. The name unreferenced_probe_table
 *       here would reference it and remove the condition that the marker exists for.
 */
void test_an_unreferenced_marked_definition_compiles_quietly(void)
{
    TEST_PASS_MESSAGE("unreferenced_probe_table is defined, marked EMBED_UNUSED, and never named again");
}
