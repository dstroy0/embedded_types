import Std.Tactic.BVDecide

/-!
# embedded_types, the flag matrix of a stamp

A stamp is one machine word. The ticket is in the low bits, the flag field is in the 16 bits above it, and the epoch
is in the high bits. `rows` holds the widths of each word that `embed_ticket_ring.h` builds on. A word of 64 bits has
a 32-bit ticket and a 16-bit epoch. A word of 32 bits has a 12-bit ticket and a 4-bit epoch. A word of 16 bits stops
the build.

Five flag bits mark the transitions of the slot state machine: granted, pushed, popped, replayed and released. The
three bits above them are the check of the job pointer. Seven bits above the check hold the lane index of the pop.
The top bit of the flag field is the reserved bit, and no transition sets it.

Each transition is one compare-and-swap on the word. The expected value is the word with the flag bits of the
transitions before it, and the new value adds its own flag bit. `Atomic.lean` proves that the compare-and-swap of
each arm succeeds for exactly one caller.

This file is the flag matrix. Each theorem holds on each row. `rows_cases` puts the widths of each row in the goal,
and `bv_decide` then checks the theorem on every value of that word.

- `rows_fill` and `rows_epoch`. On each row, the ticket, the flag field and the epoch fill the word.

- `set_retains_fields` and `set_retains_others`. A transition preserves the ticket, the epoch and every other flag bit.

- `set_once`. The word after a transition differs from the word before it. So the compare-and-swap that expects
  the word before succeeds exactly once, and a repeat of the transition fails on the word after.

- `states_distinct` and `states_retain_fields`. The five state words of a slot are pairwise distinct along the
  sequence, and each one preserves the fields of the stamp.

- `independent`. Two flag bits set in either order give one word, and each bit stays set.

- `lane_recorded`. The lane index of a pop is recoverable from the popped word.

- `fold_shifts_16`, `fold_shifts_32` and `fold_shifts_64`. The fold of `embed_ticket_job_check` shifts by 7 times
  each power of two from the byte count of the pointer down to 1.

- `job_flip_changes_check`, `job_double_flip` and `check_recorded`. On a pointer of 16, 32 or 64 bits, a single-bit
  error at any place changes the three-bit check that `embed_ticket_job_check` computes. A double-bit error changes
  it exactly where the two places are not a multiple of seven apart. The pushed word records the check where the pop
  reads it.

- `corruption_refuses`. Any nonzero bit flip in a word fails a compare that expects the word.

- `fields_tell_tickets`. Two stamps with different fields are different words, whatever their flag bits.

- `guarded_refuses`. A transition that read the current value and failed its state test expects the current value
  with the reserved bit inverted. That differs from every current value, so the compare fails, on a zero word too.

- `states_reserved_clear`, `invalid_input_refuses` and `valid_input_expects`. No state word has the reserved bit.
  An input stamp with a flag bit set expects a value with the reserved bit, which fails on every state word. An
  input stamp with the flag field clear expects the stamp with the flag bits of the prior transitions.
-/

namespace EmbeddedTypes.Flags

/-! ## The rows -/

/-- The rows of the stamp layout of `embed_ticket_ring.h`: the width of the word and the width of the ticket,
`EMBED_TICKET_BITS_32` and `EMBED_TICKET_BITS_64`. -/
def rows : List (Nat × Nat) := [(32, 12), (64, 32)]

/-- The width of the epoch on a row: the bits of the word above the ticket and the flag field. -/
def epochBits (w t : Nat) : Nat := w - t - 16

/-- On each row, the ticket, the 16 flag bits and the epoch fill the word. This is the assertion of the C for each
row. -/
theorem rows_fill : ∀ r ∈ rows, r.2 + 16 + epochBits r.1 r.2 = r.1 := by decide

/-- The epoch is 4 bits on the 32 row and 16 bits on the 64 row, `EMBED_TICKET_EPOCH_BITS_32` and
`EMBED_TICKET_EPOCH_BITS_64`. -/
theorem rows_epoch : epochBits 32 12 = 4 ∧ epochBits 64 32 = 16 := by decide

