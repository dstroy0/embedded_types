import Std.Tactic.BVDecide

/-!
# embedded_types, the lane word of the manifold

Each lane of the manifold is one word on a cache line of its own. Its fields, from bit 0 up:

| bit | field |
|---|---|
| 0 | busy: a pass, or a contraction, holds the lane |
| 1 | closed: a contraction closed the lane, above the open count |
| 2 | sleeper: a thread sleeps on the word |
| 3 up | generation: one count for each pass end, each close, each release and each takeover, rolling |

Each change to the word is one atomic operation:

- An entry is `fetch_or(BUSY)`. It takes the lane where the old value has no busy bit. It changes nothing where the
  old value has the busy bit.

- A leave is `fetch_add(UNIT - BUSY)`, on a word that the pass holds busy.

- A close is `fetch_add(UNIT + CLOSED)`, on a word that the contraction holds busy and that has no closed bit.

- The takeover of a grower is a compare-and-swap that adds `UNIT - CLOSED` to a closed word, which is busy too. The
  lane stays busy, and it belongs to the grower.

- A sleeper sets its bit with `fetch_or(SLEEPER)`. The thread that wakes it clears the bit with `fetch_and(~SLEEPER)`.

The futex compares the low 32 bits of a word. Each leave, close and takeover changes those bits, so each one ends a
sleep on the value before it.

The theorems hold at 64 bits and at 32 bits. `bv_decide` checks each one on every value of the width. The class
theorems at the end hold each operation to the step of `Open.lean` that it is.
-/

namespace EmbeddedTypes.Manifold.LaneWord

/-! ## The fields -/

def busy (w : Nat) : BitVec w := 1
def closed (w : Nat) : BitVec w := 2
def sleeper (w : Nat) : BitVec w := 4
def unit (w : Nat) : BitVec w := 8

/-- The generation of a word: the bits from bit 3 up. -/
def generation {w : Nat} (x : BitVec w) : BitVec w := x >>> 3

/-- The generation after one count: one added in the field, which rolls in the bits from bit 3 up. -/
def counted {w : Nat} (g : BitVec w) : BitVec w := (g + 1) &&& ((~~~0 : BitVec w) >>> 3)

/-- The entry: the word after the fetch-or, and whether the lane is the entrant's. -/
def entered {w : Nat} (x : BitVec w) : BitVec w := x ||| busy w
def takes {w : Nat} (x : BitVec w) : Bool := x &&& busy w == 0

/-- The leave, the close and the takeover: each one atomic addition of one constant. -/
def left {w : Nat} (x : BitVec w) : BitVec w := x + (unit w - busy w)
def shut {w : Nat} (x : BitVec w) : BitVec w := x + (unit w + closed w)
def takenOver {w : Nat} (x : BitVec w) : BitVec w := x + (unit w - closed w)

/-- The sleeper bit set, and cleared. -/
def flagged {w : Nat} (x : BitVec w) : BitVec w := x ||| sleeper w
def unflagged {w : Nat} (x : BitVec w) : BitVec w := x &&& ~~~(sleeper w)

/-- The drain's test of a lane against its snapshot: the pass is over where busy reads clear or the generation moved. -/
def over {w : Nat} (snapshot now : BitVec w) : Bool :=
  (now &&& busy w == 0) || (generation now != generation snapshot)

/-- The low 32 bits of a word, which the futex compares. -/
def low {w : Nat} (x : BitVec w) : BitVec w := x &&& 0xFFFFFFFF

/-! ## The fields do not overlap -/

theorem fields_64 :
    busy 64 &&& closed 64 = 0 ∧ busy 64 &&& sleeper 64 = 0 ∧ closed 64 &&& sleeper 64 = 0 ∧
      (busy 64 ||| closed 64 ||| sleeper 64) &&& (unit 64 - 1) = busy 64 ||| closed 64 ||| sleeper 64 := by
  simp only [busy, closed, sleeper, unit]
  bv_decide

