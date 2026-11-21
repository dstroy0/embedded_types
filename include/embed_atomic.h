/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_atomic.h
 * @brief Atomic words on the atomics of the target, with no lock and no system call.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * This header gives a 32-bit word and a 64-bit word. A host thread, a task on a part and a GPU thread
 * read, write, swap, compare and swap, add to and mask them. Each operation is one indivisible step.
 *
 * @note These steps alone retain a structure that contexts share consistent, and no context waits for
 *       another. A counter is a fetch-add. A claim is a compare-and-swap that takes, or that reports
 *       who took. A boundary is a compare-and-swap loop that moves, or that tries again over the value
 *       that it found. Nothing here spins on the progress of another context.
 * @note EMBED_ATOMIC_ARM in embed_compiler_directives.h selects one of four arms, one time. The first arm is
 *       the device atomics of CUDA in a device pass. The second is C11 atomics, where a C compiler has them.
 *       The third is the GCC and clang __atomic builtins, where C11 atomics are absent or the consumer is
 *       C++. The fourth is the interlocked intrinsics under MSVC. Each arm gives a load acquire ordering, a store
 *       release ordering, and each read-modify-write both.
 * @note There are two widths. Each arm has the 32-bit word, the CUDA atomics among them. The counters
 *       and packed boundaries of a host need the 64-bit word. CUDA, C11, the builtins and MSVC on a
 *       64-bit host all have it. MSVC on a 32-bit host does not, and EMBED_ATOMIC_HAS_64 is then 0.
 *       embed_atomic_word matches the machine word.
 * @note Every entry carries EMBED_HOST_DEVICE. Under nvcc one definition compiles into the host pass
 *       and the device pass, each on its own arm, over the same word in memory.
 * @warning A target with none of the four fails the build at the #error below. There is no plain
 *          fallback: a word that is not atomic is a defect that shows only under contention.
 */
#ifndef EMBED_ATOMIC_H
#define EMBED_ATOMIC_H

#include "embed_types.h"

#if EMBED_ATOMIC_ARM == 2
#include <stdatomic.h>
#elif EMBED_ATOMIC_ARM == 4
#include <intrin.h>
#endif

EMBED_BEGIN_DECLS

// The configuration header selects the arm. A target with none has no atomic word, and the build stops here
#if EMBED_ATOMIC_ARM == 0
#error "embed_atomic.h: this target has no atomics the header knows"
#endif

/**
 * @brief 1 where the arm has a 64-bit atomic word, and 0 where it has none. The MSVC arm has none on a 32-bit
 *        target, because MSVC gives the 64-bit interlocked intrinsics only to its 64-bit targets.
 *
 * @note Both arms define this macro. Test it with #if. Where it is 0, the header defines no 64-bit word and no
 *       operation on one, as it defines no 16-bit word. The word, the size and the pointer of such a target are 32
 *       bits.
 */
#if (EMBED_ATOMIC_ARM == 4) && !defined(_M_X64) && !defined(_M_ARM64)
#define EMBED_ATOMIC_HAS_64 0
#else
#define EMBED_ATOMIC_HAS_64 1
#endif

/**
 * @brief A 32-bit word every access to which is one indivisible step.
 *
 * @note The word is atomic on the C11 arm, and plain on the other arms. There, the device atomics,
 *       the builtins and the intrinsics make each access atomic. A word in storage of zeros holds 0 on
 *       each arm.
 */
typedef struct
{
#if EMBED_ATOMIC_ARM == 2
    _Atomic embed_u32 value; /**< The word. */
#else
    volatile embed_u32 value; /**< The word. */
#endif
} embed_atomic_u32;
EMBED_STATIC_ASSERT(sizeof(embed_atomic_u32) == 4u, "embed_atomic_u32 must be one 32-bit word on every arm");

#if EMBED_ATOMIC_HAS_64
/**
 * @brief A 64-bit word every access to which is one indivisible step, where EMBED_ATOMIC_HAS_64 is 1.
 */
