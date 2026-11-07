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
 * @note These steps alone keep a structure that contexts share consistent, and no context waits for
 *       another. A counter is a fetch-add. A claim is a compare-and-swap that takes, or that reports
 *       who took. A boundary is a compare-and-swap loop that moves, or that tries again over the value
 *       that it found. Nothing here spins on the progress of another context.
 * @note The switch below selects one of four arms, one time. The first arm is the device atomics of
 *       CUDA in a device pass. The second is C11 atomics, where a C compiler has them. The third is the
 *       GCC and clang __atomic builtins, where C11 atomics are absent or the consumer is C++. The fourth
 *       is the interlocked intrinsics under MSVC. Each arm gives a load acquire ordering, a store
 *       release ordering, and each read-modify-write both.
 * @note There are two widths. Each arm has the 32-bit word, the CUDA atomics among them. The counters
 *       and packed boundaries of a host need the 64-bit word. CUDA, C11, the builtins and MSVC on a
 *       64-bit host all have it. embed_atomic_word matches the machine word.
 * @note Every entry carries EMBED_HOST_DEVICE. Under nvcc one definition compiles into the host pass
 *       and the device pass, each on its own arm, over the same word in memory.
 * @warning A target with none of the four fails the build at the #error below. There is no plain
 *          fallback: a word that is not atomic is a defect that shows only under contention.
 */
#ifndef EMBED_ATOMIC_H
#define EMBED_ATOMIC_H

#include "embed_types.h"

/**
 * @brief 1 where the atomics run on CUDA's device atomics, 2 on C11 atomics, 3 on the GCC and clang
 *        builtins, 4 on MSVC's interlocked intrinsics.
 *
 * @note Always defined. Under nvcc the host pass and the device pass each read this header, and each
 *       selects its own arm over the same word.
 */
#if defined(__CUDA_ARCH__)
#define EMBED_ATOMIC_ARM 1
#elif !defined(__cplusplus) && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define EMBED_ATOMIC_ARM 2
#elif EMBED_GNU_ATTRIBUTES
#define EMBED_ATOMIC_ARM 3
#elif defined(_MSC_VER)
#include <intrin.h>
#define EMBED_ATOMIC_ARM 4
#else
#error "embed_atomic.h: this target has no atomics the header knows"
#endif

EMBED_BEGIN_DECLS

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

/**
 * @brief A 64-bit word every access to which is one indivisible step.
 */
typedef struct
{
#if EMBED_ATOMIC_ARM == 2
    _Atomic embed_u64 value; /**< The word. */
#else
    volatile embed_u64 value; /**< The word. */
#endif
} embed_atomic_u64;

EMBED_STATIC_ASSERT(sizeof(embed_atomic_u32) == 4u, "embed_atomic_u32 must be one 32-bit word on every arm");
EMBED_STATIC_ASSERT(sizeof(embed_atomic_u64) == 8u, "embed_atomic_u64 must be one 64-bit word on every arm");

/**
 * @brief An atomic word that holds value_, for the declaration that makes one.
 *
 * @param[in] value_ The word's first value.
 */
#define EMBED_ATOMIC_INIT(value_) {(value_)}

#if EMBED_ATOMIC_ARM == 1

/**
 * @brief The device arm's load: a volatile read, then a fence, which no later read moves ahead of.
 */
#define EMBED_ATOMIC_LOAD(atom_) ((atom_)->value)
#define EMBED_ATOMIC_FENCE() __threadfence()
#define EMBED_ATOMIC_ORDERED() __threadfence()
#define EMBED_ATOMIC_STORE(atom_, value_) ((atom_)->value = (value_))
#define EMBED_ATOMIC_EXCHANGE_32(atom_, value_) atomicExch((unsigned int *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_EXCHANGE_64(atom_, value_) atomicExch((unsigned long long *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_SWAP_IF_32(atom_, expected_, value_) atomicCAS((unsigned int *)&(atom_)->value, (expected_), (value_))
#define EMBED_ATOMIC_SWAP_IF_64(atom_, expected_, value_)                                                              \
    atomicCAS((unsigned long long *)&(atom_)->value, (expected_), (value_))
#define EMBED_ATOMIC_ADD_32(atom_, value_) atomicAdd((unsigned int *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_ADD_64(atom_, value_) atomicAdd((unsigned long long *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_OR_32(atom_, value_) atomicOr((unsigned int *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_OR_64(atom_, value_) atomicOr((unsigned long long *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_AND_32(atom_, value_) atomicAnd((unsigned int *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_AND_64(atom_, value_) atomicAnd((unsigned long long *)&(atom_)->value, (value_))

