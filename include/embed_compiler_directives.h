/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_compiler_directives.h
 * @brief Preprocessor definitions, declaring no type and defining no function.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note Carries no linkage guard of its own. It defines EMBED_BEGIN_DECLS, so it cannot be wrapped
 *       in one, and it declares nothing a C++ compiler would mangle.
 * @note A consumer aliases these under its own prefix rather than defining its own copies. Two
 *       libraries in one build then reach the same definition instead of each carrying a variant.
 */
#ifndef EMBED_COMPILER_DIRECTIVES_H
#define EMBED_COMPILER_DIRECTIVES_H

#include <stddef.h>

/**
 * @brief Set to 1 where __GNUC__ or __clang__ is defined, 0 otherwise.
 *
 * @note EMBED_HAS_ATTRIBUTE answers with this where __has_attribute is missing. A compiler old
 *       enough to lack __has_attribute may still accept the GNU attributes, so answering 0 there
 *       would switch off every attribute in this header on a compiler that supports them.
 * @note Defined on both arms, so #ifdef EMBED_GNU_ATTRIBUTES is always true. Test it with #if.
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
 * @note Every attribute wrapper below is gated on this rather than on a compiler test, so a build
 *       is asked what it supports instead of being guessed at from its identity.
 * @warning Expands to EMBED_GNU_ATTRIBUTES where __has_attribute is undefined, ignoring attribute_.
 *          That answer is the same for every attribute asked about, so a compiler without
 *          __has_attribute either gets all of them or none.
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
 * @warning Expands to 0 where __has_builtin is undefined, ignoring builtin_. Unlike the attribute
 *          test there is no fallback worth guessing, since a builtin that is absent fails to
 *          compile rather than being ignored.
 */
#if defined(__has_builtin)
#define EMBED_HAS_BUILTIN(builtin_) __has_builtin(builtin_)
#else

#define EMBED_HAS_BUILTIN(builtin_) 0
#endif

/**
 * @brief Expands to a two-operand static assertion in whichever spelling the C dialect provides.
 *
 * @param[in] cond_ Constant expression passed through unchanged.
 * @param[in] msg_  Message operand passed through unchanged.
 * @note A consumer states what the build must prove rather than testing it at run time, so this is
 *       reached from every file carrying a width or a layout claim. Spelling it once keeps the
 *       question here.
 * @note static_assert where __STDC_VERSION__ reaches 202311L, which is the revision that made it a
 *       keyword. _Static_assert otherwise, which is the C11 spelling this library is written to.
 * @note Neither arm reaches <assert.h>. Each spelling is a keyword in the revision that selects it,
 *       so a consumer inherits no header it did not ask for.
 * @note The expansion carries no trailing semicolon. Both spellings take one as part of their own
 *       grammar, so a use site writes it and the result is one declaration rather than a
 *       declaration followed by an empty one.
 * @warning #error before C11, where neither spelling exists. Left to expand, the name would parse
 *          as a function declaration with an implicit int and every assertion in the build would
 *          check nothing while appearing to.
 */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
#error "embedded_types needs C11 or later - no earlier revision has a static assertion"
#elif __STDC_VERSION__ >= 202311L
#define EMBED_STATIC_ASSERT(cond_, msg_) static_assert(cond_, msg_)
#else

#define EMBED_STATIC_ASSERT(cond_, msg_) _Static_assert(cond_, msg_)
#endif

/**
 * @brief Expands to extern "C" { where __cplusplus is defined.
 *
 * @note This library is C. The guard is here so a C++ caller can include a C header and reach the
 *       symbols by their unmangled names, which is a property of the consumer's compiler rather
 *       than of anything declared here.
 * @warning The expansion contains an unmatched {; EMBED_END_DECLS supplies the }.
 * @note Expands to nothing where __cplusplus is undefined.
 */