theorem fields_32 :
    busy 32 &&& closed 32 = 0 ∧ busy 32 &&& sleeper 32 = 0 ∧ closed 32 &&& sleeper 32 = 0 ∧
      (busy 32 ||| closed 32 ||| sleeper 32) &&& (unit 32 - 1) = busy 32 ||| closed 32 ||| sleeper 32 := by
  simp only [busy, closed, sleeper, unit]
  bv_decide

/-! ## The entry -/

/-- An entry on a lane with busy clear takes it: busy set, and every other bit as it was. -/
theorem entry_takes_64 (x : BitVec 64) (h : x &&& 1 = 0) :
    entered x = x + busy 64 ∧ entered x &&& ~~~(busy 64) = x := by
  simp only [entered, busy]
  bv_decide

theorem entry_takes_32 (x : BitVec 32) (h : x &&& 1 = 0) :
    entered x = x + busy 32 ∧ entered x &&& ~~~(busy 32) = x := by
  simp only [entered, busy]
  bv_decide

/-- An entry on a lane with busy set changes nothing. A closed lane is busy, so an entry never takes it. -/
theorem entry_refused_64 (x : BitVec 64) (h : x &&& 1 = 1) : entered x = x ∧ takes x = false := by
  simp only [entered, takes, busy]
  constructor
  · bv_decide
  · simp only [beq_eq_false_iff_ne, ne_eq]
    bv_decide

theorem entry_refused_32 (x : BitVec 32) (h : x &&& 1 = 1) : entered x = x ∧ takes x = false := by
  simp only [entered, takes, busy]
  constructor
  · bv_decide
  · simp only [beq_eq_false_iff_ne, ne_eq]
    bv_decide

/-! ## The leave -/

/-- A leave on a word that its pass holds busy clears busy, keeps closed and the sleeper bit, and adds one to the
generation. -/
theorem leave_64 (x : BitVec 64) (h : x &&& 1 = 1) :
    left x &&& busy 64 = 0 ∧ left x &&& closed 64 = x &&& closed 64 ∧ left x &&& sleeper 64 = x &&& sleeper 64 ∧
      generation (left x) = counted (generation x) := by
  simp only [left, generation, counted, busy, closed, sleeper, unit]
  bv_decide

theorem leave_32 (x : BitVec 32) (h : x &&& 1 = 1) :
    left x &&& busy 32 = 0 ∧ left x &&& closed 32 = x &&& closed 32 ∧ left x &&& sleeper 32 = x &&& sleeper 32 ∧
      generation (left x) = counted (generation x) := by
  simp only [left, generation, counted, busy, closed, sleeper, unit]
  bv_decide

/-! ## The close and the takeover -/

/-- A close on a word that the contraction holds busy, and that has no closed bit, sets the closed bit. It keeps
the busy bit and the sleeper bit, and adds one to the generation. -/
theorem close_64 (x : BitVec 64) (h : x &&& 3 = 1) :
    shut x &&& busy 64 = busy 64 ∧ shut x &&& closed 64 = closed 64 ∧ shut x &&& sleeper 64 = x &&& sleeper 64 ∧
      generation (shut x) = counted (generation x) := by
  simp only [shut, generation, counted, busy, closed, sleeper, unit]
  bv_decide

theorem close_32 (x : BitVec 32) (h : x &&& 3 = 1) :
    shut x &&& busy 32 = busy 32 ∧ shut x &&& closed 32 = closed 32 ∧ shut x &&& sleeper 32 = x &&& sleeper 32 ∧
      generation (shut x) = counted (generation x) := by
  simp only [shut, generation, counted, busy, closed, sleeper, unit]
  bv_decide

/-- A takeover of a closed word clears closed, keeps busy and the sleeper bit, and adds one to the generation. The
lane is then the grower's. -/
theorem takeover_64 (x : BitVec 64) (h : x &&& 3 = 3) :
    takenOver x &&& 3 = 1 ∧ takenOver x &&& sleeper 64 = x &&& sleeper 64 ∧
      generation (takenOver x) = counted (generation x) := by
  simp only [takenOver, generation, counted, closed, sleeper, unit]
  bv_decide

