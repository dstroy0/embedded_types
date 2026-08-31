/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_compiler_directives.h
 * @brief Compiler feature checks, attribute wrappers, diagnostic pragmas, argument counting, and
 *        target flags.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note This header has no extern "C" guard of its own because it defines that guard, as
 *       EMBED_BEGIN_DECLS. Everything here is a macro, and macros have no linkage to mangle.
 * @note If your library uses this one, alias these macros under your own prefix. Do not copy them
 *       into your own tree. Copies drift when this header changes.
 */
#ifndef EMBED_COMPILER_DIRECTIVES_H
#define EMBED_COMPILER_DIRECTIVES_H

#include <stddef.h>

/**
 * @brief Set to 1 where __GNUC__ or __clang__ is defined, 0 otherwise.
 *
 * @note EMBED_HAS_ATTRIBUTE falls back to this value when __has_attribute is missing. A compiler
 *       old enough to lack __has_attribute usually still accepts the GNU attributes, and a 0 here
 *       would turn all of them off.
 * @note This is always defined, as 0 or 1. Check it with #if. #ifdef is true either way.
 */
#if defined(__GNUC__) || defined(__clang__)
#define EMBED_GNU_ATTRIBUTES 1
#else

#define EMBED_GNU_ATTRIBUTES 0
#endif

/**
 * @brief Expands to __has_attribute(attribute_) where __has_attribute is defined.
 *
 * @param[in] attribute_ Attribute name, as passed to __has_attribute.
 * @return               The value __has_attribute gives for attribute_.
 * @note Every attribute macro below goes through this. Asking the compiler whether it supports an
 *       attribute is more reliable than checking which compiler it is.
 * @warning Expands to EMBED_GNU_ATTRIBUTES where __has_attribute is undefined, ignoring attribute_.
 *          Every attribute then gets the same answer. A compiler without __has_attribute gets all
 *          of them or none.
 */
#if defined(__has_attribute)
#define EMBED_HAS_ATTRIBUTE(attribute_) __has_attribute(attribute_)
#else

#define EMBED_HAS_ATTRIBUTE(attribute_) EMBED_GNU_ATTRIBUTES
#endif

/**
 * @brief Expands to __has_builtin(builtin_) where __has_builtin is defined.
 *
 * @param[in] builtin_ Builtin name, as passed to __has_builtin.
 * @return             The value __has_builtin gives for builtin_.
 * @note This is the builtin-side version of EMBED_HAS_ATTRIBUTE. Gate a builtin with it the same
 *       way you would gate an attribute.
 * @warning Without __has_builtin this expands to 0 and ignores builtin_. The fallback is 0 because
 *          a missing builtin fails to compile. A missing attribute is only ignored, which is why
 *          EMBED_HAS_ATTRIBUTE can fall back to a guess.
 */
#if defined(__has_builtin)
#define EMBED_HAS_BUILTIN(builtin_) __has_builtin(builtin_)
#else

#define EMBED_HAS_BUILTIN(builtin_) 0
#endif

/**
 * @brief Expands to a two-operand static assertion.
 *
 * @param[in] cond_ Constant expression passed through unchanged.
 * @param[in] msg_  Message operand passed through unchanged.
 * @note This gives one name for the assertion across C11, C23, and C++. Every file here that
 *       claims a width or a layout uses it.
 * @note C++ and C23 get static_assert, which is a keyword in both. Everything else gets
 *       _Static_assert, the C11 keyword.
 * @note The __cplusplus arm is not optional. No C++ compiler defines __STDC_VERSION__, and without
 *       that arm the #error below fires on every C++ build.
 * @note Neither arm includes <assert.h>. Both names are keywords in the revision that picks them.
 * @note The expansion has no trailing semicolon. Write one at the call site.
 * @warning #error below C11, where neither keyword exists. Without it the name would parse as a
 *          function declaration returning implicit int, and every assertion in the build would
 *          compile and check nothing.
 */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L)
