/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_compiler_directives.h
 * @brief Compiler feature checks, attribute wrappers, diagnostic pragmas, an argument count, and
 *        target flags.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 *
 * @note This header has no `extern "C"` guard of its own, because it defines that guard as
 *       EMBED_BEGIN_DECLS. Each name here is a macro, and a macro has no linkage to mangle.
 * @note If your library uses this one, make aliases of these macros under your own prefix. Do not
 *       copy them into your own tree. A copy drifts when this header changes.
 */
#ifndef EMBED_COMPILER_DIRECTIVES_H
#define EMBED_COMPILER_DIRECTIVES_H

#include <stddef.h>

/**
 * @brief Set to 1 where the compiler defines __GNUC__ or __clang__, 0 otherwise.
 *
 * @note EMBED_HAS_ATTRIBUTE falls back to this value where the compiler has no __has_attribute. A
 *       compiler too old for __has_attribute usually still accepts the GNU attributes, and a 0 here
 *       would turn all of them off.
 * @note This macro always has a value, 0 or 1. Test it with #if. #ifdef is true either way.
 */
#if defined(__GNUC__) || defined(__clang__)
#define EMBED_GNU_ATTRIBUTES 1
#else

#define EMBED_GNU_ATTRIBUTES 0
#endif

/**
 * @brief Expands to `__has_attribute(attribute_)` where the compiler has __has_attribute.
 *
 * @param[in] attribute_ Attribute name, as __has_attribute takes it.
 * @return               The value that __has_attribute gives for attribute_.
 * @note Each attribute macro below goes through this one. A question to the compiler about an
 *       attribute is more reliable than a test of which compiler it is.
 * @warning Expands to EMBED_GNU_ATTRIBUTES where the compiler has no __has_attribute, and discards
 *          attribute_. Then each attribute gets the same answer. A compiler without __has_attribute
 *          gets all of them or none.
 */
#if defined(__has_attribute)
#define EMBED_HAS_ATTRIBUTE(attribute_) __has_attribute(attribute_)
#else

#define EMBED_HAS_ATTRIBUTE(attribute_) EMBED_GNU_ATTRIBUTES
#endif

/**
 * @brief Expands to `__has_builtin(builtin_)` where the compiler has __has_builtin.
 *
 * @param[in] builtin_ Builtin name, as __has_builtin takes it.
 * @return             The value that __has_builtin gives for builtin_.
 * @note This macro is the builtin form of EMBED_HAS_ATTRIBUTE. Use it to gate a builtin as you
 *       gate an attribute.
 * @warning Without __has_builtin this expands to 0 and discards builtin_. The fallback is 0, because
 *          a builtin that the compiler does not have fails to compile. EMBED_HAS_ATTRIBUTE falls
 *          back to a guess instead, because the compiler ignores an attribute that it does not have.
 */
#if defined(__has_builtin)
#define EMBED_HAS_BUILTIN(builtin_) __has_builtin(builtin_)
#else

#define EMBED_HAS_BUILTIN(builtin_) 0
#endif

/**
 * @brief Expands to a static assertion of two operands.
 *
 * @param[in] cond_ Constant expression, passed through with no change.
 * @param[in] msg_  Message operand, passed through with no change.
 * @note This macro gives one name for the assertion across C11, C23, and C++. Each file here that
 *       claims a width or a layout uses it.
 * @note C++ and C23 get static_assert, which is a keyword in both. All other revisions get
 *       _Static_assert, the C11 keyword.
 * @note The __cplusplus arm is necessary. No C++ compiler defines __STDC_VERSION__, and without
 *       that arm the #error below fires on each C++ build.
 * @note Neither arm includes <assert.h>. Both names are keywords in the revision that takes them.
 * @note The expansion has no semicolon at its end. Write one at the call site.
 * @warning #error below C11, where neither keyword exists. Without it, the name would parse as a
 *          function declaration that returns implicit int. Then each assertion in the build would
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
 * @brief Expands to `extern "C" {` where the compiler defines __cplusplus.
 *
 * @note This library is C. A C++ translation unit that includes these headers needs the guard to
 *       reach the symbols by their names without the C++ mangle.
 * @warning The expansion leaves a `{` open. EMBED_END_DECLS closes it.
 * @note Expands to nothing where the compiler does not define __cplusplus.
 */
