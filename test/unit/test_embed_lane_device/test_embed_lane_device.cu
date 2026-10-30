/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file test_embed_lane_device.cu
 * @brief The lane's device arm run: one thread a lane over a span, each thread calling the lane's
 *        own functions as a host walk calls them, the span carried to the device and back.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-10-29
 *
 * @note nvcc compiles this unit twice. The device pass takes the lane's portable arm at 256 bits,
 *       and the kernels below are the only code of the device pass that runs. The host pass holds
 *       the launchers, which move each span onto the device, run one thread a lane, and copy the
 *       answer back.
 */
#include "test_embed_lane_device.h"

#include "embed_lane.h"

#include <cuda_runtime.h>

/** @brief The threads of a block, the lanes one block takes. */
#define LANE_BLOCK_THREADS 128u

/** @brief The lanes `bytes` hold at the device's width. */
__host__ __device__ static embed_u32 lanes_of(embed_u32 bytes)
{
    return bytes / EMBED_LANE_BYTES;
}

__global__ static void lane_bits_kernel(embed_u32 *bits)
{
    *bits = EMBED_LANE_BITS;
}

__global__ static void lane_copy_kernel(const embed_u8 *from, embed_u8 *to, embed_u32 lanes)
{
    const embed_u32 lane = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (lane < lanes)
    {
        embed_lane_put(to + (lane * EMBED_LANE_BYTES), embed_lane_load(from + (lane * EMBED_LANE_BYTES)));
    }
}

__global__ static void lane_fill_kernel(embed_u8 val, embed_u8 *to, embed_u32 lanes)
{
    const embed_u32 lane = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (lane < lanes)
    {
        embed_lane_put(to + (lane * EMBED_LANE_BYTES), embed_lane_fill_byte(val));
    }
}

__global__ static void lane_divide_kernel(const embed_u32 *x, embed_u32 *divided, embed_u32 lanes)
{
    const embed_u32 lane = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (lane < lanes)
    {
        const embed_lane loaded = embed_lane_load(x + (lane * EMBED_LANE_U32S));
        const embed_lane biased = embed_lane_add_u32(loaded, embed_lane_shift_right_u32(loaded, 8u));
        embed_lane_put(divided + (lane * EMBED_LANE_U32S), embed_lane_shift_right_u32(biased, 8u));
    }
}

__global__ static void lane_arithmetic_kernel(const embed_u32 *one, const embed_u32 *other, embed_u32 count,
                                              embed_u32 *out_and, embed_u32 *out_or, embed_u32 *out_xor,
                                              embed_u32 *out_add, embed_u32 *out_sub, embed_u32 *out_mul,
                                              embed_u32 *out_left, embed_u32 *out_right, embed_u32 lanes)
{
    const embed_u32 lane = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (lane < lanes)
    {
        const embed_u32 at = lane * EMBED_LANE_U32S;
        const embed_lane a = embed_lane_load(one + at);
        const embed_lane b = embed_lane_load(other + at);
        embed_lane_put(out_and + at, embed_lane_and(a, b));
        embed_lane_put(out_or + at, embed_lane_or(a, b));
        embed_lane_put(out_xor + at, embed_lane_xor(a, b));
        embed_lane_put(out_add + at, embed_lane_add_u32(a, b));
        embed_lane_put(out_sub + at, embed_lane_sub_u32(a, b));
        embed_lane_put(out_mul + at, embed_lane_mul_u32(a, b));
        embed_lane_put(out_left + at, embed_lane_shift_left_u32(a, count));
        embed_lane_put(out_right + at, embed_lane_shift_right_u32(a, count));
    }
}

__global__ static void lane_masks_kernel(const embed_u8 *one, const embed_u8 *other, embed_u64 *equal, embed_u64 *zero,
                                         embed_u32 *first_zero, embed_u32 lanes)
{
    const embed_u32 lane = (blockIdx.x * blockDim.x) + threadIdx.x;
    if (lane < lanes)
    {
        const embed_lane a = embed_lane_load(one + (lane * EMBED_LANE_BYTES));
        const embed_lane b = embed_lane_load(other + (lane * EMBED_LANE_BYTES));
        const embed_lane_mask zeros = embed_lane_bytes_zero(a);
        equal[lane] = embed_lane_bytes_equal(a, b);
        zero[lane] = zeros;
        first_zero[lane] = (zeros != 0u) ? embed_lane_mask_first(zeros) : 0xffffffffu;
    }
}

