/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file coverage_inline.h
 * @brief EMBED_INLINE without always_inline, for the coverage build alone, which includes this file ahead of each
 *        translation unit.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-11-14
 *
 * @note GCC puts an always_inline body in line also at -O0. Each call site then gets its own copy of the branch
 *       records of the body. The report then counts those copies and not the branches of the source. A plain static
 *       inline function keeps one body in each translation unit.
 */
#ifndef EMBED_TEST_COVERAGE_INLINE_H
#define EMBED_TEST_COVERAGE_INLINE_H

#define EMBED_INLINE static inline

#endif