theorem takeover_32 (x : BitVec 32) (h : x &&& 3 = 3) :
    takenOver x &&& 3 = 1 ∧ takenOver x &&& sleeper 32 = x &&& sleeper 32 ∧
      generation (takenOver x) = counted (generation x) := by
  simp only [takenOver, generation, counted, closed, sleeper, unit]
  bv_decide

/-! ## The sleeper bit -/

/-- The sleeper bit set and cleared keeps busy, closed and the generation. -/
theorem sleeper_bit_64 (x : BitVec 64) :
    flagged x &&& 3 = x &&& 3 ∧ generation (flagged x) = generation x ∧
      unflagged x &&& 3 = x &&& 3 ∧ generation (unflagged x) = generation x ∧ unflagged x &&& sleeper 64 = 0 := by
  simp only [flagged, unflagged, generation, sleeper]
  bv_decide

theorem sleeper_bit_32 (x : BitVec 32) :
    flagged x &&& 3 = x &&& 3 ∧ generation (flagged x) = generation x ∧
      unflagged x &&& 3 = x &&& 3 ∧ generation (unflagged x) = generation x ∧ unflagged x &&& sleeper 32 = 0 := by
  simp only [flagged, unflagged, generation, sleeper]
  bv_decide

/-! ## The futex sees each change -/

/-- A leave, a close and a takeover each change the low 32 bits. So each one ends a sleep on the word's value
before it. -/
theorem changes_seen_64 (x : BitVec 64) :
    (x &&& 1 = 1 → low (left x) ≠ low x) ∧ (x &&& 3 = 1 → low (shut x) ≠ low x) ∧
      (x &&& 3 = 3 → low (takenOver x) ≠ low x) := by
  simp only [low, left, shut, takenOver, busy, closed, unit]
  bv_decide

theorem changes_seen_32 (x : BitVec 32) :
    (x &&& 1 = 1 → low (left x) ≠ low x) ∧ (x &&& 3 = 1 → low (shut x) ≠ low x) ∧
      (x &&& 3 = 3 → low (takenOver x) ≠ low x) := by
  simp only [low, left, shut, takenOver, busy, closed, unit]
  bv_decide

/-! ## The drain's test -/

/-- The sleeper bit alone does not end a pass for the drain. A word that differs from the snapshot only in that bit,
with the busy bit, is not over. -/
theorem sleeper_not_over_64 (s : BitVec 64) (h : s &&& 1 = 1) :
    over s (flagged s) = false ∧ over s (unflagged s) = false := by
  simp only [over, flagged, unflagged, generation, busy, sleeper, Bool.or_eq_false_iff, beq_eq_false_iff_ne,
    bne_eq_false_iff_eq, ne_eq]
  bv_decide

theorem sleeper_not_over_32 (s : BitVec 32) (h : s &&& 1 = 1) :
    over s (flagged s) = false ∧ over s (unflagged s) = false := by
  simp only [over, flagged, unflagged, generation, busy, sleeper, Bool.or_eq_false_iff, beq_eq_false_iff_ne,
    bne_eq_false_iff_eq, ne_eq]
  bv_decide

/-- The leave of the pass in the snapshot ends it for the drain, and so does a close. -/
theorem end_over_64 (s : BitVec 64) :
    (s &&& 1 = 1 → over s (left s) = true) ∧ (s &&& 3 = 1 → over s (shut s) = true) := by
  simp only [over, left, shut, generation, busy, closed, unit, Bool.or_eq_true, beq_iff_eq, bne_iff_ne, ne_eq]
  bv_decide

theorem end_over_32 (s : BitVec 32) :
    (s &&& 1 = 1 → over s (left s) = true) ∧ (s &&& 3 = 1 → over s (shut s) = true) := by
  simp only [over, left, shut, generation, busy, closed, unit, Bool.or_eq_true, beq_iff_eq, bne_iff_ne, ne_eq]
  bv_decide

/-! ## The roll of the generation -/

/-- A pass on a free lane, its entry and its leave, adds one unit to the word and leaves the lane free. -/
theorem pass_64 (x : BitVec 64) (h : x &&& 1 = 0) : left (entered x) = x + unit 64 ∧ left (entered x) &&& 1 = 0 := by
  simp only [left, entered, busy, unit]
  bv_decide

