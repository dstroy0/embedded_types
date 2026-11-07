<!-- tools/api_pages.py writes this page from include/embed_turnstile.h. Edit the header, then run the tool. -->

# embed_turnstile.h

Source: [`include/embed_turnstile.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_turnstile.h)

Turnstiles, and the sleep on the address of a word that their waits use.

A set of turnstiles is a bank of lanes, with one badge word for each lane. One lane mask shows the open lanes, and a second shows the lanes with a pass under way. A reader badges in with a time stamp, waits in the queue, and takes the next free lane. A writer swaps a published pointer, then drains the lanes of a snapshot of the mask. The bank changes its size to match the pressure of the queue.

> **Note:** Three words hold the state of the set. `open` has one bit for each open lane, with the low lanes from bit 0. `busy` has one bit for each lane with a pass under way. Its top bit is the flag that shows a thread asleep on the word.

> **Note:** The badge word of each lane holds four fields, from high to low. They are the id of the reader in the lane, the completion bit, the sleeper bit, and the time of the pass. The time is the nanosecond on the clock of the host, and it rolls over in the bits below the sleeper bit. The id is an immutable place, the owner place of MMgr, and the field holds it whole.

> **Note:** A badge-in adds one to the queue. It takes the lowest bit of `open & ~busy` with one fetch-or on `busy`. Where no lane is free, it opens the next lane of the bank and takes it. Then it writes the badge with the time from the clock, and it gives the lane. Where no lane is free and the bank is full, it does one backoff step on `busy` and gives [`EMBED_TURNSTILE_NONE`](#EMBED_TURNSTILE_NONE). The reader then badges in again.

> **Note:** The passes of one id start one after the other, each with a later stamp. So the passes of an id come one after the other in time, across the lanes that they spill to. A pass ends with two fetch-and operations. One clears the state bits of the badge, and one clears the bit of the lane in `busy`.

> **Note:** The drain of a writer takes a snapshot of `busy`. It waits on each lane of the snapshot until the completion bit of the lane clears, or until its id or stamp moves. A pass that starts after the fence of the drain reads the swapped pointer, and the drain does not wait for it.

> **Note:** No branch occurs in a badge-in or at the end of a pass. Each choice is a mask, a select, or a row of a table of two entries, and the flag bit picks the row. A set at rest has one lane. When a pass ends with the queue empty, it trims `open` to the highest busy lane, and to one lane at least. A caller only enters and leaves, and the bank sets its own size.

> **Note:** A backoff step is one pause, then the wait of the host on the word for the milliseconds of the step. The first steps wait 0 milliseconds, and the host answers at once with no sleep. Each step doubles. The waits use [`embed_word_wait`](#embed_word_wait), which the host defines on its address wait: a futex, WaitOnAddress or `__ulock_wait`. Where the system has none of these, a timed sleep takes their place.

> **Note:** At its end, a pass wakes the sleepers on its lane and on `busy` through [`embed_word_wake`](#embed_word_wake). The clock is [`embed_clock_ns`](#embed_clock_ns), which the host supplies. A build that defines none of the three fails at the link, and the error names them.

> _**Warning:**_ The stamp rolls over after `2^EMBED_TURNSTILE_TIME_BITS` nanoseconds. A drain that waits on a lane can see a pass end and a new pass of the same id start with the same stamp. Then the drain waits until the new pass ends too.

## Types

### `EmbedTurnstileLane` { #EmbedTurnstileLane }

```c
typedef struct
{
    EMBED_ALIGN(64) embed_atomic_word badge; /**< The lane's badge word. */
} EmbedTurnstileLane;
EMBED_STATIC_ASSERT(sizeof(EmbedTurnstileLane) == 64u, "a lane takes one cache line");
```

One lane: its badge word on a cache line of its own, so that no reader's pass shares another's line.

- `badge`: The lane's badge word.

### `EmbedTurnstiles` { #EmbedTurnstiles }

```c
typedef struct
{
    EmbedTurnstileLane lane[EMBED_TURNSTILE_LANES_MOST]; /**< The lanes, those of `open` open. */
    EmbedTurnstileLane spare;                            /**< The badge of a badge-in with no lane. */
    EMBED_ALIGN(64) embed_atomic_word open;              /**< One bit for each open lane, bit 0 read as set. */
    EMBED_ALIGN(64) embed_atomic_word busy;              /**< One bit for each busy lane, the sleeper flag above. */
    embed_atomic_u32 waiting;                            /**< Readers queued for a lane. */
} EmbedTurnstiles;
```

A set of turnstiles, zeros at rest: one to each thing readers read while a writer releases its parts.

> **Note:** `open` holds the open lanes. Each read of `open` takes bit 0 as set, because a set in storage of zeros has one lane. `busy` holds the lanes with a pass under way, and the sleeper flag above them. `waiting` is the pressure of the queue: the readers between their badge-in and their lane. `spare` is the word that gets the badge of a badge-in with no lane. So the write occurs on each path.

- `lane`: The lanes, those of `open` open.
- `spare`: The badge of a badge-in with no lane.
- `open`: One bit for each open lane, bit 0 read as set.
- `busy`: One bit for each busy lane, the sleeper flag above.
- `waiting`: Readers queued for a lane.

## Macros

### `EMBED_TURNSTILE_LANES_MOST` { #EMBED_TURNSTILE_LANES_MOST }

```c
#define EMBED_TURNSTILE_LANES_MOST (EMBED_WORD_BITS - 1u)
```

The lanes a set can open: every bit of the word but the sleeper flag's.

### `EMBED_TURNSTILE_LANE_BITS` { #EMBED_TURNSTILE_LANE_BITS }

```c
#define EMBED_TURNSTILE_LANE_BITS ((((embed_atomic_word_value)1u << EMBED_TURNSTILE_LANES_MOST)) - 1u)
#define EMBED_TURNSTILE_SLEEPERS ((embed_atomic_word_value)1u << EMBED_TURNSTILE_LANES_MOST)
```

The bits of `busy` that are lanes, and the flag above them that a thread sleeps on the word.

### `EMBED_TURNSTILE_NONE` { #EMBED_TURNSTILE_NONE }

```c
#define EMBED_TURNSTILE_NONE 0xffffffffu
```

No lane: a badge-in that found none free with the bank full, after its backoff step.

### `EMBED_WAIT_FOREVER` { #EMBED_WAIT_FOREVER }

```c
#define EMBED_WAIT_FOREVER 0xffffffffu
```

A sleep with no end, for [`embed_word_wait`](#embed_word_wait).

### `EMBED_TURNSTILE_ID_BITS` { #EMBED_TURNSTILE_ID_BITS }

```c
#if EMBED_WORD_BITS >= 64u
#define EMBED_TURNSTILE_ID_BITS 16u
#else
#define EMBED_TURNSTILE_ID_BITS 10u
#endif
#define EMBED_TURNSTILE_ID_SHIFT (EMBED_WORD_BITS - EMBED_TURNSTILE_ID_BITS)
#define EMBED_TURNSTILE_ID_MASK ((((embed_atomic_word_value)1u << EMBED_TURNSTILE_ID_BITS) - 1u) << EMBED_TURNSTILE_ID_SHIFT)
#define EMBED_TURNSTILE_COMPLETE_SHIFT (EMBED_TURNSTILE_ID_SHIFT - 1u)
#define EMBED_TURNSTILE_SLEEPER_SHIFT (EMBED_TURNSTILE_ID_SHIFT - 2u)
#define EMBED_TURNSTILE_COMPLETE ((embed_atomic_word_value)1u << EMBED_TURNSTILE_COMPLETE_SHIFT)
#define EMBED_TURNSTILE_SLEEPER ((embed_atomic_word_value)1u << EMBED_TURNSTILE_SLEEPER_SHIFT)
#define EMBED_TURNSTILE_TIME_BITS EMBED_TURNSTILE_SLEEPER_SHIFT
#define EMBED_TURNSTILE_TIME_MASK (((embed_atomic_word_value)1u << EMBED_TURNSTILE_TIME_BITS) - 1u)
```

The badge word's fields, high to low: the id in [`EMBED_TURNSTILE_ID_BITS`](#EMBED_TURNSTILE_ID_BITS) bits, the completion bit, the sleeper bit, and the stamp in the bits left.

### `EMBED_TURNSTILE_PASS_MASK` { #EMBED_TURNSTILE_PASS_MASK }

```c
#define EMBED_TURNSTILE_PASS_MASK (EMBED_TURNSTILE_ID_MASK | EMBED_TURNSTILE_TIME_MASK)
#define EMBED_TURNSTILE_STATE_MASK (EMBED_TURNSTILE_COMPLETE | EMBED_TURNSTILE_SLEEPER)
```

The bits that tell one pass from another, its id and stamp. Then the bits that a pass clears at its end.

### `EMBED_TURNSTILE_SLEEP_SHIFT` { #EMBED_TURNSTILE_SLEEP_SHIFT }

```c
#define EMBED_TURNSTILE_SLEEP_SHIFT 7u
#define EMBED_TURNSTILE_BACKOFF_MOST_MS 64u
#define EMBED_TURNSTILE_STEP_MOST (EMBED_TURNSTILE_BACKOFF_MOST_MS << EMBED_TURNSTILE_SLEEP_SHIFT)
```

The shift that turns a backoff's step into its sleep in milliseconds, 0 for the first steps; and the longest step, whose sleep is [`EMBED_TURNSTILE_BACKOFF_MOST_MS`](#EMBED_TURNSTILE_SLEEP_SHIFT).

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

## Functions

### `embed_word_wait` { #embed_word_wait }

```c
int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value seen, embed_u32 milliseconds);
```

A sleep on word while it reads seen, at most milliseconds, [`EMBED_WAIT_FOREVER`](#EMBED_WAIT_FOREVER) for no end, and none at all for 0.

- `[in]`: word The word slept on \[BORROWS\].
- `[in]`: seen The value that the caller read.
- `[in]`: milliseconds The most the sleep lasts; 0 answers at once with no sleep and no system call.

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

The host's clock in nanoseconds, which a badge carries the low [`EMBED_TURNSTILE_TIME_BITS`](#EMBED_TURNSTILE_ID_BITS) bits of.

\*\*Returns:\*\* The nanoseconds, from a start that the host selects.

> **Note:** The host defines this beside [`embed_word_wait`](#embed_word_wait).

### `embed_turnstile_lowest` { #embed_turnstile_lowest }

```c
EMBED_INLINE unsigned int embed_turnstile_lowest(embed_atomic_word_value mask);
```

The place of the lowest bit set in mask.

- `[in]`: mask A word with a bit set.

\*\*Returns:\*\* The bit's place from 0.

> **Note:** The trailing zero count where the compiler has it, else the lowest bit isolated and placed by six masked steps.

### `embed_turnstile_count` { #embed_turnstile_count }

```c
EMBED_INLINE unsigned int embed_turnstile_count(embed_atomic_word_value mask);
```

The bits of mask set, counted.

- `[in]`: mask A word.

\*\*Returns:\*\* The count of its set bits.

> **Note:** The population count where the compiler has it, else the SWAR count.

### `embed_turnstile_lanes` { #embed_turnstile_lanes }

```c
EMBED_INLINE unsigned int embed_turnstile_lanes(const EmbedTurnstiles *set);
```

The lanes a set has open: the bits of `open`, bit 0 read as set.

- `[in]`: set The set \[BORROWS\].

\*\*Returns:\*\* The lanes open, from 1 to [`EMBED_TURNSTILE_LANES_MOST`](#EMBED_TURNSTILE_LANES_MOST).

### `embed_turnstile_badge` { #embed_turnstile_badge }

```c
EMBED_INLINE embed_atomic_word_value embed_turnstile_badge(embed_atomic_word_value id);
```

The badge of id at the current time of the clock: the id in its field, the completion bit, and the stamp.

- `[in]`: id The reader's id, nonzero, its low [`EMBED_TURNSTILE_ID_BITS`](#EMBED_TURNSTILE_ID_BITS) bits the badge.

\*\*Returns:\*\* The badge word of a pass that starts now.

### `embed_turnstile_backed_off` { #embed_turnstile_backed_off }

```c
EMBED_INLINE unsigned int embed_turnstile_backed_off(embed_atomic_word *word, embed_atomic_word_value read,
                                                     embed_atomic_word_value flag, unsigned int step, unsigned int on);
