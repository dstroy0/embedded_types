<!-- tools/api_pages.py writes this page from include/embed_ticket_ring.h. Edit the header, then run the tool. -->

# embed_ticket_ring.h

Source: [`include/embed_ticket_ring.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_ticket_ring.h)

A ring of job slots that a ticket indexes, written and read inside a pass through a turnstile.

The ring has `2^shift` slots. A slot holds a stamp and a job. The stamp is one 64-bit word: the epoch in the high 32 bits and the ticket in the low 32 bits. The ticket selects the slot: the low shift bits of the ticket are the slot's place. The ticket counts up without end and rolls at 32 bits, and the place rolls with it, because `2^shift` divides `2^32`.

> **Note:** The ring has no compare-and-swap and no guard of its own. The turnstile is the guard. The grantor of a ticket holds the turnstile while it grants, so each ticket has one grant. A push and a pop each run inside a pass: an entry, one write or one read, and a leave. The grantor also holds the depth of the ring below `2^shift`, so a push never writes a slot whose job a pop has not read.

> **Note:** A push and a pop have no branch. Each is one shift, one mask and two moves.

> **Note:** theory/`embedded_types`/EmbeddedTypes/TicketRing.lean proves the ring. It proves the stamp's fields, the place of a ticket, and the roll at `2^shift` and at `2^32`. It proves that the tickets of one window each have a place of their own. It proves that a pop reads what the push of its ticket wrote.

> _**Warning:**_ A stamp built from a ticket that no turnstile granted indexes a slot that another ticket owns. The ring does not detect that. The pop's stamp shows which ticket wrote the slot, which a replay log reads.

## Types

### `EmbedTicketRingSlot` { #EmbedTicketRingSlot }

```c
typedef struct
{
    embed_u64 stamp; /**< The stamp of the push, for a replay log. */
    void *job;       /**< The job. */
} EmbedTicketRingSlot;
EMBED_STATIC_ASSERT(sizeof(EmbedTicketRingSlot) == 16u, "a slot is two words");
```

One slot: the stamp of the push that wrote it, and the job.

- `stamp`: The stamp of the push, for a replay log.
- `job`: The job.

### `EmbedTicketRing` { #EmbedTicketRing }

```c
typedef struct
{
    EMBED_ALIGN(64) embed_u64 mask;  /**< 2^shift - 1. */
    EmbedTicketRingSlot *slots;      /**< 2^shift slots. */
} EmbedTicketRing;
```

A ring: its mask, `2^shift` - 1, on a cache line of its own, and its slots.

- `mask`: `2^shift` - 1.
- `slots`: `2^shift` slots.

## Macros

### `EMBED_TICKET_BITS` { #EMBED_TICKET_BITS }

```c
#define EMBED_TICKET_BITS 32u
#define EMBED_TICKET_MASK ((embed_u64)0xffffffffu)
#define EMBED_TICKET_EPOCH_SHIFT 32u
```

The bits of the ticket in a stamp, and where the epoch starts.

### `EMBED_TICKET_RING_SHIFT_MOST` { #EMBED_TICKET_RING_SHIFT_MOST }

```c
#define EMBED_TICKET_RING_SHIFT_MOST 31u
```

The most slots a ring has, as a shift: `2^EMBED_TICKET_RING_SHIFT_MOST` slots.

## Functions

### `embed_ticket_stamp` { #embed_ticket_stamp }

```c
EMBED_INLINE embed_u64 embed_ticket_stamp(embed_u32 epoch, embed_u32 ticket);
```

The stamp of epoch and ticket.

- `[in]`: epoch The high 32 bits.
- `[in]`: ticket The low 32 bits.

\*\*Returns:\*\* The stamp.

### `embed_ticket_of_stamp` { #embed_ticket_of_stamp }

```c
EMBED_INLINE embed_u32 embed_ticket_of_stamp(embed_u64 stamp);
```

The ticket of a stamp.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* Its low 32 bits.

### `embed_epoch_of_stamp` { #embed_epoch_of_stamp }

```c
EMBED_INLINE embed_u32 embed_epoch_of_stamp(embed_u64 stamp);
```

The epoch of a stamp.

- `[in]`: stamp The stamp.

\*\*Returns:\*\* Its high 32 bits.

### `embed_ticket_ring_begin` { #embed_ticket_ring_begin }

```c
EMBED_INLINE void embed_ticket_ring_begin(EmbedTicketRing *ring, EmbedTicketRingSlot *slots, unsigned int shift);
```

The ring begun over slots, `2^shift` of them.

- `[out]`: ring The ring \[BORROWS\].
- `[in]`: slots The slots \[BORROWS\], `2^shift` of them, which the caller owns.
- `[in]`: shift The count of slots as a power of two, at most [`EMBED_TICKET_RING_SHIFT_MOST`](#EMBED_TICKET_RING_SHIFT_MOST).

> **Note:** The count is a power of two by construction, so the mask wraps the ticket with no division.

### `embed_ticket_ring_place` { #embed_ticket_ring_place }

```c
EMBED_INLINE embed_u64 embed_ticket_ring_place(const EmbedTicketRing *ring, embed_u64 stamp);
```

The place of a stamp's ticket in a ring.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp.

\*\*Returns:\*\* The low shift bits of the ticket.

### `embed_ticket_ring_push` { #embed_ticket_ring_push }

```c
EMBED_INLINE void embed_ticket_ring_push(EmbedTicketRing *ring, embed_u64 stamp, void *job);
```

A push: the stamp and the job written at the ticket's place.

- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp of a ticket the turnstile granted.
- `[in]`: job The job \[BORROWS\].

> **Note:** The caller holds a pass through the turnstile over the ring. The push has no branch.

### `embed_ticket_ring_pop` { #embed_ticket_ring_pop }

```c
EMBED_INLINE void *embed_ticket_ring_pop(const EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *stamp_out);
```

A pop: the job at the ticket's place, and the stamp of the push that wrote it.

- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp of a ticket the turnstile granted.
- `[out]`: `stamp_out` The stamp of the push \[BORROWS\].

\*\*Returns:\*\* The job.

> **Note:** The caller holds a pass through the turnstile over the ring. The pop has no branch.

### `embed_ticket_ring_pushed` { #embed_ticket_ring_pushed }

```c
EMBED_INLINE void embed_ticket_ring_pushed(EmbedTurnstiles *set, embed_atomic_uptr_value address, EmbedTicketRing *ring,
                                           embed_u64 stamp, void *job);
```

A push inside a pass: an entry at address, the push, and the leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The worker's address, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in,out]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp of a ticket the turnstile granted.
- `[in]`: job The job \[BORROWS\].

### `embed_ticket_ring_popped` { #embed_ticket_ring_popped }

```c
EMBED_INLINE void *embed_ticket_ring_popped(EmbedTurnstiles *set, embed_atomic_uptr_value address,
                                            const EmbedTicketRing *ring, embed_u64 stamp, embed_u64 *stamp_out);
```

A pop inside a pass: an entry at address, the pop, and the leave.

- `[in,out]`: set The turnstiles over the ring \[BORROWS\].
- `[in]`: address The worker's address, as [`embed_turnstile_enter`](embed_turnstile.md#embed_turnstile_enter) takes it.
- `[in]`: ring The ring \[BORROWS\].
- `[in]`: stamp The stamp of a ticket the turnstile granted.
- `[out]`: `stamp_out` The stamp of the push \[BORROWS\].

\*\*Returns:\*\* The job.
