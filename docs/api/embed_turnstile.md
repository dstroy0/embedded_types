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

> **Note:** The host defines [`embed_word_wait`](#embed_word_wait), [`embed_word_wake`](#embed_word_wake) and [`embed_clock_ns`](#embed_clock_ns). A build that defines none of the three fails at the link, and the error names them.

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

## Macros

### `EMBED_TURNSTILE_LANES` { #EMBED_TURNSTILE_LANES }

```c
#if !defined(EMBED_TURNSTILE_LANES)
#define EMBED_TURNSTILE_LANES 64u
#endif
EMBED_STATIC_ASSERT((EMBED_TURNSTILE_LANES >= 1u) && (EMBED_TURNSTILE_LANES <= 64u),
                    "EMBED_TURNSTILE_LANES must be from 1 to 64");
```

The lanes of a set. A build can set its own count before this header, from 1 to 64.

### `EMBED_TURNSTILE_UNIT_SHIFT` { #EMBED_TURNSTILE_UNIT_SHIFT }

```c
#if !defined(EMBED_TURNSTILE_UNIT_SHIFT)
#define EMBED_TURNSTILE_UNIT_SHIFT 10u
#endif
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_UNIT_SHIFT <= 10u, "EMBED_TURNSTILE_UNIT_SHIFT must be 10 at max");
```

The shift of the backoff's unit, `2^shift` ns: 10 for a CPU host, about 1.024 us, and 0 for a device. A build can set its own before this header.

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
#define EMBED_WAIT_INFINITE (~(embed_word)0u)
```

A sleep with no end, for [`embed_word_wait`](#embed_word_wait).

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
int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value old_value, embed_word nanoseconds);
```

A sleep on word while it holds `old_value`, at max nanoseconds, [`EMBED_WAIT_INFINITE`](#EMBED_WAIT_INFINITE) for no end, and none at all for 0.

- `[in]`: word The word slept on \[BORROWS\].
- `[in]`: `old_value` The value of the word that the caller read last.
- `[in]`: nanoseconds The max the sleep lasts. 0 answers at once with no sleep and no system call.

\*\*Returns:\*\* 1 where the word moved off `old_value`, 0 where it did not. A sleep is free to end early, so a sleeper tests what it waits for in a loop.

> **Note:** The host defines this function on its address wait. That is a futex on Linux, WaitOnAddress on Windows, and `__ulock_wait` on macOS. Where the system has none, it is a timed sleep.

### `embed_word_wake` { #embed_word_wake }

```c
void embed_word_wake(embed_atomic_word *word);
```

Every thread asleep on word woken.

- `[in]`: word The word \[BORROWS\].

> **Note:** The host defines this beside [`embed_word_wait`](#embed_word_wait).

### `embed_clock_ns` { #embed_clock_ns }

```c
embed_word embed_clock_ns(void);
```

The host's clock in nanoseconds, which the backoff measures its time on.

\*\*Returns:\*\* The nanoseconds modulo `2^EMBED_WORD_BITS`, from a start that the host selects.

> **Note:** The host defines this beside [`embed_word_wait`](#embed_word_wait).

> _**Warning:**_ The clock rolls at the width of the word: after about 4.29 s on a word of 32 bits, and 65.5 us on a word of 16 bits. The backoff takes the difference of two reads, and that difference is correct across the roll. A step one roll or more after its start reads the time modulo the roll. Where that is below one unit, the step pauses and makes no call.

### `embed_turnstile_backoff_begin` { #embed_turnstile_backoff_begin }

```c
EMBED_INLINE void embed_turnstile_backoff_begin(EmbedTurnstileBackoff *backoff);
```

The start of a backoff: the clock read, and the exponent at 0.

- `[out]`: backoff The backoff \[BORROWS\].

### `embed_turnstile_backoff_step` { #embed_turnstile_backoff_step }

```c
EMBED_INLINE void embed_turnstile_backoff_step(embed_atomic_word *word, embed_atomic_word_value old_value,
                                               embed_atomic_word_value sleeper_bit, EmbedTurnstileBackoff *backoff,
                                               embed_bool sleep_allowed);
```

One step of a backoff on word, which the caller last read as `old_value`.

The step does one pause. Where the time since the start is less than one unit, or where `sleep_allowed` is false, that is all. Otherwise the step sets `sleeper_bit` on the word, and the host waits on the word for the limit of the exponent. Then the exponent goes up by one, to [`EMBED_TURNSTILE_EXPONENT_MAX`](#EMBED_TURNSTILE_EXPONENT_MAX_16) at max.

- `[in]`: word The word \[BORROWS\].
- `[in]`: `old_value` The value of the word that the caller read last.
- `[in]`: `sleeper_bit` The sleeper bit of the word.
- `[in,out]`: backoff The backoff \[BORROWS\].
- `[in]`: `sleep_allowed` True where `old_value` lets the caller sleep, false for a pause alone.

> **Note:** The sleep needs a value that a later change ends. A caller sleeps only on a value whose change comes with a wake, such as a lane word that has the busy bit.

### `embed_turnstile_open_lane_count` { #embed_turnstile_open_lane_count }

```c
EMBED_INLINE unsigned int embed_turnstile_open_lane_count(const EmbedTurnstiles *set);
```

The lanes a set has open.

- `[in]`: set The set \[BORROWS\].

\*\*Returns:\*\* The lanes open, from 1 to [`EMBED_TURNSTILE_LANES`](#EMBED_TURNSTILE_LANES).

### `embed_turnstile_lane_add` { #embed_turnstile_lane_add }

```c
EMBED_INLINE void embed_turnstile_lane_add(EmbedTurnstileLane *lane, embed_atomic_word_value addend);
```

A fetch-add on a lane word that the caller holds, and the wake of its sleepers where the old value had the sleeper bit.

- `[in,out]`: lane The lane \[BORROWS\].
- `[in]`: addend The constant of the fetch-add.

### `embed_turnstile_grow` { #embed_turnstile_grow }

```c
EMBED_INLINE unsigned int embed_turnstile_grow(EmbedTurnstiles *set, unsigned int open_lane_count,
                                               embed_atomic_word_value *held_word);
```

The growth from `open_lane_count` lanes to one more, which gives the caller lane `open_lane_count` as its pass.

The growth takes lane `open_lane_count` with one compare-and-swap on the lane word. From a free word it sets busy. From a closed word it clears closed and counts the takeover, and busy stays set. Then one compare-and-swap moves `top_open_lane` up by one.

Where that swap lands, the lane is the caller's pass. Where it fails, the growth gives the lane back as it was. A lane that had the closed bit gets a close, and a lane that was free gets a release.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: `open_lane_count` The lanes the caller read open.
- `[out]`: `held_word` The word of lane `open_lane_count` where another thread holds it, and 0 in each other case \[BORROWS\].

\*\*Returns:\*\* `open_lane_count`, the lane the caller now holds, or [`EMBED_TURNSTILE_NO_LANE`](#EMBED_TURNSTILE_NO_LANE) where it holds none.

> **Note:** A lane that a pass, a contraction or another growth holds refuses the first compare-and-swap, and nothing changes. A contraction holds its lane from its claim to its close, so no growth takes a lane between a contraction's swap and its close. The caller sleeps on `held_word`: the end of that hold changes the word and wakes it.

### `embed_turnstile_contract` { #embed_turnstile_contract }

```c
EMBED_INLINE embed_bool embed_turnstile_contract(EmbedTurnstiles *set, unsigned int open_lane_count);
```

The contraction from `open_lane_count` lanes to one less.

The contraction claims the top open lane as an entry does. Then one compare-and-swap moves `top_open_lane` down by one. Where the swap lands, the contraction closes the lane. Where the swap fails, the contraction releases the lane.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: `open_lane_count` The lanes the caller read open, above 1.

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) where the lane closed, [`EMBED_FALSE`](embed_types.md#EMBED_FALSE) where a pass held it or the swap failed.

> **Note:** A lane that a pass holds refuses the claim, and nothing changes. So each pass stays below the open count.

### `embed_turnstile_spill` { #embed_turnstile_spill }

```c
EMBED_INLINE unsigned int embed_turnstile_spill(EmbedTurnstiles *set, unsigned int open_lane_count,
                                                unsigned int start_lane);
```

The spill of an entry over the open lanes: a fetch-or of busy on each lane from `start_lane`, mod `open_lane_count`.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: `open_lane_count` The lanes the caller read open.
- `[in]`: `start_lane` The start lane, below `open_lane_count`.

\*\*Returns:\*\* The lane the caller now holds, or [`EMBED_TURNSTILE_NO_LANE`](#EMBED_TURNSTILE_NO_LANE) where every lane was busy.

> **Note:** A lane with busy in the old value refuses the fetch-or, and nothing changes. The spill writes no word but the lane words.

### `embed_turnstile_enter` { #embed_turnstile_enter }

```c
EMBED_INLINE unsigned int embed_turnstile_enter(EmbedTurnstiles *set, embed_atomic_uptr_value address);
```

An entry by the worker at address: a lane taken, which the end of the pass takes back.

The entry reads the open count and starts at the lane of the address's cache line. A fetch-or of busy takes the lane where the old value has busy clear. Where the old value has busy, the fetch-or changes nothing, and the entry spills to the next lane. Where every open lane was busy, the worker joins the queue and grows the manifold, which gives it the new lane as its pass. Where it can not grow the manifold, it backs off on its start lane. It leaves the queue when it takes a lane.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: address The worker's address: an immutable place, such as the worker's own state.

\*\*Returns:\*\* The lane's place, which the end of the pass takes.

> **Note:** The pass reads the data of a published pointer only after the entry returns, and none of it after the leave.

### `embed_turnstile_leave` { #embed_turnstile_leave }

```c
EMBED_INLINE void embed_turnstile_leave(EmbedTurnstiles *set, unsigned int place);
```

The end of the pass through the lane at `place` of set.

One fetch-add clears busy and counts the pass in the generation. Where the old value had the sleeper bit, the end clears it and wakes the word. Where the lane is the top open lane, the end contracts the manifold. Each step closes the top lane while the queue is empty, more than one lane is open, and no pass holds the top lane.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: place The lane's place, as [`embed_turnstile_enter`](#embed_turnstile_enter) answered it.

> **Note:** Only the end of a pass on the top lane contracts. So the end of a pass on a lower lane reads and writes no word but its own lane word and `top_open_lane`.

### `embed_turnstiles_drain` { #embed_turnstiles_drain }

```c
EMBED_INLINE void embed_turnstiles_drain(EmbedTurnstiles *set);
```

Every pass through set begun before this call waited out.

- `[in,out]`: set The set \[BORROWS\].

> **Note:** A writer swaps a published pointer and calls this function. Then it releases the data that the old pointer led to. A pass that starts after the fence of the drain reads the new pointer, and the drain does not wait for it.

> **Note:** The drain reads the open count after its fence, then each open lane. Each pass is below the open count, so these are each lane that a pass can hold. The drain waits on each lane whose first read has busy, and retains the generation of that read. Its wait on a lane ends where busy reads clear or where the generation moved.

> **Note:** A round that finds the lane still held backs off on that lane word, with the sleeper bit. The end of the pass changes the word and wakes it.
