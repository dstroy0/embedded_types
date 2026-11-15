import Std.Tactic.BVDecide
import EmbeddedTypes.Flags

/-!
# embedded_types, the ticket ring

`include/embed_ticket_ring.h` is a ring of 2^shift slots that a ticket indexes. A stamp is one machine word. The
ticket is in the low t bits, the flag field of `Flags.lean` in the 16 above it, and the epoch in the high bits.
`Flags.rows` holds the widths of each row. The turnstile grants each ticket once and holds each transition of a slot
inside a pass, which `Manifold/LaneWord.lean` proves exclusive.

- `stamp_value_ticket`, `stamp_value_epoch`, `stamp_value_flags`, `stamp_value_fits` and `stamp_value_injective`.
  These theorems hold on a word of any width n = e + 16 + t. The two fields come back out of the stamp, the flag bits
  are clear, and the stamp fits the word. Two stamps that agree are the same epoch and ticket.

- `stamp_ticket`, `stamp_epoch`, `stamp_flags_clear` and `stamp_injective`. The same facts on the words of each row,
  as the C computes them. Each field keeps only its own bits, so a wide epoch or ticket sets no flag bit.

- `stamp_sum`, `stamp_parts_bound` and `stamp_is_value`. On each row, the two parts of the stamp share no bit and
  each fits its field. So the stamp word is the stamp number of the theorems on any width.

- `place_lt`. The place of a ticket is below the count of slots.

- `place_wrap`. A ticket 2^shift on lands at the same place, so the ring wraps with no division.

- `place_roll` and `shift_max_rolls`. The roll of the ticket at its width t leaves the place as it is, because
  2^shift divides 2^t. Every shift that the ring takes is below t.

- `place_mask`. The mask of the C, `ticket &&& (2^shift - 1)`, is that place.

- `window_distinct`. Two tickets fewer than 2^shift apart land at places of their own. So a grantor that holds the
  depth of the ring below 2^shift gives no two live tickets one slot.

- `pop_push`, `pop_push_other` and `pop_pushes`. A pop reads what the push of its ticket wrote. A push leaves each
  other place as it was. After the pushes of one window, each pop reads its own job.
-/

namespace EmbeddedTypes.TicketRing

/-! ## The stamp on a word of any width -/

/-- The stamp of an epoch and a ticket as a number, on a word of a ticket width of t bits. The epoch is above the
ticket and the 16 flag bits. -/
def stampValue (t epoch ticket : Nat) : Nat := epoch * 2 ^ (t + 16) + ticket

/-- The ticket comes back out of the stamp as its low t bits. -/
theorem stamp_value_ticket (t epoch ticket : Nat) (h : ticket < 2 ^ t) :
    stampValue t epoch ticket % 2 ^ t = ticket := by
  rw [stampValue, Nat.pow_add, Nat.mul_comm (2 ^ t), ← Nat.mul_assoc, Nat.add_comm, Nat.add_mul_mod_self_right,
    Nat.mod_eq_of_lt h]

/-- The epoch comes back out of the stamp as its bits from t + 16 up. -/
theorem stamp_value_epoch (t epoch ticket : Nat) (h : ticket < 2 ^ t) :
    stampValue t epoch ticket / 2 ^ (t + 16) = epoch := by
  have hlt : ticket < 2 ^ (t + 16) := Nat.lt_of_lt_of_le h (Nat.pow_le_pow_right (by decide) (Nat.le_add_right t 16))
  rw [stampValue, Nat.add_comm, Nat.add_mul_div_right _ _ (Nat.two_pow_pos _), Nat.div_eq_of_lt hlt, Nat.zero_add]

/-- The 16 flag bits of a stamp are clear. -/
theorem stamp_value_flags (t epoch ticket : Nat) (h : ticket < 2 ^ t) :
    stampValue t epoch ticket / 2 ^ t % 2 ^ 16 = 0 := by
  rw [stampValue, Nat.pow_add, Nat.mul_comm (2 ^ t), ← Nat.mul_assoc, Nat.add_comm,
    Nat.add_mul_div_right _ _ (Nat.two_pow_pos _), Nat.div_eq_of_lt h, Nat.zero_add, Nat.mul_mod_left]

