<!-- tools/api_pages.py writes this page from include/embed_ticket_ring.h. Edit the header, then run the tool. -->

# embed_ticket_ring.h

Source: [`include/embed_ticket_ring.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_ticket_ring.h)

A ring of job slots that a ticket indexes, with a state machine per slot in the flag bits of its stamp.

The ring has `2^shift` slots. A slot holds a stamp and a job pointer. The stamp is one atomic machine word. The ticket is in its low [`EMBED_TICKET_BITS`](#EMBED_TICKET_BITS) bits, the 16 flag bits are above it, and the epoch is in the high bits. A word of 64 bits has a 32-bit ticket and a 16-bit epoch, and a word of 32 bits has a 12-bit ticket and a 4-bit epoch.

The ticket indexes the slot: the low shift bits of the ticket are the slot index. The ticket is a counter with wraparound, and the slot index wraps with it, because `2^shift` divides `2^EMBED_TICKET_BITS`.

> **Note:** Each transition of a slot is one compare-and-swap on the stamp word. The expected value is the stamp with the flag bits of the transitions before it. The new value adds the flag bit of the transition. So exactly one caller completes each transition, and a stamp that differs in any bit fails the compare.

> **Note:** The state sequence of a slot is grant, push, pop, replay, release. A replay is the read of a slot after its pop, which a rewind performs. A grant succeeds on a zero word or on a released word. A release succeeds after a pop. The next grant at the same index reuses the released word.

> **Note:** The pop writes the lane index of the pass that performed it into seven flag bits. The release retains them.

> **Note:** The push stores a three-bit cyclic redundancy check of the job pointer in three flag bits. The pop and the replay check the pointer they read against it. Every single-bit error in the job pointer fails the check. Every double-bit error fails it too, except one whose two bits are a multiple of seven places apart. The pop then returns no job. [`EMBED_TICKET_JOB_CHECK`](#EMBED_TICKET_JOB_CHECK) turns the check off.

> **Note:** The epoch is [`EMBED_TICKET_EPOCH_BITS`](#EMBED_TICKET_BITS) wide: 16 bits on a word of 64 bits, and 4 bits on a word of 32 bits. The grantor derives it from the host clock. The tick period covers the lifetime of a stamp, so the epoch does not wrap while the stamp is live. A compare succeeds only where the ticket and the epoch both match, and the ticket alone distinguishes any two live stamps.

> **Note:** The turnstile serializes the transitions with respect to the writer. A transition executes inside a pass, and the drain of a writer waits for every pass that started before it. The functions at the end of this header wrap each transition in a pass.

> **Note:** theory/`embedded_types`/EmbeddedTypes/TicketRing.lean proves the stamp fields and the slot index. Flags.lean proves the flag matrix: each transition preserves every other field, succeeds for exactly one caller, and fails on a corrupted word. Claim.lean proves the collision of two transitions and the drain after one.

> _**Warning:**_ A caller that accesses a slot without a successful compare-and-swap is outside every proof here.

## Types

### `EmbedTicketRingSlot` { #EmbedTicketRingSlot }

```c
typedef struct
{
    embed_atomic_word stamp; /**< The stamp with the flag bits of the transitions that completed. */
    void *job;               /**< The job [BORROWS]. */
} EmbedTicketRingSlot;
EMBED_STATIC_ASSERT(sizeof(EmbedTicketRingSlot) ==
                        2u *
                            ((sizeof(embed_atomic_word) > sizeof(void *)) ? sizeof(embed_atomic_word) : sizeof(void *)),
                    "a slot is two of its wider member: the stamp word and the job pointer");
```

One slot: the stamp word, the state of the slot, and the job pointer.

- `stamp`: The stamp with the flag bits of the transitions that completed.
- `job`: The job \[BORROWS\].

### `EmbedTicketRing` { #EmbedTicketRing }

```c
typedef struct
{
    EMBED_ALIGN(64u) embed_word mask; /**< 2^shift - 1. */
    EmbedTicketRingSlot *slots;       /**< 2^shift slots. */
} EmbedTicketRing;
```

A ring: its index mask, `2^shift` - 1, on a cache line of its own, and its slot array.

- `mask`: `2^shift` - 1.
- `slots`: `2^shift` slots.

## Macros

### `EMBED_TICKET_BITS_32` { #EMBED_TICKET_BITS_32 }

```c
#define EMBED_TICKET_BITS_32 12u
#define EMBED_TICKET_EPOCH_BITS_32 4u
#define EMBED_TICKET_BITS_64 32u
#define EMBED_TICKET_EPOCH_BITS_64 16u
```

The stamp layout of each word width that the ring builds on: the bits of the ticket and of the epoch. The flag field takes the 16 bits between them on each width.

> **Note:** The epoch outlasts the longest hold of a stamp, and that time does not grow with the word. So the epoch needs no more than 16 bits, and a wider word gives its bits to the ticket, which bounds the slot count.

> _**Warning:**_ At 32 bits the epoch covers 16 ticks. The tick of the grantor is then 1/16 of the longest hold at least.

### `EMBED_TICKET_BITS` { #EMBED_TICKET_BITS }

```c
#define EMBED_TICKET_BITS EMBED_CAT(EMBED_TICKET_BITS_, EMBED_WORD_BITS)
#define EMBED_TICKET_MASK ((((embed_word)1u) << EMBED_TICKET_BITS) - 1u)
#define EMBED_TICKET_FLAG_SHIFT EMBED_TICKET_BITS
#define EMBED_TICKET_FLAG_BITS 16u
#define EMBED_TICKET_FLAG_MASK ((embed_word)0xffffu << EMBED_TICKET_FLAG_SHIFT)
#define EMBED_TICKET_EPOCH_SHIFT (EMBED_TICKET_FLAG_SHIFT + EMBED_TICKET_FLAG_BITS)
#define EMBED_TICKET_EPOCH_BITS EMBED_CAT(EMBED_TICKET_EPOCH_BITS_, EMBED_WORD_BITS)
#define EMBED_TICKET_EPOCH_MASK ((((embed_word)1u) << EMBED_TICKET_EPOCH_BITS) - 1u)
```

The width of the ticket field, and the positions of the flag field and the epoch field, of this word.

### `EMBED_TICKET_GRANTED_BIT` { #EMBED_TICKET_GRANTED_BIT }

```c
#define EMBED_TICKET_GRANTED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 0u))
#define EMBED_TICKET_PUSHED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 1u))
#define EMBED_TICKET_POPPED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 2u))
#define EMBED_TICKET_REPLAYED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 3u))
#define EMBED_TICKET_RELEASED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 4u))
#define EMBED_TICKET_LANE_SHIFT (EMBED_TICKET_FLAG_SHIFT + 8u)
#define EMBED_TICKET_LANE_MASK ((embed_word)0x7fu << EMBED_TICKET_LANE_SHIFT)
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_LANES <= 128u, "the lane index of a pop fits seven flag bits");
```

The flag bits: one per transition of the slot state machine, and the lane index of the pop.

### `EMBED_TICKET_CHECK_SHIFT` { #EMBED_TICKET_CHECK_SHIFT }

```c
#define EMBED_TICKET_CHECK_SHIFT (EMBED_TICKET_FLAG_SHIFT + 5u)
#define EMBED_TICKET_CHECK_MASK ((embed_word)0x7u << EMBED_TICKET_CHECK_SHIFT)
```

The three check bits of the job pointer, which the push sets and the pop and the replay check.

### `EMBED_TICKET_RESERVED_BIT` { #EMBED_TICKET_RESERVED_BIT }

```c
#define EMBED_TICKET_RESERVED_BIT ((embed_word)1u << (EMBED_TICKET_FLAG_SHIFT + 15u))
```

The reserved bit. No transition sets it, so an expected value with it set always fails the compare.

### `EMBED_TICKET_JOB_CHECK` { #EMBED_TICKET_JOB_CHECK }

```c
#if !defined(EMBED_TICKET_JOB_CHECK)
#define EMBED_TICKET_JOB_CHECK 1
#endif
```

1 where the push stores the check of the job pointer in the stamp and the pop and the replay verify it, 0 where they do not. A build sets it before this header. This header defines both values. Test it with `#if`.