#ifdef __cplusplus
#define EMBED_BEGIN_DECLS                                                                                              \
    extern "C"                                                                                                         \
    {

/** @brief Expands to `}` where the compiler defines __cplusplus, and closes EMBED_BEGIN_DECLS. */
#define EMBED_END_DECLS }
#else

/** @brief Expands to nothing where the compiler does not define __cplusplus. This arm opens no brace. */
#define EMBED_BEGIN_DECLS

/** @brief Expands to nothing where the compiler does not define __cplusplus. */
#define EMBED_END_DECLS
#endif

/**
 * @brief Expands to `static inline` followed by `__attribute__((always_inline))`.
 *
 * @note GCC and clang put the body in line, also where their size heuristic would keep it out of
 *       line. A plain `static inline` gives that decision to the compiler.
 * @note Expands to `static inline` where `EMBED_HAS_ATTRIBUTE(always_inline)` is 0. The code stays
 *       correct. It can run slower.
 * @warning This header defines nothing where the build defines EMBED_INLINE first. Define it ahead
 *          of this header to set your own policy for the inline expansion.
 */
#ifndef EMBED_INLINE
#if EMBED_HAS_ATTRIBUTE(always_inline)
#define EMBED_INLINE static inline __attribute__((always_inline))
#else
#define EMBED_INLINE static inline
#endif
#endif

/**
 * @brief Expands to `__attribute__((flatten))` where `EMBED_HAS_ATTRIBUTE(flatten)` is not zero.
 *
 * @note Put this on a function, and GCC or clang puts each call of that function in line. This
 *       includes the bodies that their size heuristic would skip. EMBED_INLINE goes on the callee.
 *       This macro goes on the caller.
 * @note Each call site of the marked function holds the code put in line. Put it on one hot function.
 * @warning The bodies of the calls must be visible. Without link-time optimization this does nothing
 *          across translation units.
 * @warning Expands to nothing where `EMBED_HAS_ATTRIBUTE(flatten)` is 0. The code stays correct. It
 *          can run slower.
 */
#if EMBED_HAS_ATTRIBUTE(flatten)
#define EMBED_FLATTEN __attribute__((flatten))
#else

#define EMBED_FLATTEN
#endif

/**
 * @brief Declares name_ as a one-byte enumeration of the enumerators given, on each compiler.
 *
 * @param[in] name_ The name of the type.
 * @param[in] ...   The enumerators, as the braces of an enum hold them.
 * @note Where the compiler can pin an enum to one byte, name_ is that enum. Then the compiler still
 *       checks a switch on it, or a value of another enum.
 * @note Clang on the MSVC ABI and C++ take the fixed type `: unsigned char`. C++11 made that form
 *       standard, and clang takes it in C as the C23 form.
 * @note Clang on the MSVC ABI also accepts the packed attribute, but it keeps the enum as wide as an
 *       int, as MSVC does. So that arm takes the fixed type before it looks for the attribute. Each
 *       other compiler with the packed attribute takes the attribute.
 * @note Where the compiler cannot pin an enum, MSVC's C among them, the enumerators become constants
 *       and name_ is an unsigned char. The width is one byte either way. A struct that holds name_
 *       has the same layout on each compiler.
 * @warning An enumerator above 255 or below 0 fails the build on the arms of the fixed type. It
 *          widens the enum on the attribute arm. On the unsigned char arm, a store cuts it to a byte.
 *          embed_types.h declares a probe and asserts its size. That assertion also finds a compiler
 *          that accepts the attribute and ignores it.
 */
#if defined(__clang__) && defined(_MSC_VER)
// This arm exists for the width of the fixed type. The pragma stops the clang note that C11 has no
// fixed type, for that one warning and on this arm alone.
#pragma clang diagnostic ignored "-Wmicrosoft-fixed-enum"
#define EMBED_ENUM_BYTE(name_, ...)                                                                                    \
    typedef enum : unsigned char                                                                                       \
    {                                                                                                                  \
        __VA_ARGS__                                                                                                    \
    } name_
#elif EMBED_HAS_ATTRIBUTE(packed)
#define EMBED_ENUM_BYTE(name_, ...)                                                                                    \
    typedef enum __attribute__((packed))                                                                               \
    {                                                                                                                  \
        __VA_ARGS__                                                                                                    \
    } name_
#elif defined(__cplusplus)
#define EMBED_ENUM_BYTE(name_, ...)                                                                                    \
    typedef enum : unsigned char                                                                                       \
    {                                                                                                                  \
        __VA_ARGS__                                                                                                    \
    } name_
#else
#define EMBED_ENUM_BYTE(name_, ...)                                                                                    \
    enum                                                                                                               \
    {                                                                                                                  \
        __VA_ARGS__                                                                                                    \
    };                                                                                                                 \
    typedef unsigned char name_
#endif

/**
 * @brief Expands to `__attribute__((aligned(bytes_)))` where `EMBED_HAS_ATTRIBUTE(aligned)` is not
 *        zero.
 *
 * @param[in] bytes_ The alignment that the attribute takes.
 * @note This macro works both ways. It can raise the alignment of an object above its natural
 *       alignment, or lower it to 1. EMBED_RAW uses the form that lowers it to 1.
 * @warning Expands to nothing where `EMBED_HAS_ATTRIBUTE(aligned)` is 0, and discards bytes_. Then an
 *          object that you mean to align more keeps its natural alignment. A type that you mean to
 *          drop to alignment 1 also keeps its natural alignment, and the code still reads it from
 *          any address.
 */
#if EMBED_HAS_ATTRIBUTE(aligned)
#define EMBED_ALIGN(bytes_) __attribute__((aligned(bytes_)))
#elif defined(_MSC_VER)
// MSVC takes the C11 _Alignas. It stands before a member or an object, and it takes a constant
// expression. It can raise an alignment but it cannot lower one: a request for 1 keeps the natural
// alignment, as the warning above states. C++ writes the same specifier as alignas. That is the
// case where nvcc compiles a device source with cl as its host compiler.
#if defined(__cplusplus)
#define EMBED_ALIGN(bytes_) alignas(bytes_)
#else
#define EMBED_ALIGN(bytes_) _Alignas(bytes_)
#endif
#else

#define EMBED_ALIGN(bytes_)
#endif

/**
 * @brief Expands to `__attribute__((may_alias))` where `EMBED_HAS_ATTRIBUTE(may_alias)` is not zero.
 *
 * @note C lets a character type alias any object. A read of an eight-bit array through a word lvalue
 *       goes the other way, and C leaves that read undefined. This attribute on the word type gives
 *       the read a defined result.
 * @warning Expands to nothing where `EMBED_HAS_ATTRIBUTE(may_alias)` is 0. Nothing diagnoses that.
 *          The code compiles, and the compiler can assume that the two accesses never touch the same
 *          memory.
 */
#if EMBED_HAS_ATTRIBUTE(may_alias)
#define EMBED_ALIAS __attribute__((may_alias))
#else

#define EMBED_ALIAS
#endif

/**
 * @brief Expands to `EMBED_ALIGN(1)` followed by EMBED_ALIAS.
 *
 * @note A read of a word from an address that is not a multiple of its width needs both attributes.
 *       `EMBED_ALIGN(1)` stops the compiler from an assumption about the alignment. EMBED_ALIAS lets
 *       the read reach bytes that another type owns. Either one alone leaves the access undefined.
 * @note This macro holds both attributes. If each declaration writes out the pair, the two can drift
 *       apart at the first edit of one.
 * @warning Both warnings above apply here. Nothing diagnoses either failure.
 */
#if EMBED_HAS_ATTRIBUTE(aligned)
#define EMBED_RAW EMBED_ALIGN(1) EMBED_ALIAS
#else
// _Alignas stands before a declaration, and it cannot lower an alignment. EMBED_RAW stands after the
// name and asks for 1, so it has no form there. The type is the plain word, as the warning above
// states.
#define EMBED_RAW EMBED_ALIAS
#endif

/**
 * @brief Expands to `__host__ __device__` under nvcc, and to nothing on other compilers.
 *
 * @note A function with this macro compiles into each of the two passes of nvcc from one definition.
 *       Other compilers have one pass, and the function compiles into it. Each entry of embed_atomic.h
 *       and embed_lane.h has this macro, so one source runs on the host and on the device.
 * @note Both arms define this macro.
 */
#if defined(__CUDACC__)
#define EMBED_HOST_DEVICE __host__ __device__
#else
#define EMBED_HOST_DEVICE
#endif

/**
 * @brief Set to 1 where `EMBED_HAS_ATTRIBUTE(vector_size)` is not zero, else 0.
 *
 * @note The vector_size attribute on a scalar type makes a type that the compiler keeps in a vector
 *       register. The plain operators then run over each element at once. embed_lane.h takes its
 *       vector arm where this macro is 1, and its portable arm where it is 0.
 * @note Both arms define this macro. Test it with #if.
 */
#if EMBED_HAS_ATTRIBUTE(vector_size)
#define EMBED_HAS_VECTOR_SIZE 1
#else
#define EMBED_HAS_VECTOR_SIZE 0
#endif

/**
 * @brief Expands to `__attribute__((vector_size(bytes_)))` where EMBED_HAS_VECTOR_SIZE is 1.
 *
 * @param[in] bytes_ The width of the vector in bytes, a power of two and a multiple of the size of
 *                   the scalar.
 * @warning Expands to nothing where EMBED_HAS_VECTOR_SIZE is 0, and discards bytes_: the type is the
 *          scalar alone. A declaration that needs the width tests EMBED_HAS_VECTOR_SIZE first. On
 *          the other arm it declares a struct of the elements, as embed_lane.h does.
 */
#if EMBED_HAS_VECTOR_SIZE
#define EMBED_VECTOR(bytes_) __attribute__((vector_size(bytes_)))
#else
#define EMBED_VECTOR(bytes_)
#endif

/**
 * @brief The width in bits of the widest vector register that the target compiles for. It is 512
 *        under AVX-512, 256 under AVX2, 128 under SSE2 or NEON, and 0 where the target has none.
 *
 * @note The compiler defines a macro for each instruction set that it emits, and this value comes
 *       from those macros. The build supplies no width of its own here. A flag that turns on an
 *       instruction set changes this value with it. embed_lane.h derives the width of the lane from
 *       this value where the build does not define one.
 * @note Both arms define this macro. Test it with #if.
 */
#if defined(__AVX512F__)
#define EMBED_VECTOR_REGISTER_BITS 512
#elif defined(__AVX2__)
#define EMBED_VECTOR_REGISTER_BITS 256
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2)) || defined(__ARM_NEON) ||       \
    defined(__aarch64__) || defined(_M_ARM64)
