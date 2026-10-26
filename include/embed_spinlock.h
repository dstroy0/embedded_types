/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_spinlock.h
 * @brief A spinlock one context owns at a time, on the target's atomics, with no system call: a host
 *        thread, a part's task, or a GPU thread.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-23
 *
 * @note A waiting context spins on the lock's word and never sleeps in the operating system. Nothing
 *       here reaches one, so the same code runs on a host, on a part with no scheduler, and on a GPU,
 *       where a lock in resident memory has no other form.
 * @note The platform is switched once, below, into one of four arms: CUDA's device atomics in a
 *       device pass, C11 atomics where a C compiler has them, the GCC and clang __atomic builtins
 *       where it does not or the consumer is C++, and the interlocked intrinsics under MSVC. Every arm
 *       gives the acquire an acquire ordering and the release a release ordering.
 * @note The word is 32 bits on every arm. CUDA's atomicExch has no narrower form, and a lock a host
 *       and a device both reach must be laid out alike in both passes.
 * @note A target supplies its own lock by defining EMBED_SPINLOCK_ACQUIRE and EMBED_SPINLOCK_RELEASE
 *       ahead of this header. The arms below are then not selected, and no #error fires for a
 *       compiler with no atomics.
 * @warning Not reentrant. A context that acquires a lock it already owns spins forever.
 * @warning On a GPU before sm_70 the threads of a warp share one program counter, and a warp whose
 *          lanes contend for one lock can spin forever. sm_70 and later schedule each thread on its
 *          own and do not.
 */
#ifndef EMBED_SPINLOCK_H
#define EMBED_SPINLOCK_H

#include "embed_types.h"

#if !defined(EMBED_SPINLOCK_ACQUIRE)
#if defined(__CUDA_ARCH__)
/**
 * @brief 1 where the lock runs on CUDA's device atomics, 2 on C11 atomics, 3 on the GCC and clang
 *        builtins, 4 on MSVC's interlocked intrinsics, 0 where the target supplied its own.
 *
 * @note Always defined, so #if reads a value on every arm. Under nvcc the host pass and the device
 *       pass each read this header, and each selects its own arm over the same word.
 */
#define EMBED_SPINLOCK_ARM 1
#elif !defined(__cplusplus) && !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#define EMBED_SPINLOCK_ARM 2
#elif EMBED_GNU_ATTRIBUTES
#define EMBED_SPINLOCK_ARM 3
#elif defined(_MSC_VER)
#include <intrin.h>
#define EMBED_SPINLOCK_ARM 4
#else
#error "embed_spinlock.h: no atomics here - define EMBED_SPINLOCK_ACQUIRE and EMBED_SPINLOCK_RELEASE for this target"
#endif
#else
#define EMBED_SPINLOCK_ARM 0
#endif

#if defined(EMBED_SPINLOCK_ACQUIRE) != defined(EMBED_SPINLOCK_RELEASE)
#error "embed_spinlock.h: EMBED_SPINLOCK_ACQUIRE and EMBED_SPINLOCK_RELEASE are defined together or not at all"
#endif

/**
 * @brief Expands to __host__ __device__ under nvcc, and to nothing elsewhere.
 *
 * @note Every entry below carries it, so one definition compiles into both of nvcc's passes.
 */
#if defined(__CUDACC__)
#define EMBED_SPINLOCK_BOTH __host__ __device__
#else
#define EMBED_SPINLOCK_BOTH
#endif

EMBED_BEGIN_DECLS

/**
 * @brief The lock: one 32-bit word, 0 while no context owns it and 1 while one does.
 *
 * @note EMBED_SPINLOCK_INIT makes one free as it is declared. A lock in zeroed storage is free too,
 *       on every arm.
 * @note The word is atomic on the C11 arm and plain on the others, where the device atomics, the
 *       builtins and the intrinsics make each access atomic themselves.
 */
typedef struct
{
#if EMBED_SPINLOCK_ARM == 2
    _Atomic embed_u32 locked; /**< 0 while free, 1 while a context owns the lock. */
#else
    volatile embed_u32 locked; /**< 0 while free, 1 while a context owns the lock. */
#endif
} embed_spinlock;

/** @brief Asserts the lock is one 32-bit word on every arm, the layout host and device share. */
EMBED_STATIC_ASSERT(sizeof(embed_spinlock) == 4u, "embed_spinlock must be one 32-bit word on every arm");

/** @brief A free lock, for the declaration that makes one. */
#define EMBED_SPINLOCK_INIT {0u}

/**
 * @brief The pause a waiting context takes between two reads of the lock's word.
 *
 * @note A GPU thread sleeps a few nanoseconds where sm_70 gives it __nanosleep, x86 gets pause and
 *       ARM gets yield. Each tells the core it is spinning, so the read loop does not flood the
 *       memory system and another thread runs. Any other target pauses not at all, and the loop is a
 *       plain read loop.
 * @warning This header defines nothing when EMBED_SPINLOCK_PAUSE is already defined. Define it ahead
 *          of this header to give a target its own pause.
 */
