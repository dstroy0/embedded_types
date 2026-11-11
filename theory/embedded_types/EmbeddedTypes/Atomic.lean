import Std.Tactic.BVDecide

/-!
# embedded_types, the atomic helpers

`include/embed_atomic.h` builds three operations from the primitives of each arm. Each theorem holds one of them at
32 and at 64 bits.

- `cas_contract_32` and `cas_contract_64`. `embed_atomic_cas_u32` and `embed_atomic_cas_u64` of arms 1 and 4 swap
  where the word holds `*expected`. They answer whether they swapped, and leave in `*expected` the word that they
  found. This is the contract of the C11 compare-exchange that arms 2 and 3 call.

- `sub_by_add_32` and `sub_by_add_64`. `fetch_sub` adds the two's complement, and that add is the subtract.

- `raise_round` and `raise_exit`. Each round of `embed_atomic_*_raise` either stops on a word at or above the
  value, or swaps the value in over a word below it. A swap by `raise` never lowers the word.
-/

namespace EmbeddedTypes.Atomic

/-- One compare-and-swap of arms 1 and 4 on a word: the primitive answers the word it found and swaps where that
word is the expected one. The result is the word after, `*expected` after, and the answer. -/
def cas {w : Nat} (word expected value : BitVec w) : BitVec w × BitVec w × Bool :=
  let found := word
  ((if found = expected then value else word), found, decide (found = expected))

theorem cas_contract {w : Nat} (word expected value : BitVec w) :
    let r := cas word expected value
    (r.2.2 = true ↔ word = expected) ∧ r.2.1 = word ∧ r.1 = (if word = expected then value else word) := by
  simp [cas]

theorem cas_contract_32 (word expected value : BitVec 32) :
    let r := cas word expected value
    (r.2.2 = true ↔ word = expected) ∧ r.2.1 = word ∧ r.1 = (if word = expected then value else word) :=
  cas_contract word expected value

theorem cas_contract_64 (word expected value : BitVec 64) :
    let r := cas word expected value
    (r.2.2 = true ↔ word = expected) ∧ r.2.1 = word ∧ r.1 = (if word = expected then value else word) :=
  cas_contract word expected value

/-- `fetch_sub` adds `0u - value`, and the sum is the difference. Each one wraps at the width. -/
theorem sub_by_add_32 (x v : BitVec 32) : x + (0 - v) = x - v := by bv_decide

theorem sub_by_add_64 (x v : BitVec 64) : x + (0 - v) = x - v := by bv_decide

/-- One round of `raise`: the test on what it found, then the swap. The result is the word after, what it found
after, and whether the loop ends. -/
def raiseRound {w : Nat} (word found value : BitVec w) : BitVec w × BitVec w × Bool :=
  if found < value then
    let r := cas word found value
    (r.1, r.2.1, r.2.2)
  else (word, found, true)

/-- A round that ends either found a word at or above the value, or swapped the value in over the word that it
expected. That word was below the value. A round that does not end leaves the word as it was and finds the word now
there. -/
theorem raise_round {w : Nat} (word found value : BitVec w) :
    let r := raiseRound word found value
    (r.2.2 = true → (¬ found < value ∧ r.1 = word) ∨ (found < value ∧ word = found ∧ r.1 = value)) ∧
      (r.2.2 = false → r.1 = word ∧ r.2.1 = word) := by
  simp only [raiseRound, cas]
  by_cases hf : found < value
  · by_cases he : word = found
    · simp [hf, he]
    · simp [hf, he]
  · simp [hf]

/-- A swap by `raise` never lowers the word: it swaps only over a word below the value. -/
theorem raise_exit {w : Nat} (word found value : BitVec w) :
    let r := raiseRound word found value
    word ≤ r.1 := by
  simp only [raiseRound, cas]
  by_cases hf : found < value
  · by_cases he : word = found
    · simp only [hf, he, ↓reduceIte]; exact BitVec.le_of_lt hf
    · simp [hf, he]
  · simp [hf]

end EmbeddedTypes.Atomic
