import Std.Tactic.BVDecide
import EmbeddedTypes.TicketRing
import EmbeddedTypes.Manifold.Drain

/-!
# embedded_types, the claim of a slot

A worker acts on a slot only after one read-modify-write on it succeeds. A push is a compare-and-swap from empty to
the stamp. A claim is a compare-and-swap from the stamp to the stamp with the consumed mark and the id of the pass.
Each one succeeds for one caller and fails for every other, which `Atomic.lean` proves of the compare-and-swap of
each arm. The claim runs inside a pass through the turnstile, and the lane word gives one holder per lane
(`Manifold/LaneWord.lean`).

The first section is the stamp. A flip of any set of bits in a stamp changes it. So the compare of the echoed stamp
against the stamp that a worker holds fails on any flip in the field. The stamp of a pop does not depend on the
job. So a flip in the job is outside the stamp, and the job must carry its own check.

The second section is the slot. A flip in the local ticket of a worker can land on the live stamp of another
ticket, and the compare passes there. The claim closes that case. The slot admits one successful claim. So the
flipped worker and the owner collide on it, and one of the two fails. The fault surfaces at that failure, and no
job runs twice.

The third section is the lane. A claim step needs the lane in the hands of the pass that claims. So each consumed
slot names a pass that held a lane at its claim. The drain theorem applies to that pass. A drain that waits on a
lane that a pass holds does not end before that pass ends.

The theorems:

- `flip_changes` and `bit_flip_changes`. A nonzero mask, and so any one bit, changes a stamp.

- `pop_stamp_free_of_job`. The stamp of a pop is the same after any change to the job at any place.

- `push_once`, `claim_pushed`, `claim_other_fails`, `claim_once` and `claims_at_most_one`. A push lands on an empty
  slot alone. A claim with the stamp of the slot lands on a pushed slot. A claim with another stamp fails. After one
  claim lands, no claim lands. Any sequence of claims on one slot lands at most once.

- `collision_detected`. Two claims on one pushed slot, in either order: the first lands and the second fails.

- `claim_in_pass` and `claim_drained`. A consumed slot names a pass that held the lane at the claim. A drain that
  read that lane held then waits until that pass ends.

- `drain_waits_while_held`. A drain never reads a lane over while the pass that it read in the lane has not ended.
-/

namespace EmbeddedTypes.Claim

/-! ## The stamp -/

/-- A flip of any nonzero set of bits changes a stamp. -/
theorem flip_changes (x m : BitVec 64) (h : m ≠ 0) : x ^^^ m ≠ x := by
  bv_decide

/-- A flip of one bit, at any place below the width, changes a stamp. -/
theorem bit_flip_changes (x i : BitVec 64) (hi : i < 64) : x ^^^ ((1 : BitVec 64) <<< i) ≠ x := by
  bv_decide

/-- The ring with the job at one place changed, and nothing else. -/
def jobChanged (r : TicketRing.Ring) (at_ job : Nat) : TicketRing.Ring :=
  fun p => if p = at_ then { r p with job := job } else r p

/-- The stamp of a pop is the same after any change to a job. -/
theorem pop_stamp_free_of_job (shift : Nat) (r : TicketRing.Ring) (at_ job ticket : Nat) :
    (TicketRing.pop shift (jobChanged r at_ job) ticket).stamp = (TicketRing.pop shift r ticket).stamp := by
  simp only [TicketRing.pop, jobChanged]
  split <;> rfl

/-! ## The slot -/

/-- A slot: empty, pushed with a stamp, or consumed with the stamp and the id of the pass that claimed it. -/
inductive Slot where
  | empty
  | pushed (stamp : Nat)
  | consumed (stamp : Nat) (pass : Nat)
  deriving DecidableEq

/-- A push: a compare-and-swap from empty to the stamp. The result is the slot after and whether the swap landed. -/
def push (slot : Slot) (stamp : Nat) : Slot × Bool :=
  match slot with
  | .empty => (.pushed stamp, true)
  | _ => (slot, false)

/-- A claim by pass p with stamp s: a compare-and-swap from pushed s to consumed s p. -/
def claim (slot : Slot) (stamp pass : Nat) : Slot × Bool :=
  match slot with
  | .pushed t => if t = stamp then (.consumed stamp pass, true) else (slot, false)
  | _ => (slot, false)

/-- A push lands on an empty slot alone. A second push on the same slot fails. -/
theorem push_once (slot : Slot) (s t : Nat) : (push (push slot s).1 t).2 = false := by
  cases slot <;> simp [push]

/-- A claim with the stamp of a pushed slot lands. -/
theorem claim_pushed (s p : Nat) : claim (.pushed s) s p = (.consumed s p, true) := by
  simp [claim]

/-- A claim with another stamp than the slot holds fails and changes nothing. -/
theorem claim_other_fails (s t p : Nat) (h : t ≠ s) : claim (.pushed s) t p = (.pushed s, false) := by
  simp [claim, Ne.symm h]