#error "embedded_types needs C11 or later - no earlier revision has a static assertion"
#elif defined(__cplusplus) || __STDC_VERSION__ >= 202311L
#define EMBED_STATIC_ASSERT(cond_, msg_) static_assert(cond_, msg_)
#else

#define EMBED_STATIC_ASSERT(cond_, msg_) _Static_assert(cond_, msg_)
#endif

/**
 * @brief Expands to extern "C" { where __cplusplus is defined.
 *
 * @note This library is C. A C++ translation unit that includes these headers needs the guard to
 *       reach the symbols by their unmangled names.
 * @warning The expansion leaves a { open. EMBED_END_DECLS closes it.
 * @note Expands to nothing where __cplusplus is undefined.
 */
#ifdef __cplusplus
#define EMBED_BEGIN_DECLS                                                                                              \
    extern "C"                                                                                                         \
    {

/** @brief Expands to } where __cplusplus is defined, closing EMBED_BEGIN_DECLS. */
#define EMBED_END_DECLS }
#else

/** @brief Expands to nothing where __cplusplus is undefined. No brace is opened on this arm. */
#define EMBED_BEGIN_DECLS

/** @brief Expands to nothing where __cplusplus is undefined. */
#define EMBED_END_DECLS
#endif

/**
 * @brief Expands to left_##right_.
 *
 * @param[in] left_  Left operand of ##.
 * @param[in] right_ Right operand of ##.
 * @return           The single token formed by joining left_ and right_.
 * @note EMBED_CAT calls this. ## does not expand its own operands, and EMBED_CAT expands them
 *       before handing them here. Call EMBED_CAT when either operand is itself a macro.
 */
#define EMBED_CAT_(left_, right_) left_##right_

/**
 * @brief Expands to EMBED_CAT_(left_, right_).
 *
 * @param[in] left_  Left operand, forwarded to EMBED_CAT_.
 * @param[in] right_ Right operand, forwarded to EMBED_CAT_.
 * @return           The single token formed by joining left_ and right_.
 * @note This expands its arguments first, then EMBED_CAT_ pastes the results. A macro operand
 *       reaches EMBED_CAT_ as its value.
 * @note EMBED_TABLE_LAYOUT uses this to build an EMBED_TABLE_SLOTS_<n> name from EMBED_NARG's
 *       count.
 */
#define EMBED_CAT(left_, right_) EMBED_CAT_(left_, right_)

/**
 * @brief Expands to EMBED_ARG_N with __VA_ARGS__ followed by the constants 24 down to 0.
 *
 * @param[in] ... The list to count.
 * @return        The number of arguments, for one to twenty-four arguments.
 * @note Counting the arguments turns an arity into a number EMBED_CAT can paste into a macro name.
 *       The preprocessor has no loop.
 * @warning An empty list gives 1. The preprocessor cannot tell an empty argument from a missing one.
 * @warning Twenty-five or more arguments make EMBED_ARG_N select an argument instead of a constant.
 */
#define EMBED_NARG(...)                                                                                                \
    EMBED_NARG_(__VA_ARGS__, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

/**
 * @brief Expands to EMBED_ARG_N(__VA_ARGS__).
 *
 * @param[in] ... The list from EMBED_NARG, followed by the constants 24 down to 0.
 * @return        The value EMBED_ARG_N selects.
 * @note EMBED_NARG calls this. Going through one more macro expands the caller's list before
 *       EMBED_ARG_N picks a slot from it.
 */
#define EMBED_NARG_(...) EMBED_ARG_N(__VA_ARGS__)

/**
 * @brief Expands to its twenty-fifth argument.
 *
 * @param[in] slot1_    Arguments one through twenty-four, discarded.
 * @param[in] selected_ The twenty-fifth argument.
 * @param[in] ...       Arguments beyond the twenty-fifth, discarded.
 * @return              selected_.
 * @note EMBED_NARG appends the constants 24 down to 0 after the caller's list. The caller's
 *       arguments push those constants to the right. Whichever one lands twenty-fifth is how many
 *       arguments the caller passed.
 */
#define EMBED_ARG_N(slot1_, slot2_, slot3_, slot4_, slot5_, slot6_, slot7_, slot8_, slot9_, slot10_, slot11_, slot12_, \
                    slot13_, slot14_, slot15_, slot16_, slot17_, slot18_, slot19_, slot20_, slot21_, slot22_, slot23_, \
                    slot24_, selected_, ...)                                                                           \
    selected_

/**
 * @brief Expands to entry_(&(ArgsType_){__VA_ARGS__}).
 *
 * @param[in] entry_    Function called with the address of the literal.
 * @param[in] ArgsType_ Type of the compound literal.
 * @param[in] ...       Initializers for the compound literal.
 * @return              The value entry_ returns.
 * @note One pointer costs one register at any arity. A long parameter list fills the registers and
 *       spills the rest onto the stack.
 * @note C zero initializes any member the initializer does not name. A compound literal in argument
 *       position lives until the end of the enclosing block, so entry_ may hold the pointer for the
 *       whole call.
 * @note Use this from C only. Compound literals are not C++ in any revision. GNU C++ accepts one as
 *       an extension. Its lifetime there ends at the full-expression. That breaks the [BORROWS]
 *       contract below. No header here expands this macro. A C++ build never sees the expansion.
 * @warning entry_ receives the address of the literal [BORROWS].
 */
#define EMBED_CALL(entry_, ArgsType_, ...) entry_(&(ArgsType_){__VA_ARGS__})

/**
 * @brief Expands to __attribute__((always_inline)) prefixed by static inline.
 *
 * @note GCC and clang inline the body even where their size heuristic would leave it out of line.
 *       Plain static inline leaves that decision to the compiler.
 * @note Expands to static inline where EMBED_HAS_ATTRIBUTE(always_inline) is 0. The code stays
 *       correct. It may run slower.
 * @warning This header defines nothing when EMBED_INLINE is already defined. Define it ahead of
 *          this header to set your own inlining policy.
 */
#ifndef EMBED_INLINE
#if EMBED_HAS_ATTRIBUTE(always_inline)
#define EMBED_INLINE static inline __attribute__((always_inline))
#else
#define EMBED_INLINE static inline
#endif
#endif

/**
 * @brief Expands to __attribute__((flatten)) where EMBED_HAS_ATTRIBUTE(flatten) is non-zero.
 *
 * @note Put this on a function and GCC or clang inlines everything that function calls, including
 *       bodies their size heuristic would skip. EMBED_INLINE goes on the callee. This goes on the
 *       caller.
 * @note Every call site of the marked function carries the inlined code. Put it on one hot function.
 * @warning The called bodies have to be visible. Without link-time optimization this does nothing
 *          across translation units.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(flatten) is 0. The code stays correct. It
 *          may run slower.
 */
#if EMBED_HAS_ATTRIBUTE(flatten)
#define EMBED_FLATTEN __attribute__((flatten))
#else

#define EMBED_FLATTEN
#endif

/**
 * @brief Expands to __attribute__((packed)) where EMBED_HAS_ATTRIBUTE(packed) is non-zero.
 *
 * @note An enum carrying this takes the width its range needs. Without it the enum is int-wide.
 *       A struct holding one takes its member offsets from that width, so this changes layout.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(packed) is 0. A compiler can also accept
 *          the attribute and ignore it. Neither case shows up here. embed_types.h declares a probe
 *          enum and asserts its size to catch both.
 */
#if EMBED_HAS_ATTRIBUTE(packed)
#define EMBED_ENUM_PACKED __attribute__((packed))
#else

#define EMBED_ENUM_PACKED
#endif

/**
 * @brief Expands to __attribute__((aligned(bytes_))) where EMBED_HAS_ATTRIBUTE(aligned) is non-zero.
 *
 * @param[in] bytes_ Alignment operand passed to the attribute.
 * @note This goes both ways. Raise an object's alignment above its natural one, or lower it to 1.
 *       EMBED_RAW uses the lower-to-1 form.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(aligned) is 0, ignoring bytes_. An object
 *          you meant to over-align sits at its natural alignment instead. A type you meant to drop
 *          to alignment 1 keeps its natural alignment, and the code still reads it from any address.
 */
#if EMBED_HAS_ATTRIBUTE(aligned)
#define EMBED_ALIGN(bytes_) __attribute__((aligned(bytes_)))
#else

#define EMBED_ALIGN(bytes_)
#endif

/**
 * @brief Expands to __attribute__((may_alias)) where EMBED_HAS_ATTRIBUTE(may_alias) is non-zero.
 *
 * @note C lets a character type alias any object. Reading an eight-bit array through a word lvalue
 *       goes the other way, and C leaves that undefined. This attribute on the word type makes the
 *       read defined.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(may_alias) is 0. Nothing diagnoses that.
 *          The code compiles, and the compiler may assume the two accesses never touch the same
 *          memory.
 */
#if EMBED_HAS_ATTRIBUTE(may_alias)
#define EMBED_ALIAS __attribute__((may_alias))
#else

#define EMBED_ALIAS
#endif

/**
 * @brief Expands to EMBED_ALIGN(1) followed by EMBED_ALIAS.
 *
 * @note Reading a word from an address that is not a multiple of its width needs both attributes.
 *       EMBED_ALIGN(1) stops the compiler assuming the alignment. EMBED_ALIAS permits the read to
 *       reach bytes another type owns. Either one alone leaves the access undefined.
 * @note Both live in one macro here. Writing the pair out at each declaration lets the two drift
 *       apart the first time someone edits one.
 * @warning Both warnings above apply here. Neither failure is diagnosed.
 */
#define EMBED_RAW EMBED_ALIGN(1) EMBED_ALIAS

/**
 * @brief Expands to __attribute__((unused)) where EMBED_HAS_ATTRIBUTE(unused) is non-zero.
 *
 * @note Use this on a definition you leave unreferenced on purpose. A static definition in a header
 *       reaches every translation unit that includes it. A unit that never touches its copy warns.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(unused) is 0. The warning comes back.
 *          Correctness is unaffected.
 */
#if EMBED_HAS_ATTRIBUTE(unused)
#define EMBED_UNUSED __attribute__((unused))
#else

#define EMBED_UNUSED
#endif

/**
 * @brief Expands to __attribute__((weak)) where EMBED_HAS_ATTRIBUTE(weak) is non-zero.
 *
 * @note The linker picks a strong definition of the same name over a weak one. With no strong
 *       definition, the weak one links. That lets a default ship here and a port replace it
 *       without touching this file.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(weak) is 0. The default then has ordinary
 *          external linkage. A replacement becomes a duplicate symbol and the link fails.
 */
#if EMBED_HAS_ATTRIBUTE(weak)
#define EMBED_WEAK __attribute__((weak))
#else

#define EMBED_WEAK
#endif

/**
 * @brief Expands to #text_.
 *
 * @param[in] text_ Token sequence to stringize.
 * @return          text_ as a string literal.
 * @note EMBED_DIAGNOSTIC_IGNORE calls this. _Pragma takes a string literal, and this turns the
 *       whole pragma line into one.
 * @note This sits above the compiler arms below. Defining it inside each arm would put two
 *       identical copies in the file, and they drift the first time someone edits one.
 */
#define EMBED_DIAGNOSTIC_STRING(text_) #text_

#if defined(__clang__)
/**
 * @brief Expands to _Pragma("clang diagnostic push") where __clang__ is defined.
 *
 * @note This saves the current diagnostic state. Without a save ahead of it, an
 *       EMBED_DIAGNOSTIC_IGNORE stays in force to the end of the translation unit. Put every
 *       EMBED_DIAGNOSTIC_IGNORE between this and EMBED_DIAGNOSTIC_POP.
 */
#define EMBED_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")

/**
 * @brief Expands to _Pragma("clang diagnostic pop") where __clang__ is defined.
 *
 * @note This restores the state EMBED_DIAGNOSTIC_PUSH saved. That ends the suppression. Leave it
 *       out and the suppression runs to the end of the translation unit. Nothing warns about that.
 *       The warning that would have fired is the one being suppressed.
 */
#define EMBED_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")

/**
 * @brief Expands to a clang pragma ignoring the named warning.
 *
 * @param[in] warning_ Warning name as a string literal, such as "-Wpadded".
 * @note This turns off exactly one warning. See EMBED_DIAGNOSTIC_PUSH for where to put it.
 * @note EMBED_DIAGNOSTIC_STRING stringizes the whole pragma text, including warning_.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_) _Pragma(EMBED_DIAGNOSTIC_STRING(clang diagnostic ignored warning_))
#elif defined(__GNUC__)
/**
 * @brief Expands to _Pragma("GCC diagnostic push") where __GNUC__ is defined and __clang__ is not.
 * @note This is the GCC form of the clang push above. See that block for where to put
 *       EMBED_DIAGNOSTIC_IGNORE.
 */
#define EMBED_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")

/**
 * @brief Expands to _Pragma("GCC diagnostic pop") where __GNUC__ is defined and __clang__ is not.
 * @note This is the GCC form of the clang pop above. See that block for what happens when it is
 *       left out.
 */
#define EMBED_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")

/**
 * @brief Expands to a GCC pragma ignoring the named warning.
 *
 * @param[in] warning_ Warning name as a string literal, such as "-Wpadded".
 * @note Selected where __GNUC__ is defined and __clang__ is not.
 * @note This turns off exactly one warning. See EMBED_DIAGNOSTIC_PUSH for where to put it.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_) _Pragma(EMBED_DIAGNOSTIC_STRING(GCC diagnostic ignored warning_))
#else

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @note A compiler with no diagnostic pragma has no state to save. Calling code keeps the same
 *       push, ignore, and pop shape on every compiler, and this arm costs nothing.
 */
#define EMBED_DIAGNOSTIC_PUSH

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @note This pairs with EMBED_DIAGNOSTIC_PUSH. That macro also expands to nothing on this arm.
 */
#define EMBED_DIAGNOSTIC_POP

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @param[in] warning_ Warning name as a string literal, discarded.
 * @warning The warning is not suppressed on this arm. A compiler that raises it still reports it.
 *          Reporting is the safe direction.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_)
