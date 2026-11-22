<!-- tools/api_pages.py writes this page from include/embed_turnstile.h. Edit the header, then run the tool. -->

# embed_turnstile.h

Source: [`include/embed_turnstile.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_turnstile.h)

Turnstiles as a manifold of lanes, and the sleep on the address of a word that their waits use.

A set of turnstiles is a manifold of lanes. Each lane is one word on a cache line of its own. A worker enters at the lane that its address selects, and where that lane is busy it spills to the next. A writer swaps a published pointer, then drains: it waits until each pass that holds a lane at the drain ends. The manifold opens lanes under pressure and closes them as the pressure goes.

> **Note:** The lane word holds four fields, from bit 0 up: busy, closed, sleeper, and the generation. The busy bit shows that a pass or a contraction holds the lane. The closed bit shows a lane that a contraction closed. The sleeper bit shows that a thread sleeps on the word. The generation counts each pass end, each close and each reopen, and rolls in its bits.

> **Note:** Each change to a lane word is one atomic operation. An entry is a fetch-or of busy. It takes the lane where the old value has busy clear. Where the old value has busy, it changes nothing. A leave, a close, a release and a reopen are each one fetch-add, which changes the low 32 bits that the futex compares.

> **Note:** `top_open_lane` holds the count of open lanes less one, so a set in storage of zeros has one lane. Arrivals only read it. A growth and a contraction write it, each with one compare-and-swap. `queue_length` counts the workers that found every open lane busy. An entry on a free lane writes no word but the lane word.

> **Note:** The order of the passes is not the turnstile's. A consumer that needs an order keys it by the address of the worker and the worker's own sequence.

> **Note:** A backoff retains its start on the clock and its exponent in the caller's locals. No shared word holds a time. Inside the first unit, `2^EMBED_TURNSTILE_UNIT_SHIFT` ns, a step is a pause and no call. From one unit on, a step waits on the word for 2^([`EMBED_TURNSTILE_UNIT_SHIFT`](#EMBED_TURNSTILE_UNIT_SHIFT) + exponent) ns at max, and the exponent goes up to [`EMBED_TURNSTILE_EXPONENT_MAX`](#EMBED_TURNSTILE_EXPONENT_MAX_16). A wake or a moved word ends the wait first.

> **Note:** The host defines [`embed_word_wait`](#embed_word_wait), [`embed_word_wake`](#embed_word_wake), [`embed_clock_ns`](#embed_clock_ns) and [`embed_thread_place`](#embed_thread_place). A build that defines none of the four fails at the link, and the error names them.

> **Note:** theory/`embedded_types`/EmbeddedTypes/Manifold proves the lane word, the backoff, the open count, the queue, the drain and the sleeps.

> _**Warning:**_ The generation rolls after 2^(n - 3) counts, where n is the width of [`embed_atomic_word`](embed_atomic.md#embed_atomic_word): 64 on a word of 64 bits, and 32 below it. A drain that reads one lane twice, with exactly that count of passes between the two reads, waits for the next pass to end too.

## Types

### `EmbedTurnstileLane` { #EmbedTurnstileLane }

```c
typedef struct
{
    EMBED_ALIGN(64u) embed_atomic_word word; /**< The lane word. */
} EmbedTurnstileLane;
EMBED_STATIC_ASSERT(sizeof(EmbedTurnstileLane) == 64u, "a lane takes one cache line");
```

One lane: its word on a cache line of its own, so that no pass shares the line of another.

- `word`: The lane word.

### `EmbedTurnstiles` { #EmbedTurnstiles }

```c
typedef struct
{
    EmbedTurnstileLane lanes[EMBED_TURNSTILE_LANES];  /**< The lanes, those below the open count open. */
    EMBED_ALIGN(64u) embed_atomic_word top_open_lane; /**< The place of the top open lane. */
    EMBED_ALIGN(64u) embed_atomic_u32 queue_length;   /**< The workers that found every open lane busy. */
} EmbedTurnstiles;
```

A set of turnstiles, zeros at rest: one to each thing workers read while a writer releases its parts.

> **Note:** `top_open_lane` is the place of the top open lane, the count of open lanes less one. `queue_length` is the count of workers that found every open lane busy.

- `lanes`: The lanes, those below the open count open.
- `top_open_lane`: The place of the top open lane.
- `queue_length`: The workers that found every open lane busy.

### `EmbedTurnstileBackoff` { #EmbedTurnstileBackoff }

```c
typedef struct
{
    embed_word start_ns;   /**< The nanosecond that embed_clock_ns read at the start. */
    unsigned int exponent; /**< The exponent of the next wait, from 0 to EMBED_TURNSTILE_EXPONENT_MAX. */
} EmbedTurnstileBackoff;
```

A backoff's state, in the caller's locals: the clock at its start, and its exponent.

- `start_ns`: The nanosecond that [`embed_clock_ns`](#embed_clock_ns) read at the start.
- `exponent`: The exponent of the next wait, from 0 to [`EMBED_TURNSTILE_EXPONENT_MAX`](#EMBED_TURNSTILE_EXPONENT_MAX_16).

### `EmbedWordWaitCfg` { #EmbedWordWaitCfg }

```c
typedef struct
{
    embed_atomic_word *const word;           /**< The word slept on [BORROWS]. */
    const embed_atomic_word_value old_value; /**< The value of the word that the caller read last. */
    const embed_word nanoseconds; /**< The max the sleep lasts. 0 answers at once with no sleep and no system call. */
} EmbedWordWaitCfg;
```

The parameters of [`embed_word_wait`](#embed_word_wait).

- `word`: The word slept on \[BORROWS\].
- `old_value`: The value of the word that the caller read last.
- `nanoseconds`: The max the sleep lasts. 0 answers at once with no sleep and no system call.

### `EmbedWordWakeCfg` { #EmbedWordWakeCfg }

```c
typedef struct
{
    embed_atomic_word *const word; /**< The word [BORROWS]. */
} EmbedWordWakeCfg;
```

The parameters of [`embed_word_wake`](#embed_word_wake).

- `word`: The word \[BORROWS\].

### `EmbedTurnstileBackoffBeginCfg` { #EmbedTurnstileBackoffBeginCfg }

```c
typedef struct
{
    EmbedTurnstileBackoff *const backoff; /**< The backoff [BORROWS]. */
} EmbedTurnstileBackoffBeginCfg;
```

The parameters of [`embed_turnstile_backoff_begin`](#embed_turnstile_backoff_begin).

- `backoff`: The backoff \[BORROWS\].

### `EmbedTurnstileBackoffStepCfg` { #EmbedTurnstileBackoffStepCfg }

```c
typedef struct
{
    embed_atomic_word *const word;             /**< The word [BORROWS]. */
    const embed_atomic_word_value old_value;   /**< The value of the word that the caller read last. */
    const embed_atomic_word_value sleeper_bit; /**< The sleeper bit of the word. */
    EmbedTurnstileBackoff *const backoff;      /**< The backoff [BORROWS]. */
    const embed_bool sleep_allowed; /**< True where old_value lets the caller sleep, false for a pause alone. */
} EmbedTurnstileBackoffStepCfg;
```

The parameters of [`embed_turnstile_backoff_step`](#embed_turnstile_backoff_step).

- `word`: The word \[BORROWS\].
- `old_value`: The value of the word that the caller read last.
- `sleeper_bit`: The sleeper bit of the word.
- `backoff`: The backoff \[BORROWS\].
- `sleep_allowed`: True where `old_value` lets the caller sleep, false for a pause alone.

### `EmbedTurnstileOpenLaneCountCfg` { #EmbedTurnstileOpenLaneCountCfg }

```c
typedef struct
{
    const EmbedTurnstiles *const set; /**< The set [BORROWS]. */
} EmbedTurnstileOpenLaneCountCfg;
```

The parameters of [`embed_turnstile_open_lane_count`](#embed_turnstile_open_lane_count).

- `set`: The set \[BORROWS\].

### `EmbedTurnstileLaneAddCfg` { #EmbedTurnstileLaneAddCfg }

```c
typedef struct
{
    EmbedTurnstileLane *const lane;       /**< The lane [BORROWS]. */
    const embed_atomic_word_value addend; /**< The constant of the fetch-add. */
} EmbedTurnstileLaneAddCfg;
```

The parameters of [`embed_turnstile_lane_add`](#embed_turnstile_lane_add).

- `lane`: The lane \[BORROWS\].
- `addend`: The constant of the fetch-add.

### `EmbedTurnstileGrowCfg` { #EmbedTurnstileGrowCfg }

```c
typedef struct
{
    EmbedTurnstiles *const set;               /**< The set [BORROWS]. */
    const unsigned int open_lane_count;       /**< The lanes the caller read open. */
    embed_atomic_word_value *const held_word; /**< The word of lane open_lane_count where another thread holds it, and 0
                                                 in each other case [BORROWS]. */
} EmbedTurnstileGrowCfg;
```

The parameters of [`embed_turnstile_grow`](#embed_turnstile_grow).

- `set`: The set \[BORROWS\].
- `open_lane_count`: The lanes the caller read open.

### `EmbedTurnstileContractCfg` { #EmbedTurnstileContractCfg }

```c
typedef struct
{
    EmbedTurnstiles *const set;         /**< The set [BORROWS]. */
    const unsigned int open_lane_count; /**< The lanes the caller read open, above 1. */
} EmbedTurnstileContractCfg;
```

The parameters of [`embed_turnstile_contract`](#embed_turnstile_contract).

- `set`: The set \[BORROWS\].
- `open_lane_count`: The lanes the caller read open, above 1.

### `EmbedTurnstileSpillCfg` { #EmbedTurnstileSpillCfg }

```c
typedef struct
{
    EmbedTurnstiles *const set;         /**< The set [BORROWS]. */
    const unsigned int open_lane_count; /**< The lanes the caller read open. */
    const unsigned int start_lane;      /**< The start lane, below open_lane_count. */
} EmbedTurnstileSpillCfg;
```

The parameters of [`embed_turnstile_spill`](#embed_turnstile_spill).

- `set`: The set \[BORROWS\].
- `open_lane_count`: The lanes the caller read open.
- `start_lane`: The start lane, below `open_lane_count`.

### `EmbedTurnstileEnterCfg` { #EmbedTurnstileEnterCfg }

```c
typedef struct
{
    EmbedTurnstiles *const set; /**< The set [BORROWS]. */
    const embed_atomic_uptr_value
        address; /**< The worker's address: an immutable place, such as the worker's own state. */
} EmbedTurnstileEnterCfg;
```

The parameters of [`embed_turnstile_enter`](#embed_turnstile_enter).

- `set`: The set \[BORROWS\].
- `address`: The worker's address: an immutable place, such as the worker's own state.

### `EmbedTurnstileLeaveCfg` { #EmbedTurnstileLeaveCfg }

```c
typedef struct
{
    EmbedTurnstiles *const set; /**< The set [BORROWS]. */
    const unsigned int place;   /**< The lane's place, as embed_turnstile_enter answered it. */
} EmbedTurnstileLeaveCfg;
```

The parameters of [`embed_turnstile_leave`](#embed_turnstile_leave).

- `set`: The set \[BORROWS\].
- `place`: The lane's place, as [`embed_turnstile_enter`](#embed_turnstile_enter) answered it.

### `EmbedTurnstilesDrainCfg` { #EmbedTurnstilesDrainCfg }

```c
typedef struct
{
    EmbedTurnstiles *const set; /**< The set [BORROWS]. */
} EmbedTurnstilesDrainCfg;
```

The parameters of [`embed_turnstiles_drain`](#embed_turnstiles_drain).

- `set`: The set \[BORROWS\].

### `EmbedTurnstileNs` { #EmbedTurnstileNs }

```c
typedef struct
{
    int (*word_wait)(const EmbedWordWaitCfg *args);                   /**< embed_word_wait. */
    void (*word_wake)(const EmbedWordWakeCfg *args);                  /**< embed_word_wake. */
    embed_word (*clock_ns)(void);                                     /**< embed_clock_ns. */
    embed_u32 (*thread_place)(void);                                  /**< embed_thread_place. */
    void (*backoff_begin)(const EmbedTurnstileBackoffBeginCfg *args); /**< embed_turnstile_backoff_begin. */
    void (*backoff_step)(const EmbedTurnstileBackoffStepCfg *args);   /**< embed_turnstile_backoff_step. */
    unsigned int (*open_lane_count)(
        const EmbedTurnstileOpenLaneCountCfg *args);           /**< embed_turnstile_open_lane_count. */
    unsigned int (*enter)(const EmbedTurnstileEnterCfg *args); /**< embed_turnstile_enter. */
    void (*leave)(const EmbedTurnstileLeaveCfg *args);         /**< embed_turnstile_leave. */
    void (*drain)(const EmbedTurnstilesDrainCfg *args);        /**< embed_turnstiles_drain. */
} EmbedTurnstileNs;
```

The dispatch table of `embed_turnstile`.h: one member for each public function, in the order of the header.

> **Note:** [`EMBED_TABLE_LAYOUT`](embed_dispatch_layout.md#EMBED_TABLE_LAYOUT) asserts that the members sit at consecutive [`EMBED_FUNCTION_POINTER_BYTES`](embed_dispatch_layout.md#EMBED_FUNCTION_POINTER_BYTES) offsets.

- `args`: [`embed_word_wait`](#embed_word_wait).
- `void`: [`embed_clock_ns`](#embed_clock_ns).

## Macros

### `EMBED_TURNSTILE_LANES` { #EMBED_TURNSTILE_LANES }

```c
#if !defined(EMBED_TURNSTILE_LANES)
#if EMBED_TARGET == EMBED_TARGET_EMBEDDED
#define EMBED_TURNSTILE_LANES 4u
#else
#define EMBED_TURNSTILE_LANES 64u
#endif
#endif
EMBED_STATIC_ASSERT((EMBED_TURNSTILE_LANES >= 1u) && (EMBED_TURNSTILE_LANES <= 64u),
                    "EMBED_TURNSTILE_LANES must be from 1 to 64");
```

The lanes of a set: 64 on a host and on a device, and 4 on an embedded target. A build can set its own count before this header, from 1 to 64.

> **Note:** [`EMBED_TARGET`](embed_compiler_directives.md#EMBED_TARGET) selects the default. A lane takes one cache line, so 64 lanes take 4 KiB, and an embedded target cannot give that to each set.

> **Note:** The two passes of a CUDA build compile one set, so the device takes the count of the host.

### `EMBED_TURNSTILE_UNIT_SHIFT` { #EMBED_TURNSTILE_UNIT_SHIFT }

```c
#if !defined(EMBED_TURNSTILE_UNIT_SHIFT)
#if EMBED_TARGET == EMBED_TARGET_HOST
#define EMBED_TURNSTILE_UNIT_SHIFT 10u
#else
#define EMBED_TURNSTILE_UNIT_SHIFT 0u
#endif
#endif
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_UNIT_SHIFT <= 10u, "EMBED_TURNSTILE_UNIT_SHIFT must be 10 at max");
```

The shift of the backoff's unit, `2^shift` ns: 10 on a host, about 1.024 us, and 0 on a device and on an embedded target. A build can set its own before this header.

> **Note:** [`EMBED_TARGET`](embed_compiler_directives.md#EMBED_TARGET) selects the default. The value is a constant of the code and not of a layout, so the two passes of a CUDA build can differ.

### `EMBED_TURNSTILE_EXPONENT_MAX_16` { #EMBED_TURNSTILE_EXPONENT_MAX_16 }

```c
#define EMBED_TURNSTILE_EXPONENT_MAX_16 5u
#define EMBED_TURNSTILE_EXPONENT_MAX_32 20u
#define EMBED_TURNSTILE_EXPONENT_MAX_64 20u
#define EMBED_TURNSTILE_EXPONENT_MAX EMBED_CAT(EMBED_TURNSTILE_EXPONENT_MAX_, EMBED_WORD_BITS)
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_UNIT_SHIFT + EMBED_TURNSTILE_EXPONENT_MAX < EMBED_WORD_BITS,
                    "the longest wait of the backoff fits the word, below EMBED_WAIT_INFINITE");
