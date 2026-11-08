import Std.Tactic.BVDecide

/-!
# embedded_types, the backoff of the manifold

A backoff keeps two locals: the nanosecond that `embed_clock_ns` read at its start, and the exponent `k`. No shared
word holds a time.

- The unit is 2^s ns, with `EMBED_TURNSTILE_UNIT_SHIFT` as s: 10 for a CPU host, about 1.024 µs, and 0 for a device.
- While less than one unit has passed since the start, a step is a pause alone, with no call to `embed_word_wait`.
- From one unit on, a step waits on the word for at most 2^(s + k) ns. Then k goes up by one, to 20 at most. The
  ceiling is 2^30 ns on a CPU host, about 1.07 s, and 2^20 ns on a device, about 1.05 ms. A wake or a moved word ends
  the wait first, so the limit is a ceiling and not a delay.
- k from 0 to 20 fits in 5 bits. The C raises it with `exponent += (exponent < EMBED_TURNSTILE_K_MOST)`, which
  `update_is_next` holds to `nextK`, and its shift is below 64 bits (`shift_defined`).

The hosts take the limit in their own unit: a `timespec` for the futex, microseconds for `__ulock_wait`, and
milliseconds for `WaitOnAddress`. The theorems at the end bound each conversion.
-/

namespace EmbeddedTypes.Manifold.Backoff

/-- The most the exponent reaches. -/
def kMost : Nat := 20

/-- The most the unit shift is: the CPU host's. -/
def shiftMost : Nat := 10

/-- The limit of a wait at exponent k, in ns. -/
def limitNs (s k : Nat) : Nat := 2 ^ (s + k)

/-- The exponent after a wait: one more, and kMost at most. -/
def nextK (k : Nat) : Nat := if k < kMost then k + 1 else kMost

/-- One step: no call while less than one unit has passed, else a wait with the limit of k. -/
def step (s start now k : Nat) : Option Nat :=
  if now - start < 2 ^ s then none else some (limitNs s k)

/-! ## The exponent -/

theorem k_fits (k : Nat) (h : k ≤ kMost) : k < 2 ^ 5 := by
  simp only [kMost] at h
  have : (2 : Nat) ^ 5 = 32 := by decide
  omega

theorem next_bounded (k : Nat) : nextK k ≤ kMost := by
  unfold nextK
  split <;> omega

theorem next_at_most : nextK kMost = kMost := by
  simp [nextK]

/-- Below the ceiling, each wait doubles the limit of the next. -/
theorem next_doubles (s k : Nat) (h : k < kMost) : limitNs s (nextK k) = 2 * limitNs s k := by
  simp only [nextK, h, ite_true, limitNs, ← Nat.add_assoc, Nat.pow_succ]
  omega

/-- After j waits from k = 0, the exponent is j, or kMost where j passed it. -/
theorem after_waits (j : Nat) : Nat.repeat nextK j 0 = min j kMost := by
  induction j with
  | zero => simp [Nat.repeat]
  | succ j ih =>
    simp only [Nat.repeat, ih, nextK, kMost]
    split <;> omega

/-- The C's update, `exponent += (exponent < EMBED_TURNSTILE_K_MOST)`, is `nextK` on each exponent up to kMost. -/
theorem update_is_next (k : Nat) (h : k ≤ kMost) : k + (if k < kMost then 1 else 0) = nextK k := by
  unfold nextK
  split <;> omega

/-- The C's shift `(embed_u64)1u << (EMBED_TURNSTILE_UNIT_SHIFT + exponent)` is below the width, so it is defined,
and it is the limit. -/
theorem shift_defined (s k : Nat) (hs : s ≤ shiftMost) (hk : k ≤ kMost) :
    s + k < 64 ∧ ((1 : BitVec 64) <<< (s + k)).toNat = limitNs s k := by
  simp only [shiftMost, kMost] at hs hk
  refine ⟨by omega, ?_⟩
  have one : (1 : BitVec 64).toNat = 1 := rfl
  rw [BitVec.toNat_shiftLeft, one, Nat.shiftLeft_eq, Nat.one_mul, limitNs]
  exact Nat.mod_eq_of_lt (Nat.pow_lt_pow_right (by decide : 1 < 2) (by omega : s + k < 64))

/-! ## The limit -/

/-- Each limit fits 2^30 ns, so it fits the 64-bit nanosecond word that the hosts take with room left. -/
theorem limit_fits (s k : Nat) (hs : s ≤ shiftMost) (hk : k ≤ kMost) : limitNs s k ≤ 2 ^ 30 ∧ 2 ^ 30 < 2 ^ 64 := by
  constructor
  · exact Nat.pow_le_pow_right (by decide) (by simp only [shiftMost, kMost] at hs hk; omega)
  · decide

/-- The ceiling on a CPU host is 2^30 ns, and on a device 2^20 ns. -/
theorem ceilings : limitNs shiftMost kMost = 1073741824 ∧ limitNs 0 kMost = 1048576 := by
  decide

/-- The first wait of a backoff waits one unit. -/
theorem first_wait (s : Nat) : limitNs s 0 = 2 ^ s := by
  simp [limitNs]

/-! ## The step -/

/-- Inside the first unit, a step makes no call. -/
theorem no_call_in_unit (s start now k : Nat) (h : now - start < 2 ^ s) : step s start now k = none := by
  simp [step, h]

/-- From one unit on, a step waits with the limit of k. -/
theorem wait_from_unit (s start now k : Nat) (h : 2 ^ s ≤ now - start) : step s start now k = some (limitNs s k) := by
  simp [step, Nat.not_lt.mpr h]

/-- The elapsed time on the 64-bit clock is its difference, and the clock's roll does not change it. -/
theorem elapsed_rolls (start e : BitVec 64) : (start + e) - start = e := by
  bv_decide

/-! ## The hosts' units -/

/-- The futex's `timespec`: the seconds and the rest. The rest is below one second, the two give back the limit, and
the seconds are 1 at most. -/
theorem timespec_fits (ns : Nat) (h : ns ≤ 2 ^ 30) :
    ns % 1000000000 < 1000000000 ∧ (ns / 1000000000) * 1000000000 + ns % 1000000000 = ns ∧ ns / 1000000000 ≤ 1 := by
  refine ⟨Nat.mod_lt _ (by decide), ?_, ?_⟩
  · rw [Nat.mul_comm]
    exact Nat.div_add_mod ns 1000000000
  · have : (2 : Nat) ^ 30 = 1073741824 := by decide
    omega

/-- `__ulock_wait` takes microseconds in 32 bits. The limit in microseconds fits them. -/
theorem microseconds_fit (ns : Nat) (h : ns ≤ 2 ^ 30) : ns / 1000 < 2 ^ 32 := by
  have : (2 : Nat) ^ 30 = 1073741824 := by decide
  have : (2 : Nat) ^ 32 = 4294967296 := by decide
  omega

/-- `WaitOnAddress` takes milliseconds. The limit rounded up to whole milliseconds is 1074 at most, and a limit of
one ns or more is one millisecond at least. -/
theorem milliseconds_fit (ns : Nat) (h : ns ≤ 2 ^ 30) (h1 : 1 ≤ ns) :
    (ns + 999999) / 1000000 ≤ 1074 ∧ 1 ≤ (ns + 999999) / 1000000 := by
  have : (2 : Nat) ^ 30 = 1073741824 := by decide
  omega

end EmbeddedTypes.Manifold.Backoff
