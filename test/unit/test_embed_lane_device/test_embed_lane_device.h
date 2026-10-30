/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_lane_device.h
 * @brief The kernels of the lane's device suite as the host cases call them: each takes a span of
 *        bytes, runs the lane's operations over it on the device, and hands the answer back.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note Each entry answers 0 where the kernel ran and its results came back, and the CUDA runtime's
 *       error number otherwise. A span's byte count is a multiple of 64, which every lane width up
 *       to the widest divides, and the device steps its own lane width over it.
 */
#ifndef TEST_EMBED_LANE_DEVICE_H
#define TEST_EMBED_LANE_DEVICE_H

#include "embed_types.h"

EMBED_BEGIN_DECLS

/** @brief 1 where a device is present to run on, 0 where none is. */
int embed_lane_device_present(void);

/** @brief The lane's width in bits as the device pass derived it, written by a kernel. */
int embed_lane_device_bits(embed_u32 *bits);

/** @brief `bytes` of `from` loaded a lane at a time on the device and put at `to`. */
int embed_lane_device_copy(const embed_u8 *from, embed_u8 *to, embed_u32 bytes);

/** @brief `bytes` of `to` filled with `val` a lane at a time on the device. */
int embed_lane_device_fill(embed_u8 val, embed_u8 *to, embed_u32 bytes);

/**
 * @brief The blend's exact divide, (x + (x >> 8)) >> 8, over `bytes` of 32-bit values on the device.
 */
int embed_lane_device_divide(const embed_u32 *x, embed_u32 *divided, embed_u32 bytes);

/**
 * @brief The logic and the arithmetic over `bytes` of 32-bit values on the device: and, or, xor,
 *        add, sub, mul, and the shifts left and right by `count`.
 */
int embed_lane_device_arithmetic(const embed_u32 *one, const embed_u32 *other, embed_u32 count, embed_u32 *out_and,
                                 embed_u32 *out_or, embed_u32 *out_xor, embed_u32 *out_add, embed_u32 *out_sub,
                                 embed_u32 *out_mul, embed_u32 *out_left, embed_u32 *out_right, embed_u32 bytes);

/**
 * @brief The byte masks over `bytes` of two spans on the device, one entry a device lane: the bytes
 *        equal, the bytes of `one` that are zero, and the first zero byte or 0xffffffff where none.
 */
int embed_lane_device_masks(const embed_u8 *one, const embed_u8 *other, embed_u64 *equal, embed_u64 *zero,
                            embed_u32 *first_zero, embed_u32 bytes);

EMBED_END_DECLS

#endif
