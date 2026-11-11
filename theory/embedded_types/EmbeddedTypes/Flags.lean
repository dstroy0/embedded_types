import Std.Tactic.BVDecide

/-!
# embedded_types, the flag matrix of a stamp

A stamp is one 64-bit word: the ticket in bits 0 to 31, the flag field in bits 32 to 47, and the epoch in bits 48
to 63. Five flag bits mark the transitions of the slot state machine: granted, pushed, popped, replayed and
released. Bits 37 to 39 are the check of the job pointer. Seven bits above them hold the lane index of the pop. Bit
47 is the reserved bit, and no transition sets it.

Each transition is one compare-and-swap on the word. The expected value is the word with the flag bits of the
transitions before it, and the new value adds its own flag bit. `Atomic.lean` proves that the compare-and-swap of
each arm succeeds for exactly one caller.

This file is the flag matrix. `bv_decide` checks each theorem on every 64-bit value.

- `set_keeps_fields` and `set_keeps_others`. A transition preserves the ticket, the epoch and every other flag bit.

- `set_once`. The word after a transition differs from the word before it. So the compare-and-swap that expects
  the word before succeeds exactly once, and a repeat of the transition fails on the word after.

- `states_distinct` and `states_keep_fields`. The five state words of a slot are pairwise distinct along the
  sequence, and each one preserves the fields of the stamp.

- `independent`. Two flag bits set in either order give one word, and each bit stays set.

- `lane_recorded`. The lane index of a pop is recoverable from the popped word.

- `job_flip_changes_check`, `job_double_flip` and `check_recorded`. A single-bit error at any place in the job
  pointer changes the three-bit check that `embed_ticket_job_check` computes. A double-bit error changes it exactly
  where the two places are not a multiple of seven apart. The pushed word records the check where the pop reads it.

- `corruption_refuses`. Any nonzero bit flip in a word fails a compare that expects the word.

- `fields_tell_tickets`. Two stamps with different fields are different words, whatever their flag bits.

- `guarded_refuses`. A transition that read the current value and failed its state test expects the current value
  with the reserved bit inverted. That differs from every current value, so the compare fails, on a zero word too.

- `states_reserved_clear`, `invalid_input_refuses` and `valid_input_expects`. No state word has the reserved bit.
  An input stamp with a flag bit set expects a value with the reserved bit, which fails on every state word. An
  input stamp with the flag field clear expects the stamp with the flag bits of the prior transitions.
-/

namespace EmbeddedTypes.Flags

/-! ## The layout -/

def ticketMask : BitVec 64 := 0xffffffff
def flagMask : BitVec 64 := 0xffff00000000
def epochMask : BitVec 64 := 0xffff000000000000
def granted : BitVec 64 := 1 <<< 32
def pushed : BitVec 64 := 1 <<< 33
def popped : BitVec 64 := 1 <<< 34
def replayed : BitVec 64 := 1 <<< 35
def released : BitVec 64 := 1 <<< 36
def laneMask : BitVec 64 := 0x7f <<< 40
def reserved : BitVec 64 := 1 <<< 47

/-- The fields of a word: the ticket and the epoch, with the flag field clear. -/
def fields (x : BitVec 64) : BitVec 64 := x &&& ~~~flagMask

/-- The word after a transition: the word before with the flag bit of the transition. -/
def transition (x flag : BitVec 64) : BitVec 64 := x ||| flag

/-! ## The matrix -/

/-- A transition preserves the ticket and the epoch. -/
theorem set_keeps_fields (x flag : BitVec 64) (h : flag &&& flagMask = flag) :
    fields (transition x flag) = fields x := by
  simp only [fields, transition, flagMask] at *
  bv_decide

/-- A transition preserves every other flag bit, and the lane bits. -/
theorem set_keeps_others (x flag other : BitVec 64) (h : flag &&& other = 0) :
    transition x flag &&& other = x &&& other := by
  simp only [transition]
  bv_decide

/-- The word after a transition differs from the word before it. So the compare-and-swap that expects the word
before succeeds exactly once, and a repeat of the transition fails on the word after. -/
theorem set_once (x flag : BitVec 64) (hf : flag ≠ 0) (h : x &&& flag = 0) : transition x flag ≠ x := by
  simp only [transition]
  bv_decide