/** @brief The blocks that cover `lanes` lanes at LANE_BLOCK_THREADS a block. */
static unsigned int blocks_of(embed_u32 lanes)
{
    return (lanes + LANE_BLOCK_THREADS - 1u) / LANE_BLOCK_THREADS;
}

/** @brief A span on the device: its address there and the bytes it holds. */
struct DeviceSpan
{
    void *at;
    size_t bytes;
};

/** @brief `bytes` taken on the device, `from` copied in where it is given. */
static cudaError_t span_take(DeviceSpan *span, const void *from, size_t bytes)
{
    span->at = nullptr;
    span->bytes = bytes;
    cudaError_t status = cudaMalloc(&span->at, bytes);
    if ((status == cudaSuccess) && (from != nullptr))
    {
        status = cudaMemcpy(span->at, from, bytes, cudaMemcpyHostToDevice);
    }
    return status;
}

/** @brief The span read back into `to` where it is given, and its device memory given back. */
static cudaError_t span_give(DeviceSpan *span, void *to)
{
    cudaError_t status = cudaSuccess;
    if (span->at != nullptr)
    {
        if (to != nullptr)
        {
            status = cudaMemcpy(to, span->at, span->bytes, cudaMemcpyDeviceToHost);
        }
        const cudaError_t freed = cudaFree(span->at);
        status = (status == cudaSuccess) ? freed : status;
        span->at = nullptr;
    }
    return status;
}

/** @brief The launch's own error and the device's completion folded into one status. */
static cudaError_t launched(void)
{
    const cudaError_t status = cudaGetLastError();
    return (status == cudaSuccess) ? cudaDeviceSynchronize() : status;
}

extern "C" int embed_lane_device_present(void)
{
    int count = 0;
    return ((cudaGetDeviceCount(&count) == cudaSuccess) && (count > 0)) ? 1 : 0;
}

extern "C" int embed_lane_device_bits(embed_u32 *bits)
{
    DeviceSpan out;
    cudaError_t status = span_take(&out, nullptr, sizeof(*bits));
    if (status == cudaSuccess)
    {
        lane_bits_kernel<<<1, 1>>>(static_cast<embed_u32 *>(out.at));
        status = launched();
    }
    const cudaError_t given = span_give(&out, bits);
    return static_cast<int>((status == cudaSuccess) ? given : status);
}

extern "C" int embed_lane_device_copy(const embed_u8 *from, embed_u8 *to, embed_u32 bytes)
{
    DeviceSpan in;
    DeviceSpan out;
    cudaError_t status = span_take(&in, from, bytes);
    if (status == cudaSuccess)
    {
        status = span_take(&out, nullptr, bytes);
    }
    if (status == cudaSuccess)
    {
        const embed_u32 lanes = lanes_of(bytes);
        lane_copy_kernel<<<blocks_of(lanes), LANE_BLOCK_THREADS>>>(static_cast<const embed_u8 *>(in.at),
                                                                  static_cast<embed_u8 *>(out.at), lanes);
        status = launched();
    }
    const cudaError_t given = span_give(&out, to);
    (void)span_give(&in, nullptr);
    return static_cast<int>((status == cudaSuccess) ? given : status);
}

extern "C" int embed_lane_device_fill(embed_u8 val, embed_u8 *to, embed_u32 bytes)
{
    DeviceSpan out;
    cudaError_t status = span_take(&out, nullptr, bytes);
    if (status == cudaSuccess)
    {
        const embed_u32 lanes = lanes_of(bytes);
        lane_fill_kernel<<<blocks_of(lanes), LANE_BLOCK_THREADS>>>(val, static_cast<embed_u8 *>(out.at), lanes);
        status = launched();
    }
    const cudaError_t given = span_give(&out, to);
    return static_cast<int>((status == cudaSuccess) ? given : status);
}

extern "C" int embed_lane_device_divide(const embed_u32 *x, embed_u32 *divided, embed_u32 bytes)
{
    DeviceSpan in;
    DeviceSpan out;
    cudaError_t status = span_take(&in, x, bytes);
    if (status == cudaSuccess)
    {
        status = span_take(&out, nullptr, bytes);
    }
    if (status == cudaSuccess)
    {
        const embed_u32 lanes = lanes_of(bytes);
        lane_divide_kernel<<<blocks_of(lanes), LANE_BLOCK_THREADS>>>(static_cast<const embed_u32 *>(in.at),
                                                                    static_cast<embed_u32 *>(out.at), lanes);
        status = launched();
    }
    const cudaError_t given = span_give(&out, divided);
    (void)span_give(&in, nullptr);
    return static_cast<int>((status == cudaSuccess) ? given : status);
}

