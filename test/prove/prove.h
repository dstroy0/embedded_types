/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file prove.h
 * @brief The words of a CBMC harness: an input of any value, an assumption that bounds it, and an assertion that must
 *        hold on each input that the assumptions leave.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-21
 *
 * @note harness.py prove runs each harness under CBMC. A function with no body whose name starts with nondet_ gives
 *       any value. __CPROVER_assume bounds the inputs, and __CPROVER_assert checks the C.
 * @note The reach run defines PROVE_REACH, and that run must fail at PROVE_DONE. A harness whose assumptions leave no
 *       path passes each assertion and proves nothing.
 */
#ifndef EMBED_TEST_PROVE_H
#define EMBED_TEST_PROVE_H

#include "embed_types.h"

#include <stdint.h>

/** @brief An embed_word of any value. */
embed_word nondet_word(void);

/** @brief A word of 32 bits of any value. */
uint32_t nondet_u32(void);

/** @brief A word of 64 bits of any value. */
uint64_t nondet_u64(void);

/** @brief An integer of the width of a pointer, of any value. */
uintptr_t nondet_uptr(void);

/** @brief An unsigned int of any value. */
unsigned int nondet_uint(void);

/** @brief A boolean of any value. */
_Bool nondet_bool(void);

#define PROVE_ASSUME(cond_) __CPROVER_assume(cond_)
#define PROVE_ASSERT(cond_, msg_) __CPROVER_assert((cond_), (msg_))
#if defined(PROVE_REACH)
#define PROVE_DONE() __CPROVER_assert(0, "prove: the end of the harness is reached")
#else
#define PROVE_DONE() ((void)0)
#endif

#endif