/-- On a word of n = e + 16 + t bits, an epoch below 2^e and a ticket below 2^t give a stamp below 2^n. So the
stamp fits the word on every width whose fields fill it. -/
theorem stamp_value_fits (e t epoch ticket : Nat) (he : epoch < 2 ^ e) (h : ticket < 2 ^ t) :
    stampValue t epoch ticket < 2 ^ (e + 16 + t) := by
  have hpow : 2 ^ (e + 16 + t) = 2 ^ e * 2 ^ (t + 16) := by
    rw [← Nat.pow_add]
    congr 1
    omega
  have hstep : (epoch + 1) * 2 ^ (t + 16) ≤ 2 ^ e * 2 ^ (t + 16) := Nat.mul_le_mul_right _ he
  have hlt : ticket < 2 ^ (t + 16) := Nat.lt_of_lt_of_le h (Nat.pow_le_pow_right (by decide) (Nat.le_add_right t 16))
  rw [hpow, stampValue]
  rw [Nat.add_mul, Nat.one_mul] at hstep
  omega

/-- Two stamps that agree are the same epoch and ticket, on a word of any width. -/
theorem stamp_value_injective (t e1 e2 k1 k2 : Nat) (h1 : k1 < 2 ^ t) (h2 : k2 < 2 ^ t)
    (h : stampValue t e1 k1 = stampValue t e2 k2) : e1 = e2 ∧ k1 = k2 := by
  constructor
  · rw [← stamp_value_epoch t e1 k1 h1, ← stamp_value_epoch t e2 k2 h2, h]
  · rw [← stamp_value_ticket t e1 k1 h1, ← stamp_value_ticket t e2 k2 h2, h]

/-! ## The stamp on each row of the C -/

/-- The stamp of an epoch and a ticket, as `embed_ticket_stamp` builds it: each field keeps only its own bits. -/
def stamp (w t : Nat) (epoch ticket : BitVec w) : BitVec w :=
  ((epoch &&& Flags.epochMask w t) <<< (t + 16)) ||| (ticket &&& Flags.ticketMask w t)

/-- The ticket of a stamp, as `embed_ticket_of_stamp` reads it. -/
def ticketOf (w t : Nat) (s : BitVec w) : BitVec w := s &&& Flags.ticketMask w t

/-- The epoch of a stamp, as `embed_epoch_of_stamp` reads it. -/
def epochOf (t : Nat) (s : BitVec w) : BitVec w := s >>> (t + 16)

