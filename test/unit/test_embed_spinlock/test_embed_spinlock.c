/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_spinlock.c
 * @brief Exercises the spinlock: free as declared and in zeroed storage, owned between an acquire and
 *        a release, one 32-bit word, and exclusive across host threads contending for it.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-23
 *
 * @note The contention case runs SPINLOCK_THREADS host threads, each adding one to a plain counter
 *       SPINLOCK_ROUNDS times under the lock. A counter short of the product is an increment two
 *       threads made at once, which the lock exists to prevent.
 * @note The threads start in test/support/host_threads.c, where the platform's headers are. Unity's
 *       generator copies every #include of this file into the runner and reads no #if.
 */
#include <string.h>

#include "embed_spinlock.h"
#include "host_threads.h"

#include "unity.h"

/** @brief The host threads the contention case runs. */
#define SPINLOCK_THREADS 4u

/** @brief The increments each of those threads makes under the lock. */
#define SPINLOCK_ROUNDS 200000u

/** @brief The lock the contention case's threads contend for. */
static embed_spinlock s_shared = EMBED_SPINLOCK_INIT;

/** @brief The plain counter the lock guards, read and written only by the lock's owner. */
static embed_u32 s_count;

/**
 * @brief Runs before each case, and frees the shared lock and zeroes its counter.
 */
void setUp(void)
{
    memset(&s_shared, 0, sizeof(s_shared));
    s_count = 0u;
}

/** @brief Runs after each case, and has nothing to release. */
void tearDown(void)
{
}

/**
 * @brief Adds one to the counter SPINLOCK_ROUNDS times, each under the lock.
 *
 * @note The read and the write are two statements on a plain counter, so two threads in them at once
 *       lose an increment. Only the lock keeps them apart.
 */
static void spinlock_rounds(void)
{
    for (embed_u32 step = 0u; step < SPINLOCK_ROUNDS; step += 1u)
    {
        EMBED_SPINLOCK_ACQUIRE(&s_shared);
        const embed_u32 before = s_count;
        s_count = before + 1u;
        EMBED_SPINLOCK_RELEASE(&s_shared);
    }
}

/**
 * @brief Checks a lock made free by EMBED_SPINLOCK_INIT is taken and freed by one context.
 */
void test_a_lock_declared_free_is_taken_and_freed(void)
{
    embed_spinlock lock = EMBED_SPINLOCK_INIT;
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, lock.locked, "a lock is free as it is declared");
    EMBED_SPINLOCK_ACQUIRE(&lock);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, lock.locked, "an acquired lock is owned");
    EMBED_SPINLOCK_RELEASE(&lock);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, lock.locked, "a released lock is free again");
}

/**
 * @brief Checks a lock in storage zeroed at run time is free, as calloc's storage is.
 */
void test_a_lock_in_zeroed_storage_is_free(void)
{
    embed_spinlock lock;
    memset(&lock, 0, sizeof(lock));
    EMBED_SPINLOCK_ACQUIRE(&lock);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, lock.locked, "a lock in zeroed storage is taken at once");
    EMBED_SPINLOCK_RELEASE(&lock);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, lock.locked, "and freed");
}

/**
 * @brief Checks the lock is one 32-bit word, the layout a host and a device share.
 */
void test_the_lock_is_one_32_bit_word(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(4u, sizeof(embed_spinlock), "the lock is four bytes");
}

/**
 * @brief Checks contending threads lose no increment of a plain counter the lock guards.
 */
void test_contending_threads_lose_no_increment(void)
{
    TEST_ASSERT_TRUE_MESSAGE(host_threads_run(spinlock_rounds, SPINLOCK_THREADS), "every thread started");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(SPINLOCK_THREADS * SPINLOCK_ROUNDS, s_count,
                                     "every increment made under the lock is counted");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s_shared.locked, "the lock is free once every thread ended");
}
