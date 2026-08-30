/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_dispatch_layout.c
 * @brief Exercises the argument count, the token paste, the compound-literal call, and that a table
 *        reaches the function each slot names.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note EMBED_TABLE_LAYOUT is proved by this file compiling: its assertions are static, and a table
 *       whose members were at the wrong offsets would fail the build rather than a case below.
 * @note What runs here is what a static assertion cannot see. A table can satisfy every offset
 *       assertion and still be wired to the wrong functions, because an initializer names members
 *       and the assertions measure positions.
 * @note EMBED_NARG, EMBED_CAT and EMBED_CALL are declared in embed_compiler_directives.h and are
 *       exercised here because EMBED_TABLE_LAYOUT is built out of them. test_embed_compiler_
 *       directives owns them as its header's own machinery.
 */
#include "embed_dispatch_layout.h"
#include "embed_types.h"

#include "unity.h"

/**
 * @brief Runs before each case, and has nothing to prepare.
 *
 * @note Unity calls this whether or not it does anything, and unity_internals.h declares it. The
 *       table below is const, so no case can leave state behind for the next one.
 */
void setUp(void)
{
}

/** @brief Runs after each case, and has nothing to release. */
void tearDown(void)
{
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

/** @brief A value the paste case reaches only by building its name from two tokens. */
#define TEST_PASTED_NAME_7 7

/**
 * @brief Checks that the two-step paste joins a name to an expanded count.
 *
 * @note The point of the outer step. Pasting the name of a macro rather than its value is the
 *       failure this shape exists to avoid, and it is what EMBED_TABLE_LAYOUT depends on.
 */
void test_the_paste_joins_a_name_to_an_expanded_count(void)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, EMBED_CAT(TEST_PASTED_NAME_, 7), "a literal suffix pastes");
    TEST_ASSERT_EQUAL_INT_MESSAGE(7, EMBED_CAT(TEST_PASTED_NAME_, EMBED_NARG(a, b, c, d, e, f, g)),
                                  "an expanded count pastes");
}

/** @brief The operand block the compound-literal call builds at its call site. */
typedef struct
{
    embed_index first;
    embed_index second;
    embed_index third;
} CallProbeArgs;

/**
 * @brief Sums the three members so a caller can see which of them arrived.
 *
 * @param[in] args Operand block built by the caller [BORROWS].
 * @return         first plus twice second plus four times third, which no two inputs share.
 * @note The weights make the answer name which members were set, rather than only their total.
 */
static embed_index call_probe(const CallProbeArgs *args)
{
    return (embed_index)(args->first + (2u * args->second) + (4u * args->third));
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

/** @brief Answers 1, so a caller can tell which slot it reached. */
static embed_index slot_zero_entry(void)
{
    return 1u;
}

/** @brief Answers 2, so a caller can tell which slot it reached. */
static embed_index slot_one_entry(void)
{
    return 2u;
}

/** @brief Answers 4, so a caller can tell which slot it reached. */
static embed_index slot_two_entry(void)
{
    return 4u;
}

/**
 * @brief A dispatch table of three entries, whose layout the assertion below pins.
 *
 * @note Each entry answers a distinct power of two, so a sum over the table names exactly which
 *       members were reached rather than only how many.
 */
typedef struct
{
    embed_index (*first)(void);
    embed_index (*second)(void);
    embed_index (*third)(void);
} ProbeTable;
EMBED_TABLE_LAYOUT(ProbeTable, first, second, third);

/**
 * @brief The table under test, wired in the order its members are declared.
 *
 * @note EMBED_TABLE_STORAGE gives it internal linkage and const, and EMBED_UNUSED keeps a
 *       translation unit that calls nothing through it quiet.
 */
EMBED_TABLE_STORAGE ProbeTable probe EMBED_UNUSED = {
    .first = slot_zero_entry,
    .second = slot_one_entry,
    .third = slot_two_entry,
};

/**
 * @brief Checks that each member reaches the function it was wired to.
 *
 * @note What the static assertions cannot see. They measure where the members sit; an initializer
 *       naming the wrong function satisfies every offset and still dispatches wrongly.
 */
void test_each_member_reaches_the_function_it_names(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, probe.first(), "the first member reaches its own function");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, probe.second(), "the second member reaches its own function");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(4u, probe.third(), "the third member reaches its own function");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(7u, probe.first() + probe.second() + probe.third(),
                                   "no two members reach the same function");
}

/**
 * @brief Checks that the table holds nothing but its function pointers.
 *
 * @note The size is compared against a count written as a literal times the pointer width, which is
 *       what the static assertion inside EMBED_TABLE_LAYOUT proves at compile time. Repeating it
 *       here catches a build where that assertion was compiled out.
 */
void test_the_table_is_exactly_its_three_pointers(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u * EMBED_FUNCTION_POINTER_BYTES, sizeof(ProbeTable),
                                     "the table is three pointers wide");
}
