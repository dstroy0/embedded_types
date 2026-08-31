/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_dispatch_layout.c
 * @brief Exercises the argument count, the token paste, the compound-literal call, and that each
 *        table entry reaches the function it was wired to.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note EMBED_TABLE_LAYOUT expands to static assertions. A table whose members sat at the wrong
 *       offsets would fail the build, and no case below would run.
 * @note A table can satisfy every offset assertion and still be wired to the wrong functions. An
 *       initializer names members, and the assertions measure positions. The cases below call
 *       through the table to catch that.
 * @note EMBED_NARG, EMBED_CAT and EMBED_CALL are declared in embed_compiler_directives.h and are
 *       exercised here because EMBED_TABLE_LAYOUT is built out of them.
 *       test_embed_compiler_directives.c covers the same three macros against their own header.
 */
#include "embed_dispatch_layout.h"
#include "embed_types.h"

#include "unity.h"

/**
 * @brief Runs before each case, and has nothing to prepare.
 *
 * @note Unity calls this before every case, and a suite has to define it. The only object this file
 *       declares at file scope is the const table below, and no case writes to it.
 */
void setUp(void)
{
}

/** @brief Runs after each case, and has nothing to release. */
void tearDown(void)
{
}

/**
 * @brief Checks that EMBED_NARG evaluates to the number of arguments it was handed.
 *
 * @note Each expectation is the literal number of items written on the same line, counted by hand.
 *       Deriving it from EMBED_NARG would compare the macro with itself.
 * @note The five lengths are one, two, eight, twenty-three and twenty-four. One is the shortest
 *       list EMBED_NARG covers and twenty-four is the longest. Twenty-three sits one below the
 *       longest, and two and eight fall in between.
 */
void test_the_argument_count_matches_the_length_of_the_list(void)
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
 * @note ## suppresses expansion of its own operands. EMBED_CAT expands its arguments first and
 *       then pastes the results. EMBED_TABLE_LAYOUT needs that order, because it pastes
 *       EMBED_NARG's count onto EMBED_TABLE_SLOTS_ to select an arity line.
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
 * @brief Sums the three members under distinct weights.
 *
 * @param[in] args Operand block built by the caller [BORROWS].
 * @return         first plus twice second plus four times third.
 * @note The weights are 1, 2 and 4. With each member set to 0 or 1, no two combinations produce
 *       the same sum.
 */
static embed_index call_probe(const CallProbeArgs *args)
{
    // The 2u and 4u operands promote the sum to unsigned int. Narrowing back to embed_index is safe
    // because every call site passes members of 0 or 1, and the sum is at most 7.
    return (embed_index)(args->first + (2u * args->second) + (4u * args->third));
}

/**
 * @brief Checks that the call shape passes named members and zeroes the ones left out.
 *
 * @note C zero initializes any member a compound literal's initializer does not name. That comes
 *       from the language, and a consumer relies on it every time it omits a default.
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

/** @brief Returns 1, distinct from the values the other two entries return. */
static embed_index first_entry(void)
{
    return 1u;
}

/** @brief Returns 2, distinct from the values the other two entries return. */
static embed_index second_entry(void)
{
    return 2u;
}

/** @brief Returns 4, distinct from the values the other two entries return. */
static embed_index third_entry(void)
{
    return 4u;
}

/**
 * @brief A dispatch table of three entries, whose layout the assertion below pins.
 *
 * @note Each entry returns a distinct power of two. No two subsets of the three entries sum to the
 *       same total.
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
 * @note EMBED_TABLE_STORAGE expands to static const. EMBED_UNUSED suppresses the unused-object
 *       warning on a table nothing calls through. The cases below call through this one.
 */
EMBED_TABLE_STORAGE ProbeTable dispatch_probe_table EMBED_UNUSED = {
    .first = first_entry,
    .second = second_entry,
    .third = third_entry,
};

/**
 * @brief Checks that each member reaches the function it was wired to.
 *
 * @note EMBED_TABLE_LAYOUT asserts offsets on the struct type. The initializer is on the object, and
 *       naming the wrong function there changes no offset. Calling through each member catches it.
 */
void test_each_member_reaches_the_function_it_names(void)
{
    TEST_ASSERT_EQUAL_UINT_MESSAGE(1u, dispatch_probe_table.first(), "the first member reaches its own function");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(2u, dispatch_probe_table.second(), "the second member reaches its own function");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(4u, dispatch_probe_table.third(), "the third member reaches its own function");
    TEST_ASSERT_EQUAL_UINT_MESSAGE(
        7u, dispatch_probe_table.first() + dispatch_probe_table.second() + dispatch_probe_table.third(),
        "no two members reach the same function");
}

/**
 * @brief Checks that the table holds nothing but its function pointers.
 *
 * @note EMBED_TABLE_LAYOUT asserts this size at compile time. This case checks it again at run time.
 *       The suite's report then lists it alongside every other case.
 */
void test_the_table_is_exactly_its_three_pointers(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(3u * EMBED_FUNCTION_POINTER_BYTES, sizeof(ProbeTable),
                                     "the table is three pointers wide");
}