/-- The three check bits of the job pointer, which the push stores. -/
def checkMask : BitVec 64 := 7 <<< 37

/-- The five state words of a slot, from the fields of a stamp, the check of the job and the lane of the pop. -/
def stateGranted (s : BitVec 64) : BitVec 64 := transition (fields s) granted
def statePushed (s parity : BitVec 64) : BitVec 64 :=
  transition (stateGranted s) (pushed ||| ((parity &&& 7) <<< 37))
def statePopped (s parity lane : BitVec 64) : BitVec 64 :=
  transition (statePushed s parity) (popped ||| ((lane &&& 0x7f) <<< 40))
def stateReplayed (s parity lane : BitVec 64) : BitVec 64 := transition (statePopped s parity lane) replayed
def stateReleased (s parity lane : BitVec 64) : BitVec 64 := transition (stateReplayed s parity lane) released

/-- Each state word differs from the one before it, so each transition succeeds exactly once per grant. -/
theorem states_distinct (s parity lane : BitVec 64) :
    stateGranted s ≠ fields s ∧ statePushed s parity ≠ stateGranted s ∧
      statePopped s parity lane ≠ statePushed s parity ∧
      stateReplayed s parity lane ≠ statePopped s parity lane ∧
      stateReleased s parity lane ≠ stateReplayed s parity lane := by
  simp only [stateGranted, statePushed, statePopped, stateReplayed, stateReleased, transition, fields, flagMask,
    granted, pushed, popped, replayed, released]
  bv_decide

/-- Every state word preserves the fields of the stamp. -/
theorem states_keep_fields (s parity lane : BitVec 64) :
    fields (stateGranted s) = fields s ∧ fields (statePushed s parity) = fields s ∧
      fields (statePopped s parity lane) = fields s ∧ fields (stateReplayed s parity lane) = fields s ∧
      fields (stateReleased s parity lane) = fields s := by
  simp only [stateGranted, statePushed, statePopped, stateReplayed, stateReleased, transition, fields, flagMask,
    granted, pushed, popped, replayed, released]
  bv_decide

/-! ## The check of the job pointer -/

/-- The three-bit check of a 64-bit job pointer, as `embed_ticket_job_check` computes it: the pointer as a polynomial
modulo x^3 + x + 1. The fold by 56, 28, 14 and 7 sums each residue class modulo 7 into the low seven bits, because
x^7 is 1 modulo the polynomial. The remainder of those seven bits reads off the table of x^3 to x^6. -/
def check (job : BitVec 64) : BitVec 64 :=
  let a := job ^^^ (job >>> 56)
  let b := a ^^^ (a >>> 28)
  let c := b ^^^ (b >>> 14)
  let d := c ^^^ (c >>> 7)
  let r0 := d &&& 1
  let r1 := (d >>> 1) &&& 1
  let r2 := (d >>> 2) &&& 1
  let r3 := (d >>> 3) &&& 1
  let r4 := (d >>> 4) &&& 1
  let r5 := (d >>> 5) &&& 1
  let r6 := (d >>> 6) &&& 1
  (r0 ^^^ r3 ^^^ r5 ^^^ r6) ||| ((r1 ^^^ r3 ^^^ r4 ^^^ r5) <<< 1) ||| ((r2 ^^^ r4 ^^^ r5 ^^^ r6) <<< 2)

/-- A single-bit error at any place in the job pointer changes its check. So the check at the pop fails, and the
pop returns no job. -/
theorem job_flip_changes_check (job i : BitVec 64) (hi : i < 64) :
    check (job ^^^ ((1 : BitVec 64) <<< i)) ≠ check job := by
  simp only [check]
  bv_decide

/-- A double-bit error changes the check exactly where the two places are not a multiple of seven apart. -/
theorem job_double_flip (job i d : BitVec 64) (hi : i < 64) (hd : 0 < d) (hd64 : d < 64) (hs : i + d < 64) :
    (check (job ^^^ ((1 : BitVec 64) <<< i) ^^^ ((1 : BitVec 64) <<< (i + d))) ≠ check job) ↔ d % 7 ≠ 0 := by
  simp only [check]
  bv_decide