/-- After one claim lands, no claim with any stamp by any pass lands. -/
theorem claim_once (slot : Slot) (s p t q : Nat) (h : (claim slot s p).2 = true) :
    (claim (claim slot s p).1 t q).2 = false := by
  cases slot with
  | empty => simp [claim] at h
  | pushed u =>
    by_cases hu : u = s
    · subst hu
      simp [claim]
    · simp [claim, hu] at h
  | consumed u r => simp [claim] at h

/-- The claims of a list on one slot, in order, and the count of those that landed. -/
def claims (slot : Slot) : List (Nat × Nat) → Slot × Nat
  | [] => (slot, 0)
  | (s, p) :: rest =>
    let step := claim slot s p
    let after := claims step.1 rest
    (after.1, after.2 + (if step.2 then 1 else 0))

/-- No claim lands on a consumed slot. -/
theorem claim_consumed_fails (s p t q : Nat) : (claim (.consumed s p) t q).2 = false := by
  simp [claim]

/-- No claim of a list lands on a consumed slot. -/
theorem claims_consumed_zero (s p : Nat) (l : List (Nat × Nat)) : (claims (.consumed s p) l).2 = 0 := by
  induction l with
  | nil => rfl
  | cons head rest ih =>
    obtain ⟨t, q⟩ := head
    simp [claims, claim, ih]

/-- Any sequence of claims on one slot lands at most once. -/
theorem claims_at_most_one (slot : Slot) (l : List (Nat × Nat)) : (claims slot l).2 ≤ 1 := by
  induction l generalizing slot with
  | nil => simp [claims]
  | cons head rest ih =>
    obtain ⟨s, p⟩ := head
    cases slot with
    | empty => simp [claims, claim, ih]
    | pushed u =>
      simp only [claims, claim]
      split
      · simp [claims_consumed_zero]
      · simpa using ih (.pushed u)
    | consumed u r => simp [claims, claim, claims_consumed_zero]

/-- Two claims on one pushed slot with its stamp, in either order: the first lands and the second fails. The owner
of the ticket and a worker with a flipped ticket collide here on one slot. The one that fails raises the fault. -/
theorem collision_detected (s p q : Nat) :
    (claim (.pushed s) s p).2 = true ∧ (claim (claim (.pushed s) s p).1 s q).2 = false := by
  refine ⟨by simp [claim], ?_⟩
  exact claim_once _ s p s q (by simp [claim])

/-! ## The lane -/

/-- One lane and one slot. -/
structure State where
  lane : Manifold.Drain.State
  slot : Slot

/-- A step: an operation on the lane word, or a claim by the pass that holds the lane. -/
inductive Step : State → State → Prop where
  /-- Any step of the lane word, with the slot as it was. -/
  | lane {s : State} {t : Manifold.Drain.State} (st : Manifold.Drain.Step s.lane t) : Step s { s with lane := t }
  /-- A claim by pass p, which holds the lane, with stamp v. -/
  | claim {s : State} (p v : Nat) (h : s.lane.holder = some p) :
      Step s { s with slot := (claim s.slot v p).1 }

/-- A claim step that lands names the pass that held the lane at the claim. -/
theorem claim_in_pass {s t : State} (st : Step s t) (v p : Nat) (hs : s.slot ≠ .consumed v p)
    (ht : t.slot = .consumed v p) : s.lane.holder = some p := by
  cases st with
  | lane _ => exact absurd ht hs
  | claim q w h =>
    simp only at ht
    cases hslot : s.slot with
    | empty =>
      rw [hslot] at ht
      simp [claim] at ht
    | pushed u =>
      rw [hslot] at ht
      by_cases hu : u = w
      · subst hu
        simp [claim] at ht
        rw [← ht.2]
        exact h
      · simp [claim, hu] at ht
    | consumed u r =>
      rw [hslot] at ht
      simp only [claim] at ht
      exact absurd (hslot.trans ht) hs

/-- A drain that read the lane held by the pass of a claim, at the claim, waits until that pass ends. The read at
state u is over only where p ended, by the drain theorem of the lane. -/
theorem claim_drained {s t : State} (st : Step s t) (v p : Nat) (i : Manifold.Drain.Inv s.lane)
    (hs : s.slot ≠ .consumed v p) (ht : t.slot = .consumed v p) {u : Manifold.Drain.State}
    (f : Manifold.Drain.Follows s.lane u) (ho : Manifold.Drain.over s.lane u = true) : u.ended p = true :=
  Manifold.Drain.over_means_ended p i (claim_in_pass st v p hs ht) f ho

/-- A drain never reads a lane over while the pass that it read in the lane has not ended. -/
theorem drain_waits_while_held {s u : Manifold.Drain.State} (p : Nat) (i : Manifold.Drain.Inv s)
    (hp : s.holder = some p) (f : Manifold.Drain.Follows s u) (hn : u.ended p = false) :
    Manifold.Drain.over s u = false := by
  obtain ⟨_, held | done⟩ := Manifold.Drain.held_until_end p i hp f
  · obtain ⟨_, hb, _, hg⟩ := held
    simp [Manifold.Drain.over, hb, hg]
  · rw [done] at hn
    cases hn

end EmbeddedTypes.Claim