#elif EMBED_ATOMIC_ARM == 2

#define EMBED_ATOMIC_LOAD(atom_) atomic_load_explicit(&(atom_)->value, memory_order_acquire)
#define EMBED_ATOMIC_FENCE() ((void)0)
#define EMBED_ATOMIC_ORDERED() atomic_thread_fence(memory_order_seq_cst)
#define EMBED_ATOMIC_STORE(atom_, value_) atomic_store_explicit(&(atom_)->value, (value_), memory_order_release)
#define EMBED_ATOMIC_EXCHANGE_32(atom_, value_) atomic_exchange_explicit(&(atom_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_EXCHANGE_64(atom_, value_) EMBED_ATOMIC_EXCHANGE_32(atom_, value_)
#define EMBED_ATOMIC_ADD_32(atom_, value_) atomic_fetch_add_explicit(&(atom_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_ADD_64(atom_, value_) EMBED_ATOMIC_ADD_32(atom_, value_)
#define EMBED_ATOMIC_OR_32(atom_, value_) atomic_fetch_or_explicit(&(atom_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_OR_64(atom_, value_) EMBED_ATOMIC_OR_32(atom_, value_)
#define EMBED_ATOMIC_AND_32(atom_, value_) atomic_fetch_and_explicit(&(atom_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_AND_64(atom_, value_) EMBED_ATOMIC_AND_32(atom_, value_)

#elif EMBED_ATOMIC_ARM == 3

#define EMBED_ATOMIC_LOAD(atom_) __atomic_load_n(&(atom_)->value, __ATOMIC_ACQUIRE)
#define EMBED_ATOMIC_FENCE() ((void)0)
#define EMBED_ATOMIC_ORDERED() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define EMBED_ATOMIC_STORE(atom_, value_) __atomic_store_n(&(atom_)->value, (value_), __ATOMIC_RELEASE)
#define EMBED_ATOMIC_EXCHANGE_32(atom_, value_) __atomic_exchange_n(&(atom_)->value, (value_), __ATOMIC_ACQ_REL)
#define EMBED_ATOMIC_EXCHANGE_64(atom_, value_) EMBED_ATOMIC_EXCHANGE_32(atom_, value_)
#define EMBED_ATOMIC_ADD_32(atom_, value_) __atomic_fetch_add(&(atom_)->value, (value_), __ATOMIC_ACQ_REL)
#define EMBED_ATOMIC_ADD_64(atom_, value_) EMBED_ATOMIC_ADD_32(atom_, value_)
#define EMBED_ATOMIC_OR_32(atom_, value_) __atomic_fetch_or(&(atom_)->value, (value_), __ATOMIC_ACQ_REL)
#define EMBED_ATOMIC_OR_64(atom_, value_) EMBED_ATOMIC_OR_32(atom_, value_)
#define EMBED_ATOMIC_AND_32(atom_, value_) __atomic_fetch_and(&(atom_)->value, (value_), __ATOMIC_ACQ_REL)
#define EMBED_ATOMIC_AND_64(atom_, value_) EMBED_ATOMIC_AND_32(atom_, value_)

#else

/**
 * @brief MSVC's intrinsics take a long for 32 bits and an __int64 for 64; the casts read the same
 *        bits as the unsigned words they are. A plain volatile read and write are one step each on
 *        the targets MSVC compiles for, and the fences give them their ordering.
 */
#define EMBED_ATOMIC_LOAD(atom_) ((atom_)->value)
#define EMBED_ATOMIC_FENCE() _ReadWriteBarrier()
#define EMBED_ATOMIC_ORDERED()                                                                                         \
    do                                                                                                                 \
    {                                                                                                                  \
        volatile long full_barrier_ = 0;                                                                               \
        (void)_InterlockedExchange(&full_barrier_, 0);                                                                 \
    } while (0)
#define EMBED_ATOMIC_STORE(atom_, value_) ((atom_)->value = (value_))
#define EMBED_ATOMIC_EXCHANGE_32(atom_, value_)                                                                        \
    ((embed_u32)_InterlockedExchange((volatile long *)&(atom_)->value, (long)(value_)))