/-- The check stored in the pushed word comes back out of it, and the pop reads it there. -/
theorem check_recorded (s parity lane : BitVec 64) :
    (statePushed s parity &&& checkMask) >>> 37 = parity &&& 7 ∧
      (statePopped s parity lane &&& checkMask) >>> 37 = parity &&& 7 := by
  simp only [statePopped, statePushed, stateGranted, transition, fields, flagMask, granted, pushed, popped, checkMask]
  bv_decide

/-- Two flag bits set in either order give one word, and each bit stays set. -/
theorem independent (x one other : BitVec 64) :
    transition (transition x one) other = transition (transition x other) one ∧
      transition (transition x one) other &&& one = one ∧ transition (transition x one) other &&& other = other := by
  simp only [transition]
  bv_decide

/-- The lane index of a pop is recoverable from the popped word. -/
theorem lane_recorded (s parity lane : BitVec 64) (h : lane < 128) :
    (statePopped s parity lane &&& laneMask) >>> 40 = lane := by
  simp only [statePopped, statePushed, stateGranted, transition, fields, flagMask, granted, pushed, popped, laneMask]
  bv_decide

/-- Any nonzero bit flip in a word fails a compare that expects the word. -/
theorem corruption_refuses (x m : BitVec 64) (h : m ≠ 0) : x ^^^ m ≠ x := by
  bv_decide

/-- Two stamps with different fields are different words, whatever their flag bits. -/
theorem fields_tell_tickets (x y : BitVec 64) (h : fields x ≠ fields y) : x ≠ y := by
  intro he
  exact h (he ▸ rfl)

/-! ## An invalid input stamp -/

/-- The expected value of a transition from an input stamp, as `embed_ticket_ring_expected` computes it. It is the
stamp with the flag bits of the prior transitions, and the reserved bit where the input has a flag bit set. -/
def expected (stamp flags : BitVec 64) : BitVec 64 :=
  stamp ||| flags ||| (if stamp &&& flagMask = 0 then 0 else reserved)

/-- No state word has the reserved bit. -/
theorem states_reserved_clear (s parity lane : BitVec 64) :
    stateGranted s &&& reserved = 0 ∧ statePushed s parity &&& reserved = 0 ∧
      statePopped s parity lane &&& reserved = 0 ∧ stateReplayed s parity lane &&& reserved = 0 ∧
      stateReleased s parity lane &&& reserved = 0 := by
  simp only [stateGranted, statePushed, statePopped, stateReplayed, stateReleased, transition, fields, flagMask,
    granted, pushed, popped, replayed, released, reserved]
  bv_decide

/-- An input stamp with a flag bit set expects a value with the reserved bit, which no state word has. So the
compare fails on every state word. -/
theorem invalid_input_refuses (stamp flags word : BitVec 64) (hs : stamp &&& flagMask ≠ 0)
    (hw : word &&& reserved = 0) : expected stamp flags ≠ word := by
  simp only [expected, reserved, flagMask] at *
  bv_decide

/-- The expected value of a transition that read the current value first, as `embed_ticket_ring_guarded` computes it.
It is the current value where the state test passed, and the current value with the reserved bit inverted where
not. -/
def guarded (current : BitVec 64) (valid : Bool) : BitVec 64 :=
  if valid then current else current ^^^ reserved

/-- A guarded expected value from a failed state test differs from the current value, so the compare fails on every
current value, zero among them. -/
theorem guarded_refuses (current : BitVec 64) : guarded current false ≠ current := by
  simp only [guarded, reserved]
  bv_decide

/-- An input stamp with the flag field clear expects the stamp with the flag bits of the prior transitions. -/
theorem valid_input_expects (stamp flags : BitVec 64) (hs : stamp &&& flagMask = 0) :
    expected stamp flags = stamp ||| flags := by
  simp only [expected, hs, ite_true]
  bv_decide

end EmbeddedTypes.Flags