#define EMBED_VECTOR_REGISTER_BITS 128
#else
#define EMBED_VECTOR_REGISTER_BITS 0
#endif

/**
 * @brief Set to 1 where the target runs AVX2 and the compiler has the builtin for the mask of 32
 *        bytes, else 0. The builtin reads the sign bit of each byte into an integer.
 *
 * @note The builtin is the compiler's own name for the instruction, and it needs no vendor header.
 *       A byte compare of two vectors of 32 bytes sets each equal byte to all ones. This builtin
 *       then reads those bytes into one bit for each byte, in one instruction.
 * @note Both arms define this macro. Test it with #if.
 */
#if defined(__AVX2__) && EMBED_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
#define EMBED_HAS_BYTE_MASK_256 1
#else
#define EMBED_HAS_BYTE_MASK_256 0
#endif

/**
 * @brief The sign bits of 32 bytes as an int, bit 0 from the first byte, where EMBED_HAS_BYTE_MASK_256
 *        is 1.
 *
 * @param[in] chars_ A vector of 32 char.
 * @warning Expands to 0 where EMBED_HAS_BYTE_MASK_256 is 0, and discards chars_. A use tests the flag
 *          first and takes another arm where it is 0, as embed_lane.h does.
 */
#if EMBED_HAS_BYTE_MASK_256
#define EMBED_BYTE_MASK_256(chars_) __builtin_ia32_pmovmskb256(chars_)
#else
#define EMBED_BYTE_MASK_256(chars_) 0
#endif