#ifdef __cplusplus
#define EMBED_BEGIN_DECLS                                                                                              \
    extern "C"                                                                                                         \
    {

/** @brief Expands to } where __cplusplus is defined, closing EMBED_BEGIN_DECLS. */
#define EMBED_END_DECLS }
#else

/** @brief Expands to nothing where __cplusplus is undefined, so no brace is opened and none is owed. */
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
 * @note The inner half of the two-step paste. ## suppresses expansion of its own operands, so a
 *       caller pasting a macro's value rather than its name has to go through EMBED_CAT.
 */
#define EMBED_CAT_(left_, right_) left_##right_

/**
 * @brief Expands to EMBED_CAT_(left_, right_).
 *
 * @param[in] left_  Left operand, forwarded to EMBED_CAT_.
 * @param[in] right_ Right operand, forwarded to EMBED_CAT_.
 * @return           The single token formed by joining left_ and right_.
 * @note The outer half. Its arguments expand before substitution, so a count arrives as a number
 *       and EMBED_CAT_ pastes that rather than the name of the macro that produced it.
 * @note Builds a macro name from a count, which is how the dispatch layout family selects its arity.
 */
#define EMBED_CAT(left_, right_) EMBED_CAT_(left_, right_)

/**
 * @brief Expands to EMBED_ARG_N with __VA_ARGS__ followed by the constants 24 down to 0.
 *
 * @param[in] ... The list to count.
 * @return        The number of arguments, for one to twenty-four arguments.
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
 * @note Called by EMBED_NARG. The indirection is what lets the caller's list expand before the
 *       selection happens.
 */
#define EMBED_NARG_(...) EMBED_ARG_N(__VA_ARGS__)

/**
 * @brief Expands to its twenty-fifth argument.
 *
 * @param[in] slot1_    Arguments one through twenty-four, discarded.
 * @param[in] selected_ The twenty-fifth argument, which is the one returned.
 * @param[in] ...       Arguments beyond the twenty-fifth, discarded.
 * @return              selected_.
 * @note How the count is taken. EMBED_NARG hands the caller's list followed by 24 down to 0, so the
 *       list shifts the constants along and whichever one lands in the twenty-fifth slot is the
 *       number of arguments the caller passed.
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
 * @note A long argument list is passed in as many registers as it has and spills the rest; one
 *       pointer to a block the caller already laid out is one register whatever the arity.
 * @note A member the initializer does not name is zero initialized, and a compound literal in
 *       argument position lives to the end of the enclosing block, so the entry may hold the
 *       pointer for the whole call. Both are properties of the standard rather than of a compiler.
 * @warning entry_ receives the address of the literal [BORROWS].
 */
#define EMBED_CALL(entry_, ArgsType_, ...) entry_(&(ArgsType_){__VA_ARGS__})

/**
 * @brief Expands to __attribute__((always_inline)) prefixed by static inline.
 *
 * @note always_inline requires rather than asks, so a helper the inliner would have left out of
 *       line on size is inlined anyway. That is the difference between a helper naming a step and a
 *       helper costing a call.
 * @note Expands to static inline where EMBED_HAS_ATTRIBUTE(always_inline) is 0, which costs speed
 *       and never correctness.
 * @warning No definition is made when EMBED_INLINE is already defined. A build wanting a different
 *          inlining policy defines it ahead of this header rather than editing this.
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
 * @note Asks the compiler to inline everything the function it marks calls, reaching bodies the
 *       inliner would otherwise leave out of line on size. EMBED_INLINE marks a body; this marks
 *       the function whose calls are to be flattened into it.
 * @note Costs the inlined code at every site that carries it, so it belongs on one hot function
 *       rather than on a translation unit.
 * @warning Needs the called bodies visible, so a build without link-time optimization gets nothing
 *          from it.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(flatten) is 0, which costs speed and never
 *          correctness.
 */
#if EMBED_HAS_ATTRIBUTE(flatten)
#define EMBED_FLATTEN __attribute__((flatten))
#else

#define EMBED_FLATTEN
#endif

/**
 * @brief Expands to __attribute__((packed)) where EMBED_HAS_ATTRIBUTE(packed) is non-zero.
 *
 * @note An enum carrying this is declared at the width its range needs rather than at int width. A
 *       struct with such a member takes its offsets from that width, so the attribute belongs to
 *       the layout and is not a size optimization.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(packed) is 0, and a compiler may also
 *          accept the attribute and then ignore it. Neither case is visible from here, which is why
 *          embed_types.h declares a probe enum and asserts its size rather than trusting this #if.
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
 * @note Used in both directions. It raises the alignment of an object above its natural one, and it
 *       lowers alignment to 1, which is half of EMBED_RAW.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(aligned) is 0, ignoring bytes_. A raise
 *          that vanishes leaves an object less aligned than its use expects. A lower to 1 that
 *          vanishes leaves the type at its natural alignment while the code still reads it from any
 *          address.
 */
#if EMBED_HAS_ATTRIBUTE(aligned)
#define EMBED_ALIGN(bytes_) __attribute__((aligned(bytes_)))
#else

#define EMBED_ALIGN(bytes_)
#endif

/**
 * @brief Expands to __attribute__((may_alias)) where EMBED_HAS_ATTRIBUTE(may_alias) is non-zero.
 *
 * @note A character type may alias any object. A word lvalue reading the bytes of an eight-bit
 *       array is the direction the aliasing rules forbid, and this attribute is what permits it.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(may_alias) is 0. Nothing diagnoses that.
 *          The code still compiles and the compiler is free to assume the two accesses never meet.
 */
#if EMBED_HAS_ATTRIBUTE(may_alias)
#define EMBED_ALIAS __attribute__((may_alias))
#else

#define EMBED_ALIAS
#endif

/**
 * @brief Expands to EMBED_ALIGN(1) followed by EMBED_ALIAS.
 *
 * @note The pair a type needs before a word may be read from an address that is not a multiple of
 *       its width. The alignment says the compiler may not assume one; the aliasing says the read
 *       may reach bytes another type owns. Either alone leaves the access undefined.
 * @note Spelled once here because a consumer that writes the pair per declaration gets them out of
 *       step the first time one is edited.
 * @warning Carries both warnings of the two macros it expands to, and neither failure is diagnosed.
 */
#define EMBED_RAW EMBED_ALIGN(1) EMBED_ALIAS

/**
 * @brief Expands to __attribute__((unused)) where EMBED_HAS_ATTRIBUTE(unused) is non-zero.
 *
 * @note Suppresses the unused-variable diagnostic on a definition deliberately left unreferenced. A
 *       table defined in a header with internal linkage reaches every translation unit that
 *       includes it, and one calling nothing through its copy would warn about a definition it
 *       never asked for.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(unused) is 0. That costs a diagnostic and
 *          never correctness.
 */
#if EMBED_HAS_ATTRIBUTE(unused)
#define EMBED_UNUSED __attribute__((unused))
#else

#define EMBED_UNUSED
#endif

/**
 * @brief Expands to __attribute__((weak)) where EMBED_HAS_ATTRIBUTE(weak) is non-zero.
 *
 * @note A weak definition is replaced by a strong one of the same name, and links as itself where
 *       none is supplied. A default that refuses can therefore ship, and a build that supplies the
 *       real symbol overrides it without editing the default away.
 * @warning Expands to nothing where EMBED_HAS_ATTRIBUTE(weak) is 0. The default then has external
 *          linkage like any other definition, so a replacement collides with it instead of
 *          overriding it and the link fails on a duplicate symbol.
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
 * @note Called by EMBED_DIAGNOSTIC_IGNORE, which builds a whole pragma line and stringizes it in
 *       one step because _Pragma takes a string literal.
 * @note Defined once above the compiler arms rather than inside each. The definition does not vary
 *       by compiler, and two identical copies drift the moment one is edited.
 */
#define EMBED_DIAGNOSTIC_STRING(text_) #text_

#if defined(__clang__)
/**
 * @brief Expands to _Pragma("clang diagnostic push") where __clang__ is defined.
 *
 * @note Saves the diagnostic state so a suppression can be bounded. An ignore with nothing saved
 *       ahead of it runs to the end of the translation unit and silences code it was never meant to
 *       cover, so every EMBED_DIAGNOSTIC_IGNORE sits between this and EMBED_DIAGNOSTIC_POP.
 */
#define EMBED_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")

/**
 * @brief Expands to _Pragma("clang diagnostic pop") where __clang__ is defined.
 *
 * @note Restores the state EMBED_DIAGNOSTIC_PUSH saved, which is what ends a suppression. Omitting
 *       it leaves the ignore in force for the rest of the translation unit, and nothing diagnoses
 *       that because the diagnostic it would have raised is the one being suppressed.
 */
#define EMBED_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")

/**
 * @brief Expands to a clang pragma ignoring the named warning.
 *
 * @param[in] warning_ Warning name as a string literal, such as "-Wpadded".
 * @note Suppresses one named diagnostic, and belongs between an EMBED_DIAGNOSTIC_PUSH and an
 *       EMBED_DIAGNOSTIC_POP so it ends where the code needing it ends.
 * @note EMBED_DIAGNOSTIC_STRING stringizes the whole pragma text, including warning_.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_) _Pragma(EMBED_DIAGNOSTIC_STRING(clang diagnostic ignored warning_))
#elif defined(__GNUC__)
/** @brief Expands to _Pragma("GCC diagnostic push") where __GNUC__ is defined and __clang__ is not. */
#define EMBED_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")

/** @brief Expands to _Pragma("GCC diagnostic pop") where __GNUC__ is defined and __clang__ is not. */
#define EMBED_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")

/**
 * @brief Expands to a GCC pragma ignoring the named warning.
 *
 * @param[in] warning_ Warning name as a string literal, such as "-Wpadded".
 * @note Selected where __GNUC__ is defined and __clang__ is not.
 * @note Belongs between an EMBED_DIAGNOSTIC_PUSH and an EMBED_DIAGNOSTIC_POP so it ends where the
 *       code needing it ends.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_) _Pragma(EMBED_DIAGNOSTIC_STRING(GCC diagnostic ignored warning_))
#else

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @note A compiler with no diagnostic pragma has nothing to save, so the bracket a suppression sits
 *       in costs nothing and the calling code needs no arm of its own.
 */
#define EMBED_DIAGNOSTIC_PUSH

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @note Pairs with EMBED_DIAGNOSTIC_PUSH, which also expands to nothing here.
 */
#define EMBED_DIAGNOSTIC_POP

/**
 * @brief Expands to nothing when __clang__ and __GNUC__ are both undefined.
 *
 * @param[in] warning_ Warning name as a string literal, discarded.
 * @warning The diagnostic is not suppressed on this arm. A compiler here that raises it anyway
 *          reports it, which is the safe direction.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_)
#endif

/**
 * @brief Set to 1 where __BYTE_ORDER__ and __ORDER_BIG_ENDIAN__ are both defined and equal, 0 otherwise.
 *
 * @warning Neither definition is made when EMBED_BIG_ENDIAN is already defined, so a target whose
 *          compiler states neither macro sets it on the command line rather than being guessed at.
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
 * @note Not whether an unaligned load compiles. A type carrying EMBED_RAW accepts one everywhere.
 *       It is whether the target performs one directly.
 * @note __ARM_FEATURE_UNALIGNED is the compiler's own answer, and is switched off by
 *       -mno-unaligned-access. No part is named here: a target list would be a guess from a
 *       compiler's identity, which is the thing every other gate in this file avoids.
 * @note Answers 0 where nothing is stated, which is the safe direction.
 * @warning Neither definition is made when EMBED_FAST_UNALIGNED_LOAD is already defined.
 */
#ifndef EMBED_FAST_UNALIGNED_LOAD
#if defined(__ARM_FEATURE_UNALIGNED)
#define EMBED_FAST_UNALIGNED_LOAD 1
#else

#define EMBED_FAST_UNALIGNED_LOAD 0
#endif
#endif

#endif