/-- The stamp of a row in the goal, as constants for `bv_decide`. -/
macro "stamp_unfold" : tactic =>
  `(tactic| (simp only [stamp, ticketOf, epochOf] at *; layout_unfold))

theorem stamp_ticket (w t : Nat) (hr : (w, t) ∈ Flags.rows) (epoch ticket : BitVec w) :
    ticketOf w t (stamp w t epoch ticket) = ticket &&& Flags.ticketMask w t := by
  rows_cases hr <;> stamp_unfold <;> bv_decide

theorem stamp_epoch (w t : Nat) (hr : (w, t) ∈ Flags.rows) (epoch ticket : BitVec w) :
    epochOf t (stamp w t epoch ticket) = epoch &&& Flags.epochMask w t := by
  rows_cases hr <;> stamp_unfold <;> bv_decide

/-- A wide epoch or ticket sets no flag bit, so a stamp is its own fields. -/
theorem stamp_flags_clear (w t : Nat) (hr : (w, t) ∈ Flags.rows) (epoch ticket : BitVec w) :
    stamp w t epoch ticket &&& Flags.flagMask w t = 0 ∧ Flags.fields t (stamp w t epoch ticket) = stamp w t epoch ticket := by
  rows_cases hr <;> stamp_unfold <;> bv_decide

/-- Two stamps that agree are the same epoch and ticket, in the bits that the stamp keeps of each. -/
theorem stamp_injective (w t : Nat) (hr : (w, t) ∈ Flags.rows) (e1 e2 k1 k2 : BitVec w)
    (h : stamp w t e1 k1 = stamp w t e2 k2) :
    e1 &&& Flags.epochMask w t = e2 &&& Flags.epochMask w t ∧ k1 &&& Flags.ticketMask w t = k2 &&& Flags.ticketMask w t := by
  constructor
  · rw [← stamp_epoch w t hr e1 k1, ← stamp_epoch w t hr e2 k2, h]
  · rw [← stamp_ticket w t hr e1 k1, ← stamp_ticket w t hr e2 k2, h]

/-- The two parts of a stamp share no bit, so the stamp is their sum. -/
theorem stamp_sum (w t : Nat) (hr : (w, t) ∈ Flags.rows) (epoch ticket : BitVec w) :
    stamp w t epoch ticket = ((epoch &&& Flags.epochMask w t) <<< (t + 16)) + (ticket &&& Flags.ticketMask w t) := by
  rw [stamp, BitVec.add_eq_or_of_and_eq_zero]
  rows_cases hr <;> layout_unfold <;> bv_decide

/-- A word below a constant that fits the word is below it as a number. -/
theorem toNat_below {x : BitVec w} {n : Nat} (hn : n < 2 ^ w) (h : x < BitVec.ofNat w n) : x.toNat < n := by
  rw [BitVec.lt_def, BitVec.toNat_ofNat, Nat.mod_eq_of_lt hn] at h
  exact h

/-- The epoch part of a stamp is below 2^e, and the ticket part is below 2^t. -/
theorem stamp_parts_bound (w t : Nat) (hr : (w, t) ∈ Flags.rows) (epoch ticket : BitVec w) :
    (epoch &&& Flags.epochMask w t).toNat < 2 ^ Flags.epochBits w t ∧
      (ticket &&& Flags.ticketMask w t).toNat < 2 ^ t := by
  rows_cases hr <;> layout_unfold <;> simp only [Nat.reducePow] <;>
    exact ⟨toNat_below (by decide) (by bv_decide), toNat_below (by decide) (by bv_decide)⟩

/-- On each row, the stamp as a word is the stamp as a number, so the theorems on any width hold on the row. -/
theorem stamp_is_value (w t : Nat) (hr : (w, t) ∈ Flags.rows) (epoch ticket : BitVec w) :
    (stamp w t epoch ticket).toNat =
      stampValue t (epoch &&& Flags.epochMask w t).toNat (ticket &&& Flags.ticketMask w t).toNat := by
  obtain ⟨he, hk⟩ := stamp_parts_bound w t hr epoch ticket
  have hfit := stamp_value_fits _ _ _ _ he hk
  have hw : Flags.epochBits w t + 16 + t = w := by
    have := Flags.rows_fill (w, t) hr
    simp only at this
    omega
  rw [hw] at hfit
  have hhigh : (epoch &&& Flags.epochMask w t).toNat * 2 ^ (t + 16) < 2 ^ w := by
    simp only [stampValue] at hfit
    omega
  rw [stamp_sum w t hr, BitVec.toNat_add, BitVec.toNat_shiftLeft, Nat.shiftLeft_eq, Nat.mod_eq_of_lt hhigh]
  exact Nat.mod_eq_of_lt hfit

/-! ## The place -/

/-- The place of a ticket in a ring of 2^shift slots: the ticket's low shift bits. -/
def place (shift ticket : Nat) : Nat := ticket % 2 ^ shift

theorem place_lt (shift ticket : Nat) : place shift ticket < 2 ^ shift :=
  Nat.mod_lt _ (Nat.two_pow_pos shift)

theorem place_wrap (shift ticket : Nat) : place shift (ticket + 2 ^ shift) = place shift ticket := by
  simp [place, Nat.add_mod_right]

/-- The ticket rolls at its width of t bits, and 2^shift divides 2^t, so the place is the same before and after the
roll. -/
theorem place_roll (t shift ticket : Nat) (h : shift ≤ t) : place shift (ticket % 2 ^ t) = place shift ticket := by
  simp only [place]
  exact Nat.mod_mod_of_dvd ticket (Nat.pow_dvd_pow 2 h)

/-- On each row, `EMBED_TICKET_RING_SHIFT_MAX` is the ticket width less one, so every shift that the ring takes
meets `place_roll`. -/
theorem shift_max_rolls : ∀ r ∈ Flags.rows, r.2 - 1 ≤ r.2 ∧ 0 < r.2 - 1 := by decide

/-- The C masks with `2^shift - 1`, which is the modulus. -/
theorem place_mask (shift ticket : Nat) : ticket &&& (2 ^ shift - 1) = place shift ticket := by
  simp [place, Nat.and_two_pow_sub_one_eq_mod]

/-- Two tickets fewer than 2^shift apart are at places of their own. -/
theorem window_distinct (shift t1 t2 : Nat) (hlt : t1 < t2) (hwin : t2 - t1 < 2 ^ shift) :
    place shift t1 ≠ place shift t2 := by
  intro heq
  simp only [place] at heq
  have hzero : (t2 - t1) % 2 ^ shift = 0 := Nat.sub_mod_eq_zero_of_mod_eq heq.symm
  rw [Nat.mod_eq_of_lt hwin] at hzero
  omega

/-! ## The slots -/

/-- One slot: the stamp of the push that wrote it, and the job, an address. -/
structure Slot where
  stamp : Nat
  job : Nat
  deriving DecidableEq

/-- A ring of 2^shift slots, by place. -/
def Ring := Nat → Slot

/-- A push: the slot at the ticket's place written, as `embed_ticket_ring_push` writes it. -/
def push (shift : Nat) (r : Ring) (ticket job : Nat) : Ring :=
  fun p => if p = place shift ticket then ⟨ticket, job⟩ else r p

/-- A pop: the slot at the ticket's place read, as `embed_ticket_ring_pop` reads it. -/
def pop (shift : Nat) (r : Ring) (ticket : Nat) : Slot := r (place shift ticket)

theorem pop_push (shift : Nat) (r : Ring) (ticket job : Nat) :
    pop shift (push shift r ticket job) ticket = ⟨ticket, job⟩ := by
  simp [pop, push]

theorem pop_push_other (shift : Nat) (r : Ring) (t1 job t2 : Nat) (h : place shift t1 ≠ place shift t2) :
    pop shift (push shift r t1 job) t2 = pop shift r t2 := by
  simp [pop, push, Ne.symm h]

/-- The pushes of a list of tickets and jobs, in order. -/
def pushes (shift : Nat) (r : Ring) : List (Nat × Nat) → Ring
  | [] => r
  | (t, j) :: rest => pushes shift (push shift r t j) rest

/-- The pushes of tickets at other places leave the pop of a ticket as it was. -/
theorem pop_pushes_other (shift : Nat) (r : Ring) (l : List (Nat × Nat)) (t : Nat)
    (hother : ∀ s ∈ l, place shift s.1 ≠ place shift t) : pop shift (pushes shift r l) t = pop shift r t := by
  induction l generalizing r with
  | nil => rfl
  | cons s more ih =>
    simp only [pushes]
    rw [ih (push shift r s.1 s.2) (fun x hx => hother x (List.mem_cons_of_mem _ hx))]
    exact pop_push_other shift r s.1 s.2 t (hother s (List.mem_cons_self ..))

/-- After the pushes of a list whose tickets each are at a place of their own, each pop reads its own job. -/
theorem pop_pushes (shift : Nat) (r : Ring) (l : List (Nat × Nat)) (t j : Nat) (hmem : (t, j) ∈ l)
    (hdistinct : l.Pairwise (fun a b => place shift a.1 ≠ place shift b.1)) :
    pop shift (pushes shift r l) t = ⟨t, j⟩ := by
  induction l generalizing r with
  | nil => simp at hmem
  | cons head rest ih =>
    obtain ⟨t0, j0⟩ := head
    rw [List.pairwise_cons] at hdistinct
    obtain ⟨hhead, hrest⟩ := hdistinct
    simp only [pushes]
    rcases List.mem_cons.mp hmem with heq | hin
    · cases heq
      rw [pop_pushes_other shift _ rest t (fun s hs => (hhead s hs).symm), pop_push]
    · exact ih (push shift r t0 j0) hin hrest

end EmbeddedTypes.TicketRing