#define EMBED_ATOMIC_EXCHANGE_64(atom_, value_)                                                                        \
    ((embed_u64)_InterlockedExchange64((volatile __int64 *)&(atom_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_SWAP_IF_32(atom_, expected_, value_)                                                              \
    ((embed_u32)_InterlockedCompareExchange((volatile long *)&(atom_)->value, (long)(value_), (long)(expected_)))
#define EMBED_ATOMIC_SWAP_IF_64(atom_, expected_, value_)                                                              \
    ((embed_u64)_InterlockedCompareExchange64((volatile __int64 *)&(atom_)->value, (__int64)(value_),                  \
                                              (__int64)(expected_)))
#define EMBED_ATOMIC_ADD_32(atom_, value_)                                                                             \
    ((embed_u32)_InterlockedExchangeAdd((volatile long *)&(atom_)->value, (long)(value_)))
#define EMBED_ATOMIC_ADD_64(atom_, value_)                                                                             \
    ((embed_u64)_InterlockedExchangeAdd64((volatile __int64 *)&(atom_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_OR_32(atom_, value_) ((embed_u32)_InterlockedOr((volatile long *)&(atom_)->value, (long)(value_)))
#define EMBED_ATOMIC_OR_64(atom_, value_)                                                                              \
    ((embed_u64)_InterlockedOr64((volatile __int64 *)&(atom_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_AND_32(atom_, value_)                                                                             \
    ((embed_u32)_InterlockedAnd((volatile long *)&(atom_)->value, (long)(value_)))
#define EMBED_ATOMIC_AND_64(atom_, value_)                                                                             \
    ((embed_u64)_InterlockedAnd64((volatile __int64 *)&(atom_)->value, (__int64)(value_)))

#endif

/**
 * @brief Defines the operations over one atomic width: name_ is the suffix, type_ the word,
 *        exchange_, swap_if_, add_, or_ and and_ the arm's primitives for that width.
 *
 * @note One text for both widths and each arm. A width or an arm cannot drift from the others.
 * @note The compare-and-swap uses the primitive of the arm where the arm has one, and the exchange
 *       where it does not. The C11 arm and the builtin arm use the generic call below.
 */
#define EMBED_ATOMIC_OPERATIONS(name_, type_, exchange_, swap_if_, add_, or_, and_)                                    \
    /** @brief The word, with acquire ordering: no later read moves ahead of it. */                                    \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_load(const embed_atomic_##name_ *atom)                 \
    {                                                                                                                  \
        const type_ found = EMBED_ATOMIC_LOAD(atom);                                                                   \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief The word set to value, with release ordering: every earlier write is out before it. */                  \
    EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_##name_##_store(embed_atomic_##name_ *atom, type_ value)          \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        EMBED_ATOMIC_STORE(atom, value);                                                                               \
    }                                                                                                                  \
    /** @brief The word set to value, and the word before. */                                                         \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_exchange(embed_atomic_##name_ *atom, type_ value)      \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const type_ found = exchange_(atom, value);                                                                    \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief The word set to value where it holds *expected, else *expected set to what it holds. */                 \
    EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_##name_##_swap_if(embed_atomic_##name_ *atom,               \
                                                                           type_ *expected, type_ value)              \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const embed_bool swapped = swap_if_(atom, expected, value);                                                    \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return swapped;                                                                                                \
    }                                                                                                                  \
    /** @brief value added to the word, with wrap around, and the word before the add. */                              \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_add(embed_atomic_##name_ *atom, type_ value)     \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const type_ found = add_(atom, value);                                                                         \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief value taken from the word, with wrap around, and the word before the subtract. */                       \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_sub(embed_atomic_##name_ *atom, type_ value)     \
    {                                                                                                                  \
        /* The add of the two's complement is the subtract, because the add of each arm wraps. */                      \
        return embed_atomic_##name_##_fetch_add(atom, (type_)(0u - value));                                            \
    }                                                                                                                  \
    /** @brief The word ored with value, and the word before the or. */                                                \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_or(embed_atomic_##name_ *atom, type_ value)      \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const type_ found = or_(atom, value);                                                                          \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief The word anded with value, and the word before the and. */                                              \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_and(embed_atomic_##name_ *atom, type_ value)     \
    {                                                                                                                  \
        EMBED_ATOMIC_FENCE();                                                                                          \
        const type_ found = and_(atom, value);                                                                         \
        EMBED_ATOMIC_FENCE();                                                                                          \
        return found;                                                                                                  \
    }                                                                                                                  \
    /** @brief The word raised to value where value is above it, by a swap, and the word before. */                    \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_raise(embed_atomic_##name_ *atom, type_ value)         \
    {                                                                                                                  \
        type_ found = embed_atomic_##name_##_load(atom);                                                               \
        while ((found < value) && !embed_atomic_##name_##_swap_if(atom, &found, value))                                \
        {                                                                                                              \
            /* found now holds the word that another context wrote, and the test runs again on it. */                  \
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
#define EMBED_ATOMIC_CAS_32(atom_, expected_, value_) embed_atomic_cas_u32(atom_, expected_, value_)
#define EMBED_ATOMIC_CAS_64(atom_, expected_, value_) embed_atomic_cas_u64(atom_, expected_, value_)

EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_cas_u32(embed_atomic_u32 *atom, embed_u32 *expected, embed_u32 value)
{
    const embed_u32 found = EMBED_ATOMIC_SWAP_IF_32(atom, *expected, value);
    const embed_bool swapped = (embed_bool)(found == *expected);
    *expected = found;
    return swapped;
}

EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_cas_u64(embed_atomic_u64 *atom, embed_u64 *expected, embed_u64 value)
{
    const embed_u64 found = EMBED_ATOMIC_SWAP_IF_64(atom, *expected, value);
    const embed_bool swapped = (embed_bool)(found == *expected);
    *expected = found;
    return swapped;
}

#elif EMBED_ATOMIC_ARM == 2

#define EMBED_ATOMIC_CAS_32(atom_, expected_, value_)                                                                  \
    ((embed_bool)atomic_compare_exchange_strong_explicit(&(atom_)->value, (expected_), (value_), memory_order_acq_rel, \
                                                         memory_order_acquire))
#define EMBED_ATOMIC_CAS_64(atom_, expected_, value_) EMBED_ATOMIC_CAS_32(atom_, expected_, value_)

#else

#define EMBED_ATOMIC_CAS_32(atom_, expected_, value_)                                                                  \
    ((embed_bool)__atomic_compare_exchange_n(&(atom_)->value, (expected_), (value_), 0, __ATOMIC_ACQ_REL,              \
                                             __ATOMIC_ACQUIRE))
#define EMBED_ATOMIC_CAS_64(atom_, expected_, value_) EMBED_ATOMIC_CAS_32(atom_, expected_, value_)

#endif

EMBED_ATOMIC_OPERATIONS(u32, embed_u32, EMBED_ATOMIC_EXCHANGE_32, EMBED_ATOMIC_CAS_32, EMBED_ATOMIC_ADD_32,
                        EMBED_ATOMIC_OR_32, EMBED_ATOMIC_AND_32)
EMBED_ATOMIC_OPERATIONS(u64, embed_u64, EMBED_ATOMIC_EXCHANGE_64, EMBED_ATOMIC_CAS_64, EMBED_ATOMIC_ADD_64,
                        EMBED_ATOMIC_OR_64, EMBED_ATOMIC_AND_64)

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
typedef embed_atomic_u64 embed_atomic_word;

/** @brief The plain integer embed_atomic_word holds and its operations take and answer. */
typedef embed_u64 embed_atomic_word_value;
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
 * @note Keyed on size_t, not on EMBED_WORD_BITS, since a build may define the word narrower than the
 *       sizes it counts and a count must still fit whole.
 */
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFu
typedef embed_atomic_u64 embed_atomic_usize;

/** @brief The plain integer embed_atomic_usize holds and its operations take and answer. */
typedef embed_u64 embed_atomic_usize_value;
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
 * @note Keyed on the pointer, not on EMBED_WORD_BITS, since a build may define the word narrower than
 *       the pointer and an address must still fit whole.
 */
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
typedef embed_atomic_u64 embed_atomic_uptr;

/** @brief The plain integer embed_atomic_uptr holds and its operations take and answer. */
typedef embed_u64 embed_atomic_uptr_value;
#define embed_atomic_uptr_load embed_atomic_u64_load
#define embed_atomic_uptr_store embed_atomic_u64_store
#define embed_atomic_uptr_exchange embed_atomic_u64_exchange
#define embed_atomic_uptr_swap_if embed_atomic_u64_swap_if
#else
typedef embed_atomic_u32 embed_atomic_uptr;

/** @brief The plain integer embed_atomic_uptr holds and its operations take and answer. */
typedef embed_u32 embed_atomic_uptr_value;
#define embed_atomic_uptr_load embed_atomic_u32_load
#define embed_atomic_uptr_store embed_atomic_u32_store
#define embed_atomic_uptr_exchange embed_atomic_u32_exchange
#define embed_atomic_uptr_swap_if embed_atomic_u32_swap_if
#endif

EMBED_END_DECLS

#endif