#endif

/**
 * @brief Set to 1 where __BYTE_ORDER__ and __ORDER_BIG_ENDIAN__ are both defined and equal, 0 otherwise.
 *
 * @note A consumer that reads or writes multi-byte values one byte at a time branches on this.
 *       Nothing in this library reads it.
 * @warning This header defines nothing when EMBED_BIG_ENDIAN is already defined. A compiler that
 *          defines neither __BYTE_ORDER__ nor __ORDER_BIG_ENDIAN__ falls to 0 here. Set it on the
 *          command line for a big-endian target with such a compiler.
 */
#ifndef EMBED_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define EMBED_BIG_ENDIAN 1
#else

#define EMBED_BIG_ENDIAN 0
#endif
#endif

/**
 * @brief Set to 1 where the target loads a word from any address in one instruction, 0 otherwise.
 *
 * @note An unaligned load compiles on any target through a type carrying EMBED_RAW. This flag is 1
 *       only where the target does that load in one instruction.
 * @note GCC and clang define __ARM_FEATURE_UNALIGNED on targets that support it. Passing
 *       -mno-unaligned-access turns it off.
 * @note No target is named here. The flag falls to 0 wherever that macro is absent, and 0 is the
 *       safe answer. A consumer seeing 0 takes the byte-wise path, which is correct everywhere.
 * @warning This header defines nothing when EMBED_FAST_UNALIGNED_LOAD is already defined. Nothing
 *          in this library reads this flag.
 */
#ifndef EMBED_FAST_UNALIGNED_LOAD
#if defined(__ARM_FEATURE_UNALIGNED)
#define EMBED_FAST_UNALIGNED_LOAD 1
#else

#define EMBED_FAST_UNALIGNED_LOAD 0
#endif
#endif

#endif