extern "C" int embed_lane_device_arithmetic(const embed_u32 *one, const embed_u32 *other, embed_u32 count,
                                            embed_u32 *out_and, embed_u32 *out_or, embed_u32 *out_xor,
                                            embed_u32 *out_add, embed_u32 *out_sub, embed_u32 *out_mul,
                                            embed_u32 *out_left, embed_u32 *out_right, embed_u32 bytes)
{
    embed_u32 *outs[8] = {out_and, out_or, out_xor, out_add, out_sub, out_mul, out_left, out_right};
    DeviceSpan in_one;
    DeviceSpan in_other;
    DeviceSpan spans[8];
    cudaError_t status = span_take(&in_one, one, bytes);
    if (status == cudaSuccess)
    {
        status = span_take(&in_other, other, bytes);
    }
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        spans[at].at = nullptr;
        spans[at].bytes = bytes;
        if (status == cudaSuccess)
        {
            status = span_take(&spans[at], nullptr, bytes);
        }
    }
    if (status == cudaSuccess)
    {
        const embed_u32 lanes = lanes_of(bytes);
        lane_arithmetic_kernel<<<blocks_of(lanes), LANE_BLOCK_THREADS>>>(
            static_cast<const embed_u32 *>(in_one.at), static_cast<const embed_u32 *>(in_other.at), count,
            static_cast<embed_u32 *>(spans[0].at), static_cast<embed_u32 *>(spans[1].at),
            static_cast<embed_u32 *>(spans[2].at), static_cast<embed_u32 *>(spans[3].at),
            static_cast<embed_u32 *>(spans[4].at), static_cast<embed_u32 *>(spans[5].at),
            static_cast<embed_u32 *>(spans[6].at), static_cast<embed_u32 *>(spans[7].at), lanes);
        status = launched();
    }
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        const cudaError_t given = span_give(&spans[at], (status == cudaSuccess) ? outs[at] : nullptr);
        status = (status == cudaSuccess) ? given : status;
    }
    (void)span_give(&in_one, nullptr);
    (void)span_give(&in_other, nullptr);
    return static_cast<int>(status);
}

extern "C" int embed_lane_device_masks(const embed_u8 *one, const embed_u8 *other, embed_u64 *equal, embed_u64 *zero,
                                       embed_u32 *first_zero, embed_u32 bytes)
{
    const embed_u32 lanes = lanes_of(bytes);
    DeviceSpan in_one;
    DeviceSpan in_other;
    DeviceSpan out_equal;
    DeviceSpan out_zero;
    DeviceSpan out_first;
    cudaError_t status = span_take(&in_one, one, bytes);
    if (status == cudaSuccess)
    {
        status = span_take(&in_other, other, bytes);
    }
    if (status == cudaSuccess)
    {
        status = span_take(&out_equal, nullptr, lanes * sizeof(embed_u64));
    }
    if (status == cudaSuccess)
    {
        status = span_take(&out_zero, nullptr, lanes * sizeof(embed_u64));
    }
    if (status == cudaSuccess)
    {
        status = span_take(&out_first, nullptr, lanes * sizeof(embed_u32));
    }
    if (status == cudaSuccess)
    {
        lane_masks_kernel<<<blocks_of(lanes), LANE_BLOCK_THREADS>>>(
            static_cast<const embed_u8 *>(in_one.at), static_cast<const embed_u8 *>(in_other.at),
            static_cast<embed_u64 *>(out_equal.at), static_cast<embed_u64 *>(out_zero.at),
            static_cast<embed_u32 *>(out_first.at), lanes);
        status = launched();
    }
    const bool ran = (status == cudaSuccess);
    cudaError_t given = span_give(&out_equal, ran ? equal : nullptr);
    status = ran ? given : status;
    given = span_give(&out_zero, (status == cudaSuccess) ? zero : nullptr);
    status = (status == cudaSuccess) ? given : status;
    given = span_give(&out_first, (status == cudaSuccess) ? first_zero : nullptr);
    status = (status == cudaSuccess) ? given : status;
    (void)span_give(&in_one, nullptr);
    (void)span_give(&in_other, nullptr);
    return static_cast<int>(status);
}
