import Std.Tactic.BVDecide

/-!
# embedded_types, the ticket ring

`include/embed_ticket_ring.h` is a ring of 2^shift slots that a ticket indexes. A stamp is one 64-bit word: the epoch
in the high 32 bits and the ticket in the low 32 bits. The ring has no guard of its own: the turnstile grants each
ticket once and holds a push or a pop inside a pass, which `Manifold/LaneWord.lean` proves exclusive.

- `stamp_ticket`, `stamp_epoch` and `stamp_injective`: the two fields come back out of the stamp, and two stamps that
  agree are the same epoch and ticket.
- `place_lt`: the place of a ticket is below the count of slots.
- `place_wrap`: a ticket 2^shift on lands at the same place, so the ring wraps with no division.
- `place_roll`: the ticket's roll at 32 bits leaves the place as it is, because 2^shift divides 2^32.
- `place_mask`: the mask of the C, `ticket &&& (2^shift - 1)`, is that place.
- `window_distinct`: two tickets fewer than 2^shift apart land at places of their own. So the grantor that holds the
  depth of the ring below 2^shift gives no two live tickets one slot.
- `pop_push`, `pop_push_other` and `pop_pushes`: a pop reads what the push of its ticket wrote, a push leaves each
  other place as it was, and after the pushes of one window each pop reads its own job.
-/

namespace EmbeddedTypes.TicketRing

/-! ## The stamp -/

/-- The stamp of an epoch and a ticket, as `embed_ticket_stamp` builds it. -/
def stamp (epoch ticket : BitVec 32) : BitVec 64 :=
  (epoch.zeroExtend 64 <<< 32) ||| ticket.zeroExtend 64

/-- The ticket of a stamp, as `embed_ticket_of_stamp` reads it. -/
def ticketOf (s : BitVec 64) : BitVec 32 := (s &&& 0xffffffff#64).truncate 32

/-- The epoch of a stamp, as `embed_epoch_of_stamp` reads it. -/
def epochOf (s : BitVec 64) : BitVec 32 := (s >>> 32).truncate 32

theorem stamp_ticket (epoch ticket : BitVec 32) : ticketOf (stamp epoch ticket) = ticket := by
  simp only [stamp, ticketOf]
  bv_decide

theorem stamp_epoch (epoch ticket : BitVec 32) : epochOf (stamp epoch ticket) = epoch := by
  simp only [stamp, epochOf]
  bv_decide

theorem stamp_injective (e1 t1 e2 t2 : BitVec 32) (h : stamp e1 t1 = stamp e2 t2) : e1 = e2 ∧ t1 = t2 := by
  constructor
  · have := congrArg epochOf h
    rwa [stamp_epoch, stamp_epoch] at this
  · have := congrArg ticketOf h
    rwa [stamp_ticket, stamp_ticket] at this

/-! ## The place -/

/-- The place of a ticket in a ring of 2^shift slots: the ticket's low shift bits. -/
def place (shift ticket : Nat) : Nat := ticket % 2 ^ shift

theorem place_lt (shift ticket : Nat) : place shift ticket < 2 ^ shift :=
  Nat.mod_lt _ (Nat.two_pow_pos shift)

theorem place_wrap (shift ticket : Nat) : place shift (ticket + 2 ^ shift) = place shift ticket := by
  simp [place, Nat.add_mod_right]

/-- The ticket rolls at 32 bits, and 2^shift divides 2^32, so the place is the same before and after the roll. -/
theorem place_roll (shift ticket : Nat) (h : shift ≤ 32) : place shift (ticket % 2 ^ 32) = place shift ticket := by
  simp only [place]
  exact Nat.mod_mod_of_dvd ticket (Nat.pow_dvd_pow 2 h)

/-- The C masks with `2^shift - 1`, which is the modulus. -/
theorem place_mask (shift ticket : Nat) : ticket &&& (2 ^ shift - 1) = place shift ticket := by
  simp [place, Nat.and_two_pow_sub_one_eq_mod]

/-- Two tickets fewer than 2^shift apart stand at places of their own. -/
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

/-- After the pushes of a list whose tickets each stand at a place of their own, each pop reads its own job. -/
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
