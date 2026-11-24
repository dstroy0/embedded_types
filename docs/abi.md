# The ABI

This page states the interface of the eight headers. It covers the types, the host hooks, the stamp, and each value that follows the width of the word. The API pages hold the contract of each name.

## The word and the atomic words

| name                                                       | width                                                       |
| ---------------------------------------------------------- | ----------------------------------------------------------- |
| [`embed_word`](api/embed_types.md#embed_word)              | `EMBED_WORD_BITS`: 64, 32 or 16                             |
| [`embed_index`](api/embed_types.md#embed_index)            | `EMBED_INDEX_BITS`: 32, or 16 on a word of 16 bits          |
| [`embed_atomic_u32`](api/embed_atomic.md#embed_atomic_u32) | 32 bits, on every arm                                       |
| [`embed_atomic_u64`](api/embed_atomic.md#embed_atomic_u64) | 64 bits, where `EMBED_ATOMIC_HAS_64` is 1                   |
| [`embed_atomic_word`](api/embed_atomic.md#embed_atomic_word) | 64 bits on a word of 64 bits, and 32 bits below it        |
| [`embed_atomic_usize`](api/embed_atomic.md#embed_atomic_usize) | the width of `size_t`                                   |
| [`embed_atomic_uptr`](api/embed_atomic.md#embed_atomic_uptr) | the width of `uintptr_t`                                  |
| [`embed_stored_u64`](api/embed_types.md#embed_stored_u64)  | 64 bits: one `embed_u64`, or two `embed_u32` halves on an embedded target |

No arm has an atomic word of 16 bits. So a word of 16 bits has an `embed_atomic_word` of 32 bits, and its lane words are 32 bits.

[`embed_stored_u64_get`](api/embed_types.md#embed_stored_u64_get) and [`embed_stored_u64_set`](api/embed_types.md#embed_stored_u64_set) read and write a count in its stored form. On an embedded target each half is one plain load or store of 32 bits, and a set writes the upper half only where it changes.

[`EMBED_ATOMIC_HAS_64`](api/embed_atomic.md#EMBED_ATOMIC_HAS_64) is 0 under MSVC on a 32-bit target, and 1 on every other target. MSVC gives its 64-bit interlocked intrinsics only to its 64-bit targets. Where the macro is 0, the header defines no 64-bit atomic word. Each use of `embed_atomic_u64` then stops the compile of its file, and the error names the cause. A word, a `size_t` or a `uintptr_t` of 64 bits on such a target stops the build at an `#error`.

## The call

Each function takes one parameter, a pointer to a `const` Cfg of its own. The Cfg is a struct of the parameters of that function, and each member is `const`. A call must give each member, and no pointer member takes zero. A C caller builds the Cfg as a compound literal at the call with `EMBED_CALL`:

```c
const unsigned int place = EMBED_CALL(embed_turnstile_table.enter, EmbedTurnstileEnterCfg, .set = &set,
                                      .address = (embed_atomic_uptr_value)(uintptr_t)&worker);
```

Each header has a dispatch table of its public functions, a `static const` struct of function pointers. `EMBED_TABLE_LAYOUT` asserts the slots of each table. The optimizer folds a call through a table to a direct call. Link-time optimization does the same across translation units. This build turns it on with `EMBEDDED_TYPES_LTO`, and a consumer sets `-flto` or `CMAKE_INTERPROCEDURAL_OPTIMIZATION` for its own build.

The tables of the atomics are `embed_atomic_u32_table` and `embed_atomic_u64_table`. The word, the size and the pointer have aliases of them, as `embed_atomic_word_table` is. A function with no parameter, such as `embed_clock_ns`, takes no Cfg.

In C++, `EMBED_CALL` builds a braced temporary. Before C++20, its initializers go in the order of the members, with no designators. Under MSVC, a C++ unit needs `/Zc:preprocessor` for `EMBED_TABLE_LAYOUT`. A CUDA kernel has no table, and calls each function by its name.

## The host hooks

The turnstile calls three functions, and a consumer with a store for each thread calls a fourth. The host defines the four:

```c
typedef struct
{
    embed_atomic_word *const word;
    const embed_atomic_word_value old_value;
    const embed_word nanoseconds;
} EmbedWordWaitCfg;

typedef struct
{
    embed_atomic_word *const word;
} EmbedWordWakeCfg;

int embed_word_wait(const EmbedWordWaitCfg *args);
void embed_word_wake(const EmbedWordWakeCfg *args);
embed_word embed_clock_ns(void);
embed_u32 embed_thread_place(void);
```

[`embed_word_wait`](api/embed_turnstile.md#embed_word_wait) sleeps while `word` holds `old_value`, for `nanoseconds` at max. A limit of `EMBED_WAIT_INFINITE`, each bit of the word set, has no end. A limit of 0 answers at once. The call returns 1 where the word moved off `old_value`, and 0 where it did not.

The compare and the sleep are one step with respect to [`embed_word_wake`](api/embed_turnstile.md#embed_word_wake), and a wake ends each sleep on the word. That is the contract of the futex.

[`embed_clock_ns`](api/embed_turnstile.md#embed_clock_ns) gives the nanoseconds of the host modulo 2^`EMBED_WORD_BITS`. The clock rolls after about 4.29 s on a word of 32 bits, and after 65.5 µs on a word of 16 bits. The backoff takes the difference of two reads, and that difference holds across the roll.

[`embed_thread_place`](api/embed_turnstile.md#embed_thread_place) gives the place of the thread that calls: a count from 0 that the thread answers for its life, and that no other live thread answers. A consumer indexes its stores by the place modulo their count.

`test/support/host_wait.c` defines the four on the futex of Linux, on `WaitOnAddress` of Windows and on `__ulock_wait` of macOS. Elsewhere it sleeps for the limit. A build that defines none of the four fails at the link, and the error names them.

## The values that follow the word

| value                                              | 64-bit word | 32-bit word | 16-bit word     |
| -------------------------------------------------- | ----------- | ----------- | --------------- |
| `EMBED_TICKET_BITS`, the width of the ticket       | 32          | 12          | no ticket ring  |
| `EMBED_TICKET_EPOCH_BITS`, the width of the epoch  | 16          | 4           | no ticket ring  |
| `EMBED_TICKET_RING_SHIFT_MAX`                      | 31          | 11          | no ticket ring  |
| `EMBED_TURNSTILE_EXPONENT_MAX`                     | 20          | 20          | 5               |
| the longest wait of the backoff on a CPU host      | 2^30 ns     | 2^30 ns     | 2^15 ns         |
| the width of the generation in a lane word         | 61 bits     | 29 bits     | 29 bits         |

The ring has at max 2^`EMBED_TICKET_RING_SHIFT_MAX` slots. The generation rolls after 2^n counts, with n its width.

## The stamp

A stamp is one `embed_word`. The ticket is in the low `EMBED_TICKET_BITS` bits, the 16 flag bits are above it, and the epoch is in the high bits.

| field       | 64-bit word | 32-bit word |
| ----------- | ----------- | ----------- |
| the ticket  | bits 0 to 31  | bits 0 to 11  |
| the flags   | bits 32 to 47 | bits 12 to 27 |
| the epoch   | bits 48 to 63 | bits 28 to 31 |

The flag field holds these fields, from its bit 0 up:

| bits of the flag field | field                                        |
| ---------------------- | -------------------------------------------- |
| 0 to 4                 | granted, pushed, popped, replayed, released  |
| 5 to 7                 | the three-bit check of the job pointer       |
| 8 to 14                | the lane index of the pop                    |
| 15                     | the reserved bit, which no transition sets   |

A word of 16 bits has no room for the three fields, and `embed_ticket_ring.h` stops the build there.

Each transition of a slot takes an input stamp with its flag field clear. It answers in `found_stamp` the stamp that the compare found in the slot. [`embed_ticket_job_check`](api/embed_ticket_ring.md#embed_ticket_job_check) folds the job pointer at the width of `uintptr_t`. So the check holds on a pointer of 16, 32 or 64 bits.

## What a build can set

A build defines each of these ahead of the first header that it includes.

| macro                          | default                                        | what it sets                                 |
| ------------------------------ | ---------------------------------------------- | -------------------------------------------- |
| `EMBED_WORD_BITS`              | the width of `UINTPTR_MAX`                     | the word                                     |
| `EMBED_INDEX_BITS`             | 32, or 16 on a word of 16 bits                 | the index                                    |
| `EMBED_LANE_BITS`              | the widest vector register                     | the lane                                     |
| `EMBED_LANE_PORTABLE`          | 0                                              | 1 takes the portable arm of the lane on any target |
| `EMBED_BIG_ENDIAN`             | 1 where `__BYTE_ORDER__` is big-endian, else 0 | the byte order                               |
| `EMBED_FAST_UNALIGNED_LOAD`    | 1 where `__ARM_FEATURE_UNALIGNED` is defined, else 0 | whether one instruction loads a word from any address |
| `EMBED_NULL`                   | `((void *)0)`                                  | the null pointer of the platform             |
| `EMBED_NULL_ZERO_BITS`         | 1                                              | whether the null pointer is all zero bits    |
| `EMBED_CLOCK_HZ`               | 1000000, with a build warning                  | the ticks of the clock in one second         |
| `EMBED_CLOCK_SOURCE`           | `EMBED_CLOCK_OWN` where the architecture has a cycle counter, else `EMBED_CLOCK_CALLER`, with a build warning | the caller's clock, or a counter pinned to a core |
| `EMBED_CLOCK_CORE`             | 0, with a build warning                        | the core of the pinned counter, under `EMBED_CLOCK_OWN` alone |
| `EMBED_TARGET`                 | device on CUDA, host with a vector register, else embedded | the class of the target, which the defaults below read |
| `EMBED_TURNSTILE_LANES`        | 4 on an embedded target, else 64               | the lanes of a set, from 1 to 64             |
| `EMBED_TURNSTILE_UNIT_SHIFT`   | 10 on a host, else 0                           | the unit of the backoff, 2^shift ns, 10 at max |
| `EMBED_TURNSTILE_SPIN_PAUSE()` | the pause instruction where the compiler has one | the pause of a spin                        |
| `EMBED_TICKET_JOB_CHECK`       | 1                                              | 0 turns off the check of the job pointer     |
| `EMBED_INLINE`                 | `static inline` with `always_inline`           | the inline policy of each function           |

## The targets

| target                    | what builds                                       |
| ------------------------- | ------------------------------------------------- |
| a word of 64 bits         | each header                                       |
| a word of 32 bits         | each header                                       |
| a word of 16 bits         | each header but `embed_ticket_ring.h`             |
| MSVC on a 32-bit target   | each header, and no 64-bit atomic word            |

The Lean proofs under `theory/` hold the stamp and the flag matrix on the words of 32 and 64 bits. They hold the lane word on the atomic words of 32 and 64 bits, and the backoff on each word.