#ifndef EMBED_SPINLOCK_PAUSE
#if defined(__CUDA_ARCH__)
#if __CUDA_ARCH__ >= 700
#define EMBED_SPINLOCK_PAUSE() __nanosleep(32u)
#else
#define EMBED_SPINLOCK_PAUSE() ((void)0)
#endif
#elif defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#if EMBED_GNU_ATTRIBUTES
#define EMBED_SPINLOCK_PAUSE() __builtin_ia32_pause()
#else
#include <intrin.h>
#define EMBED_SPINLOCK_PAUSE() _mm_pause()
#endif
#elif defined(__aarch64__) || defined(__arm__)
#define EMBED_SPINLOCK_PAUSE() __asm__ __volatile__("yield")
#elif defined(_M_ARM64) || defined(_M_ARM)
#include <intrin.h>
#define EMBED_SPINLOCK_PAUSE() __yield()
#else
#define EMBED_SPINLOCK_PAUSE() ((void)0)
#endif
#endif

#if EMBED_SPINLOCK_ARM != 0
/**
 * @brief Swaps 1 into the lock's word with acquire ordering, and returns the word it found.
 *
 * @param[in,out] lock The lock to swap into [BORROWS].
 * @return             0 where the lock was free and is now this context's, 1 where another owns it.
 * @note The device arm's atomicExch orders nothing itself, so a fence follows a swap that took the
 *       lock: no read of what the lock guards moves ahead of it.
 */
EMBED_INLINE EMBED_SPINLOCK_BOTH embed_u32 embed_spinlock_swapped(embed_spinlock *lock)
{
#if EMBED_SPINLOCK_ARM == 1
    // atomicExch takes the word without its volatile, which the device atomic does not need
    const embed_u32 found = atomicExch((unsigned int *)&lock->locked, 1u);
    if (found == 0u)
    {
        __threadfence();
    }
    return found;
#elif EMBED_SPINLOCK_ARM == 2
    return atomic_exchange_explicit(&lock->locked, 1u, memory_order_acquire);
#elif EMBED_SPINLOCK_ARM == 3
    return __atomic_exchange_n(&lock->locked, 1u, __ATOMIC_ACQUIRE);
#else
    // the intrinsic takes a long, 32 bits under MSVC; the word it swaps and returns is 0 or 1
    return (embed_u32)_InterlockedExchange((volatile long *)&lock->locked, 1L);
#endif
}

/**
 * @brief Reads the lock's word with no ordering, for the wait between two swaps.
 *
 * @param[in] lock The lock to read [BORROWS].
 * @return         0 where the lock looks free, 1 where it looks owned.
 * @note The read orders nothing. The swap after it is what takes the lock, so a stale 0 here costs
 *       one failed swap and nothing more.
 */
EMBED_INLINE EMBED_SPINLOCK_BOTH embed_u32 embed_spinlock_read(embed_spinlock *lock)
{
#if EMBED_SPINLOCK_ARM == 2
    return atomic_load_explicit(&lock->locked, memory_order_relaxed);
#elif EMBED_SPINLOCK_ARM == 3
    return __atomic_load_n(&lock->locked, __ATOMIC_RELAXED);
#else
    return lock->locked;
#endif
}

/**
 * @brief Takes the lock, spinning until it is free.
 *
 * @param[in,out] lock The lock to take [BORROWS].
 * @note Swaps first, and only on a miss reads the word in a loop until it looks free, pausing between
 *       reads. The loop reads a line it shares with the owner and writes nothing, so the owner's
 *       release reaches it without every waiter writing the line in turn.
 * @warning Spins forever where the owner never releases, and where this context is the owner.
 */
EMBED_INLINE EMBED_SPINLOCK_BOTH void embed_spinlock_acquire(embed_spinlock *lock)
{
    while (embed_spinlock_swapped(lock) != 0u)
    {
        while (embed_spinlock_read(lock) != 0u)
        {
            EMBED_SPINLOCK_PAUSE();
        }
    }
}

/**
 * @brief Frees the lock with release ordering, so every write made while it was owned is visible to
 *        the next context that takes it.
 *
 * @param[in,out] lock The lock to free [BORROWS].
 * @note The device arm fences ahead of the swap, so every write made while the lock was owned is out
 *       before the word reads 0.
 * @warning Frees the lock whoever calls it. Only its owner may.
 */
EMBED_INLINE EMBED_SPINLOCK_BOTH void embed_spinlock_release(embed_spinlock *lock)
{
#if EMBED_SPINLOCK_ARM == 1
    __threadfence();
    (void)atomicExch((unsigned int *)&lock->locked, 0u);
#elif EMBED_SPINLOCK_ARM == 2
    atomic_store_explicit(&lock->locked, 0u, memory_order_release);
#elif EMBED_SPINLOCK_ARM == 3
    __atomic_store_n(&lock->locked, 0u, __ATOMIC_RELEASE);
#else
    (void)_InterlockedExchange((volatile long *)&lock->locked, 0L);
#endif
}

/**
 * @brief Takes the lock at lock_, spinning until it is free.
 *
 * @param[in,out] lock_ Address of the lock.
 * @note The entry a consumer calls. A target that defines this and EMBED_SPINLOCK_RELEASE ahead of
 *       this header puts its own lock in place of the one above.
 */
#define EMBED_SPINLOCK_ACQUIRE(lock_) embed_spinlock_acquire(lock_)

/**
 * @brief Frees the lock at lock_.
 *
 * @param[in,out] lock_ Address of the lock.
 */
#define EMBED_SPINLOCK_RELEASE(lock_) embed_spinlock_release(lock_)
#endif

EMBED_END_DECLS

#endif
