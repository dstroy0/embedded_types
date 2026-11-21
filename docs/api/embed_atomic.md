<!-- tools/api_pages.py writes this page from include/embed_atomic.h. Edit the header, then run the tool. -->

# embed_atomic.h

Source: [`include/embed_atomic.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_atomic.h)

Atomic words on the atomics of the target, with no lock and no system call.

This header gives a 32-bit word and a 64-bit word. A host thread, a task on a part and a GPU thread read, write, swap, compare and swap, add to and mask them. Each operation is one indivisible step.

> **Note:** These steps alone retain a structure that contexts share consistent, and no context waits for another. A counter is a fetch-add. A claim is a compare-and-swap that takes, or that reports who took. A boundary is a compare-and-swap loop that moves, or that tries again over the value that it found. Nothing here spins on the progress of another context.

> **Note:** [`EMBED_ATOMIC_ARM`](embed_compiler_directives.md#EMBED_ATOMIC_ARM) in `embed_compiler_directives`.h selects one of four arms, one time. The first arm is the device atomics of CUDA in a device pass. The second is C11 atomics, where a C compiler has them. The third is the GCC and clang `__atomic` builtins, where C11 atomics are absent or the consumer is C++. The fourth is the interlocked intrinsics under MSVC. Each arm gives a load acquire ordering, a store release ordering, and each read-modify-write both.

> **Note:** There are two widths. Each arm has the 32-bit word, the CUDA atomics among them. The counters and packed boundaries of a host need the 64-bit word. CUDA, C11, the builtins and MSVC on a 64-bit host all have it. MSVC on a 32-bit host does not, and [`EMBED_ATOMIC_HAS_64`](#EMBED_ATOMIC_HAS_64) is then 0. [`embed_atomic_word`](#embed_atomic_word) matches the machine word.

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
EMBED_STATIC_ASSERT(sizeof(embed_atomic_u32) == 4u, "embed_atomic_u32 must be one 32-bit word on every arm");
```

A 32-bit word every access to which is one indivisible step.

> **Note:** The word is atomic on the C11 arm, and plain on the other arms. There, the device atomics, the builtins and the intrinsics make each access atomic. A word in storage of zeros holds 0 on each arm.

- `value`: The word.

### `embed_atomic_u64` { #embed_atomic_u64 }

```c
#if EMBED_ATOMIC_HAS_64
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
#define embed_atomic_u64 embed_atomic_u64_needs_a_target_with_64_bit_atomics
#endif
```

A 64-bit word every access to which is one indivisible step, where [`EMBED_ATOMIC_HAS_64`](#EMBED_ATOMIC_HAS_64) is 1.

The 64-bit word on a target whose arm has none. Each use of the type stops the compile of that file, and the error names the cause.

- `value`: The word.

### `EmbedAtomicCasU32Cfg` { #EmbedAtomicCasU32Cfg }

```c
#if (EMBED_ATOMIC_ARM == 1) || (EMBED_ATOMIC_ARM == 4)
typedef struct
{
    embed_atomic_u32 *const word; /**< The word [BORROWS]. */
    embed_u32 *const expected;    /**< The word that the caller expects, and then the word found [BORROWS]. */
    const embed_u32 value;        /**< The word that the swap writes. */
} EmbedAtomicCasU32Cfg;
#endif
```

The parameters of [`embed_atomic_cas_u32`](#embed_atomic_cas_u32).

- `word`: The word \[BORROWS\].
- `expected`: The word that the caller expects, and then the word found \[BORROWS\].
- `value`: The word that the swap writes.

### `EmbedAtomicCasU64Cfg` { #EmbedAtomicCasU64Cfg }

```c
#if (EMBED_ATOMIC_ARM == 1) || (EMBED_ATOMIC_ARM == 4)
#if EMBED_ATOMIC_HAS_64
typedef struct
{
    embed_atomic_u64 *const word; /**< The word [BORROWS]. */
    embed_u64 *const expected;    /**< The word that the caller expects, and then the word found [BORROWS]. */
    const embed_u64 value;        /**< The word that the swap writes. */
} EmbedAtomicCasU64Cfg;
#endif
#endif
```

The parameters of [`embed_atomic_cas_u64`](#embed_atomic_cas_u64).

- `word`: The word \[BORROWS\].
- `expected`: The word that the caller expects, and then the word found \[BORROWS\].
- `value`: The word that the swap writes.

### `EmbedAtomicU32LoadCfg` { #EmbedAtomicU32LoadCfg }

```c
    typedef struct
    {
        const embed_atomic_u32 *const word; /**< The word [BORROWS]. */
    } EmbedAtomicU32LoadCfg;
#if EMBED_ATOMIC_HAS_64
    typedef struct
    {
        const embed_atomic_u64 *const word; /**< The word [BORROWS]. */
    } EmbedAtomicU64LoadCfg;
#endif
```

The parameters of the load.

- `word`: The word \[BORROWS\].

### `EmbedAtomicU32ValueCfg` { #EmbedAtomicU32ValueCfg }

```c
    typedef struct
    {
        embed_atomic_u32 *const word; /**< The word [BORROWS]. */
        const embed_u32 value;                /**< The value of the operation. */
    } EmbedAtomicU32ValueCfg;
#if EMBED_ATOMIC_HAS_64
    typedef struct
    {
        embed_atomic_u64 *const word; /**< The word [BORROWS]. */
        const embed_u64 value;                /**< The value of the operation. */
    } EmbedAtomicU64ValueCfg;
#endif
```

The parameters of an operation that writes value into the word or combines it with the word.

- `word`: The word \[BORROWS\].
- `value`: The value of the operation.

### `EmbedAtomicU32StoreCfg` { #EmbedAtomicU32StoreCfg }

```c
    typedef EmbedAtomicU32ValueCfg EmbedAtomicU32StoreCfg;
#if EMBED_ATOMIC_HAS_64
    typedef EmbedAtomicU64ValueCfg EmbedAtomicU64StoreCfg;
#endif
```

The parameters of the store.

### `EmbedAtomicU32ExchangeCfg` { #EmbedAtomicU32ExchangeCfg }

```c
    typedef EmbedAtomicU32ValueCfg EmbedAtomicU32ExchangeCfg;
#if EMBED_ATOMIC_HAS_64
    typedef EmbedAtomicU64ValueCfg EmbedAtomicU64ExchangeCfg;
#endif
```

The parameters of the exchange.

### `EmbedAtomicU32FetchAddCfg` { #EmbedAtomicU32FetchAddCfg }

```c
    typedef EmbedAtomicU32ValueCfg EmbedAtomicU32FetchAddCfg;
#if EMBED_ATOMIC_HAS_64
    typedef EmbedAtomicU64ValueCfg EmbedAtomicU64FetchAddCfg;
#endif
```

The parameters of the fetch-add.

### `EmbedAtomicU32FetchSubCfg` { #EmbedAtomicU32FetchSubCfg }

```c
    typedef EmbedAtomicU32ValueCfg EmbedAtomicU32FetchSubCfg;
#if EMBED_ATOMIC_HAS_64
    typedef EmbedAtomicU64ValueCfg EmbedAtomicU64FetchSubCfg;
#endif
```

The parameters of the fetch-subtract.

### `EmbedAtomicU32FetchOrCfg` { #EmbedAtomicU32FetchOrCfg }

```c
    typedef EmbedAtomicU32ValueCfg EmbedAtomicU32FetchOrCfg;
#if EMBED_ATOMIC_HAS_64
    typedef EmbedAtomicU64ValueCfg EmbedAtomicU64FetchOrCfg;
#endif
```

The parameters of the fetch-or.

### `EmbedAtomicU32FetchAndCfg` { #EmbedAtomicU32FetchAndCfg }

```c
    typedef EmbedAtomicU32ValueCfg EmbedAtomicU32FetchAndCfg;
#if EMBED_ATOMIC_HAS_64
    typedef EmbedAtomicU64ValueCfg EmbedAtomicU64FetchAndCfg;
#endif
```

The parameters of the fetch-and.

### `EmbedAtomicU32RaiseCfg` { #EmbedAtomicU32RaiseCfg }

```c
    typedef EmbedAtomicU32ValueCfg EmbedAtomicU32RaiseCfg;
#if EMBED_ATOMIC_HAS_64
    typedef EmbedAtomicU64ValueCfg EmbedAtomicU64RaiseCfg;
#endif
```

The parameters of the raise.

### `EmbedAtomicU32SwapIfCfg` { #EmbedAtomicU32SwapIfCfg }

```c
    typedef struct
    {
        embed_atomic_u32 *const word; /**< The word [BORROWS]. */
        embed_u32 *const expected;            /**< The word that the caller expects, and then the word found [BORROWS]. */
        const embed_u32 value;                /**< The word that the swap writes. */
    } EmbedAtomicU32SwapIfCfg;
#if EMBED_ATOMIC_HAS_64
    typedef struct
    {
        embed_atomic_u64 *const word; /**< The word [BORROWS]. */
        embed_u64 *const expected;            /**< The word that the caller expects, and then the word found [BORROWS]. */
        const embed_u64 value;                /**< The word that the swap writes. */
    } EmbedAtomicU64SwapIfCfg;
#endif
```

The parameters of the compare-and-swap.

- `word`: The word \[BORROWS\].
- `expected`: The word that the caller expects, and then the word found \[BORROWS\].
- `value`: The word that the swap writes.

### `EmbedAtomicU32Ns` { #EmbedAtomicU32Ns }

```c
    typedef struct
    {
        embed_u32 (*load)(const EmbedAtomicU32LoadCfg *args);           /**< The load. */
        void (*store)(const EmbedAtomicU32StoreCfg *args);          /**< The store. */
        embed_u32 (*exchange)(const EmbedAtomicU32ExchangeCfg *args);   /**< The exchange. */
        embed_bool (*swap_if)(const EmbedAtomicU32SwapIfCfg *args); /**< The compare-and-swap. */
        embed_u32 (*fetch_add)(const EmbedAtomicU32FetchAddCfg *args);  /**< The fetch-add. */
        embed_u32 (*fetch_sub)(const EmbedAtomicU32FetchSubCfg *args);  /**< The fetch-subtract. */
        embed_u32 (*fetch_or)(const EmbedAtomicU32FetchOrCfg *args);    /**< The fetch-or. */
        embed_u32 (*fetch_and)(const EmbedAtomicU32FetchAndCfg *args);  /**< The fetch-and. */
        embed_u32 (*raise)(const EmbedAtomicU32RaiseCfg *args);         /**< The raise. */
    } EmbedAtomicU32Ns;
#if EMBED_ATOMIC_HAS_64
    typedef struct
    {
        embed_u64 (*load)(const EmbedAtomicU64LoadCfg *args);           /**< The load. */
        void (*store)(const EmbedAtomicU64StoreCfg *args);          /**< The store. */
        embed_u64 (*exchange)(const EmbedAtomicU64ExchangeCfg *args);   /**< The exchange. */
        embed_bool (*swap_if)(const EmbedAtomicU64SwapIfCfg *args); /**< The compare-and-swap. */
        embed_u64 (*fetch_add)(const EmbedAtomicU64FetchAddCfg *args);  /**< The fetch-add. */
        embed_u64 (*fetch_sub)(const EmbedAtomicU64FetchSubCfg *args);  /**< The fetch-subtract. */
        embed_u64 (*fetch_or)(const EmbedAtomicU64FetchOrCfg *args);    /**< The fetch-or. */
        embed_u64 (*fetch_and)(const EmbedAtomicU64FetchAndCfg *args);  /**< The fetch-and. */
        embed_u64 (*raise)(const EmbedAtomicU64RaiseCfg *args);         /**< The raise. */
    } EmbedAtomicU64Ns;
#endif
```

The dispatch table of the operations of one width, in the order of the header.

- `args`: The load.

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
#else
typedef embed_u32 embed_atomic_word_value;
#endif
```

The plain integer [`embed_atomic_word`](#embed_atomic_word) holds and its operations take and answer.

The plain integer [`embed_atomic_word`](#embed_atomic_word) holds and its operations take and answer.

### `EmbedAtomicWordLoadCfg` { #EmbedAtomicWordLoadCfg }

```c
#if EMBED_WORD_BITS == 64
typedef EmbedAtomicU64LoadCfg EmbedAtomicWordLoadCfg;
typedef EmbedAtomicU64StoreCfg EmbedAtomicWordStoreCfg;
typedef EmbedAtomicU64ExchangeCfg EmbedAtomicWordExchangeCfg;
typedef EmbedAtomicU64SwapIfCfg EmbedAtomicWordSwapIfCfg;
typedef EmbedAtomicU64FetchAddCfg EmbedAtomicWordFetchAddCfg;
typedef EmbedAtomicU64FetchSubCfg EmbedAtomicWordFetchSubCfg;
typedef EmbedAtomicU64FetchOrCfg EmbedAtomicWordFetchOrCfg;
typedef EmbedAtomicU64FetchAndCfg EmbedAtomicWordFetchAndCfg;
typedef EmbedAtomicU64RaiseCfg EmbedAtomicWordRaiseCfg;
typedef EmbedAtomicU64Ns EmbedAtomicWordNs;
#define embed_atomic_word_table embed_atomic_u64_table
#else
typedef EmbedAtomicU32LoadCfg EmbedAtomicWordLoadCfg;
typedef EmbedAtomicU32StoreCfg EmbedAtomicWordStoreCfg;
typedef EmbedAtomicU32ExchangeCfg EmbedAtomicWordExchangeCfg;
typedef EmbedAtomicU32SwapIfCfg EmbedAtomicWordSwapIfCfg;
typedef EmbedAtomicU32FetchAddCfg EmbedAtomicWordFetchAddCfg;
typedef EmbedAtomicU32FetchSubCfg EmbedAtomicWordFetchSubCfg;
typedef EmbedAtomicU32FetchOrCfg EmbedAtomicWordFetchOrCfg;
typedef EmbedAtomicU32FetchAndCfg EmbedAtomicWordFetchAndCfg;
typedef EmbedAtomicU32RaiseCfg EmbedAtomicWordRaiseCfg;
typedef EmbedAtomicU32Ns EmbedAtomicWordNs;
#define embed_atomic_word_table embed_atomic_u32_table
#endif
```

The Cfg of each operation of [`embed_atomic_word`](#embed_atomic_word), and its table: those of [`embed_atomic_u64`](#embed_atomic_u64).

The Cfg of each operation of [`embed_atomic_word`](#embed_atomic_word), and its table: those of [`embed_atomic_u32`](#embed_atomic_u32).

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
#else
typedef embed_u32 embed_atomic_usize_value;
#endif
```

The plain integer [`embed_atomic_usize`](#embed_atomic_usize) holds and its operations take and answer.

The plain integer [`embed_atomic_usize`](#embed_atomic_usize) holds and its operations take and answer.

### `EmbedAtomicUsizeLoadCfg` { #EmbedAtomicUsizeLoadCfg }

```c
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFu
typedef EmbedAtomicU64LoadCfg EmbedAtomicUsizeLoadCfg;
typedef EmbedAtomicU64StoreCfg EmbedAtomicUsizeStoreCfg;
typedef EmbedAtomicU64ExchangeCfg EmbedAtomicUsizeExchangeCfg;
typedef EmbedAtomicU64SwapIfCfg EmbedAtomicUsizeSwapIfCfg;
typedef EmbedAtomicU64FetchAddCfg EmbedAtomicUsizeFetchAddCfg;
typedef EmbedAtomicU64FetchSubCfg EmbedAtomicUsizeFetchSubCfg;
typedef EmbedAtomicU64FetchOrCfg EmbedAtomicUsizeFetchOrCfg;
typedef EmbedAtomicU64FetchAndCfg EmbedAtomicUsizeFetchAndCfg;
typedef EmbedAtomicU64RaiseCfg EmbedAtomicUsizeRaiseCfg;
typedef EmbedAtomicU64Ns EmbedAtomicUsizeNs;
#define embed_atomic_usize_table embed_atomic_u64_table
#else
typedef EmbedAtomicU32LoadCfg EmbedAtomicUsizeLoadCfg;
typedef EmbedAtomicU32StoreCfg EmbedAtomicUsizeStoreCfg;
typedef EmbedAtomicU32ExchangeCfg EmbedAtomicUsizeExchangeCfg;
typedef EmbedAtomicU32SwapIfCfg EmbedAtomicUsizeSwapIfCfg;
typedef EmbedAtomicU32FetchAddCfg EmbedAtomicUsizeFetchAddCfg;
typedef EmbedAtomicU32FetchSubCfg EmbedAtomicUsizeFetchSubCfg;
typedef EmbedAtomicU32FetchOrCfg EmbedAtomicUsizeFetchOrCfg;
typedef EmbedAtomicU32FetchAndCfg EmbedAtomicUsizeFetchAndCfg;
typedef EmbedAtomicU32RaiseCfg EmbedAtomicUsizeRaiseCfg;
typedef EmbedAtomicU32Ns EmbedAtomicUsizeNs;
#define embed_atomic_usize_table embed_atomic_u32_table
#endif
```

The Cfg of each operation of [`embed_atomic_usize`](#embed_atomic_usize), and its table: those of [`embed_atomic_u64`](#embed_atomic_u64).

The Cfg of each operation of [`embed_atomic_usize`](#embed_atomic_usize), and its table: those of [`embed_atomic_u32`](#embed_atomic_u32).

### `embed_atomic_uptr` { #embed_atomic_uptr }

```c
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
typedef embed_atomic_u64 embed_atomic_uptr;
#else
typedef embed_atomic_u32 embed_atomic_uptr;
#endif
```

The atomic word as wide as a pointer, for an address as `uintptr_t` holds it. It is the 64-bit word where `UINTPTR_MAX` is 64 bits wide, and the 32-bit word below that.

> **Note:** The key is the pointer, and not [`EMBED_WORD_BITS`](embed_types.md#EMBED_WORD_BITS). A build can define the word narrower than the pointer, and an address must still fit whole.

### `embed_atomic_uptr_value` { #embed_atomic_uptr_value }

```c
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
typedef embed_u64 embed_atomic_uptr_value;
#else
typedef embed_u32 embed_atomic_uptr_value;
#endif
```

The plain integer [`embed_atomic_uptr`](#embed_atomic_uptr) holds and its operations take and answer.

The plain integer [`embed_atomic_uptr`](#embed_atomic_uptr) holds and its operations take and answer.

### `EmbedAtomicUptrLoadCfg` { #EmbedAtomicUptrLoadCfg }

```c
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
typedef EmbedAtomicU64LoadCfg EmbedAtomicUptrLoadCfg;
typedef EmbedAtomicU64StoreCfg EmbedAtomicUptrStoreCfg;
typedef EmbedAtomicU64ExchangeCfg EmbedAtomicUptrExchangeCfg;
typedef EmbedAtomicU64SwapIfCfg EmbedAtomicUptrSwapIfCfg;
typedef EmbedAtomicU64Ns EmbedAtomicUptrNs;
#define embed_atomic_uptr_table embed_atomic_u64_table
#else
typedef EmbedAtomicU32LoadCfg EmbedAtomicUptrLoadCfg;
typedef EmbedAtomicU32StoreCfg EmbedAtomicUptrStoreCfg;
typedef EmbedAtomicU32ExchangeCfg EmbedAtomicUptrExchangeCfg;
typedef EmbedAtomicU32SwapIfCfg EmbedAtomicUptrSwapIfCfg;
typedef EmbedAtomicU32Ns EmbedAtomicUptrNs;
#define embed_atomic_uptr_table embed_atomic_u32_table
#endif
```

The Cfg of each operation of [`embed_atomic_uptr`](#embed_atomic_uptr), and its table: those of [`embed_atomic_u64`](#embed_atomic_u64).

The Cfg of each operation of [`embed_atomic_uptr`](#embed_atomic_uptr), and its table: those of [`embed_atomic_u32`](#embed_atomic_u32).

## Macros

### `EMBED_ATOMIC_HAS_64` { #EMBED_ATOMIC_HAS_64 }

```c
#if (EMBED_ATOMIC_ARM == 4) && !defined(_M_X64) && !defined(_M_ARM64)
#define EMBED_ATOMIC_HAS_64 0
#else
#define EMBED_ATOMIC_HAS_64 1
#endif
```

1 where the arm has a 64-bit atomic word, and 0 where it has none. The MSVC arm has none on a 32-bit target, because MSVC gives the 64-bit interlocked intrinsics only to its 64-bit targets.

> **Note:** Both arms define this macro. Test it with `#if`. Where it is 0, the header defines no 64-bit word and no operation on one, as it defines no 16-bit word. The word, the size and the pointer of such a target are 32 bits.

### `EMBED_ATOMIC_INIT` { #EMBED_ATOMIC_INIT }

```c
#define EMBED_ATOMIC_INIT(value_) {(value_)}
```

An atomic word that holds `value_`, for the declaration that makes one.

- `[in]`: `value_` The word's first value.

### `EMBED_ATOMIC_LOAD` { #EMBED_ATOMIC_LOAD }

```c
#if EMBED_ATOMIC_ARM == 1
#define EMBED_ATOMIC_LOAD(word_) ((word_)->value)
#define EMBED_ATOMIC_FENCE() __threadfence()
#define EMBED_ATOMIC_ORDERED() __threadfence()
#define EMBED_ATOMIC_STORE(word_, value_) ((word_)->value = (value_))
#define EMBED_ATOMIC_EXCHANGE_32(word_, value_) atomicExch((unsigned int *)&(word_)->value, (value_))
#define EMBED_ATOMIC_EXCHANGE_64(word_, value_) atomicExch((unsigned long long *)&(word_)->value, (value_))
#define EMBED_ATOMIC_SWAP_IF_32(word_, expected_, value_) \
    atomicCAS((unsigned int *)&(word_)->value, (expected_), (value_))
#define EMBED_ATOMIC_SWAP_IF_64(word_, expected_, value_) \
    atomicCAS((unsigned long long *)&(word_)->value, (expected_), (value_))
#define EMBED_ATOMIC_ADD_32(word_, value_) atomicAdd((unsigned int *)&(word_)->value, (value_))
#define EMBED_ATOMIC_ADD_64(word_, value_) atomicAdd((unsigned long long *)&(word_)->value, (value_))
#define EMBED_ATOMIC_OR_32(word_, value_) atomicOr((unsigned int *)&(word_)->value, (value_))
#define EMBED_ATOMIC_OR_64(word_, value_) atomicOr((unsigned long long *)&(word_)->value, (value_))
#define EMBED_ATOMIC_AND_32(word_, value_) atomicAnd((unsigned int *)&(word_)->value, (value_))
#define EMBED_ATOMIC_AND_64(word_, value_) atomicAnd((unsigned long long *)&(word_)->value, (value_))
#elif EMBED_ATOMIC_ARM == 2
#define EMBED_ATOMIC_LOAD(word_) atomic_load_explicit(&(word_)->value, memory_order_acquire)
#define EMBED_ATOMIC_FENCE() ((void)0)
#define EMBED_ATOMIC_ORDERED() atomic_thread_fence(memory_order_seq_cst)
#define EMBED_ATOMIC_STORE(word_, value_) atomic_store_explicit(&(word_)->value, (value_), memory_order_release)
#define EMBED_ATOMIC_EXCHANGE_32(word_, value_) \
    atomic_exchange_explicit(&(word_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_EXCHANGE_64(word_, value_) EMBED_ATOMIC_EXCHANGE_32(word_, value_)
#define EMBED_ATOMIC_ADD_32(word_, value_) atomic_fetch_add_explicit(&(word_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_ADD_64(word_, value_) EMBED_ATOMIC_ADD_32(word_, value_)
#define EMBED_ATOMIC_OR_32(word_, value_) atomic_fetch_or_explicit(&(word_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_OR_64(word_, value_) EMBED_ATOMIC_OR_32(word_, value_)
#define EMBED_ATOMIC_AND_32(word_, value_) atomic_fetch_and_explicit(&(word_)->value, (value_), memory_order_acq_rel)
#define EMBED_ATOMIC_AND_64(word_, value_) EMBED_ATOMIC_AND_32(word_, value_)
#elif EMBED_ATOMIC_ARM == 3
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
#define EMBED_ATOMIC_LOAD(word_) ((word_)->value)
#define EMBED_ATOMIC_FENCE() _ReadWriteBarrier()
#define EMBED_ATOMIC_ORDERED() \
    do \
    { \
        volatile long full_barrier_ = 0; \
        (void)_InterlockedExchange(&full_barrier_, 0); \
    } while (0)
#define EMBED_ATOMIC_STORE(word_, value_) ((word_)->value = (value_))
#define EMBED_ATOMIC_EXCHANGE_32(word_, value_) \
    ((embed_u32)_InterlockedExchange((volatile long *)&(word_)->value, (long)(value_)))
#define EMBED_ATOMIC_SWAP_IF_32(word_, expected_, value_) \
    ((embed_u32)_InterlockedCompareExchange((volatile long *)&(word_)->value, (long)(value_), (long)(expected_)))
#define EMBED_ATOMIC_ADD_32(word_, value_) \
    ((embed_u32)_InterlockedExchangeAdd((volatile long *)&(word_)->value, (long)(value_)))
#define EMBED_ATOMIC_OR_32(word_, value_) ((embed_u32)_InterlockedOr((volatile long *)&(word_)->value, (long)(value_)))
#define EMBED_ATOMIC_AND_32(word_, value_) \
    ((embed_u32)_InterlockedAnd((volatile long *)&(word_)->value, (long)(value_)))
#if EMBED_ATOMIC_HAS_64
#define EMBED_ATOMIC_EXCHANGE_64(word_, value_) \
    ((embed_u64)_InterlockedExchange64((volatile __int64 *)&(word_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_SWAP_IF_64(word_, expected_, value_) \
    ((embed_u64)_InterlockedCompareExchange64((volatile __int64 *)&(word_)->value, (__int64)(value_), \
                                              (__int64)(expected_)))
#define EMBED_ATOMIC_ADD_64(word_, value_) \
    ((embed_u64)_InterlockedExchangeAdd64((volatile __int64 *)&(word_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_OR_64(word_, value_) \
    ((embed_u64)_InterlockedOr64((volatile __int64 *)&(word_)->value, (__int64)(value_)))
#define EMBED_ATOMIC_AND_64(word_, value_) \
    ((embed_u64)_InterlockedAnd64((volatile __int64 *)&(word_)->value, (__int64)(value_)))
#endif
#endif
```

The primitives of the device arm, on the device atomics of CUDA.

> **Note:** The load is a volatile read, and [`EMBED_ATOMIC_FENCE`](#EMBED_ATOMIC_LOAD) after it keeps each later read after the load. The store is a volatile write after the fence. Each read-modify-write is the atomic of its width, and each answers the word before it.

> **Note:** The device atomics take unsigned int for 32 bits and unsigned long long for 64. The casts read the same bits as the unsigned words that they are.

The primitives of the C11 arm, on the explicit atomics of stdatomic.h.

> **Note:** The load has acquire ordering, the store release ordering, and each read-modify-write both. The orders are in the calls, so [`EMBED_ATOMIC_FENCE`](#EMBED_ATOMIC_LOAD) is empty. The generic calls take either width, so each 64-bit primitive is the 32-bit one.

The primitives of the builtin arm, on the `__atomic` builtins of GCC and clang.

> **Note:** The orders are those of the C11 arm, and they are in the calls, so [`EMBED_ATOMIC_FENCE`](#EMBED_ATOMIC_LOAD) is empty. The builtins take either width, so each 64-bit primitive is the 32-bit one.

The primitives of the MSVC arm, on the interlocked intrinsics.

> **Note:** MSVC's intrinsics take a long for 32 bits and an `__int64` for 64. The casts read the same bits as the unsigned words that they are.

> **Note:** A plain volatile read and write of a 32-bit word are one step each on every target of MSVC. Those of a 64-bit word are one step on its 64-bit targets. The fences give them their ordering.

### `EMBED_ATOMIC_OPERATIONS` { #EMBED_ATOMIC_OPERATIONS }

```c
#define EMBED_ATOMIC_OPERATIONS(name_, Width_, type_, exchange_, swap_if_, add_, or_, and_) \
    /** @brief The parameters of the load. */ \
    typedef struct \
    { \
        const embed_atomic_##name_ *const word; /**< The word [BORROWS]. */ \
    } EmbedAtomic##Width_##LoadCfg; \
    /** @brief The parameters of an operation that writes value into the word or combines it with the word. */ \
    typedef struct \
    { \
        embed_atomic_##name_ *const word; /**< The word [BORROWS]. */ \
        const type_ value;                /**< The value of the operation. */ \
    } EmbedAtomic##Width_##ValueCfg; \
    /** @brief The parameters of the store. */ \
    typedef EmbedAtomic##Width_##ValueCfg EmbedAtomic##Width_##StoreCfg; \
    /** @brief The parameters of the exchange. */ \
    typedef EmbedAtomic##Width_##ValueCfg EmbedAtomic##Width_##ExchangeCfg; \
    /** @brief The parameters of the fetch-add. */ \
    typedef EmbedAtomic##Width_##ValueCfg EmbedAtomic##Width_##FetchAddCfg; \
    /** @brief The parameters of the fetch-subtract. */ \
    typedef EmbedAtomic##Width_##ValueCfg EmbedAtomic##Width_##FetchSubCfg; \
    /** @brief The parameters of the fetch-or. */ \
    typedef EmbedAtomic##Width_##ValueCfg EmbedAtomic##Width_##FetchOrCfg; \
    /** @brief The parameters of the fetch-and. */ \
    typedef EmbedAtomic##Width_##ValueCfg EmbedAtomic##Width_##FetchAndCfg; \
    /** @brief The parameters of the raise. */ \
    typedef EmbedAtomic##Width_##ValueCfg EmbedAtomic##Width_##RaiseCfg; \
    /** @brief The parameters of the compare-and-swap. */ \
    typedef struct \
    { \
        embed_atomic_##name_ *const word; /**< The word [BORROWS]. */ \
        type_ *const expected;            /**< The word that the caller expects, and then the word found [BORROWS]. */ \
        const type_ value;                /**< The word that the swap writes. */ \
    } EmbedAtomic##Width_##SwapIfCfg; \
    /** @brief The word, with acquire ordering: no later read moves ahead of it. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_load(const EmbedAtomic##Width_##LoadCfg *args) \
    { \
        const type_ found = EMBED_ATOMIC_LOAD(args->word); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief The word set to value, with release ordering: every earlier write is out before it. */ \
    EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_##name_##_store(const EmbedAtomic##Width_##StoreCfg *args) \
    { \
        EMBED_ATOMIC_FENCE(); \
        EMBED_ATOMIC_STORE(args->word, args->value); \
    } \
    /** @brief The word set to value, and the word before. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_exchange(const EmbedAtomic##Width_##ExchangeCfg *args) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const type_ found = exchange_(args->word, args->value); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief The word set to value where it holds *expected, else *expected set to what it holds. */ \
    EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_##name_##_swap_if( \
        const EmbedAtomic##Width_##SwapIfCfg *args) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const embed_bool swapped = swap_if_(args->word, args->expected, args->value); \
        EMBED_ATOMIC_FENCE(); \
        return swapped; \
    } \
    /** @brief value added to the word, with wrap around, and the word before the add. \
     *  @warning A sum past the max of type_ wraps modulo the width of the word, and nothing reports it. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_add( \
        const EmbedAtomic##Width_##FetchAddCfg *args) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const type_ found = add_(args->word, args->value); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief value taken from the word, with wrap around, and the word before the subtract. \
     *  @warning A value above the word wraps modulo the width of the word, and nothing reports it. \
     *  @note The add of the two's complement is the subtract, because the add of each arm wraps. The cast holds \
     *        the complement at the width of type_. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_sub( \
        const EmbedAtomic##Width_##FetchSubCfg *args) \
    { \
        return EMBED_CALL(embed_atomic_##name_##_fetch_add, EmbedAtomic##Width_##FetchAddCfg, args->word, \
                          (type_)(0u - args->value)); \
    } \
    /** @brief The word ored with value, and the word before the or. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_or(const EmbedAtomic##Width_##FetchOrCfg *args) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const type_ found = or_(args->word, args->value); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief The word anded with value, and the word before the and. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_fetch_and( \
        const EmbedAtomic##Width_##FetchAndCfg *args) \
    { \
        EMBED_ATOMIC_FENCE(); \
        const type_ found = and_(args->word, args->value); \
        EMBED_ATOMIC_FENCE(); \
        return found; \
    } \
    /** @brief The word raised to value where value is above it, by a swap, and the word before. \
     *  @note A swap that fails puts the word that another context wrote in found, and the test runs again on it. */ \
    EMBED_HOST_DEVICE EMBED_INLINE type_ embed_atomic_##name_##_raise(const EmbedAtomic##Width_##RaiseCfg *args) \
    { \
        type_ found = EMBED_CALL(embed_atomic_##name_##_load, EmbedAtomic##Width_##LoadCfg, args->word); \
        embed_bool raised = EMBED_FALSE; \
        while ((found < args->value) && !raised) \
        { \
            raised = EMBED_CALL(embed_atomic_##name_##_swap_if, EmbedAtomic##Width_##SwapIfCfg, args->word, &found, \
                                args->value); \
        } \
        return found; \
    } \
    /** @brief The dispatch table of the operations of one width, in the order of the header. */ \
    typedef struct \
    { \
        type_ (*load)(const EmbedAtomic##Width_##LoadCfg *args);           /**< The load. */ \
        void (*store)(const EmbedAtomic##Width_##StoreCfg *args);          /**< The store. */ \
        type_ (*exchange)(const EmbedAtomic##Width_##ExchangeCfg *args);   /**< The exchange. */ \
        embed_bool (*swap_if)(const EmbedAtomic##Width_##SwapIfCfg *args); /**< The compare-and-swap. */ \
        type_ (*fetch_add)(const EmbedAtomic##Width_##FetchAddCfg *args);  /**< The fetch-add. */ \
        type_ (*fetch_sub)(const EmbedAtomic##Width_##FetchSubCfg *args);  /**< The fetch-subtract. */ \
        type_ (*fetch_or)(const EmbedAtomic##Width_##FetchOrCfg *args);    /**< The fetch-or. */ \
        type_ (*fetch_and)(const EmbedAtomic##Width_##FetchAndCfg *args);  /**< The fetch-and. */ \
        type_ (*raise)(const EmbedAtomic##Width_##RaiseCfg *args);         /**< The raise. */ \
    } EmbedAtomic##Width_##Ns; \
    EMBED_TABLE_LAYOUT(EmbedAtomic##Width_##Ns, load, store, exchange, swap_if, fetch_add, fetch_sub, fetch_or, \
                       fetch_and, raise);
```

Defines the operations over one atomic width, the Cfg of each and the dispatch table type. `name_` is the suffix, `Width_` the same suffix in the case of a type, and `type_` the word. The other five parameters are the primitives of the arm for that width.

> **Note:** One text for both widths and each arm. A width or an arm cannot drift from the others.

> **Note:** The compare-and-swap uses the primitive of the arm where the arm has one, and the exchange where it does not. The C11 arm and the builtin arm use the generic call below.

> **Note:** [`EMBED_ATOMIC_TABLE`](#EMBED_ATOMIC_TABLE) defines the table object of the width, apart from this macro, because the device pass of CUDA has no table.

### `EMBED_ATOMIC_TABLE` { #EMBED_ATOMIC_TABLE }

```c
#define EMBED_ATOMIC_TABLE(name_, Width_) \
    static const EmbedAtomic##Width_##Ns embed_atomic_##name_##_table = { \
        embed_atomic_##name_##_load,     embed_atomic_##name_##_store,     embed_atomic_##name_##_exchange, \
        embed_atomic_##name_##_swap_if,  embed_atomic_##name_##_fetch_add, embed_atomic_##name_##_fetch_sub, \
        embed_atomic_##name_##_fetch_or, embed_atomic_##name_##_fetch_and, embed_atomic_##name_##_raise}
```

Defines the dispatch table of one atomic width. A call through it folds to a direct call under the optimizer and LTO.

- `[in]`: `name_` The suffix of the width.
- `[in]`: `Width_` The same suffix in the case of a type.

### `EMBED_ATOMIC_CAS_32` { #EMBED_ATOMIC_CAS_32 }

```c
#if (EMBED_ATOMIC_ARM == 1) || (EMBED_ATOMIC_ARM == 4)
#define EMBED_ATOMIC_CAS_32(word_, expected_, value_) \
    EMBED_CALL(embed_atomic_cas_u32, EmbedAtomicCasU32Cfg, (word_), (expected_), (value_))
#elif EMBED_ATOMIC_ARM == 2
#define EMBED_ATOMIC_CAS_32(word_, expected_, value_) \
    ((embed_bool)atomic_compare_exchange_strong_explicit(&(word_)->value, (expected_), (value_), memory_order_acq_rel, \
                                                         memory_order_acquire))
#else
#define EMBED_ATOMIC_CAS_32(word_, expected_, value_) \
    ((embed_bool)__atomic_compare_exchange_n(&(word_)->value, (expected_), (value_), 0, __ATOMIC_ACQ_REL, \
                                             __ATOMIC_ACQUIRE))
#endif
```

The compare-and-swap on an arm whose primitive gives the word that it found.

The swap occurred where that word is the expected word. Otherwise, \*expected takes the word that the primitive found.

The compare-and-swap of the C11 arm, the strong form, with acquire and release ordering where it swaps and acquire ordering where it does not. Where it does not swap, \*`expected_` takes the word that it found.

> **Note:** The generic call takes either width, so the 64-bit form is the 32-bit one. The cast converts the `_Bool` of the call to [`embed_bool`](embed_types.md#embed_bool).

The compare-and-swap of the builtin arm, the strong form, with the orders of the C11 arm. Where it does not swap, \*`expected_` takes the word that it found.

> **Note:** The builtin takes either width, so the 64-bit form is the 32-bit one. The cast converts the bool of the builtin to [`embed_bool`](embed_types.md#embed_bool).

### `EMBED_ATOMIC_CAS_64` { #EMBED_ATOMIC_CAS_64 }

```c
#if (EMBED_ATOMIC_ARM == 1) || (EMBED_ATOMIC_ARM == 4)
#if EMBED_ATOMIC_HAS_64
#define EMBED_ATOMIC_CAS_64(word_, expected_, value_) \
    EMBED_CALL(embed_atomic_cas_u64, EmbedAtomicCasU64Cfg, (word_), (expected_), (value_))
#endif
#elif EMBED_ATOMIC_ARM == 2
#define EMBED_ATOMIC_CAS_64(word_, expected_, value_) EMBED_ATOMIC_CAS_32(word_, expected_, value_)
#else
#define EMBED_ATOMIC_CAS_64(word_, expected_, value_) EMBED_ATOMIC_CAS_32(word_, expected_, value_)
#endif
```

The compare-and-swap of the 64-bit word on this arm, as for 32 bits.

### `embed_atomic_word_load` { #embed_atomic_word_load }

```c
#if EMBED_WORD_BITS == 64
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

The operations of [`embed_atomic_word`](#embed_atomic_word), each the operation of [`embed_atomic_u64`](#embed_atomic_u64) with the same suffix.

The operations of [`embed_atomic_word`](#embed_atomic_word), each the operation of [`embed_atomic_u32`](#embed_atomic_u32) with the same suffix.

### `embed_atomic_usize_load` { #embed_atomic_usize_load }

```c
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFu
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

The operations of [`embed_atomic_usize`](#embed_atomic_usize), each the operation of [`embed_atomic_u64`](#embed_atomic_u64) with the same suffix.

The operations of [`embed_atomic_usize`](#embed_atomic_usize), each the operation of [`embed_atomic_u32`](#embed_atomic_u32) with the same suffix.

### `embed_atomic_uptr_load` { #embed_atomic_uptr_load }

```c
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define embed_atomic_uptr_load embed_atomic_u64_load
#define embed_atomic_uptr_store embed_atomic_u64_store
#define embed_atomic_uptr_exchange embed_atomic_u64_exchange
#define embed_atomic_uptr_swap_if embed_atomic_u64_swap_if
#else
#define embed_atomic_uptr_load embed_atomic_u32_load
#define embed_atomic_uptr_store embed_atomic_u32_store
#define embed_atomic_uptr_exchange embed_atomic_u32_exchange
#define embed_atomic_uptr_swap_if embed_atomic_u32_swap_if
#endif
```

The operations of [`embed_atomic_uptr`](#embed_atomic_uptr), each the operation of [`embed_atomic_u64`](#embed_atomic_u64) with the same suffix.

The operations of [`embed_atomic_uptr`](#embed_atomic_uptr), each the operation of [`embed_atomic_u32`](#embed_atomic_u32) with the same suffix.

## Functions

### `embed_atomic_cas_u32` { #embed_atomic_cas_u32 }

```c
#if (EMBED_ATOMIC_ARM == 1) || (EMBED_ATOMIC_ARM == 4)
EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_cas_u32(const EmbedAtomicCasU32Cfg *args);
#endif
```

The 32-bit compare-and-swap of an arm whose primitive answers the word that it found.

- `[in]`: args The parameters, as EmbedAtomicCasU32Cfg holds them \[BORROWS\].

\*\*Returns:\*\* 1 where the swap occurred, 0 where it did not.

### `embed_atomic_cas_u64` { #embed_atomic_cas_u64 }

```c
#if (EMBED_ATOMIC_ARM == 1) || (EMBED_ATOMIC_ARM == 4)
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_cas_u64(const EmbedAtomicCasU64Cfg *args);
#endif
#endif
```

The 64-bit compare-and-swap of an arm whose primitive answers the word that it found.

- `[in]`: args The parameters, as EmbedAtomicCasU64Cfg holds them \[BORROWS\].

\*\*Returns:\*\* 1 where the swap occurred, 0 where it did not.

### `embed_atomic_u32_load` { #embed_atomic_u32_load }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_load(const EmbedAtomicU32LoadCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_load(const EmbedAtomicU64LoadCfg *args);
#endif
```

The word, with acquire ordering: no later read moves ahead of it.

### `embed_atomic_u32_store` { #embed_atomic_u32_store }

```c
EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_u32_store(const EmbedAtomicU32StoreCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_u64_store(const EmbedAtomicU64StoreCfg *args);
#endif
```

The word set to value, with release ordering: every earlier write is out before it.

### `embed_atomic_u32_exchange` { #embed_atomic_u32_exchange }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_exchange(const EmbedAtomicU32ExchangeCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_exchange(const EmbedAtomicU64ExchangeCfg *args);
#endif
```

The word set to value, and the word before.

### `embed_atomic_u32_swap_if` { #embed_atomic_u32_swap_if }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_u32_swap_if( const EmbedAtomicU32SwapIfCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_bool embed_atomic_u64_swap_if( const EmbedAtomicU64SwapIfCfg *args);
#endif
```

The word set to value where it holds \*expected, else \*expected set to what it holds.

### `embed_atomic_u32_fetch_add` { #embed_atomic_u32_fetch_add }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_fetch_add( const EmbedAtomicU32FetchAddCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_fetch_add( const EmbedAtomicU64FetchAddCfg *args);
#endif
```

value added to the word, with wrap around, and the word before the add.

> _**Warning:**_ A sum past the max of `type_` wraps modulo the width of the word, and nothing reports it.

### `embed_atomic_u32_fetch_sub` { #embed_atomic_u32_fetch_sub }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_fetch_sub( const EmbedAtomicU32FetchSubCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_fetch_sub( const EmbedAtomicU64FetchSubCfg *args);
#endif
```

value taken from the word, with wrap around, and the word before the subtract.

> _**Warning:**_ A value above the word wraps modulo the width of the word, and nothing reports it.

> **Note:** The add of the two's complement is the subtract, because the add of each arm wraps. The cast holds the complement at the width of `type_`.

### `embed_atomic_u32_fetch_or` { #embed_atomic_u32_fetch_or }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_fetch_or(const EmbedAtomicU32FetchOrCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_fetch_or(const EmbedAtomicU64FetchOrCfg *args);
#endif
```

The word ored with value, and the word before the or.

### `embed_atomic_u32_fetch_and` { #embed_atomic_u32_fetch_and }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_fetch_and( const EmbedAtomicU32FetchAndCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_fetch_and( const EmbedAtomicU64FetchAndCfg *args);
#endif
```

The word anded with value, and the word before the and.

### `embed_atomic_u32_raise` { #embed_atomic_u32_raise }

```c
EMBED_HOST_DEVICE EMBED_INLINE embed_u32 embed_atomic_u32_raise(const EmbedAtomicU32RaiseCfg *args);
#if EMBED_ATOMIC_HAS_64
EMBED_HOST_DEVICE EMBED_INLINE embed_u64 embed_atomic_u64_raise(const EmbedAtomicU64RaiseCfg *args);
#endif
```

The word raised to value where value is above it, by a swap, and the word before.

> **Note:** A swap that fails puts the word that another context wrote in found, and the test runs again on it.

### `embed_atomic_ordered` { #embed_atomic_ordered }

```c
EMBED_HOST_DEVICE EMBED_INLINE void embed_atomic_ordered(void);
```

Orders this context's earlier writes ahead of its later reads, on every arm.

> **Note:** The operations above order a read before later reads, and a write after earlier writes. None of them orders a write before a later read. Two contexts that each write their own word and read the word of the other need this call between the two. Without it, each can read the word of the other from before its write.
