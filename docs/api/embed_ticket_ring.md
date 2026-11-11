<!-- tools/api_pages.py writes this page from include/embed_ticket_ring.h. Edit the header, then run the tool. -->

# embed_ticket_ring.h

Source: [`include/embed_ticket_ring.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_ticket_ring.h)

A ring of job slots that a ticket indexes, with a state machine per slot in the flag bits of its stamp.

The ring has `2^shift` slots. A slot holds a stamp and a job pointer. The stamp is one atomic 64-bit word: the epoch in bits 48 to 63, the flag bits in bits 32 to 47, and the ticket in bits 0 to 31. The ticket indexes the slot: the low shift bits of the ticket are the slot index. The ticket is a 32-bit counter with wraparound, and the slot index wraps with it, because `2^shift` divides `2^32`.

> **Note:** Each transition of a slot is one compare-and-swap on the stamp word. The expected value is the stamp with the flag bits of the transitions before it. The new value adds the flag bit of the transition. So exactly one caller completes each transition, and a stamp that differs in any bit fails the compare.

> **Note:** The state sequence of a slot is grant, push, pop, replay, release. A replay is the read of a slot after its pop, which a rewind performs. A grant succeeds on a zero word or on a released word. A release succeeds after a pop. The next grant at the same index reuses the released word.

> **Note:** The pop writes the lane index of the pass that performed it into seven flag bits. The release keeps them.

> **Note:** The epoch is 16 bits. The grantor derives it from the host clock. The tick period covers the lifetime of a stamp, so the epoch does not wrap while the stamp is live. A compare succeeds only where the ticket and the epoch both match, and the ticket alone distinguishes any two live stamps.

> **Note:** The turnstile serializes the transitions with respect to the writer. A transition executes inside a pass, and the drain of a writer waits for every pass that started before it. The functions at the end of this header wrap each transition in a pass.

> **Note:** theory/`embedded_types`/EmbeddedTypes/TicketRing.lean proves the stamp fields and the slot index. Flags.lean proves the flag matrix: each transition preserves every other field, succeeds for exactly one caller, and fails on a corrupted word. Claim.lean proves the collision of two transitions and the drain after one.

> _**Warning:**_ A caller that accesses a slot without a successful compare-and-swap is outside every proof here.

## Types

### `EmbedTicketRingSlot` { #EmbedTicketRingSlot }

```c
typedef struct
{
    embed_atomic_u64 stamp; /**< The stamp with the flag bits of the transitions that completed. */
    void *job;              /**< The job [BORROWS]. */
} EmbedTicketRingSlot;
EMBED_STATIC_ASSERT(sizeof(EmbedTicketRingSlot) == 16u, "a slot is two words");
```

One slot: the stamp word, the state of the slot, and the job pointer.

- `stamp`: The stamp with the flag bits of the transitions that completed.
- `job`: The job \[BORROWS\].

### `EmbedTicketRing` { #EmbedTicketRing }

```c
typedef struct
{
    EMBED_ALIGN(64) embed_u64 mask; /**< 2^shift - 1. */
    EmbedTicketRingSlot *slots;     /**< 2^shift slots. */
} EmbedTicketRing;
```

A ring: its index mask, `2^shift` - 1, on a cache line of its own, and its slot array.

- `mask`: `2^shift` - 1.
- `slots`: `2^shift` slots.

## Macros

### `EMBED_TICKET_BITS` { #EMBED_TICKET_BITS }

```c
#define EMBED_TICKET_BITS 32u
#define EMBED_TICKET_MASK ((embed_u64)0xffffffffu)
#define EMBED_TICKET_FLAG_SHIFT 32u
#define EMBED_TICKET_FLAG_BITS 16u
#define EMBED_TICKET_FLAG_MASK ((embed_u64)0xffffu << EMBED_TICKET_FLAG_SHIFT)
#define EMBED_TICKET_EPOCH_SHIFT 48u
#define EMBED_TICKET_EPOCH_BITS 16u
```

The width of the ticket field, and the positions of the flag field and the epoch field.

### `EMBED_TICKET_GRANTED` { #EMBED_TICKET_GRANTED }

```c
#define EMBED_TICKET_GRANTED ((embed_u64)1u << 32)
#define EMBED_TICKET_PUSHED ((embed_u64)1u << 33)
#define EMBED_TICKET_POPPED ((embed_u64)1u << 34)
#define EMBED_TICKET_REPLAYED ((embed_u64)1u << 35)
#define EMBED_TICKET_RELEASED ((embed_u64)1u << 36)
#define EMBED_TICKET_LANE_SHIFT 40u
#define EMBED_TICKET_LANE_MASK ((embed_u64)0x7fu << EMBED_TICKET_LANE_SHIFT)
EMBED_STATIC_ASSERT(EMBED_TURNSTILE_LANES <= 128u, "the lane index of a pop fits seven flag bits");
```

The flag bits: one per transition of the slot state machine, and the lane index of the pop.

### `EMBED_TICKET_RESERVED` { #EMBED_TICKET_RESERVED }

```c
#define EMBED_TICKET_RESERVED ((embed_u64)1u << 47)
```

The reserved bit. No transition sets it, so an expected value with it set always fails the compare.

### `EMBED_TICKET_RING_SHIFT_MOST` { #EMBED_TICKET_RING_SHIFT_MOST }

```c
#define EMBED_TICKET_RING_SHIFT_MOST 31u
```

The maximum slot count as a shift: `2^EMBED_TICKET_RING_SHIFT_MOST` slots.

## Functions

### `embed_ticket_stamp` { #embed_ticket_stamp }

```c
EMBED_INLINE embed_u64 embed_ticket_stamp(embed_u16 epoch, embed_u32 ticket);
```

The stamp of an epoch and a ticket, with the flag bits clear.

- `[in]`: epoch Bits 48 to 63.
- `[in]`: ticket Bits 0 to 31.

\*\*Returns:\*\* The stamp.

### `embed_ticket_of_stamp` { #embed_ticket_of_stamp }

```c
EMBED_INLINE embed_u32 embed_ticket_of_stamp(embed_u64 stamp);
```

The ticket field of a stamp.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* Bits 0 to 31.

### `embed_epoch_of_stamp` { #embed_epoch_of_stamp }

```c
EMBED_INLINE embed_u16 embed_epoch_of_stamp(embed_u64 stamp);
```

The epoch field of a stamp.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* Bits 48 to 63.

### `embed_ticket_flags_of_stamp` { #embed_ticket_flags_of_stamp }

```c
EMBED_INLINE embed_u16 embed_ticket_flags_of_stamp(embed_u64 stamp);
```

The flag field of a stamp, the lane index included.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* Bits 32 to 47.

### `embed_ticket_lane_of_stamp` { #embed_ticket_lane_of_stamp }

```c
EMBED_INLINE unsigned int embed_ticket_lane_of_stamp(embed_u64 stamp);
```

The lane index of the pass that performed the pop, from the stamp of the slot.

- `[in]`: stamp The stamp of a slot after its pop.

\*\*Returns:\*\* The lane index, seven bits.

### `embed_ticket_fields_of_stamp` { #embed_ticket_fields_of_stamp }

```c
EMBED_INLINE embed_u64 embed_ticket_fields_of_stamp(embed_u64 stamp);
```

The epoch and ticket fields of a stamp, with the flag field cleared.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* The stamp with bits 32 to 47 clear.

### `embed_ticket_ring_begin` { #embed_ticket_ring_begin }

```c
EMBED_INLINE void embed_ticket_ring_begin(EmbedTicketRing *ring, EmbedTicketRingSlot *slots, unsigned int shift);
```

Initializes the ring over a slot array of `2^shift` slots.

- `[out]`: ring The ring \[BORROWS\].
- `[in]`: slots The slot array \[BORROWS\], `2^shift` slots, zero-initialized, which the caller owns.
- `[in]`: shift The slot count as a power of two, at most [`EMBED_TICKET_RING_SHIFT_MOST`](#EMBED_TICKET_RING_SHIFT_MOST).

> **Note:** The slot count is a power of two by construction, so the index is a mask and not a modulo.

### `embed_ticket_ring_place` { #embed_ticket_ring_place }

```c
EMBED_INLINE embed_u64 embed_ticket_ring_place(const EmbedTicketRing *ring, embed_u64 stamp);
```

The slot index of the ticket of a stamp.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp.

\*\*Returns:\*\* The low shift bits of the ticket.

### `embed_ticket_ring_slot` { #embed_ticket_ring_slot }

```c
EMBED_INLINE EmbedTicketRingSlot *embed_ticket_ring_slot(const EmbedTicketRing *ring, embed_u64 stamp);
```

The slot of a stamp.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp.

\*\*Returns:\*\* The slot at the index of the ticket \[BORROWS\].

### `embed_ticket_ring_expected` { #embed_ticket_ring_expected }

```c
EMBED_INLINE embed_u64 embed_ticket_ring_expected(embed_u64 stamp, embed_u64 flags);
```

The expected value of a transition: the input stamp with the flag bits of the transitions before it.

- `[in]`: stamp The input stamp, as the caller holds it.
- `[in]`: flags The flag bits of the transitions before this one.

\*\*Returns:\*\* The expected value.

> **Note:** An input stamp with any flag bit set is invalid. The expected value then has the reserved bit set, and no slot word has it, so the compare fails. An input stamp with a corrupted ticket or epoch expects a word of another stamp, so the compare fails too.

### `embed_ticket_ring_transition` { #embed_ticket_ring_transition }

```c
EMBED_INLINE embed_bool embed_ticket_ring_transition(EmbedTicketRingSlot *slot, embed_u64 expected, embed_u64 value,
                                                     embed_u64 *seen);
```

One transition of a slot: a compare-and-swap from the expected value to the new value.

- `[in,out]`: slot The slot \[BORROWS\].
- `[in]`: expected The expected value.
- `[in]`: value The new value: the expected value with the flag bit of the transition.
- `[out]`: seen The value found \[BORROWS\]: the expected value on success, the current value on failure.

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_grant` { #embed_ticket_ring_grant }

```c
EMBED_INLINE embed_bool embed_ticket_ring_grant(EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *seen);
```

The grant transition: the stamp with the granted bit, from a zero word or from a released word.

- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp, flag bits clear.
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** The expected value is the current value where it has the released bit, and zero where it does not. A word in any other state fails the compare. The next grant at an index reuses the released word.

> **Note:** An input stamp with a flag bit set adds the reserved bit to the expected value, so the compare fails.

### `embed_ticket_ring_push` { #embed_ticket_ring_push }

```c
EMBED_INLINE embed_bool embed_ticket_ring_push(EmbedTicketRing *ring, embed_u64 stamp, void *job, embed_u64 *seen);
```

The push transition: the job pointer stored, then the pushed bit added to the granted word.

- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a granted slot.
- `[in]`: job The job \[BORROWS\].
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** The store of the job pointer precedes the compare-and-swap, which has release semantics. A pop that succeeds after it has acquire semantics, so the pop reads the job pointer.

### `embed_ticket_ring_pop` { #embed_ticket_ring_pop }

```c
EMBED_INLINE embed_bool embed_ticket_ring_pop(EmbedTicketRing *ring, embed_u64 stamp, unsigned int lane, void **job,
                                              embed_u64 *seen);
```

The pop transition: the popped bit and the lane index added to the pushed word, and the job pointer read.

- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a pushed slot.
- `[in]`: lane The lane index of the pass of the caller, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) returned it.
- `[out]`: job The job on success \[BORROWS\], and NULL on failure.
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** Two pops of one stamp contend here. One succeeds, and the other finds the popped bit in seen.

### `embed_ticket_ring_replay` { #embed_ticket_ring_replay }

```c
EMBED_INLINE embed_bool embed_ticket_ring_replay(EmbedTicketRing *ring, embed_u64 stamp, void **job, embed_u64 *seen);
```

The replay transition: the replayed bit added to the popped word, and the job pointer read again.

- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a popped slot.
- `[out]`: job The job on success \[BORROWS\], and NULL on failure.
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** A rewind of k tickets is a replay at the ticket k back. The expected value is the current value where the current value is the popped word of the stamp, lane bits aside. Any other current value fails the compare.

### `embed_ticket_ring_release` { #embed_ticket_ring_release }

```c
EMBED_INLINE embed_bool embed_ticket_ring_release(EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *seen);
```

The release transition: the released bit added to the popped word, replayed or not.

- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a popped slot.
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** The writer releases after its drain, so no pass that started before the release reads the slot after it.

### `embed_ticket_ring_granted` { #embed_ticket_ring_granted }

```c
EMBED_INLINE embed_bool embed_ticket_ring_granted(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                  EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *seen);
```

The grant transition inside a pass: enter at address, grant, leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The address of the worker, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp, flag bits clear.
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_pushed` { #embed_ticket_ring_pushed }

```c
EMBED_INLINE embed_bool embed_ticket_ring_pushed(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                 EmbedTicketRing *ring, embed_u64 stamp, void *job, embed_u64 *seen);
```

The push transition inside a pass: enter at address, push, leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The address of the worker, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a granted slot.
- `[in]`: job The job \[BORROWS\].
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_popped` { #embed_ticket_ring_popped }

```c
EMBED_INLINE embed_bool embed_ticket_ring_popped(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                 EmbedTicketRing *ring, embed_u64 stamp, void **job, embed_u64 *seen);
```

The pop transition inside a pass: enter at address, pop with the lane index of that pass, leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The address of the worker, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a pushed slot.
- `[out]`: job The job on success \[BORROWS\], and NULL on failure.
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_replayed` { #embed_ticket_ring_replayed }

```c
EMBED_INLINE embed_bool embed_ticket_ring_replayed(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                                   EmbedTicketRing *ring, embed_u64 stamp, void **job,
                                                   embed_u64 *seen);
```

The replay transition inside a pass: enter at address, replay, leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The address of the worker, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a popped slot.
- `[out]`: job The job on success \[BORROWS\], and NULL on failure.
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

### `embed_ticket_ring_released` { #embed_ticket_ring_released }

```c
EMBED_INLINE embed_bool embed_ticket_ring_released(EmbedTurnstiles *set, EmbedTicketRing *ring, embed_u64 stamp,
                                                   embed_u64 *seen);
```

The release transition after a drain: drain set, then release the stamp.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The input stamp of a popped slot.
- `[out]`: seen The value found \[BORROWS\].

\*\*Returns:\*\* [`EMBED_TRUE`](embed_types.md#EMBED_TRUE) on success.

> **Note:** The writer calls this after it observed the popped bit and replaced what the slot referenced. The pass of that pop started before the drain, so the drain waits for it, and the release follows.
