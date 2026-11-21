import EmbeddedTypes.Widths
import EmbeddedTypes.Flags
import EmbeddedTypes.Manifold.Backoff
import EmbeddedTypes.Manifold.Open
import EmbeddedTypes.Integrated

/-!
# embedded_types, the configuration that each target selects

`include/embed_turnstile.h` selects `EMBED_TURNSTILE_LANES` and `EMBED_TURNSTILE_UNIT_SHIFT` where the build does not
set them. `EMBED_VECTOR_REGISTER_BITS` and the CUDA device pass select them, and `Widths.turnstileLanes` and
`Widths.unitShift` are that selection. This file puts the two values of each target into each model that reads them.
So each theorem of the open count, the backoff, the integrated model and the flag matrix holds with the values that
the target builds with.

The theorems:

- `open_configured`. The open count of `Manifold/Open.lean` with the lane count of the target. Each pass and each held
  lane is below the open count, no closed lane is, and the open count is from 1 to the lane count.

- `backoff_configured`. The backoff of `Manifold/Backoff.lean` with the unit shift of the target. The shift of the C
  is below the width of the word, and each limit is below `EMBED_WAIT_INFINITE` and at max 2^30 ns.

- `integrated_configured`. The integrated model with the lane count of the target. Each busy lane is below the lane
  count. The popped word of `Flags.lean` records the index of that lane, and `Flags.lane_recorded` reads it back.

- `microcontroller_configured`. The microcontroller has no vector register and no CUDA. It has 4 lanes and an open
  count at max 4. Each busy lane is below 4, and each wait has a ceiling of 2^20 ns.
-/

namespace EmbeddedTypes.Config

open Widths

theorem lanes_pos (cuda : Bool) (v : Nat) : 1 ≤ turnstileLanes cuda v := by
  unfold turnstileLanes
  split <;> decide

theorem lanes_le_64 (cuda : Bool) (v : Nat) : turnstileLanes cuda v ≤ 64 := by
  unfold turnstileLanes
  split <;> decide

theorem shift_le (cuda : Bool) (v : Nat) : unitShift cuda v ≤ Manifold.Backoff.shiftMost := by
  unfold unitShift Manifold.Backoff.shiftMost
  split <;> decide

/-- The open count with the lane count of the target. -/
theorem open_configured (cuda : Bool) (v : Nat) {s : Manifold.Open.State}
    (r : Manifold.Open.Reach (turnstileLanes cuda v) s) :
    (∀ l, s.lane l = .pass → l < s.count) ∧ (∀ l, s.lane l = .held → l < s.count) ∧
      (∀ l, s.lane l = .closed → s.count ≤ l) ∧ 1 ≤ s.count ∧ s.count ≤ turnstileLanes cuda v :=
  Manifold.Open.passes_below_open _ (lanes_pos cuda v) r

/-- The backoff with the unit shift of the target. -/
theorem backoff_configured (cuda : Bool) (v w k : Nat) (hw : w ∈ Manifold.Backoff.words)
    (hk : k ≤ Manifold.Backoff.kMost w) :
    unitShift cuda v + k < w ∧ Manifold.Backoff.limitNs (unitShift cuda v) k < 2 ^ w - 1 ∧
      Manifold.Backoff.limitNs (unitShift cuda v) k ≤ 2 ^ 30 :=
  ⟨(Manifold.Backoff.shift_defined w _ k hw (shift_le cuda v) hk).1,
    Manifold.Backoff.limit_below_infinite w _ k hw (shift_le cuda v) hk,
    Manifold.Backoff.limit_fits w _ k hw (shift_le cuda v) hk⟩

/-- A lane below 64 is below 128 as a word of each row of the flag matrix. -/
theorem lane_word_below (w t l : Nat) (hr : (w, t) ∈ Flags.rows) (hl : l < 64) : BitVec.ofNat w l < 128 := by
  have hw : w = 32 ∨ w = 64 := by
    simp only [Flags.rows, List.mem_cons, List.mem_nil_iff, Prod.mk.injEq, or_false] at hr
    omega
  rcases hw with rfl | rfl
  · rw [BitVec.lt_def, BitVec.toNat_ofNat, show (128 : BitVec 32).toNat = 128 from rfl]
    omega
  · rw [BitVec.lt_def, BitVec.toNat_ofNat, show (128 : BitVec 64).toNat = 128 from rfl]
    omega

/-- The integrated model with the lane count of the target, and the lane index that the popped word records. -/
theorem integrated_configured (cuda : Bool) (v : Nat) {s : Integrated.State}
    (r : Integrated.Reach (turnstileLanes cuda v) s) (l : Nat) (h : (s.lane l).busy = true)
    (w t : Nat) (hr : (w, t) ∈ Flags.rows) (stamp parity : BitVec w) :
    l < turnstileLanes cuda v ∧
      (Flags.statePopped w t stamp parity (BitVec.ofNat w l) &&& Flags.laneMask w t) >>> (t + 8) =
        BitVec.ofNat w l := by
  have hl := Integrated.busy_below_count r l h
  exact ⟨hl, Flags.lane_recorded w t hr stamp parity _
    (lane_word_below w t l hr (Nat.lt_of_lt_of_le hl (lanes_le_64 cuda v)))⟩

/-- The microcontroller: no vector register and no CUDA. It has 4 lanes and an open count at max 4. Each busy lane
of the integrated model is below 4, and each wait of the backoff is at max 2^20 ns. -/
theorem microcontroller_configured :
    turnstileLanes false 0 = 4 ∧ unitShift false 0 = 0 ∧
      (∀ s, Manifold.Open.Reach (turnstileLanes false 0) s → s.count ≤ 4) ∧
      (∀ s, Integrated.Reach (turnstileLanes false 0) s → ∀ l, (s.lane l).busy = true → l < 4) ∧
      (∀ w ∈ Manifold.Backoff.words, ∀ k ≤ Manifold.Backoff.kMost w,
        Manifold.Backoff.limitNs (unitShift false 0) k ≤ 2 ^ 20) := by
  refine ⟨by decide, by decide, fun s r => (open_configured false 0 r).2.2.2.2,
    fun s r l h => Integrated.busy_below_count r l h, ?_⟩
  intro w hw k hk
  have : Manifold.Backoff.kMost w ≤ 20 := by
    simp only [Manifold.Backoff.words, List.mem_cons, List.mem_nil_iff, or_false] at hw
    rcases hw with rfl | rfl | rfl <;> decide
  simp only [unitShift, Manifold.Backoff.limitNs]
  exact Nat.pow_le_pow_right (by decide) (by simp; omega)

end EmbeddedTypes.Config