```

The max the backoff's exponent reaches on each word width, and on this word. The longest wait is 2^([`EMBED_TURNSTILE_UNIT_SHIFT`](#EMBED_TURNSTILE_UNIT_SHIFT) + [`EMBED_TURNSTILE_EXPONENT_MAX`](#EMBED_TURNSTILE_EXPONENT_MAX_16)) ns, and that fits the word below [`EMBED_WAIT_INFINITE`](#EMBED_WAIT_INFINITE).

> **Note:** The ceiling is `2^30` ns, about 1.07 s, on a word of 32 or 64 bits, and `2^15` ns, about 32.8 us, on a word of 16 bits.

### `EMBED_TURNSTILE_BUSY_BIT` { #EMBED_TURNSTILE_BUSY_BIT }

```c
#define EMBED_TURNSTILE_BUSY_BIT ((embed_atomic_word_value)1u)
#define EMBED_TURNSTILE_CLOSED_BIT ((embed_atomic_word_value)2u)
#define EMBED_TURNSTILE_SLEEPER_BIT ((embed_atomic_word_value)4u)
#define EMBED_TURNSTILE_GENERATION_UNIT ((embed_atomic_word_value)8u)
#define EMBED_TURNSTILE_GENERATION_SHIFT 3u
```

The fields of a lane word, and the unit of its generation.

### `EMBED_WAIT_INFINITE` { #EMBED_WAIT_INFINITE }

```c
#define EMBED_WAIT_INFINITE ((embed_word) ~(embed_word)0u)
```

A sleep with no end, for [`embed_word_wait`](#embed_word_wait): each bit of the word set.

> **Note:** A word narrower than int promotes to int before the complement. The outer cast takes the complement back to the word. So a limit of 16 bits compares equal to it.

### `EMBED_TURNSTILE_SPIN_PAUSE` { #EMBED_TURNSTILE_SPIN_PAUSE }

```c
#if !defined(EMBED_TURNSTILE_SPIN_PAUSE)
#if EMBED_HAS_BUILTIN(__builtin_ia32_pause)
#define EMBED_TURNSTILE_SPIN_PAUSE() __builtin_ia32_pause()
#elif EMBED_HAS_BUILTIN(__builtin_arm_yield)
#define EMBED_TURNSTILE_SPIN_PAUSE() __builtin_arm_yield()
#else
#define EMBED_TURNSTILE_SPIN_PAUSE() EMBED_ATOMIC_ORDERED()
#endif
#endif
```

The processor's pause in a spin, a compiler barrier alone where the processor has none the compiler reaches.

> **Note:** A build that defines it before this header retains its own, as a model checker's build does with no processor.

### `EMBED_TURNSTILE_NO_LANE` { #EMBED_TURNSTILE_NO_LANE }

```c
#define EMBED_TURNSTILE_NO_LANE 0xffffffffu
```

The place of no lane: the answer of a growth or a spill that took none.

## Functions

### `embed_word_wait` { #embed_word_wait }

```c
int embed_word_wait(const EmbedWordWaitCfg *args);
```

A sleep on word while it holds `old_value`, at max nanoseconds, [`EMBED_WAIT_INFINITE`](#EMBED_WAIT_INFINITE) for no end, and none at all for 0.

- `[in]`: args The parameters, as EmbedWordWaitCfg holds them \[BORROWS\].

\*\*Returns:\*\* 1 where the word moved off `old_value`, 0 where it did not. A sleep is free to end early, so a sleeper tests what it waits for in a loop.

> **Note:** The host defines this function on its address wait. That is a futex on Linux, WaitOnAddress on Windows, and `__ulock_wait` on macOS. Where the system has none, it is a timed sleep.

### `embed_word_wake` { #embed_word_wake }

```c
void embed_word_wake(const EmbedWordWakeCfg *args);
```

Every thread asleep on word woken.

- `[in]`: args The parameters, as EmbedWordWakeCfg holds them \[BORROWS\].

> **Note:** The host defines this beside [`embed_word_wait`](#embed_word_wait).

### `embed_clock_ns` { #embed_clock_ns }

```c
embed_word embed_clock_ns(void);
```

The host's clock in nanoseconds, which the backoff measures its time on.

\*\*Returns:\*\* The nanoseconds modulo `2^EMBED_WORD_BITS`, from a start that the host selects.

> **Note:** The host defines this beside [`embed_word_wait`](#embed_word_wait).

> _**Warning:**_ The clock rolls at the width of the word: after about 4.29 s on a word of 32 bits, and 65.5 us on a word of 16 bits. The backoff takes the difference of two reads, and that difference is correct across the roll. A step one roll or more after its start reads the time modulo the roll. Where that is below one unit, the step pauses and makes no call.

### `embed_thread_place` { #embed_thread_place }

```c
embed_u32 embed_thread_place(void);
```

The place of the thread that calls: a count that names it among the threads of the program.

\*\*Returns:\*\* The place, from 0. The first thread that calls answers 0, the next answers 1, and each thread answers its own place for its life. No two live threads answer one place.

> **Note:** The host defines this beside [`embed_word_wait`](#embed_word_wait). A host with one thread answers 0.

> **Note:** A consumer that holds a store for each thread indexes the stores by this place, modulo the count of its stores. Two threads whose places meet modulo that count then share one store, which is correct and slower.

### `embed_turnstile_backoff_begin` { #embed_turnstile_backoff_begin }

```c
EMBED_INLINE void embed_turnstile_backoff_begin(const EmbedTurnstileBackoffBeginCfg *args);
```

The start of a backoff: the clock read, and the exponent at 0.

- `[in]`: args The parameters, as EmbedTurnstileBackoffBeginCfg holds them \[BORROWS\].

### `embed_turnstile_backoff_step` { #embed_turnstile_backoff_step }

```c
EMBED_INLINE void embed_turnstile_backoff_step(const EmbedTurnstileBackoffStepCfg *args);
```

One step of a backoff on word, which the caller last read as `old_value`.

The step does one pause. Where the time since the start is less than one unit, or where `sleep_allowed` is false, that is all. Otherwise the step sets `sleeper_bit` on the word, and the host waits on the word for the limit of the exponent. Then the exponent goes up by one, to [`EMBED_TURNSTILE_EXPONENT_MAX`](#EMBED_TURNSTILE_EXPONENT_MAX_16) at max.

- `[in]`: args The parameters, as EmbedTurnstileBackoffStepCfg holds them \[BORROWS\].

> **Note:** The sleep needs a value that a later change ends. A caller sleeps only on a value whose change comes with a wake, such as a lane word that has the busy bit.

### `embed_turnstile_open_lane_count` { #embed_turnstile_open_lane_count }

```c
EMBED_INLINE unsigned int embed_turnstile_open_lane_count(const EmbedTurnstileOpenLaneCountCfg *args);
```

The lanes a set has open.

- `[in]`: args The parameters, as EmbedTurnstileOpenLaneCountCfg holds them \[BORROWS\].

\*\*Returns:\*\* The lanes open, from 1 to [`EMBED_TURNSTILE_LANES`](#EMBED_TURNSTILE_LANES).

### `embed_turnstile_lane_add` { #embed_turnstile_lane_add }

```c
EMBED_INLINE void embed_turnstile_lane_add(const EmbedTurnstileLaneAddCfg *args);
```

A fetch-add on a lane word that the caller holds, and the wake of its sleepers where the old value had the sleeper bit.

- `[in]`: args The parameters, as EmbedTurnstileLaneAddCfg holds them \[BORROWS\].

### `embed_turnstile_grow` { #embed_turnstile_grow }

```c
EMBED_INLINE unsigned int embed_turnstile_grow(const EmbedTurnstileGrowCfg *args);
```

The growth from `open_lane_count` lanes to one more, which gives the caller lane `open_lane_count` as its pass.

The growth takes lane `open_lane_count` with one compare-and-swap on the lane word. From a free word it sets busy. From a closed word it clears closed and counts the takeover, and busy stays set. Then one compare-and-swap moves `top_open_lane` up by one.

Where that swap lands, the lane is the caller's pass. Where it fails, the growth gives the lane back as it was. A lane that had the closed bit gets a close, and a lane that was free gets a release.

- `[in]`: args The parameters, as EmbedTurnstileGrowCfg holds them \[BORROWS\].

\*\*Returns:\*\* `open_lane_count`, the lane the caller now holds, or [`EMBED_TURNSTILE_NO_LANE`](#EMBED_TURNSTILE_NO_LANE) where it holds none.

> **Note:** A lane that a pass, a contraction or another growth holds refuses the first compare-and-swap, and nothing changes. A contraction holds its lane from its claim to its close, so no growth takes a lane between a contraction's swap and its close. The caller sleeps on `held_word`: the end of that hold changes the word and wakes it.

### `embed_turnstile_contract` { #embed_turnstile_contract }

```c
EMBED_INLINE embed_bool embed_turnstile_contract(const EmbedTurnstileContractCfg *args);
```

The contraction from `open_lane_count` lanes to one less.

The contraction claims the top open lane as an entry does. Then one compare-and-swap moves `top_open_lane` down by one. Where the swap lands, the contraction closes the lane. Where the swap fails, the contraction releases the lane.

- `[in]`: args The parameters, as EmbedTurnstileContractCfg holds them \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) where the lane closed, [`EMBED_FALSE`](embed_types.md#EMBED_FALSE) where a pass held it or the swap failed.

> **Note:** A lane that a pass holds refuses the claim, and nothing changes. So each pass stays below the open count.

### `embed_turnstile_spill` { #embed_turnstile_spill }

```c
EMBED_INLINE unsigned int embed_turnstile_spill(const EmbedTurnstileSpillCfg *args);
```

The spill of an entry over the open lanes: a fetch-or of busy on each lane from `start_lane`, mod `open_lane_count`.

- `[in]`: args The parameters, as EmbedTurnstileSpillCfg holds them \[BORROWS\].

\*\*Returns:\*\* The lane the caller now holds, or [`EMBED_TURNSTILE_NO_LANE`](#EMBED_TURNSTILE_NO_LANE) where every lane was busy.

> **Note:** A lane with busy in the old value refuses the fetch-or, and nothing changes. The spill writes no word but the lane words.

### `embed_turnstile_enter` { #embed_turnstile_enter }

```c
EMBED_INLINE unsigned int embed_turnstile_enter(const EmbedTurnstileEnterCfg *args);
```

An entry by the worker at address: a lane taken, which the end of the pass takes back.

The entry reads the open count and starts at the lane of the address's cache line. A fetch-or of busy takes the lane where the old value has busy clear. Where the old value has busy, the fetch-or changes nothing, and the entry spills to the next lane. Where every open lane was busy, the worker joins the queue and grows the manifold, which gives it the new lane as its pass. Where it can not grow the manifold, it backs off on its start lane. It leaves the queue when it takes a lane.

- `[in]`: args The parameters, as EmbedTurnstileEnterCfg holds them \[BORROWS\].

\*\*Returns:\*\* The lane's place, which the end of the pass takes.

> **Note:** The pass reads the data of a published pointer only after the entry returns, and none of it after the leave.

### `embed_turnstile_leave` { #embed_turnstile_leave }

```c
EMBED_INLINE void embed_turnstile_leave(const EmbedTurnstileLeaveCfg *args);
```

The end of the pass through the lane at `place` of set.

One fetch-add clears busy and counts the pass in the generation. Where the old value had the sleeper bit, the end clears it and wakes the word. Where the lane is the top open lane, the end contracts the manifold. Each step closes the top lane while the queue is empty, more than one lane is open, and no pass holds the top lane.

- `[in]`: args The parameters, as EmbedTurnstileLeaveCfg holds them \[BORROWS\].

> **Note:** Only the end of a pass on the top lane contracts. So the end of a pass on a lower lane reads and writes no word but its own lane word and `top_open_lane`.

### `embed_turnstiles_drain` { #embed_turnstiles_drain }

```c
EMBED_INLINE void embed_turnstiles_drain(const EmbedTurnstilesDrainCfg *args);
```

Every pass through set begun before this call waited out.

- `[in]`: args The parameters, as EmbedTurnstilesDrainCfg holds them \[BORROWS\].

> **Note:** A writer swaps a published pointer and calls this function. Then it releases the data that the old pointer led to. A pass that starts after the fence of the drain reads the new pointer, and the drain does not wait for it.

> **Note:** The drain reads the open count after its fence, then each open lane. Each pass is below the open count, so these are each lane that a pass can hold. The drain waits on each lane whose first read has busy, and retains the generation of that read. Its wait on a lane ends where busy reads clear or where the generation moved.

> **Note:** A round that finds the lane still held backs off on that lane word, with the sleeper bit. The end of the pass changes the word and wakes it.

## Tables

### `embed_turnstile_table` { #embed_turnstile_table }

```c
#if !defined(__CUDA_ARCH__)
static const EmbedTurnstileNs embed_turnstile_table = {embed_word_wait,
                                                       embed_word_wake,
                                                       embed_clock_ns,
                                                       embed_thread_place,
                                                       embed_turnstile_backoff_begin,
                                                       embed_turnstile_backoff_step,
                                                       embed_turnstile_open_lane_count,
                                                       embed_turnstile_enter,
                                                       embed_turnstile_leave,
                                                       embed_turnstiles_drain};
#endif
```

The table of `embed_turnstile`.h. A call through it folds to a direct call under the optimizer and LTO.

> **Note:** The device pass of CUDA has no table, because a kernel cannot read the address of a host function. A kernel calls each function by its name.