/-- The goal on each row: the hypothesis `(w, t) ∈ rows` becomes the widths of the row, in place of `w` and `t`. -/
macro "rows_cases " hr:ident : tactic =>
  `(tactic| (simp only [rows, List.mem_cons, List.mem_nil_iff, or_false, Prod.mk.injEq] at $hr:ident;
             rcases $hr:ident with ⟨hw, ht⟩ | ⟨hw, ht⟩ <;> subst hw <;> subst ht))

/-! ## The layout -/

def ticketMask (w t : Nat) : BitVec w := ((1 : BitVec w) <<< t) - 1
def flagMask (w t : Nat) : BitVec w := (0xffff : BitVec w) <<< t
def epochMask (w t : Nat) : BitVec w := ((1 : BitVec w) <<< epochBits w t) - 1
def granted (w t : Nat) : BitVec w := (1 : BitVec w) <<< t
def pushed (w t : Nat) : BitVec w := (1 : BitVec w) <<< (t + 1)
def popped (w t : Nat) : BitVec w := (1 : BitVec w) <<< (t + 2)
def replayed (w t : Nat) : BitVec w := (1 : BitVec w) <<< (t + 3)
def released (w t : Nat) : BitVec w := (1 : BitVec w) <<< (t + 4)
def checkMask (w t : Nat) : BitVec w := (7 : BitVec w) <<< (t + 5)
def laneMask (w t : Nat) : BitVec w := (0x7f : BitVec w) <<< (t + 8)
def reserved (w t : Nat) : BitVec w := (1 : BitVec w) <<< (t + 15)

/-- The fields of a word: the ticket and the epoch, with the flag field clear. -/
def fields (t : Nat) (x : BitVec w) : BitVec w := x &&& ~~~flagMask w t

/-- The word after a transition: the word before with the flag bit of the transition. -/
def transition (x flag : BitVec w) : BitVec w := x ||| flag

/-- The masks of each row are the constants of the C on that row. -/
theorem layout_values :
    ticketMask 64 32 = 0xffffffff ∧ flagMask 64 32 = 0xffff00000000 ∧ epochMask 64 32 = 0xffff ∧
      checkMask 64 32 = 0xe000000000 ∧ laneMask 64 32 = 0x7f0000000000 ∧ reserved 64 32 = 0x800000000000 ∧
      ticketMask 32 12 = 0xfff ∧ flagMask 32 12 = 0x0ffff000 ∧ epochMask 32 12 = 0xf ∧
      checkMask 32 12 = 0x000e0000 ∧ laneMask 32 12 = 0x07f00000 ∧ reserved 32 12 = 0x08000000 := by
  decide

/-- The layout of a row in the goal, as constants for `bv_decide`. -/
macro "layout_unfold" : tactic =>
  `(tactic| simp only [ticketMask, flagMask, epochMask, epochBits, granted, pushed, popped, replayed, released,
    checkMask, laneMask, reserved, fields, transition, Nat.reduceAdd, Nat.reduceSub] at *)

/-! ## The matrix -/

/-- A transition preserves the ticket and the epoch. -/
theorem set_retains_fields (w t : Nat) (hr : (w, t) ∈ rows) (x flag : BitVec w) (h : flag &&& flagMask w t = flag) :
    fields t (transition x flag) = fields t x := by
  rows_cases hr <;> layout_unfold <;> bv_decide

/-- A transition preserves every other flag bit, and the lane bits. -/
theorem set_retains_others (x flag other : BitVec w) (h : flag &&& other = 0) :
    transition x flag &&& other = x &&& other := by
  rw [transition, BitVec.and_or_distrib_right, h]
  simp

/-- The word after a transition differs from the word before it. So the compare-and-swap that expects the word
before succeeds exactly once, and a repeat of the transition fails on the word after. -/
theorem set_once (x flag : BitVec w) (hf : flag ≠ 0) (h : x &&& flag = 0) : transition x flag ≠ x := by
  intro he
  apply hf
  have hand := congrArg (· &&& flag) he
  simp only [transition, BitVec.and_or_distrib_right, h, BitVec.and_self] at hand
  simpa using hand

/-- The five state words of a slot, from the fields of a stamp, the check of the job and the lane of the pop. -/
def stateGranted (w t : Nat) (s : BitVec w) : BitVec w := transition (fields t s) (granted w t)
def statePushed (w t : Nat) (s parity : BitVec w) : BitVec w :=
  transition (stateGranted w t s) (pushed w t ||| ((parity &&& 7) <<< (t + 5)))
def statePopped (w t : Nat) (s parity lane : BitVec w) : BitVec w :=
  transition (statePushed w t s parity) (popped w t ||| ((lane &&& 0x7f) <<< (t + 8)))
def stateReplayed (w t : Nat) (s parity lane : BitVec w) : BitVec w :=
  transition (statePopped w t s parity lane) (replayed w t)
def stateReleased (w t : Nat) (s parity lane : BitVec w) : BitVec w :=
  transition (stateReplayed w t s parity lane) (released w t)