/**
 * @brief Set to 1 where the target runs SSE2 and the compiler has the builtin for the mask of 16
 *        bytes, else 0. The builtin reads the sign bit of each byte into an integer.
 *
 * @note Both arms define this macro. Test it with #if.
 */
#if defined(__SSE2__) && EMBED_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
#define EMBED_HAS_BYTE_MASK_128 1
#else
#define EMBED_HAS_BYTE_MASK_128 0
#endif

/**
 * @brief The sign bits of 16 bytes as an int, bit 0 from the first byte, where EMBED_HAS_BYTE_MASK_128
 *        is 1.
 *
 * @param[in] chars_ A vector of 16 char.
 * @warning Expands to 0 where EMBED_HAS_BYTE_MASK_128 is 0, and discards chars_. A use tests the flag
 *          first and takes another arm where it is 0, as embed_lane.h does.
 */
#if EMBED_HAS_BYTE_MASK_128
#define EMBED_BYTE_MASK_128(chars_) __builtin_ia32_pmovmskb128(chars_)
#else
#define EMBED_BYTE_MASK_128(chars_) 0
#endif

/**
 * @brief Expands to `__attribute__((unused))` where `EMBED_HAS_ATTRIBUTE(unused)` is not zero.
 *
 * @note Put this on a definition that the code can leave with no reference. A static definition in
 *       a header goes into each translation unit that includes it. A unit that never uses its copy
 *       gives a warning.
 * @warning Expands to nothing where `EMBED_HAS_ATTRIBUTE(unused)` is 0. Then the warning comes back.
 *          The code stays correct.
 */