theorem pass_32 (x : BitVec 32) (h : x &&& 1 = 0) : left (entered x) = x + unit 32 ∧ left (entered x) &&& 1 = 0 := by
  simp only [left, entered, busy, unit]
  bv_decide

/-- The generation rolls after 2^61 units at 64 bits and 2^29 at 32 bits: that many passes on one lane bring the
word back to its value. A drain that reads one lane twice, with exactly that count of passes between, waits for the
next pass to end too. -/
theorem roll_64 (x : BitVec 64) : x + unit 64 * 0x2000000000000000 = x := by
  simp only [unit]
  bv_decide

theorem roll_32 (x : BitVec 32) : x + unit 32 * 0x20000000 = x := by
  simp only [unit]
  bv_decide

/-! ## The bits and the lane's states

`Open.lean` holds a lane as free, held by a pass or a contraction, or closed. The bits give the same three classes.
A word with neither bit is free. A word with the busy bit alone has a holder. A word with both bits has the closed
state. A word with the closed bit alone does not occur.

Each operation moves the class as the step of `Open.lean` that it is. -/

/-- The class of a word: 0 free, 1 taken, 3 closed, and 2 for closed with busy clear. -/
def lane {w : Nat} (x : BitVec w) : BitVec w := x &&& 3

theorem classes_64 (x : BitVec 64) :
    -- an entry takes a free lane, and refuses a taken or closed lane with no change
    (lane x = 0 → lane (entered x) = 1) ∧ (lane x = 1 → entered x = x) ∧ (lane x = 3 → entered x = x) ∧
      -- a leave and a release make a taken lane free
      (lane x = 1 → lane (left x) = 0) ∧
      -- a close makes a taken lane closed, and a takeover makes a closed lane taken
      (lane x = 1 → lane (shut x) = 3) ∧ (lane x = 3 → lane (takenOver x) = 1) ∧
      -- the sleeper bit keeps the class
      lane (flagged x) = lane x ∧ lane (unflagged x) = lane x := by
  simp only [lane, entered, left, shut, takenOver, flagged, unflagged, busy, closed, sleeper, unit]
  bv_decide

theorem classes_32 (x : BitVec 32) :
    (lane x = 0 → lane (entered x) = 1) ∧ (lane x = 1 → entered x = x) ∧ (lane x = 3 → entered x = x) ∧
      (lane x = 1 → lane (left x) = 0) ∧
      (lane x = 1 → lane (shut x) = 3) ∧ (lane x = 3 → lane (takenOver x) = 1) ∧
      lane (flagged x) = lane x ∧ lane (unflagged x) = lane x := by
  simp only [lane, entered, left, shut, takenOver, flagged, unflagged, busy, closed, sleeper, unit]
  bv_decide

/-- From a word of class 0, 1 or 3, each operation that the C applies to that class leaves a class of 0, 1 or 3.
So closed with busy clear, class 2, never occurs. -/
theorem class_two_never_64 (x : BitVec 64) (h : lane x ≠ 2) :
    lane (entered x) ≠ 2 ∧ (lane x = 1 → lane (left x) ≠ 2) ∧ (lane x = 1 → lane (shut x) ≠ 2) ∧
      (lane x = 3 → lane (takenOver x) ≠ 2) ∧ lane (flagged x) ≠ 2 ∧ lane (unflagged x) ≠ 2 := by
  simp only [lane, entered, left, shut, takenOver, flagged, unflagged, busy, closed, sleeper, unit] at h ⊢
  bv_decide

theorem class_two_never_32 (x : BitVec 32) (h : lane x ≠ 2) :
    lane (entered x) ≠ 2 ∧ (lane x = 1 → lane (left x) ≠ 2) ∧ (lane x = 1 → lane (shut x) ≠ 2) ∧
      (lane x = 3 → lane (takenOver x) ≠ 2) ∧ lane (flagged x) ≠ 2 ∧ lane (unflagged x) ≠ 2 := by
  simp only [lane, entered, left, shut, takenOver, flagged, unflagged, busy, closed, sleeper, unit] at h ⊢
  bv_decide

end EmbeddedTypes.Manifold.LaneWord