/-- The state words of a row in the goal, with the layout. -/
macro "states_unfold" : tactic =>
  `(tactic| (simp only [stateGranted, statePushed, statePopped, stateReplayed, stateReleased] at *; layout_unfold))

/-- Each state word differs from the one before it, so each transition succeeds exactly once per grant. -/
theorem states_distinct (w t : Nat) (hr : (w, t) ∈ rows) (s parity lane : BitVec w) :
    stateGranted w t s ≠ fields t s ∧ statePushed w t s parity ≠ stateGranted w t s ∧
      statePopped w t s parity lane ≠ statePushed w t s parity ∧
      stateReplayed w t s parity lane ≠ statePopped w t s parity lane ∧
      stateReleased w t s parity lane ≠ stateReplayed w t s parity lane := by
  rows_cases hr <;> states_unfold <;> bv_decide

/-- Every state word preserves the fields of the stamp. -/
theorem states_retain_fields (w t : Nat) (hr : (w, t) ∈ rows) (s parity lane : BitVec w) :
    fields t (stateGranted w t s) = fields t s ∧ fields t (statePushed w t s parity) = fields t s ∧
      fields t (statePopped w t s parity lane) = fields t s ∧
      fields t (stateReplayed w t s parity lane) = fields t s ∧
      fields t (stateReleased w t s parity lane) = fields t s := by
  rows_cases hr <;> states_unfold <;> bv_decide

/-! ## The check of the job pointer -/

/-- The fold shifts of `embed_ticket_job_check` from a start shift, as its loop takes them: the shift, then half of
it, while the shift is 7 at least. The fuel bounds the recursion, and a fuel of the start shift is enough. -/
def foldShiftsFrom : Nat → Nat → List Nat
  | 0, _ => []
  | fuel + 1, shift => if 7 ≤ shift then shift :: foldShiftsFrom fuel (shift / 2) else []

/-- The fold shifts for a pointer of `bytes` bytes. The loop starts at 7 times the byte count. -/
def foldShifts (bytes : Nat) : List Nat := foldShiftsFrom (7 * bytes) (7 * bytes)

theorem fold_shifts_16 : foldShifts (16 / 8) = [14, 7] := by decide
theorem fold_shifts_32 : foldShifts (32 / 8) = [28, 14, 7] := by decide
theorem fold_shifts_64 : foldShifts (64 / 8) = [56, 28, 14, 7] := by decide

/-- The pointer widths of `uintptr_t` that `EMBED_WORD_BITS` admits. -/
def pointers : List Nat := [16, 32, 64]

/-- The remainder of the seven folded bits, from the table of x^3 to x^6 modulo x^3 + x + 1. -/
def remainder (d : BitVec p) : BitVec p :=
  let r0 := d &&& 1
  let r1 := (d >>> 1) &&& 1
  let r2 := (d >>> 2) &&& 1
  let r3 := (d >>> 3) &&& 1
  let r4 := (d >>> 4) &&& 1
  let r5 := (d >>> 5) &&& 1
  let r6 := (d >>> 6) &&& 1
  (r0 ^^^ r3 ^^^ r5 ^^^ r6) ||| ((r1 ^^^ r3 ^^^ r4 ^^^ r5) <<< 1) ||| ((r2 ^^^ r4 ^^^ r5 ^^^ r6) <<< 2)

/-- The three-bit check of a job pointer of p bits, as `embed_ticket_job_check` computes it: the pointer as a
polynomial modulo x^3 + x + 1. Each fold shift is a multiple of 7, and x^7 is 1 modulo the polynomial. So the fold
sums each residue class modulo 7 into the low seven bits. -/
def check (job : BitVec p) : BitVec p :=
  remainder ((foldShifts (p / 8)).foldl (fun bits shift => bits ^^^ (bits >>> shift)) job)

/-- The goal on each pointer width: the hypothesis `p ∈ pointers` becomes the width, in place of `p`, with the fold
of that width. -/
macro "pointers_cases " hp:ident : tactic =>
  `(tactic| (simp only [pointers, List.mem_cons, List.mem_nil_iff, or_false] at $hp:ident;
             rcases $hp:ident with hw | hw | hw <;> subst hw <;>
             simp only [check, remainder, fold_shifts_16, fold_shifts_32, fold_shifts_64, List.foldl_cons,
               List.foldl_nil]))

/-- A single-bit error at any place in the job pointer changes its check. So the check at the pop fails, and the
pop returns no job. -/
theorem job_flip_changes_check (p : Nat) (hp : p ∈ pointers) (job i : BitVec p) (hi : i < BitVec.ofNat p p) :
    check (job ^^^ ((1 : BitVec p) <<< i)) ≠ check job := by
  pointers_cases hp <;> bv_decide

/-- A double-bit error changes the check exactly where the two places are not a multiple of seven apart. -/
theorem job_double_flip (p : Nat) (hp : p ∈ pointers) (job i d : BitVec p) (hi : i < BitVec.ofNat p p)
    (hd : 0 < d) (hdp : d < BitVec.ofNat p p) (hs : i + d < BitVec.ofNat p p) :
    (check (job ^^^ ((1 : BitVec p) <<< i) ^^^ ((1 : BitVec p) <<< (i + d))) ≠ check job) ↔ d % 7 ≠ 0 := by
  pointers_cases hp <;> bv_decide