typedef struct
{
#if EMBED_ATOMIC_ARM == 2
    _Atomic embed_u64 value; /**< The word. */
#else
    volatile embed_u64 value; /**< The word. */
#endif
} embed_atomic_u64;
EMBED_STATIC_ASSERT(sizeof(embed_atomic_u64) == 8u, "embed_atomic_u64 must be one 64-bit word on every arm");
#else
/**
 * @brief The 64-bit word on a target whose arm has none. Each use of the type stops the compile of that file, and
 *        the error names the cause.
 */
#define embed_atomic_u64 embed_atomic_u64_needs_a_target_with_64_bit_atomics
#endif

/**
 * @brief An atomic word that holds value_, for the declaration that makes one.
 *
 * @param[in] value_ The word's first value.
 */
#define EMBED_ATOMIC_INIT(value_) {(value_)}

#if EMBED_ATOMIC_ARM == 1

/**
 * @brief The primitives of the device arm, on the device atomics of CUDA.
 *
 * @note The load is a volatile read, and EMBED_ATOMIC_FENCE after it keeps each later read after the load. The
 *       store is a volatile write after the fence. Each read-modify-write is the atomic of its width, and each
 *       answers the word before it.
 * @note The device atomics take unsigned int for 32 bits and unsigned long long for 64. The casts read the
 *       same bits as the unsigned words that they are.
 */
#define EMBED_ATOMIC_LOAD(word_) ((word_)->value)
#define EMBED_ATOMIC_FENCE() __threadfence()
#define EMBED_ATOMIC_ORDERED() __threadfence()
#define EMBED_ATOMIC_STORE(word_, value_) ((word_)->value = (value_))
#define EMBED_ATOMIC_EXCHANGE_32(word_, value_) atomicExch((unsigned int *)&(word_)->value, (value_))
#define EMBED_ATOMIC_EXCHANGE_64(word_, value_) atomicExch((unsigned long long *)&(word_)->value, (value_))
#define EMBED_ATOMIC_SWAP_IF_32(word_, expected_, value_)                                                              \
    atomicCAS((unsigned int *)&(word_)->value, (expected_), (value_))
#define EMBED_ATOMIC_SWAP_IF_64(word_, expected_, value_)                                                              \
    atomicCAS((unsigned long long *)&(word_)->value, (expected_), (value_))
#define EMBED_ATOMIC_ADD_32(word_, value_) atomicAdd((unsigned int *)&(word_)->value, (value_))
#define EMBED_ATOMIC_ADD_64(word_, value_) atomicAdd((unsigned long long *)&(word_)->value, (value_))
#define EMBED_ATOMIC_OR_32(word_, value_) atomicOr((unsigned int *)&(word_)->value, (value_))
#define EMBED_ATOMIC_OR_64(word_, value_) atomicOr((unsigned long long *)&(word_)->value, (value_))
#define EMBED_ATOMIC_AND_32(word_, value_) atomicAnd((unsigned int *)&(word_)->value, (value_))
#define EMBED_ATOMIC_AND_64(word_, value_) atomicAnd((unsigned long long *)&(word_)->value, (value_))

#elif EMBED_ATOMIC_ARM == 2

/**
 * @brief The primitives of the C11 arm, on the explicit atomics of stdatomic.h.
 *
 * @note The load has acquire ordering, the store release ordering, and each read-modify-write both. The
 *       orders are in the calls, so EMBED_ATOMIC_FENCE is empty. The generic calls take either width, so
 *       each 64-bit primitive is the 32-bit one.
 */