### `EMBED_TICKET_RING_SHIFT_MAX` { #EMBED_TICKET_RING_SHIFT_MAX }

```c
#define EMBED_TICKET_RING_SHIFT_MAX (EMBED_TICKET_BITS - 1u)
```

The max slot count as a shift: `2^EMBED_TICKET_RING_SHIFT_MAX` slots, below the ticket width.

## Functions

### `embed_ticket_stamp` { #embed_ticket_stamp }

```c
EMBED_INLINE embed_word embed_ticket_stamp(embed_word epoch, embed_word ticket);
```

The stamp of an epoch and a ticket, with the flag bits clear.

- `[in]`: epoch The epoch, of which the stamp keeps the low [`EMBED_TICKET_EPOCH_BITS`](#EMBED_TICKET_BITS) bits.
- `[in]`: ticket The ticket, of which the stamp keeps the low [`EMBED_TICKET_BITS`](#EMBED_TICKET_BITS) bits.

\*\*Returns:\*\* The stamp.

> **Note:** Each field keeps only its own bits, so a wide epoch or ticket sets no flag bit.

### `embed_ticket_of_stamp` { #embed_ticket_of_stamp }

```c
EMBED_INLINE embed_word embed_ticket_of_stamp(embed_word stamp);
```

The ticket field of a stamp.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* The low [`EMBED_TICKET_BITS`](#EMBED_TICKET_BITS) bits.

### `embed_epoch_of_stamp` { #embed_epoch_of_stamp }

```c
EMBED_INLINE embed_word embed_epoch_of_stamp(embed_word stamp);
```

The epoch field of a stamp.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* The high [`EMBED_TICKET_EPOCH_BITS`](#EMBED_TICKET_BITS) bits, shifted down.

### `embed_ticket_flags_of_stamp` { #embed_ticket_flags_of_stamp }

```c
EMBED_INLINE embed_u16 embed_ticket_flags_of_stamp(embed_word stamp);
```

The flag field of a stamp, the lane index included.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* The 16 bits from [`EMBED_TICKET_FLAG_SHIFT`](#EMBED_TICKET_BITS) up.

### `embed_ticket_lane_of_stamp` { #embed_ticket_lane_of_stamp }

```c
EMBED_INLINE unsigned int embed_ticket_lane_of_stamp(embed_word stamp);
```

The lane index of the pass that performed the pop, from the stamp of the slot.

- `[in]`: stamp The stamp of a slot after its pop.

\*\*Returns:\*\* The lane index, seven bits.

### `embed_ticket_job_check` { #embed_ticket_job_check }

```c
EMBED_INLINE embed_word embed_ticket_job_check(const void *job);
```

The three-bit cyclic redundancy check of a job pointer: its bits as a polynomial, modulo x^3 + x + 1.

- `[in]`: job The job pointer.

\*\*Returns:\*\* The remainder, three bits.

> **Note:** The polynomial has period 7, so x^7 is 1 modulo it. The fold shifts by 7 times each power of two, from the byte count of the pointer down to 1. That sums the bits of each residue class modulo 7 into the low seven bits. The shifts are 56, 28, 14 and 7 places for a pointer of 64 bits, and 28, 14 and 7 for 32 bits. The remainder of the seven bits reads off the table x^3 = x + 1, x^4 = x^2 + x, x^5 = x^2 + x + 1, x^6 = x^2 + 1.

> **Note:** A single-bit error at any place changes the remainder, because no power of x is zero modulo the polynomial. A double-bit error changes it unless the two places are a multiple of seven apart.

### `embed_ticket_job_check_bits` { #embed_ticket_job_check_bits }

```c
EMBED_INLINE embed_word embed_ticket_job_check_bits(const void *job);
```

The check bits that the push stores for a job pointer: the check where the build verifies it, 0 where not.

- `[in]`: job The job pointer.

\*\*Returns:\*\* The check at [`EMBED_TICKET_CHECK_SHIFT`](#EMBED_TICKET_CHECK_SHIFT), or 0.

### `embed_ticket_fields_of_stamp` { #embed_ticket_fields_of_stamp }

```c
EMBED_INLINE embed_word embed_ticket_fields_of_stamp(embed_word stamp);
```

The epoch and ticket fields of a stamp, with the flag field cleared.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* The stamp with the 16 flag bits clear.

### `embed_ticket_ring_begin` { #embed_ticket_ring_begin }

```c
EMBED_INLINE void embed_ticket_ring_begin(EmbedTicketRing *ring, EmbedTicketRingSlot *slots, unsigned int shift);
```

Initializes the ring over a slot array of `2^shift` slots.

- `[out]`: ring The ring \[BORROWS\].
- `[in]`: slots The slot array \[BORROWS\], `2^shift` slots, zero-initialized, which the caller owns.
- `[in]`: shift The slot count as a power of two, at max [`EMBED_TICKET_RING_SHIFT_MAX`](#EMBED_TICKET_RING_SHIFT_MAX).

> **Note:** The slot count is a power of two by construction, so the index is a mask and not a modulo.

### `embed_ticket_ring_place` { #embed_ticket_ring_place }

```c
EMBED_INLINE embed_word embed_ticket_ring_place(const EmbedTicketRing *ring, embed_word stamp);
```

The slot index of the ticket of a stamp.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp.

\*\*Returns:\*\* The low shift bits of the ticket.

### `embed_ticket_ring_slot` { #embed_ticket_ring_slot }

```c
EMBED_INLINE EmbedTicketRingSlot *embed_ticket_ring_slot(const EmbedTicketRing *ring, embed_word stamp);
```

The slot of a stamp.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp.

\*\*Returns:\*\* The slot at the index of the ticket \[BORROWS\].

### `embed_ticket_ring_expected_stamp` { #embed_ticket_ring_expected_stamp }

```c
EMBED_INLINE embed_word embed_ticket_ring_expected_stamp(embed_word stamp, embed_word flags);
```

The expected stamp of a transition: the input stamp with the flag bits of the transitions before it.

- `[in]`: stamp The input stamp, as the caller holds it.
- `[in]`: flags The flag bits of the transitions before this one.

\*\*Returns:\*\* The expected stamp.

> **Note:** An input stamp with any flag bit set is invalid. The expected stamp then has the reserved bit set, and no slot word has it, so the compare fails. An input stamp with a corrupted ticket or epoch expects a word of another stamp, so the compare fails too.

### `embed_ticket_ring_guarded_stamp` { #embed_ticket_ring_guarded_stamp }

```c
EMBED_INLINE embed_word embed_ticket_ring_guarded_stamp(embed_word current_stamp, embed_word valid_mask);
```

The expected stamp of a transition that read the current stamp first. It is the current stamp where the state test passed, and the current stamp with the reserved bit inverted where it did not.

- `[in]`: `current_stamp` The current value of the slot word.
- `[in]`: `valid_mask` All ones where the state test passed, all zeros where it did not.

\*\*Returns:\*\* The expected stamp.

> **Note:** A word with the reserved bit inverted differs from the current stamp in one bit, so the compare fails. An expected stamp of zero does not serve: a zero current stamp would match it.

### `embed_ticket_ring_transition` { #embed_ticket_ring_transition }

```c
EMBED_INLINE embed_bool embed_ticket_ring_transition(EmbedTicketRingSlot *slot, embed_word expected_stamp,
                                                     embed_word new_stamp, embed_word *found_stamp);
```

One transition of a slot: a compare-and-swap from the expected stamp to the new stamp.

- `[in,out]`: slot The slot \[BORROWS\].
- `[in]`: `expected_stamp` The expected stamp.
- `[in]`: `new_stamp` The new stamp: the expected stamp with the flag bit of the transition.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\]: the expected stamp on success, the current stamp on failure.

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_grant` { #embed_ticket_ring_grant }

```c
EMBED_INLINE embed_bool embed_ticket_ring_grant(const EmbedTicketRing *ring, embed_word stamp, embed_word *found_stamp);
```

The grant transition: the stamp with the granted bit, from a zero word or from a released word.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp, flag bits clear.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** The expected stamp is the current stamp where it has the released bit, and zero where it does not. A word in any other state fails the compare. The next grant at an index reuses the released word.

> **Note:** An input stamp with a flag bit set adds the reserved bit to the expected stamp, so the compare fails.

### `embed_ticket_ring_push` { #embed_ticket_ring_push }

```c
EMBED_INLINE embed_bool embed_ticket_ring_push(const EmbedTicketRing *ring, embed_word stamp, void *job,
                                               embed_word *found_stamp);
```

The push transition: the job pointer stored, then the pushed bit added to the granted word.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a granted slot.
- `[in]`: job The job \[BORROWS\].
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** The store of the job pointer precedes the compare-and-swap, which has release semantics. A pop that succeeds after it has acquire semantics, so the pop reads the job pointer.

> **Note:** The new stamp carries the check bits of the job pointer where the build verifies them.

### `embed_ticket_ring_job_verify` { #embed_ticket_ring_job_verify }

```c
EMBED_INLINE embed_bool embed_ticket_ring_job_verify(const EmbedTicketRingSlot *slot, embed_word current_stamp,
                                                     void **job, embed_word *found_stamp);
```

The job pointer of a slot after a successful read transition, verified against the check bits of the word.

- `[in]`: slot The slot \[BORROWS\].
- `[in]`: `current_stamp` The stamp of the slot before the transition.
- `[out]`: job The job where the check matches \[BORROWS\], and NULL where it does not.
- `[out]`: `found_stamp` The stamp, with the reserved bit set where the check does not match \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) where the check matches, or where the build does not verify it.

> **Note:** The reserved bit in `found_stamp` marks a corrupted job pointer, which no contention produces, since no slot word ever holds that bit.

### `embed_ticket_ring_pop` { #embed_ticket_ring_pop }

```c
EMBED_INLINE embed_bool embed_ticket_ring_pop(const EmbedTicketRing *ring, embed_word stamp, unsigned int lane_place,
                                              void **job, embed_word *found_stamp);
```

The pop transition: the popped bit and the lane index added to the pushed word, and the job pointer read.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a pushed slot.
- `[in]`: `lane_place` The place of the lane of the pass of the caller, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) returned it.
- `[out]`: job The job on success \[BORROWS\], and NULL on failure.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\], with the reserved bit set where the job pointer failed its check.

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** Two pops of one stamp contend here. One succeeds, and the other finds the popped bit in `found_stamp`.

> **Note:** The expected stamp is the current stamp where the current stamp is the pushed word of the stamp, check bits aside. A pop whose compare succeeds but whose job pointer fails its check returns failure, with the reserved bit in `found_stamp`. The slot stays popped, so no caller runs the corrupted job.

### `embed_ticket_ring_replay` { #embed_ticket_ring_replay }

```c
EMBED_INLINE embed_bool embed_ticket_ring_replay(const EmbedTicketRing *ring, embed_word stamp, void **job,
                                                 embed_word *found_stamp);
```

The replay transition: the replayed bit added to the popped word, and the job pointer read again.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a popped slot.
- `[out]`: job The job on success \[BORROWS\], and NULL on failure.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** A rewind of a count of tickets is a replay at the ticket that count back. The expected stamp is the current stamp where the current stamp is the popped word of the stamp, lane and check bits aside. Any other current stamp fails the compare. The job pointer passes the same check as at the pop.

### `embed_ticket_ring_release` { #embed_ticket_ring_release }

```c
EMBED_INLINE embed_bool embed_ticket_ring_release(const EmbedTicketRing *ring, embed_word stamp,
                                                  embed_word *found_stamp);
```

The release transition: the released bit added to the popped word, replayed or not.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a popped slot.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** The writer releases after its drain, so no pass that started before the release reads the slot after it.

### `embed_ticket_ring_pass_grant` { #embed_ticket_ring_pass_grant }

```c
EMBED_INLINE embed_bool embed_ticket_ring_pass_grant(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                     const EmbedTicketRing *ring, embed_word stamp,
                                                     embed_word *found_stamp);
```

The grant transition inside a pass: enter at address, grant, leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The address of the worker, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp, flag bits clear.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_pass_push` { #embed_ticket_ring_pass_push }

```c
EMBED_INLINE embed_bool embed_ticket_ring_pass_push(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                    const EmbedTicketRing *ring, embed_word stamp, void *job,
                                                    embed_word *found_stamp);
```

The push transition inside a pass: enter at address, push, leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The address of the worker, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a granted slot.
- `[in]`: job The job \[BORROWS\].
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_pass_pop` { #embed_ticket_ring_pass_pop }

```c
EMBED_INLINE embed_bool embed_ticket_ring_pass_pop(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                   const EmbedTicketRing *ring, embed_word stamp, void **job,
                                                   embed_word *found_stamp);
```

The pop transition inside a pass: enter at address, pop with the lane index of that pass, leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The address of the worker, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a pushed slot.
- `[out]`: job The job on success \[BORROWS\], and NULL on failure.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_pass_replay` { #embed_ticket_ring_pass_replay }

```c
EMBED_INLINE embed_bool embed_ticket_ring_pass_replay(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                      const EmbedTicketRing *ring, embed_word stamp, void **job,
                                                      embed_word *found_stamp);
```

The replay transition inside a pass: enter at address, replay, leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The address of the worker, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a popped slot.
- `[out]`: job The job on success \[BORROWS\], and NULL on failure.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_drain_release` { #embed_ticket_ring_drain_release }

```c
EMBED_INLINE embed_bool embed_ticket_ring_drain_release(EmbedTurnstiles *set, const EmbedTicketRing *ring,
                                                        embed_word stamp, embed_word *found_stamp);
```

The release transition after a drain: drain set, then release the stamp.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a popped slot.
- `[out]`: `found_stamp` The stamp that the compare found in the slot \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** The writer calls this after it observed the popped bit and replaced what the slot referenced. The pass of that pop started before the drain, so the drain waits for it, and the release follows.