/-- The check stored in the pushed word comes back out of it, and the pop reads it there. -/
theorem check_recorded (w t : Nat) (hr : (w, t) ∈ rows) (s parity lane : BitVec w) :
    (statePushed w t s parity &&& checkMask w t) >>> (t + 5) = parity &&& 7 ∧
      (statePopped w t s parity lane &&& checkMask w t) >>> (t + 5) = parity &&& 7 := by
  rows_cases hr <;> states_unfold <;> bv_decide

/-- Two flag bits set in either order give one word, and each bit stays set. -/
theorem independent (x one other : BitVec w) :
    transition (transition x one) other = transition (transition x other) one ∧
      transition (transition x one) other &&& one = one ∧ transition (transition x one) other &&& other = other := by
  simp only [transition]
  refine ⟨?_, ?_, ?_⟩ <;> ext i hi <;> simp only [BitVec.getElem_and, BitVec.getElem_or] <;>
    cases x[i] <;> cases one[i] <;> cases other[i] <;> rfl

/-- The lane index of a pop is recoverable from the popped word. -/
theorem lane_recorded (w t : Nat) (hr : (w, t) ∈ rows) (s parity lane : BitVec w) (h : lane < 128) :
    (statePopped w t s parity lane &&& laneMask w t) >>> (t + 8) = lane := by
  rows_cases hr <;> states_unfold <;> bv_decide

/-- Any nonzero bit flip in a word fails a compare that expects the word. -/
theorem corruption_refuses (x m : BitVec w) (h : m ≠ 0) : x ^^^ m ≠ x := by
  intro he
  apply h
  have := congrArg (· ^^^ x) he
  simpa [BitVec.xor_comm x m, BitVec.xor_assoc] using this

/-- Two stamps with different fields are different words, whatever their flag bits. -/
theorem fields_tell_tickets (t : Nat) (x y : BitVec w) (h : fields t x ≠ fields t y) : x ≠ y := by
  intro he
  exact h (he ▸ rfl)

/-! ## An invalid input stamp -/

/-- The expected stamp of a transition from an input stamp, as `embed_ticket_ring_expected_stamp` computes it. It
is the stamp with the flag bits of the prior transitions, and the reserved bit where the input has a flag bit set. -/
def expected (w t : Nat) (stamp flags : BitVec w) : BitVec w :=
  stamp ||| flags ||| (if stamp &&& flagMask w t = 0 then 0 else reserved w t)

/-- No state word has the reserved bit. -/
theorem states_reserved_clear (w t : Nat) (hr : (w, t) ∈ rows) (s parity lane : BitVec w) :
    stateGranted w t s &&& reserved w t = 0 ∧ statePushed w t s parity &&& reserved w t = 0 ∧
      statePopped w t s parity lane &&& reserved w t = 0 ∧
      stateReplayed w t s parity lane &&& reserved w t = 0 ∧
      stateReleased w t s parity lane &&& reserved w t = 0 := by
  rows_cases hr <;> states_unfold <;> bv_decide

/-- An input stamp with a flag bit set expects a value with the reserved bit, which no state word has. So the
compare fails on every state word. -/
theorem invalid_input_refuses (w t : Nat) (hr : (w, t) ∈ rows) (stamp flags word : BitVec w)
    (hs : stamp &&& flagMask w t ≠ 0) (hw : word &&& reserved w t = 0) : expected w t stamp flags ≠ word := by
  simp only [expected, hs, ite_false]
  rows_cases hr <;> layout_unfold <;> bv_decide

/-- The expected stamp of a transition that read the current stamp first, as `embed_ticket_ring_guarded_stamp`
computes it. It is the current stamp where the state test passed, and the current stamp with the reserved bit
inverted where not. -/
def guarded (w t : Nat) (current : BitVec w) (valid : Bool) : BitVec w :=
  if valid then current else current ^^^ reserved w t

/-- A guarded expected value from a failed state test differs from the current value, so the compare fails on every
current value, zero among them. -/
theorem guarded_refuses (w t : Nat) (hr : (w, t) ∈ rows) (current : BitVec w) :
    guarded w t current false ≠ current := by
  simp only [guarded]
  rows_cases hr <;> layout_unfold <;> bv_decide

/-- An input stamp with the flag field clear expects the stamp with the flag bits of the prior transitions. -/
theorem valid_input_expects (w t : Nat) (stamp flags : BitVec w) (hs : stamp &&& flagMask w t = 0) :
    expected w t stamp flags = stamp ||| flags := by
  simp only [expected, hs, ite_true]
  ext i
  simp

end EmbeddedTypes.Flags
