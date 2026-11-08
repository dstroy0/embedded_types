<!-- tools/api_pages.py writes this page from include/embed_turnstile.h. Edit the header, then run the tool. -->

# embed_turnstile.h

Source: [`include/embed_turnstile.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_turnstile.h)

Turnstiles as a manifold of lanes, and the sleep on the address of a word that their waits use.

A set of turnstiles is a manifold of lanes. Each lane is one word on a cache line of its own. A worker enters at the lane that its address selects, and where that lane is busy it spills to the next. A writer swaps a published pointer, then drains: it waits until each pass that holds a lane at the drain ends. The manifold opens lanes under pressure and closes them as the pressure goes.

> **Note:** The lane word holds four fields, from bit 0 up: busy, closed, sleeper, and the generation. The busy bit shows that a pass or a contraction holds the lane. The closed bit shows a lane that a contraction closed. The sleeper bit shows that a thread sleeps on the word. The generation counts each pass end, each close and each reopen, and rolls in its bits.

> **Note:** Each change to a lane word is one atomic operation. An entry is a fetch-or of busy. It takes the lane where the old value has busy clear. Where the old value has busy, it changes nothing. A leave, a close, a release and a reopen are each one fetch-add, which changes the low 32 bits that the futex compares.

> **Note:** `open` holds the count of open lanes less one, so a set in storage of zeros has one lane. Arrivals only read it. A growth and a contraction write it, each with one compare-and-swap. `queue` counts the workers that found every open lane busy. An entry on a free lane writes no word but the lane word.

> **Note:** The order of the passes is not the turnstile's. A consumer that needs an order keys it by the address of the worker and the worker's own sequence.

> **Note:** A backoff keeps its start on the clock and its exponent in the caller's locals. No shared word holds a time. Inside the first unit, `2^EMBED_TURNSTILE_UNIT_SHIFT` ns, a step is a pause and no call. From one unit on, a step waits on the word for 2^([`EMBED_TURNSTILE_UNIT_SHIFT`](#EMBED_TURNSTILE_UNIT_SHIFT) + k) ns at most, and k goes up to [`EMBED_TURNSTILE_K_MOST`](#EMBED_TURNSTILE_K_MOST). A wake or a moved word ends the wait first.

> **Note:** The host defines [`embed_word_wait`](#embed_word_wait), [`embed_word_wake`](#embed_word_wake) and [`embed_clock_ns`](#embed_clock_ns). A build that defines none of the three fails at the link, and the error names them.

> **Note:** theory/`embedded_types`/EmbeddedTypes/Manifold proves the lane word, the backoff, the open count, the queue, the drain and the sleeps.

> _**Warning:**_ The generation rolls after 2^([`EMBED_WORD_BITS`](embed_types.md#EMBED_WORD_BITS) - 3) counts. A drain that reads one lane twice, with exactly that count of passes between the two reads, waits for the next pass to end too.

## Types

### `EmbedTurnstileLane` { #EmbedTurnstileLane }

```c
typedef struct
{
    EMBED_ALIGN(64) embed_atomic_word word; /**< The lane word. */
} EmbedTurnstileLane;
EMBED_STATIC_ASSERT(sizeof(EmbedTurnstileLane) == 64u, "a lane takes one cache line");
```

One lane: its word on a cache line of its own, so that no pass shares the line of another.

- `word`: The lane word.

### `EmbedTurnstiles` { #EmbedTurnstiles }

```c
typedef struct
{
    EmbedTurnstileLane lane[EMBED_TURNSTILE_LANES]; /**< The lanes, those below the open count open. */
    EMBED_ALIGN(64) embed_atomic_word open;         /**< The count of open lanes less one. */
    EMBED_ALIGN(64) embed_atomic_u32 queue;         /**< The workers that found every open lane busy. */
} EmbedTurnstiles;
```

A set of turnstiles, zeros at rest: one to each thing workers read while a writer releases its parts.

> **Note:** `open` is the count of open lanes less one. `queue` is the count of workers that found every open lane busy.

- `lane`: The lanes, those below the open count open.
- `open`: The count of open lanes less one.
- `queue`: The workers that found every open lane busy.

### `EmbedTurnstileBackoff` { #EmbedTurnstileBackoff }

```c
typedef struct
{
    embed_u64 start;  /**< The nanosecond that embed_clock_ns read at the start. */
    unsigned int exponent; /**< The exponent of the next wait, from 0 to EMBED_TURNSTILE_K_MOST. */
} EmbedTurnstileBackoff;
```

A backoff's state, in the caller's locals: the clock at its start, and its exponent.

- `start`: The nanosecond that [`embed_clock_ns`](#embed_clock_ns) read at the start.
- `exponent`: The exponent of the next wait, from 0 to [`EMBED_TURNSTILE_K_MOST`](#EMBED_TURNSTILE_K_MOST).

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
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_UNIT_SHIFT <= 10u, "EMBED_TURNSTILE_UNIT_SHIFT must be 10 at most");
```

The shift of the backoff's unit, `2^shift` ns: 10 for a CPU host, about 1.024 us, and 0 for a device. A build can set its own before this header.

### `EMBED_TURNSTILE_K_MOST` { #EMBED_TURNSTILE_K_MOST }

```c
#define EMBED_TURNSTILE_K_MOST 20u
```

The most the backoff's exponent reaches: a wait of 2^([`EMBED_TURNSTILE_UNIT_SHIFT`](#EMBED_TURNSTILE_UNIT_SHIFT) + 20) ns at most.

### `EMBED_TURNSTILE_BUSY` { #EMBED_TURNSTILE_BUSY }

```c
#define EMBED_TURNSTILE_BUSY ((embed_atomic_word_value)1u)
#define EMBED_TURNSTILE_CLOSED ((embed_atomic_word_value)2u)
#define EMBED_TURNSTILE_SLEEPER ((embed_atomic_word_value)4u)
#define EMBED_TURNSTILE_GENERATION ((embed_atomic_word_value)8u)
#define EMBED_TURNSTILE_GENERATION_SHIFT 3u
```

The fields of a lane word, and the unit of its generation.

### `EMBED_WAIT_FOREVER` { #EMBED_WAIT_FOREVER }

```c
#define EMBED_WAIT_FOREVER (~(embed_u64)0u)
```

A sleep with no end, for [`embed_word_wait`](#embed_word_wait).

### `EMBED_TURNSTILE_PAUSE` { #EMBED_TURNSTILE_PAUSE }

```c
#if !defined(EMBED_TURNSTILE_PAUSE) && defined(__has_builtin)
#if __has_builtin(__builtin_ia32_pause)
#define EMBED_TURNSTILE_PAUSE() __builtin_ia32_pause()
#elif __has_builtin(__builtin_arm_yield)
#define EMBED_TURNSTILE_PAUSE() __builtin_arm_yield()
#endif
#endif
#if !defined(EMBED_TURNSTILE_PAUSE)
#define EMBED_TURNSTILE_PAUSE() EMBED_ATOMIC_ORDERED()
#endif
```

The processor's pause in a spin, a compiler barrier alone where the processor has none the compiler reaches.

> **Note:** A build that defines it before this header keeps its own, as a model checker's build does with no processor.

### `EMBED_TURNSTILE_NONE` { #EMBED_TURNSTILE_NONE }

```c
#define EMBED_TURNSTILE_NONE 0xffffffffu
```

No lane: a growth that took none.

## Functions

### `embed_word_wait` { #embed_word_wait }

```c
int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u64 nanoseconds);
```

A sleep on word while it reads seen, at most nanoseconds, [`EMBED_WAIT_FOREVER`](#EMBED_WAIT_FOREVER) for no end, and none at all for 0.

- `[in]`: word The word slept on \[BORROWS\].
- `[in]`: seen The value that the caller read.
- `[in]`: nanoseconds The most the sleep lasts. 0 answers at once with no sleep and no system call.

\*\*Returns:\*\* 1 where the word moved off seen, 0 where it did not. A sleep is free to end early, so a sleeper tests what it waits for in a loop.

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
embed_u64 embed_clock_ns(void);
```

The host's clock in nanoseconds, which the backoff measures its time on.

\*\*Returns:\*\* The nanoseconds, from a start that the host selects.

> **Note:** The host defines this beside [`embed_word_wait`](#embed_word_wait).

### `embed_turnstile_backoff_begin` { #embed_turnstile_backoff_begin }

```c
EMBED_INLINE void embed_turnstile_backoff_begin(EmbedTurnstileBackoff *backoff);
```

The start of a backoff: the clock read, and the exponent at 0.

- `[out]`: backoff The backoff \[BORROWS\].

### `embed_turnstile_backed_off` { #embed_turnstile_backed_off }

```c
EMBED_INLINE void embed_turnstile_backed_off(embed_atomic_word *word, embed_atomic_word_value read,
                                             embed_atomic_word_value flag, EmbedTurnstileBackoff *backoff,
                                             unsigned int on);
```

One step of a backoff on word, which the caller last read as read.

The step does one pause. Where the time since the start is less than one unit, or where `on` is 0, that is all. Otherwise the step sets the sleeper bit `flag` on the word, and the host waits on the word for the limit of the exponent. Then the exponent goes up by one, to [`EMBED_TURNSTILE_K_MOST`](#EMBED_TURNSTILE_K_MOST) at most.

- `[in]`: word The word \[BORROWS\].
- `[in]`: read What it was last read as.
- `[in]`: flag The word's sleeper bit.
- `[in,out]`: backoff The backoff \[BORROWS\].
- `[in]`: on 1 where the value read lets the caller sleep, 0 for a pause alone.

> **Note:** The sleep needs a value that a later change ends. A caller sleeps only on a value whose change comes with a wake, such as a lane word that has the busy bit.

### `embed_turnstile_lanes` { #embed_turnstile_lanes }

```c
EMBED_INLINE unsigned int embed_turnstile_lanes(const EmbedTurnstiles *set);
```

The lanes a set has open.

- `[in]`: set The set \[BORROWS\].

\*\*Returns:\*\* The lanes open, from 1 to [`EMBED_TURNSTILE_LANES`](#EMBED_TURNSTILE_LANES).

### `embed_turnstile_changed` { #embed_turnstile_changed }

```c
EMBED_INLINE void embed_turnstile_changed(EmbedTurnstileLane *lane, embed_atomic_word_value change);
```

A fetch-add on a lane word that the caller holds, and the wake of its sleepers where the old value had the sleeper bit.

- `[in,out]`: lane The lane \[BORROWS\].
- `[in]`: change The constant of the fetch-add.

### `embed_turnstile_grow` { #embed_turnstile_grow }

```c
EMBED_INLINE unsigned int embed_turnstile_grow(EmbedTurnstiles *set, unsigned int count, embed_atomic_word_value *held);
```

The growth from count lanes to count + 1, which gives the caller lane count as its pass.

The growth takes lane count with one compare-and-swap on the lane word. From a free word it sets busy. From a closed word it clears closed and counts the takeover, and busy stays set. Then one compare-and-swap moves `open` up by one.

Where that swap lands, the lane is the caller's pass. Where it fails, the growth gives the lane back as it was. A lane that had the closed bit gets a close, and a lane that was free gets a release.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: count The lanes the caller read open.
- `[out]`: held The value of lane count where another thread holds it, and 0 in each other case \[BORROWS\].

\*\*Returns:\*\* count, the lane the caller now holds, or [`EMBED_TURNSTILE_NONE`](#EMBED_TURNSTILE_NONE) where it holds none.

> **Note:** A lane that a pass, a contraction or another growth holds refuses the first compare-and-swap, and nothing changes. A contraction holds its lane from its claim to its close, so no growth takes a lane between a contraction's swap and its close. The caller sleeps on `held`: the end of that hold changes the word and wakes it.

### `embed_turnstile_contract` { #embed_turnstile_contract }

```c
EMBED_INLINE embed_bool embed_turnstile_contract(EmbedTurnstiles *set, unsigned int count);
```

The contraction from count lanes to count - 1.

The contraction claims lane count - 1 as an entry does. Then one compare-and-swap moves `open` down by one. Where the swap lands, the contraction closes the lane. Where the swap fails, the contraction releases the lane.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: count The lanes the caller read open, above 1.

\*\*Returns:\*\* 1 where the lane closed, 0 where a pass held it or the swap failed.

> **Note:** A lane that a pass holds refuses the claim, and nothing changes. So each pass stays below the open count.

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
EMBED_INLINE void embed_turnstile_leave(EmbedTurnstiles *set, unsigned int at);
```

The end of the pass through the lane at `at` of set.

One fetch-add clears busy and counts the pass in the generation. Where the old value had the sleeper bit, the end clears it and wakes the word. Where the lane is the top open lane, the end contracts the manifold. Each step closes the top lane while the queue is empty, more than one lane is open, and no pass holds the top lane.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: at The lane's place, as [`embed_turnstile_enter`](#embed_turnstile_enter) answered it.

> **Note:** Only the end of a pass on the top lane contracts. So the end of a pass on a lower lane reads and writes no word but its own lane word and `open`.

### `embed_turnstiles_drain` { #embed_turnstiles_drain }

```c
EMBED_INLINE void embed_turnstiles_drain(EmbedTurnstiles *set);
```

Every pass through set begun before this call waited out.

- `[in,out]`: set The set \[BORROWS\].

> **Note:** A writer swaps a published pointer and calls this function. Then it releases the data that the old pointer led to. A pass that starts after the fence of the drain reads the new pointer, and the drain does not wait for it.

> **Note:** The drain reads the open count after its fence, then each open lane. Each pass is below the open count, so these are each lane that a pass can hold. The drain waits on each lane whose first read has busy, and keeps the generation of that read. Its wait on a lane ends where busy reads clear or where the generation moved.

> **Note:** A round that finds the lane still held backs off on that lane word, with the sleeper bit. The end of the pass changes the word and wakes it.