#define EMBED_ATOMIC_LOAD(word_) atomic_load_explicit(&(word_)->value, memory_order_acquire)
#define EMBED_ATOMIC_FENCE() ((void)0)
#define EMBED_ATOMIC_ORDERED() atomic_thread_fence(memory_order_seq_cst)
#define EMBED_ATOMIC_STORE(word_, value_) atomic_store_explicit(&(word_)->value, (value_), memory_order_release)
#define EMBED_ATOMIC_EXCHANGE_32(word_, value_)                                                                        \
    atomic_exchange_explicit(&(word_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_EXCHANGE_64(word_, value_) EMBED_ATOMIC_EXCHANGE_32(word_, value_)
#define EMBED_ATOMIC_ADD_32(word_, value_) atomic_fetch_add_explicit(&(word_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_ADD_64(word_, value_) EMBED_ATOMIC_ADD_32(word_, value_)
#define EMBED_ATOMIC_OR_32(word_, value_) atomic_fetch_or_explicit(&(word_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_OR_64(word_, value_) EMBED_ATOMIC_OR_32(word_, value_)
#define EMBED_ATOMIC_AND_32(word_, value_) atomic_fetch_and_explicit(&(word_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_AND_64(word_, value_) EMBED_ATOMIC_AND_32(word_, value_)

#elif EMBED_ATOMIC_ARM == 3

/**
 * @brief The primitives of the builtin arm, on the __atomic builtins of GCC and clang.
 *
 * @note The orders are those of the C11 arm, and they are in the calls, so EMBED_ATOMIC_FENCE is empty.
 *       The builtins take either width, so each 64-bit primitive is the 32-bit one.
 */
#define EMBED_ATOMIC_LOAD(word_) __atomic_load_n(&(word_)->value, __ATOMIC_ACQUIRE)
#define EMBED_ATOMIC_FENCE() ((void)0)
#define EMBED_ATOMIC_ORDERED() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define EMBED_ATOMIC_STORE(word_, value_) __atomic_store_n(&(word_)->value, (value_), __ATOMIC_RELEASE)
#define EMBED_ATOMIC_EXCHANGE_32(word_, value_) __atomic_exchange_n(&(word_)->value, (value_), __ATOMIC_ACQ_REL)
#define EMBED_ATOMIC_EXCHANGE_64(word_, value_) EMBED_ATOMIC_EXCHANGE_32(word_, value_)
#define EMBED_ATOMIC_ADD_32(word_, value_) __atomic_fetch_add(&(word_)->value, (value_), __ATOMIC_ACQ_REL)
#define EMBED_ATOMIC_ADD_64(word_, value_) EMBED_ATOMIC_ADD_32(word_, value_)
#define EMBED_ATOMIC_OR_32(word_, value_) __atomic_fetch_or(&(word_)->value, (value_), __ATOMIC_ACQ_REL)
#define EMBED_ATOMIC_OR_64(word_, value_) EMBED_ATOMIC_OR_32(word_, value_)
#define EMBED_ATOMIC_AND_32(word_, value_) __atomic_fetch_and(&(word_)->value, (value_), __ATOMIC_ACQ_REL)
#define EMBED_ATOMIC_AND_64(word_, value_) EMBED_ATOMIC_AND_32(word_, value_)

#else

/**
 * @brief The primitives of the MSVC arm, on the interlocked intrinsics.
 *
 * @note MSVC's intrinsics take a long for 32 bits and an __int64 for 64. The casts read the same bits as
 *       the unsigned words that they are.
 * @note A plain volatile read and write of a 32-bit word are one step each on every target of MSVC. Those
 *       of a 64-bit word are one step on its 64-bit targets. The fences give them their ordering.
 */
#define EMBED_ATOMIC_LOAD(word_) ((word_)->value)
#define EMBED_ATOMIC_FENCE() _ReadWriteBarrier()
#define EMBED_ATOMIC_ORDERED()                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        volatile long full_barrier_ = 0;                                                                               \
        (void)_InterlockedExchange(&full_barrier_, 0);                                                                 \
    } while (0)
#define EMBED_ATOMIC_STORE(word_, value_) ((word_)->value = (value_))
#define EMBED_ATOMIC_EXCHANGE_32(word_, value_)                                                                        \
    ((embed_u32)_InterlockedExchange((volatile long *)&(word_)->value, (long)(value_)))
#define EMBED_ATOMIC_SWAP_IF_32(word_, expected_, value_)                                                              \
    ((embed_u32)_InterlockedCompareExchange((volatile long *)&(word_)->value, (long)(value_), (long)(expected_)))
#define EMBED_ATOMIC_ADD_32(word_, value_)                                                                             \
    ((embed_u32)_InterlockedExchangeAdd((volatile long *)&(word_)->value, (long)(value_)))
#define EMBED_ATOMIC_OR_32(word_, value_) ((embed_u32)_InterlockedOr((volatile long *)&(word_)->value, (long)(value_)))
#define EMBED_ATOMIC_AND_32(word_, value_)                                                                             \
    ((embed_u32)_InterlockedAnd((volatile long *)&(word_)->value, (long)(value_)))
#if EMBED_ATOMIC_HAS_64
#define EMBED_ATOMIC_EXCHANGE_64(word_, value_)                                                                        \
    ((embed_u64)_InterlockedExchange64((volatile __int64 *)&(word_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_SWAP_IF_64(word_, expected_, value_)                                                              \
    ((embed_u64)_InterlockedCompareExchange64((volatile __int64 *)&(word_)->value, (__int64)(value_),                  \
                                              (__int64)(expected_)))
#define EMBED_ATOMIC_ADD_64(word_, value_)                                                                             \
    ((embed_u64)_InterlockedExchangeAdd64((volatile __int64 *)&(word_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_OR_64(word_, value_)                                                                              \
    ((embed_u64)_InterlockedOr64((volatile __int64 *)&(word_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_AND_64(word_, value_)                                                                             \
    ((embed_u64)_InterlockedAnd64((volatile __int64 *)&(word_)->value, (__int64)(value_)))
#endif

#endif

/**
 * @brief Defines the operations over one atomic width. name_ is the suffix and type_ the word. The
 *        other five parameters are the primitives of the arm for that width.
 *
 * @note One text for both widths and each arm. A width or an arm cannot drift from the others.
 * @note The compare-and-swap uses the primitive of the arm where the arm has one, and the exchange
 *       where it does not. The C11 arm and the builtin arm use the generic call below.
 */
#define EMBED_ATOMIC_OPERATIONS(name_, type_, exchange_, swap_if_, add_, or_, and_)                                    \
    /** @brief The word, with acquire ordering: no later read moves ahead of it. */                                    \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_load(const embed_atomic_##name_ *word)                 \
    {                                                                                                                  \
        const type_ found = EMBED_ATOMIC_LOAD(word);                                                                   \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief The word set to value, with release ordering: every earlier write is out before it. */                  \
    EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_##name_##_store(embed_atomic_##name_ *word, type_ value)          \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        EMBED_ATOMIC_STORE(word, value);                                                                               \
    }                                                                                                                  \
    /** @brief The word set to value, and the word before. */                                                          \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_exchange(embed_atomic_##name_ *word, type_ value)      \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const type_ found = exchange_(word, value);                                                                    \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief The word set to value where it holds *expected, else *expected set to what it holds. */                 \
    EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_##name_##_swap_if(embed_atomic_##name_ *word,               \
                                                                             type_ *expected, type_ value)             \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const embed_bool swapped = swap_if_(word, expected, value);                                                    \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return swapped;                                                                                                \
    }                                                                                                                  \
    /** @brief value added to the word, with wrap around, and the word before the add.                                 \
     *  @warning A sum past the max of type_ wraps modulo the width of the word, and nothing reports it. */            \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_add(embed_atomic_##name_ *word, type_ value)     \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const type_ found = add_(word, value);                                                                         \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief value taken from the word, with wrap around, and the word before the subtract.                          \
     *  @warning A value above the word wraps modulo the width of the word, and nothing reports it.                    \
     *  @note The add of the two's complement is the subtract, because the add of each arm wraps. The cast holds       \
     *        the complement at the width of type_. */                                                                 \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_sub(embed_atomic_##name_ *word, type_ value)     \
    {                                                                                                                  \
        return embed_atomic_##name_##_fetch_add(word, (type_)(0u - value));                                            \
    }                                                                                                                  \
    /** @brief The word ored with value, and the word before the or. */                                                \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_or(embed_atomic_##name_ *word, type_ value)      \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const type_ found = or_(word, value);                                                                          \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief The word anded with value, and the word before the and. */                                              \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_and(embed_atomic_##name_ *word, type_ value)     \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const type_ found = and_(word, value);                                                                         \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief The word raised to value where value is above it, by a swap, and the word before.                       \
     *  @note A swap that fails puts the word that another context wrote in found, and the test runs again on it. */   \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_raise(embed_atomic_##name_ *word, type_ value)         \
    {                                                                                                                  \
        type_ found = embed_atomic_##name_##_load(word);                                                               \
        embed_bool raised = EMBED_FALSE;                                                                               \
        while ((found < value) && !raised)                                                                             \
        {                                                                                                              \
            raised = embed_atomic_##name_##_swap_if(word, &found, value);                                              \
        }                                                                                                              \
        return found;                                                                                                  \
    }

#if (EMBED_ATOMIC_ARM == 1) || (EMBED_ATOMIC_ARM == 4)

/**
 * @brief The compare-and-swap on an arm whose primitive gives the word that it found.
 *
 * The swap occurred where that word is the expected word. Otherwise, *expected takes the word that the
 * primitive found.
 */
#define EMBED_ATOMIC_CAS_32(word_, expected_, value_) embed_atomic_cas_u32(word_, expected_, value_)

/**
 * @brief The 32-bit compare-and-swap of an arm whose primitive answers the word that it found.
 *
 * @param[in,out] word     The word [BORROWS].
 * @param[in,out] expected The word that the caller expects, and after the call the word that the primitive found
 *                         [BORROWS].
 * @param[in]     value    The word that the swap writes.
 * @return                 1 where the swap occurred, 0 where it did not.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_cas_u32(embed_atomic_u32 *word, embed_u32 *expected,
                                                               embed_u32 value)
{
    const embed_u32 found = EMBED_ATOMIC_SWAP_IF_32(word, *expected, value);
    // Explicit cast converts the comparison, an int of 0 or 1, to embed_bool
    const embed_bool swapped = (embed_bool)(found == *expected);
    *expected = found;
    return swapped;
}

#if EMBED_ATOMIC_HAS_64
/** @brief The compare-and-swap of the 64-bit word on this arm, as for 32 bits. */
#define EMBED_ATOMIC_CAS_64(word_, expected_, value_) embed_atomic_cas_u64(word_, expected_, value_)

/**
 * @brief The 64-bit compare-and-swap of an arm whose primitive answers the word that it found.
 *
 * @param[in,out] word     The word [BORROWS].
 * @param[in,out] expected The word that the caller expects, and after the call the word that the primitive found
 *                         [BORROWS].
 * @param[in]     value    The word that the swap writes.
 * @return                 1 where the swap occurred, 0 where it did not.
 */
EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_cas_u64(embed_atomic_u64 *word, embed_u64 *expected,
                                                               embed_u64 value)
{
    const embed_u64 found = EMBED_ATOMIC_SWAP_IF_64(word, *expected, value);
    // Explicit cast converts the comparison, an int of 0 or 1, to embed_bool
    const embed_bool swapped = (embed_bool)(found == *expected);
    *expected = found;
    return swapped;
}
#endif

#elif EMBED_ATOMIC_ARM == 2

/**
 * @brief The compare-and-swap of the C11 arm, the strong form, with acquire and release ordering where it swaps
 *        and acquire ordering where it does not. Where it does not swap, *expected_ takes the word that it found.
 *
 * @note The generic call takes either width, so the 64-bit form is the 32-bit one. The cast converts the _Bool of
 *       the call to embed_bool.
 */
#define EMBED_ATOMIC_CAS_32(word_, expected_, value_)                                                                  \
    ((embed_bool)atomic_compare_exchange_strong_explicit(&(word_)->value, (expected_), (value_), memory_order_acq_rel, \
                                                         memory_order_acquire))
#define EMBED_ATOMIC_CAS_64(word_, expected_, value_) EMBED_ATOMIC_CAS_32(word_, expected_, value_)

#else

/**
 * @brief The compare-and-swap of the builtin arm, the strong form, with the orders of the C11 arm. Where it does
 *        not swap, *expected_ takes the word that it found.
 *
 * @note The builtin takes either width, so the 64-bit form is the 32-bit one. The cast converts the bool of the
 *       builtin to embed_bool.
 */
#define EMBED_ATOMIC_CAS_32(word_, expected_, value_)                                                                  \
    ((embed_bool)__atomic_compare_exchange_n(&(word_)->value, (expected_), (value_), 0, __ATOMIC_ACQ_REL,              \
                                             __ATOMIC_ACQUIRE))
#define EMBED_ATOMIC_CAS_64(word_, expected_, value_) EMBED_ATOMIC_CAS_32(word_, expected_, value_)

#endif

// The operations of the 32-bit word, embed_atomic_u32_load to embed_atomic_u32_raise, on every arm
EMBED_ATOMIC_OPERATIONS(u32, embed_u32, EMBED_ATOMIC_EXCHANGE_32, EMBED_ATOMIC_CAS_32, EMBED_ATOMIC_ADD_32,
                        EMBED_ATOMIC_OR_32, EMBED_ATOMIC_AND_32)
#if EMBED_ATOMIC_HAS_64
// The operations of the 64-bit word, embed_atomic_u64_load to embed_atomic_u64_raise, where the arm has the word
EMBED_ATOMIC_OPERATIONS(u64, embed_u64, EMBED_ATOMIC_EXCHANGE_64, EMBED_ATOMIC_CAS_64, EMBED_ATOMIC_ADD_64,
                        EMBED_ATOMIC_OR_64, EMBED_ATOMIC_AND_64)
#endif

/**
 * @brief Orders this context's earlier writes ahead of its later reads, on every arm.
 *
 * @note The operations above order a read before later reads, and a write after earlier writes. None of
 *       them orders a write before a later read. Two contexts that each write their own word and read the
 *       word of the other need this call between the two. Without it, each can read the word of the other
 *       from before its write.
 */
EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_ordered(void)
{
    EMBED_ATOMIC_ORDERED();
}

/**
 * @brief The atomic word of the machine word's width: the 64-bit word on a 64-bit word, the 32-bit
 *        word below it.
 *
 * @note A count of bytes or an offset within the reach of the word fits it. The operations below use
 *       its name, with no width in the name.
 */
#if EMBED_WORD_BITS == 64
#if !EMBED_ATOMIC_HAS_64
#error "embed_atomic.h: EMBED_WORD_BITS is 64, and the atomics of this target have no 64-bit word"
#endif
typedef embed_atomic_u64 embed_atomic_word;

/** @brief The plain integer embed_atomic_word holds and its operations take and answer. */
typedef embed_u64 embed_atomic_word_value;
/** @brief The operations of embed_atomic_word, each the operation of embed_atomic_u64 with the same suffix. */
#define embed_atomic_word_load embed_atomic_u64_load
#define embed_atomic_word_store embed_atomic_u64_store
#define embed_atomic_word_exchange embed_atomic_u64_exchange
#define embed_atomic_word_swap_if embed_atomic_u64_swap_if
#define embed_atomic_word_fetch_add embed_atomic_u64_fetch_add
#define embed_atomic_word_fetch_sub embed_atomic_u64_fetch_sub
#define embed_atomic_word_fetch_or embed_atomic_u64_fetch_or
#define embed_atomic_word_fetch_and embed_atomic_u64_fetch_and
#define embed_atomic_word_raise embed_atomic_u64_raise
#else
typedef embed_atomic_u32 embed_atomic_word;

/** @brief The plain integer embed_atomic_word holds and its operations take and answer. */
typedef embed_u32 embed_atomic_word_value;
/** @brief The operations of embed_atomic_word, each the operation of embed_atomic_u32 with the same suffix. */
#define embed_atomic_word_load embed_atomic_u32_load
#define embed_atomic_word_store embed_atomic_u32_store
#define embed_atomic_word_exchange embed_atomic_u32_exchange
#define embed_atomic_word_swap_if embed_atomic_u32_swap_if
#define embed_atomic_word_fetch_add embed_atomic_u32_fetch_add
#define embed_atomic_word_fetch_sub embed_atomic_u32_fetch_sub
#define embed_atomic_word_fetch_or embed_atomic_u32_fetch_or
#define embed_atomic_word_fetch_and embed_atomic_u32_fetch_and
#define embed_atomic_word_raise embed_atomic_u32_raise
#endif

/**
 * @brief The atomic word as wide as size_t, for a count of bytes or an offset as size_t holds it. It
 *        is the 64-bit word where SIZE_MAX is 64 bits wide, and the 32-bit word below that.
 *
 * @note The key is size_t, and not EMBED_WORD_BITS. A build can define the word narrower than the
 *       sizes that it counts, and a count must still fit whole.
 */
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFu
#if !EMBED_ATOMIC_HAS_64
#error "embed_atomic.h: size_t is 64 bits, and the atomics of this target have no 64-bit word"
#endif
typedef embed_atomic_u64 embed_atomic_usize;

/** @brief The plain integer embed_atomic_usize holds and its operations take and answer. */
typedef embed_u64 embed_atomic_usize_value;
/** @brief The operations of embed_atomic_usize, each the operation of embed_atomic_u64 with the same suffix. */
#define embed_atomic_usize_load embed_atomic_u64_load
#define embed_atomic_usize_store embed_atomic_u64_store
#define embed_atomic_usize_exchange embed_atomic_u64_exchange
#define embed_atomic_usize_swap_if embed_atomic_u64_swap_if
#define embed_atomic_usize_fetch_add embed_atomic_u64_fetch_add
#define embed_atomic_usize_fetch_sub embed_atomic_u64_fetch_sub
#define embed_atomic_usize_fetch_or embed_atomic_u64_fetch_or
#define embed_atomic_usize_fetch_and embed_atomic_u64_fetch_and
#define embed_atomic_usize_raise embed_atomic_u64_raise
#else
typedef embed_atomic_u32 embed_atomic_usize;

/** @brief The plain integer embed_atomic_usize holds and its operations take and answer. */
typedef embed_u32 embed_atomic_usize_value;
/** @brief The operations of embed_atomic_usize, each the operation of embed_atomic_u32 with the same suffix. */
#define embed_atomic_usize_load embed_atomic_u32_load
#define embed_atomic_usize_store embed_atomic_u32_store
#define embed_atomic_usize_exchange embed_atomic_u32_exchange
#define embed_atomic_usize_swap_if embed_atomic_u32_swap_if
#define embed_atomic_usize_fetch_add embed_atomic_u32_fetch_add
#define embed_atomic_usize_fetch_sub embed_atomic_u32_fetch_sub
#define embed_atomic_usize_fetch_or embed_atomic_u32_fetch_or
#define embed_atomic_usize_fetch_and embed_atomic_u32_fetch_and
#define embed_atomic_usize_raise embed_atomic_u32_raise
#endif

/**
 * @brief The atomic word as wide as a pointer, for an address as uintptr_t holds it. It is the 64-bit
 *        word where UINTPTR_MAX is 64 bits wide, and the 32-bit word below that.
 *
 * @note The key is the pointer, and not EMBED_WORD_BITS. A build can define the word narrower than the
 *       pointer, and an address must still fit whole.
 */
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#if !EMBED_ATOMIC_HAS_64
#error "embed_atomic.h: uintptr_t is 64 bits, and the atomics of this target have no 64-bit word"
#endif
typedef embed_atomic_u64 embed_atomic_uptr;

/** @brief The plain integer embed_atomic_uptr holds and its operations take and answer. */
typedef embed_u64 embed_atomic_uptr_value;
/** @brief The operations of embed_atomic_uptr, each the operation of embed_atomic_u64 with the same suffix. */
#define embed_atomic_uptr_load embed_atomic_u64_load
#define embed_atomic_uptr_store embed_atomic_u64_store
#define embed_atomic_uptr_exchange embed_atomic_u64_exchange
#define embed_atomic_uptr_swap_if embed_atomic_u64_swap_if
#else
typedef embed_atomic_u32 embed_atomic_uptr;

/** @brief The plain integer embed_atomic_uptr holds and its operations take and answer. */
typedef embed_u32 embed_atomic_uptr_value;
/** @brief The operations of embed_atomic_uptr, each the operation of embed_atomic_u32 with the same suffix. */
#define embed_atomic_uptr_load embed_atomic_u32_load
#define embed_atomic_uptr_store embed_atomic_u32_store
#define embed_atomic_uptr_exchange embed_atomic_u32_exchange
#define embed_atomic_uptr_swap_if embed_atomic_u32_swap_if
#endif

EMBED_END_DECLS

#endif
