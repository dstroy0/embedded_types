<!-- tools/api_pages.py writes this page from include/embed_atomic.h. Edit the header, then run the tool. -->

# embed_atomic.h

Source: [`include/embed_atomic.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_atomic.h)

Atomic words on the atomics of the target, with no lock and no system call.

This header gives a 32-bit word and a 64-bit word. A host thread, a task on a part and a GPU thread read, write, swap, compare and swap, add to and mask them. Each operation is one indivisible step.

> **Note:** These steps alone keep a structure that contexts share consistent, and no context waits for another. A counter is a fetch-add. A claim is a compare-and-swap that takes, or that reports who took. A boundary is a compare-and-swap loop that moves, or that tries again over the value that it found. Nothing here spins on the progress of another context.

> **Note:** The switch below selects one of four arms, one time. The first arm is the device atomics of CUDA in a device pass. The second is C11 atomics, where a C compiler has them. The third is the GCC and clang `__atomic` builtins, where C11 atomics are absent or the consumer is C++. The fourth is the interlocked intrinsics under MSVC. Each arm gives a load acquire ordering, a store release ordering, and each read-modify-write both.

> **Note:** There are two widths. Each arm has the 32-bit word, the CUDA atomics among them. The counters and packed boundaries of a host need the 64-bit word. CUDA, C11, the builtins and MSVC on a 64-bit host all have it. [`embed_atomic_word`](#embed_atomic_word) matches the machine word.

> **Note:** Every entry carries [`EMBED_HOST_DEVICE`](embed_compiler_directives.md#EMBED_HOST_DEVICE). Under nvcc one definition compiles into the host pass and the device pass, each on its own arm, over the same word in memory.

> _**Warning:**_ A target with none of the four fails the build at the `#error` below. There is no plain fallback: a word that is not atomic is a defect that shows only under contention.

## Types

### `embed_atomic_u32` { #embed_atomic_u32 }

```c
typedef struct
{
#if EMBED_ATOMIC_ARM == 2
    _Atomic embed_u32 value; /**< The word. */
#else
    volatile embed_u32 value; /**< The word. */
#endif
} embed_atomic_u32;
```

A 32-bit word every access to which is one indivisible step.

> **Note:** The word is atomic on the C11 arm, and plain on the other arms. There, the device atomics, the builtins and the intrinsics make each access atomic. A word in storage of zeros holds 0 on each arm.

- `value`: The word.

### `embed_atomic_u64` { #embed_atomic_u64 }

```c
typedef struct
{
#if EMBED_ATOMIC_ARM == 2
    _Atomic embed_u64 value; /**< The word. */
#else
    volatile embed_u64 value; /**< The word. */
#endif
} embed_atomic_u64;
```

A 64-bit word every access to which is one indivisible step.

- `value`: The word.

### `embed_atomic_word` { #embed_atomic_word }

```c
#if EMBED_WORD_BITS == 64
typedef embed_atomic_u64 embed_atomic_word;
#else
typedef embed_atomic_u32 embed_atomic_word;
#endif
```

The atomic word of the machine word's width: the 64-bit word on a 64-bit word, the 32-bit word below it.

> **Note:** A count of bytes or an offset within the reach of the word fits it. The operations below use its name, with no width in the name.

### `embed_atomic_word_value` { #embed_atomic_word_value }

```c
#if EMBED_WORD_BITS == 64
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
```

The plain integer [`embed_atomic_word`](#embed_atomic_word) holds and its operations take and answer.

The plain integer [`embed_atomic_word`](#embed_atomic_word) holds and its operations take and answer.

### `embed_atomic_usize` { #embed_atomic_usize }

```c
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFu
typedef embed_atomic_u64 embed_atomic_usize;
#else
typedef embed_atomic_u32 embed_atomic_usize;
#endif
```

The atomic word as wide as `size_t`, for a count of bytes or an offset as `size_t` holds it. It is the 64-bit word where `SIZE_MAX` is 64 bits wide, and the 32-bit word below that.

> **Note:** The key is `size_t`, and not [`EMBED_WORD_BITS`](embed_types.md#EMBED_WORD_BITS). A build can define the word narrower than the sizes that it counts, and a count must still fit whole.

### `embed_atomic_usize_value` { #embed_atomic_usize_value }

```c
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFu
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
```

The plain integer [`embed_atomic_usize`](#embed_atomic_usize) holds and its operations take and answer.

The plain integer [`embed_atomic_usize`](#embed_atomic_usize) holds and its operations take and answer.

### `embed_atomic_uptr` { #embed_atomic_uptr }

```c
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
typedef embed_atomic_u64 embed_atomic_uptr;
#else
typedef embed_atomic_u32 embed_atomic_uptr;
#endif
```

The atomic word as wide as a pointer, for an address as `uintptr_t` holds it. It is the 64-bit word where `UINTPTR_MAX` is 64 bits wide, and the 32-bit word below that.

> **Note:** Keyed on the pointer, not on [`EMBED_WORD_BITS`](embed_types.md#EMBED_WORD_BITS), since a build may define the word narrower than the pointer and an address must still fit whole.

### `embed_atomic_uptr_value` { #embed_atomic_uptr_value }

```c
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
typedef embed_u64 embed_atomic_uptr_value;
#define embed_atomic_uptr_load embed_atomic_u64_load
#define embed_atomic_uptr_store embed_atomic_u64_store
#define embed_atomic_uptr_exchange embed_atomic_u64_exchange
#define embed_atomic_uptr_swap_if embed_atomic_u64_swap_if
#else
typedef embed_u32 embed_atomic_uptr_value;
#define embed_atomic_uptr_load embed_atomic_u32_load
#define embed_atomic_uptr_store embed_atomic_u32_store
#define embed_atomic_uptr_exchange embed_atomic_u32_exchange
#define embed_atomic_uptr_swap_if embed_atomic_u32_swap_if
#endif
```

The plain integer [`embed_atomic_uptr`](#embed_atomic_uptr) holds and its operations take and answer.

The plain integer [`embed_atomic_uptr`](#embed_atomic_uptr) holds and its operations take and answer.

## Macros

### `EMBED_ATOMIC_ARM` { #EMBED_ATOMIC_ARM }

```c
#if defined(__CUDA_ARCH__)
#define EMBED_ATOMIC_ARM 1
#elif !defined(__cplusplus) && !defined(__STDC_NO_ATOMICS__)
#define EMBED_ATOMIC_ARM 2
#elif EMBED_GNU_ATTRIBUTES
#define EMBED_ATOMIC_ARM 3
#elif defined(_MSC_VER)
#define EMBED_ATOMIC_ARM 4
#endif
```

1 where the atomics run on CUDA's device atomics, 2 on C11 atomics, 3 on the GCC and clang builtins, 4 on MSVC's interlocked intrinsics.

> **Note:** Always defined. Under nvcc the host pass and the device pass each read this header, and each selects its own arm over the same word.

### `EMBED_ATOMIC_INIT` { #EMBED_ATOMIC_INIT }

```c
#define EMBED_ATOMIC_INIT(value_) {(value_)}
```

An atomic word that holds `value_`, for the declaration that makes one.

- `[in]`: `value_` The word's first value.

### `EMBED_ATOMIC_LOAD` { #EMBED_ATOMIC_LOAD }

```c
#if EMBED_ATOMIC_ARM == 1
#define EMBED_ATOMIC_LOAD(atom_) ((atom_)->value)
#define EMBED_ATOMIC_FENCE() __threadfence()
#define EMBED_ATOMIC_ORDERED() __threadfence()
#define EMBED_ATOMIC_STORE(atom_, value_) ((atom_)->value = (value_))
#define EMBED_ATOMIC_EXCHANGE_32(atom_, value_) atomicExch((unsigned int *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_EXCHANGE_64(atom_, value_) atomicExch((unsigned long long *)&(atom_)->value, (value_))
#define EMBED_ATOMIC_SWAP_IF_32(atom_, expected_, value_) atomicCAS((unsigned int *)&(atom_)->value, (expected_), (value_))
#define EMBED_ATOMIC_SWAP_IF_64(atom_, expected_, value_) \
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
#define EMBED_ATOMIC_LOAD(atom_) ((atom_)->value)
#define EMBED_ATOMIC_FENCE() _ReadWriteBarrier()
#define EMBED_ATOMIC_ORDERED() \
    do \
    { \
        volatile long full_barrier_ = 0; \
        (void)_InterlockedExchange(&full_barrier_, 0); \
    } while (0)
#define EMBED_ATOMIC_STORE(atom_, value_) ((atom_)->value = (value_))
#define EMBED_ATOMIC_EXCHANGE_32(atom_, value_) \
    ((embed_u32)_InterlockedExchange((volatile long *)&(atom_)->value, (long)(value_)))
#define EMBED_ATOMIC_EXCHANGE_64(atom_, value_) \
    ((embed_u64)_InterlockedExchange64((volatile __int64 *)&(atom_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_SWAP_IF_32(atom_, expected_, value_) \
    ((embed_u32)_InterlockedCompareExchange((volatile long *)&(atom_)->value, (long)(value_), (long)(expected_)))
#define EMBED_ATOMIC_SWAP_IF_64(atom_, expected_, value_) \
    ((embed_u64)_InterlockedCompareExchange64((volatile __int64 *)&(atom_)->value, (__int64)(value_), \
                                              (__int64)(expected_)))
#define EMBED_ATOMIC_ADD_32(atom_, value_) \
    ((embed_u32)_InterlockedExchangeAdd((volatile long *)&(atom_)->value, (long)(value_)))
#define EMBED_ATOMIC_ADD_64(atom_, value_) \
    ((embed_u64)_InterlockedExchangeAdd64((volatile __int64 *)&(atom_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_OR_32(atom_, value_) ((embed_u32)_InterlockedOr((volatile long *)&(atom_)->value, (long)(value_)))
#define EMBED_ATOMIC_OR_64(atom_, value_) \
    ((embed_u64)_InterlockedOr64((volatile __int64 *)&(atom_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_AND_32(atom_, value_) \
    ((embed_u32)_InterlockedAnd((volatile long *)&(atom_)->value, (long)(value_)))
#define EMBED_ATOMIC_AND_64(atom_, value_) \
    ((embed_u64)_InterlockedAnd64((volatile __int64 *)&(atom_)->value, (__int64)(value_)))
#endif
```

The device arm's load: a volatile read, then a fence, which no later read moves ahead of.

MSVC's intrinsics take a long for 32 bits and an `__int64` for 64; the casts read the same bits as the unsigned words they are. A plain volatile read and write are one step each on the targets MSVC compiles for, and the fences give them their ordering.

### `EMBED_ATOMIC_OPERATIONS` { #EMBED_ATOMIC_OPERATIONS }

```c
#define EMBED_ATOMIC_OPERATIONS(name_, type_, exchange_, swap_if_, add_, or_, and_) \
    /** @brief The word, with acquire ordering: no later read moves ahead of it. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_load(const embed_atomic_##name_ *atom) \
    { \
        const type_ found = EMBED_ATOMIC_LOAD(atom); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief The word set to value, with release ordering: every earlier write is out before it. */ \
    EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_##name_##_store(embed_atomic_##name_ *atom, type_ value) \
    { \
        EMBED_ATOMIC_FENCE(); \
        EMBED_ATOMIC_STORE(atom, value); \
    } \
    /** @brief The word set to value, and the word before. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_exchange(embed_atomic_##name_ *atom, type_ value) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const type_ found = exchange_(atom, value); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief The word set to value where it holds *expected, else *expected set to what it holds. */ \
    EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_##name_##_swap_if(embed_atomic_##name_ *atom, \
                                                                           type_ *expected, type_ value) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const embed_bool swapped = swap_if_(atom, expected, value); \
        EMBED_ATOMIC_FENCE(); \
        return swapped; \
    } \
    /** @brief value added to the word, with wrap around, and the word before the add. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_add(embed_atomic_##name_ *atom, type_ value) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const type_ found = add_(atom, value); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief value taken from the word, with wrap around, and the word before the subtract. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_sub(embed_atomic_##name_ *atom, type_ value) \
    { \
        /* The add of the two's complement is the subtract, because the add of each arm wraps. */ \
        return embed_atomic_##name_##_fetch_add(atom, (type_)(0u - value)); \
    } \
    /** @brief The word ored with value, and the word before the or. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_or(embed_atomic_##name_ *atom, type_ value) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const type_ found = or_(atom, value); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief The word anded with value, and the word before the and. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_and(embed_atomic_##name_ *atom, type_ value) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const type_ found = and_(atom, value); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief The word raised to value where value is above it, by a swap, and the word before. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_raise(embed_atomic_##name_ *atom, type_ value) \
    { \
        type_ found = embed_atomic_##name_##_load(atom); \
        while ((found < value) && !embed_atomic_##name_##_swap_if(atom, &found, value)) \
        { \
            /* found now holds the word that another context wrote, and the test runs again on it. */ \
        } \
        return found; \
    }
```

Defines the operations over one atomic width. `name_` is the suffix and `type_` the word. The other five parameters are the primitives of the arm for that width.

> **Note:** One text for both widths and each arm. A width or an arm cannot drift from the others.

> **Note:** The compare-and-swap uses the primitive of the arm where the arm has one, and the exchange where it does not. The C11 arm and the builtin arm use the generic call below.

### `EMBED_ATOMIC_CAS_32` { #EMBED_ATOMIC_CAS_32 }

```c
#if (EMBED_ATOMIC_ARM == 1) || (EMBED_ATOMIC_ARM == 4)
#define EMBED_ATOMIC_CAS_32(atom_, expected_, value_) embed_atomic_cas_u32(atom_, expected_, value_)
#define EMBED_ATOMIC_CAS_64(atom_, expected_, value_) embed_atomic_cas_u64(atom_, expected_, value_)
#elif EMBED_ATOMIC_ARM == 2
#define EMBED_ATOMIC_CAS_32(atom_, expected_, value_) \
    ((embed_bool)atomic_compare_exchange_strong_explicit(&(atom_)->value, (expected_), (value_), memory_order_acq_rel, \
                                                         memory_order_acquire))
#define EMBED_ATOMIC_CAS_64(atom_, expected_, value_) EMBED_ATOMIC_CAS_32(atom_, expected_, value_)
#else
#define EMBED_ATOMIC_CAS_32(atom_, expected_, value_) \
    ((embed_bool)__atomic_compare_exchange_n(&(atom_)->value, (expected_), (value_), 0, __ATOMIC_ACQ_REL, \
                                             __ATOMIC_ACQUIRE))
#define EMBED_ATOMIC_CAS_64(atom_, expected_, value_) EMBED_ATOMIC_CAS_32(atom_, expected_, value_)
#endif
```

The compare-and-swap on an arm whose primitive gives the word that it found.

The swap occurred where that word is the expected word. Otherwise, \*expected takes the word that the primitive found.

## Functions

### `embed_atomic_u32_load` { #embed_atomic_u32_load }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_load(const embed_atomic_u32 *atom);
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_load(const embed_atomic_u64 *atom);
```

The word, with acquire ordering: no later read moves ahead of it.

### `embed_atomic_u32_store` { #embed_atomic_u32_store }

```c
EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_u32_store(embed_atomic_u32 *atom, embed_u32 value);
EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_u64_store(embed_atomic_u64 *atom, embed_u64 value);
```

The word set to value, with release ordering: every earlier write is out before it.

### `embed_atomic_u32_exchange` { #embed_atomic_u32_exchange }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_exchange(embed_atomic_u32 *atom, embed_u32 value);
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_exchange(embed_atomic_u64 *atom, embed_u64 value);
```

The word set to value, and the word before.

### `embed_atomic_u32_swap_if` { #embed_atomic_u32_swap_if }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_u32_swap_if(embed_atomic_u32 *atom, embed_u32 *expected, embed_u32 value);
EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_u64_swap_if(embed_atomic_u64 *atom, embed_u64 *expected, embed_u64 value);
```

The word set to value where it holds \*expected, else \*expected set to what it holds.

### `embed_atomic_u32_fetch_add` { #embed_atomic_u32_fetch_add }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_fetch_add(embed_atomic_u32 *atom, embed_u32 value);
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_fetch_add(embed_atomic_u64 *atom, embed_u64 value);
```

value added to the word, with wrap around, and the word before the add.

### `embed_atomic_u32_fetch_sub` { #embed_atomic_u32_fetch_sub }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_fetch_sub(embed_atomic_u32 *atom, embed_u32 value);
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_fetch_sub(embed_atomic_u64 *atom, embed_u64 value);
```

value taken from the word, with wrap around, and the word before the subtract.

### `embed_atomic_u32_fetch_or` { #embed_atomic_u32_fetch_or }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_fetch_or(embed_atomic_u32 *atom, embed_u32 value);
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_fetch_or(embed_atomic_u64 *atom, embed_u64 value);
```

The word ored with value, and the word before the or.

### `embed_atomic_u32_fetch_and` { #embed_atomic_u32_fetch_and }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_fetch_and(embed_atomic_u32 *atom, embed_u32 value);
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_fetch_and(embed_atomic_u64 *atom, embed_u64 value);
```

The word anded with value, and the word before the and.

### `embed_atomic_u32_raise` { #embed_atomic_u32_raise }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_raise(embed_atomic_u32 *atom, embed_u32 value);
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_raise(embed_atomic_u64 *atom, embed_u64 value);
```

The word raised to value where value is above it, by a swap, and the word before.

### `embed_atomic_ordered` { #embed_atomic_ordered }

```c
EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_ordered(void);
```

Orders this context's earlier writes ahead of its later reads, on every arm.

> **Note:** The operations above order a read before later reads, and a write after earlier writes. None of them orders a write before a later read. Two contexts that each write their own word and read the word of the other need this call between the two. Without it, each can read the word of the other from before its write.