```

One step of a backoff on word, which the caller last read as read.

The step does one pause. Where the step is at its sleeps, it sets the sleeper bit `flag` on the word. Then the host waits on the word for the milliseconds of the step. That time is 0 below the sleeps, and 0 where `on` is 0, and the host then answers at once.

- `[in]`: word The word \[BORROWS\].
- `[in]`: read What it was last read as.
- `[in]`: flag The word's sleeper bit.
- `[in]`: step The step.
- `[in]`: on 1 for a step, 0 for none: the pause alone, the flag and the sleep masked to nothing.

\*\*Returns:\*\* The step after it, doubled up to [`EMBED_TURNSTILE_STEP_MOST`](#EMBED_TURNSTILE_SLEEP_SHIFT), or the step itself where `on` is 0.

### `embed_turnstile_claim` { #embed_turnstile_claim }

```c
EMBED_INLINE unsigned int embed_turnstile_claim(EmbedTurnstiles *set, embed_atomic_word_value permit);
```

One claim on set: the lowest free lane's bit set in `busy` with one fetch-or, where `permit` allows it.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: permit Every bit set for a claim, none for a claim that takes nothing.

\*\*Returns:\*\* The lane's place, or [`EMBED_TURNSTILE_NONE`](#EMBED_TURNSTILE_NONE). That is where no lane was free, where `permit` stops the claim, or where the fetch-or of another reader set the same bit first.

> **Note:** The code has no branch. It does two loads, makes the free mask, and takes the trailing zero count. The bit of the sleeper flag is the sentinel, where the count of a full bank lands. Then it does the fetch-or, and on the sentinel a mask takes its bit to nothing.

### `embed_turnstile_enter` { #embed_turnstile_enter }

```c
EMBED_INLINE unsigned int embed_turnstile_enter(EmbedTurnstiles *set, embed_atomic_word_value id);
```

A badge-in by the reader id, which gives a lane or [`EMBED_TURNSTILE_NONE`](#EMBED_TURNSTILE_NONE).

The badge-in adds one to the queue and claims the lowest free lane. Where no lane is free, it opens the next lane of the bank and claims it. It writes the badge with the time from the clock, and gives the lane. Where it gets no lane and the bank is full, it does one backoff step on `busy` and gives [`EMBED_TURNSTILE_NONE`](#EMBED_TURNSTILE_NONE). The reader then badges in again.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: id The reader's id, nonzero: an immutable place, MMgr's owner place.

\*\*Returns:\*\* The lane's place, which the end of the pass takes, or [`EMBED_TURNSTILE_NONE`](#EMBED_TURNSTILE_NONE).

> **Note:** The code has no branch. The miss of the first claim permits the second claim. The lane that it opens is the next bit of the bank, or nothing where the bank is full. The badge goes to the lane, or to the spare word of the set. The miss also turns the backoff step on or off.

> **Note:** The pass reads the data of a published pointer only after the pass starts, and none of it after the pass ends.

### `embed_turnstile_wake_none` { #embed_turnstile_wake_none }

```c
EMBED_INLINE void embed_turnstile_wake_none(embed_atomic_word *word);
```

A wake of nothing, the row of the wake table a clear flag takes.

### `embed_turnstile_trim_none` { #embed_turnstile_trim_none }

```c
EMBED_INLINE void embed_turnstile_trim_none(EmbedTurnstiles *set, embed_atomic_word_value open,
                                            embed_atomic_word_value trimmed);