#if EMBED_HAS_ATTRIBUTE(unused)
#define EMBED_UNUSED __attribute__((unused))
#else

#define EMBED_UNUSED
#endif

/**
 * @brief Expands to `__attribute__((weak))` where `EMBED_HAS_ATTRIBUTE(weak)` is not zero.
 *
 * @note The linker takes a strong definition of a name before a weak one. With no strong definition,
 *       the weak one links. So a default can ship here, and a port can replace it with no edit to
 *       this file.
 * @warning Expands to nothing where `EMBED_HAS_ATTRIBUTE(weak)` is 0. The default then has ordinary
 *          external linkage. A replacement becomes a duplicate symbol, and the link fails.
 */
#if EMBED_HAS_ATTRIBUTE(weak)
#define EMBED_WEAK __attribute__((weak))
#else

#define EMBED_WEAK
#endif

/**
 * @brief The name of the definition of a weak default. It is name_ itself where
 *        `EMBED_HAS_ATTRIBUTE(weak)` is not zero, and name_ with `_default` after it under MSVC
 *        without the attribute.
 *
 * @param[in] name_ The name that callers reach and that a port defines.
 * @return          The name of the definition of the default.
 * @note The MSVC linker has no weak definition. Its /alternatename option gives a second name to a
 *       name that no unit defines. So the default stands under the second name. A port that defines
 *       name_ then wins, with no duplicate. EMBED_WEAK_DEFAULT emits that alternate name.
 * @note Write both at the default: `EMBED_WEAK_DEFAULT(name_)` at file scope, then the definition
 *       with EMBED_WEAK and `EMBED_WEAK_NAME(name_)`.
 * @warning Under MSVC, a call to the default by name_ in the unit that defines it goes through the
 *          linker, and never in line.
 */
#if !EMBED_HAS_ATTRIBUTE(weak) && defined(_MSC_VER)
#define EMBED_WEAK_NAME(name_) name_##_default
#else
#define EMBED_WEAK_NAME(name_) name_
#endif

/**
 * @brief Emits the alternate name of the linker for a weak default under MSVC without the weak
 *        attribute, and nothing on other compilers.
 *
 * @param[in] name_ The name that callers reach and that a port defines.
 * @note A C symbol on 32-bit x86 has an underscore at its start, and the linker reads the decorated
 *       names. So the x86 arm adds one underscore to each side.
 */
#if !EMBED_HAS_ATTRIBUTE(weak) && defined(_MSC_VER) && defined(_M_IX86)
#define EMBED_WEAK_DEFAULT(name_) __pragma(comment(linker, "/alternatename:_" #name_ "=_" #name_ "_default"))
#elif !EMBED_HAS_ATTRIBUTE(weak) && defined(_MSC_VER)
#define EMBED_WEAK_DEFAULT(name_) __pragma(comment(linker, "/alternatename:" #name_ "=" #name_ "_default"))
#else
#define EMBED_WEAK_DEFAULT(name_)
#endif

/**
 * @brief Expands to `#text_`.
 *
 * @param[in] text_ The tokens to make into a string.
 * @return          text_ as a string literal.
 * @note EMBED_DIAGNOSTIC_IGNORE calls this macro. _Pragma takes a string literal, and this macro
 *       makes the full pragma line into one.
 * @note This macro stands above the compiler arms below. A definition in each arm would put two
 *       copies in the file, and they would drift at the first edit of one.
 */
#define EMBED_DIAGNOSTIC_STRING(text_) #text_

#if defined(__clang__)
/**
 * @brief Expands to `_Pragma("clang diagnostic push")` where the compiler defines __clang__.
 *
 * @note This macro saves the current diagnostic state. Without a save ahead of it, an
 *       EMBED_DIAGNOSTIC_IGNORE stays in force to the end of the translation unit. Put each
 *       EMBED_DIAGNOSTIC_IGNORE between this macro and EMBED_DIAGNOSTIC_POP.
 */
#define EMBED_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")

/**
 * @brief Expands to `_Pragma("clang diagnostic pop")` where the compiler defines __clang__.
 *
 * @note This macro restores the state that EMBED_DIAGNOSTIC_PUSH saved, and that ends the
 *       suppression. Without it, the suppression runs to the end of the translation unit. Nothing
 *       reports that, because the suppressed warning is the warning that would report it.
 */
#define EMBED_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")

/**
 * @brief Expands to a clang pragma that ignores the named warning.
 *
 * @param[in] warning_ Warning name as a string literal, such as "-Wpadded".
 * @note This macro turns off exactly one warning. See EMBED_DIAGNOSTIC_PUSH for where to put it.
 * @note EMBED_DIAGNOSTIC_STRING makes the full pragma text into a string, warning_ with it.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_) _Pragma(EMBED_DIAGNOSTIC_STRING(clang diagnostic ignored warning_))
#elif defined(__GNUC__)
/**
 * @brief Expands to `_Pragma("GCC diagnostic push")` where the compiler defines __GNUC__ and not
 *        __clang__.
 * @note This macro is the GCC form of the clang push above. See that block for where to put
 *       EMBED_DIAGNOSTIC_IGNORE.
 */
#define EMBED_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")

/**
 * @brief Expands to `_Pragma("GCC diagnostic pop")` where the compiler defines __GNUC__ and not
 *        __clang__.
 * @note This macro is the GCC form of the clang pop above. See that block for what occurs without
 *       it.
 */
#define EMBED_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")

/**
 * @brief Expands to a GCC pragma that ignores the named warning.
 *
 * @param[in] warning_ Warning name as a string literal, such as "-Wpadded".
 * @note The compiler takes this arm where it defines __GNUC__ and not __clang__.
 * @note This macro turns off exactly one warning. See EMBED_DIAGNOSTIC_PUSH for where to put it.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_) _Pragma(EMBED_DIAGNOSTIC_STRING(GCC diagnostic ignored warning_))
#else

/**
 * @brief Expands to nothing where the compiler defines neither __clang__ nor __GNUC__.
 *
 * @note A compiler with no diagnostic pragma has no state to save. The code that calls these macros
 *       keeps the same push, ignore, and pop shape on each compiler, and this arm costs nothing.
 */
#define EMBED_DIAGNOSTIC_PUSH

/**
 * @brief Expands to nothing where the compiler defines neither __clang__ nor __GNUC__.
 *
 * @note This macro pairs with EMBED_DIAGNOSTIC_PUSH. That macro also expands to nothing on this arm.
 */
#define EMBED_DIAGNOSTIC_POP

/**
 * @brief Expands to nothing where the compiler defines neither __clang__ nor __GNUC__.
 *
 * @param[in] warning_ Warning name as a string literal, which this arm discards.
 * @warning This arm does not stop the warning. A compiler that raises it still reports it. A report
 *          is the safe direction.
 */
#define EMBED_DIAGNOSTIC_IGNORE(warning_)
#endif

/**
 * @brief Set to 1 where __BYTE_ORDER__ and __ORDER_BIG_ENDIAN__ both have a value and the two are
 *        equal, 0 otherwise.
 *
 * @note A consumer that reads or writes values of more than one byte, one byte at a time, branches
 *       on this macro. Nothing in this library reads it.
 * @warning This header defines nothing where the build defines EMBED_BIG_ENDIAN first. A compiler
 *          that defines neither __BYTE_ORDER__ nor __ORDER_BIG_ENDIAN__ falls to 0 here. For a
 *          big-endian target with such a compiler, set the macro on the command line.
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
 * @note An unaligned load compiles on any target through a type with EMBED_RAW. This flag is 1 only
 *       where the target does that load in one instruction.
 * @note GCC and clang define __ARM_FEATURE_UNALIGNED on the targets that can do it. The option
 *       -mno-unaligned-access turns it off.
 * @note This macro names no target. The flag falls to 0 wherever that macro is absent, and 0 is the
 *       safe answer. A consumer that sees 0 takes the path one byte at a time, which is correct on
 *       each target.
 * @warning This header defines nothing where the build defines EMBED_FAST_UNALIGNED_LOAD first.
 *          Nothing in this library reads this flag.
 */
#ifndef EMBED_FAST_UNALIGNED_LOAD
#if defined(__ARM_FEATURE_UNALIGNED)
#define EMBED_FAST_UNALIGNED_LOAD 1
#else

#define EMBED_FAST_UNALIGNED_LOAD 0
#endif
#endif

#endif