```

A trim of nothing, the row of the trim table a trim with nothing to take takes.

### `embed_turnstile_trim` { #embed_turnstile_trim }

```c
EMBED_INLINE void embed_turnstile_trim(EmbedTurnstiles *set, embed_atomic_word_value open,
                                       embed_atomic_word_value trimmed);
```

The trim: one fetch-and cuts `open` to `trimmed`.

The fetch-and keeps what the end of another pass does to the word at the same time. Two ends at the same time each cut the lanes above their own highest busy lane. The word then stays at the lower of the two.

### `embed_turnstile_leave` { #embed_turnstile_leave }

```c
EMBED_INLINE void embed_turnstile_leave(EmbedTurnstiles *set, unsigned int at);
```

The end of the pass through the lane at `at` of set.

The end clears the state bits of the badge and the bit of the lane in `busy`. It wakes the sleepers on the lane and on `busy`. Where the queue is empty, it retires the free lanes above the highest busy lane.

- `[in,out]`: set The set \[BORROWS\].
- `[in]`: at The lane's place, as [`embed_turnstile_enter`](#embed_turnstile_enter) answered it.

> **Note:** The code has no branch. Each wake is a row of a table of two entries, and the flag bit of the word picks the row. The trim is a row of another table. Its row shows whether `open` has bits above the highest busy lane while the queue is empty.

> **Note:** The id and the stamp stay in the badge until the next pass takes the lane. So a drain uses them to tell this pass from the next.

### `embed_turnstiles_drain` { #embed_turnstiles_drain }

```c
EMBED_INLINE void embed_turnstiles_drain(EmbedTurnstiles *set);
```

Every pass through set begun before this call waited out.

- `[in,out]`: set The set \[BORROWS\].

> **Note:** A writer swaps a published pointer and calls this function. Then it releases the data that the old pointer led to. A pass that starts after the swap reads the new pointer, and the drain does not wait for it.

> **Note:** The drain waits on the lanes in the snapshot of `busy`. Each round reads the lowest lane in the snapshot. Where the pass of that lane is over, the round takes the lane off the snapshot. Where it is not over, the round does one backoff step.

> **Note:** A pass is over where the bit of its lane leaves `busy`. It is also over where the id or stamp of the badge moves from the first live badge that the drain read on the lane. That move is the next pass through the lane.

> **Note:** A lane whose badge is not live holds a pass between its claim and its badge, or a pass at its end. The drain waits on that lane until its bit leaves `busy`.

> **Note:** The backoff sleeps on `busy` with the flag [`EMBED_TURNSTILE_SLEEPERS`](#EMBED_TURNSTILE_LANE_BITS), at the value of `busy` that the round read. A pass is over only after the fetch-and of its end on `busy`. That fetch-and changes the word, clears the flag, and wakes each sleeper where the flag was on.
