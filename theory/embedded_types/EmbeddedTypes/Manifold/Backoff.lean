import Std.Tactic.BVDecide

/-!
# embedded_types, the backoff of the manifold

A backoff retains two locals: the nanosecond that `embed_clock_ns` read at its start, and the exponent `k`. No shared
word holds a time. The clock, the start and each limit are machine words, `embed_word`, of w bits.

- The unit is 2^s ns, with `EMBED_TURNSTILE_UNIT_SHIFT` as s. That is 10 for a CPU host, about 1.024 µs, and 0 for
  a device.

- While the time since the start is below one unit, a step is a pause alone, with no call to `embed_word_wait`.

- From one unit on, a step waits on the word for at max 2^(s + k) ns. Then k goes up by one, to the max of the word,
  `EMBED_TURNSTILE_EXPONENT_MAX`: 20 on a word of 32 or 64 bits, and 5 on a word of 16 bits. The ceiling is 2^30 ns
  on a CPU host with a word of 32 or 64 bits, about 1.07 s, and 2^20 ns on a device, about 1.05 ms. A wake or a
  moved word ends the wait first. So the limit is a ceiling and not a delay.

- k fits in 5 bits. The C raises it with `exponent += (exponent < EMBED_TURNSTILE_EXPONENT_MAX)`, which
  `update_is_next` holds to `nextK`. Its shift is below the width of the word, and the limit is below
  `EMBED_WAIT_INFINITE` (`shift_defined` and `limit_below_infinite`).

- The clock rolls at 2^w. The elapsed time is the difference of two reads modulo 2^w, and that is the true time
  wherever the true time is below 2^w (`elapsed_rolls` and `elapsed_true`).

The hosts take the limit in their own unit: a `timespec` for the futex, microseconds for `__ulock_wait`, and
milliseconds for `WaitOnAddress`. The theorems at the end bound each conversion.
-/

namespace EmbeddedTypes.Manifold.Backoff

/-- The word widths that `EMBED_WORD_BITS` admits. -/
def words : List Nat := [16, 32, 64]

/-- The max the exponent reaches on a word of w bits, `EMBED_TURNSTILE_EXPONENT_MAX`. -/
def kMost (w : Nat) : Nat := if w = 16 then 5 else 20

/-- The max the unit shift is: the CPU host's. -/
def shiftMost : Nat := 10

/-- The limit of a wait at exponent k, in ns. -/
def limitNs (s k : Nat) : Nat := 2 ^ (s + k)

/-- The exponent after a wait: one more, and kMost at max. -/
def nextK (w k : Nat) : Nat := if k < kMost w then k + 1 else kMost w

/-- One step: no call while the time since the start is below one unit, else a wait with the limit of k. -/
def step (s start now k : Nat) : Option Nat :=
  if now - start < 2 ^ s then none else some (limitNs s k)

/-! ## The exponent -/

/-- The assertion of the C on each word: the unit shift and the max exponent sum to less than the width. -/
theorem shift_assert : ∀ w ∈ words, shiftMost + kMost w < w := by decide

theorem k_fits (w k : Nat) (hw : w ∈ words) (h : k ≤ kMost w) : k < 2 ^ 5 := by
  have : kMost w ≤ 20 := by
    simp only [words, List.mem_cons, List.mem_nil_iff, or_false] at hw
    rcases hw with rfl | rfl | rfl <;> decide
  have : (2 : Nat) ^ 5 = 32 := by decide
  omega

theorem next_bounded (w k : Nat) : nextK w k ≤ kMost w := by
  unfold nextK
  split <;> omega

theorem next_at_max (w : Nat) : nextK w (kMost w) = kMost w := by
  simp [nextK]

/-- Below the ceiling, each wait doubles the limit of the next. -/
theorem next_doubles (w s k : Nat) (h : k < kMost w) : limitNs s (nextK w k) = 2 * limitNs s k := by
  simp only [nextK, h, ite_true, limitNs, ← Nat.add_assoc, Nat.pow_succ]
  omega

/-- After j waits from k = 0, the exponent is j, or kMost where j passed it. -/
theorem after_waits (w j : Nat) : Nat.repeat (nextK w) j 0 = min j (kMost w) := by
  induction j with
  | zero => simp [Nat.repeat]
  | succ j ih =>
    simp only [Nat.repeat, ih, nextK]
    split <;> omega

/-- The C's update, `exponent += (exponent < EMBED_TURNSTILE_EXPONENT_MAX)`, is `nextK` on each exponent up to
kMost. -/
theorem update_is_next (w k : Nat) (h : k ≤ kMost w) : k + (if k < kMost w then 1 else 0) = nextK w k := by
  unfold nextK
  split <;> omega

/-- The shift of the C, `(embed_word)1u << (EMBED_TURNSTILE_UNIT_SHIFT + exponent)`, is below the width of the word.
So C defines it, and it is the limit. -/
theorem shift_defined (w s k : Nat) (hw : w ∈ words) (hs : s ≤ shiftMost) (hk : k ≤ kMost w) :
    s + k < w ∧ ((1 : BitVec w) <<< (s + k)).toNat = limitNs s k := by
  have hlt : s + k < w := Nat.lt_of_le_of_lt (Nat.add_le_add hs hk) (shift_assert w hw)
  refine ⟨hlt, ?_⟩
  have hpos : 0 < w := Nat.lt_of_le_of_lt (Nat.zero_le _) hlt
  have one : (1 : BitVec w).toNat = 1 := by
    rw [show (1 : BitVec w) = BitVec.ofNat w 1 from rfl, BitVec.toNat_ofNat]
    exact Nat.mod_eq_of_lt (Nat.one_lt_two_pow (Nat.pos_iff_ne_zero.mp hpos))
  rw [BitVec.toNat_shiftLeft, one, Nat.shiftLeft_eq, Nat.one_mul, limitNs]
  exact Nat.mod_eq_of_lt (Nat.pow_lt_pow_right (by decide : 1 < 2) hlt)

/-! ## The limit -/

/-- Each limit is below `EMBED_WAIT_INFINITE`, 2^w - 1, so no limit reads as a sleep with no end. -/
theorem limit_below_infinite (w s k : Nat) (hw : w ∈ words) (hs : s ≤ shiftMost) (hk : k ≤ kMost w) :
    limitNs s k < 2 ^ w - 1 := by
  have hlt : s + k < w := (shift_defined w s k hw hs hk).1
  have hstep : 2 ^ (s + k + 1) ≤ 2 ^ w := Nat.pow_le_pow_right (by decide) hlt
  have hpos : 0 < 2 ^ (s + k) := Nat.two_pow_pos _
  have hwide : 2 ^ 2 ≤ 2 ^ w := by
    apply Nat.pow_le_pow_right (by decide)
    simp only [words, List.mem_cons, List.mem_nil_iff, or_false] at hw
    omega
  rw [Nat.pow_succ] at hstep
  simp only [limitNs]
  generalize 2 ^ w = whole at hstep hwide ⊢
  generalize 2 ^ (s + k) = limit at hstep hpos ⊢
  omega

/-- Each limit fits 2^30 ns on every word, so the hosts' conversions below hold for it. -/
theorem limit_fits (w s k : Nat) (hw : w ∈ words) (hs : s ≤ shiftMost) (hk : k ≤ kMost w) : limitNs s k ≤ 2 ^ 30 := by
  have : kMost w ≤ 20 := by
    simp only [words, List.mem_cons, List.mem_nil_iff, or_false] at hw
    rcases hw with rfl | rfl | rfl <;> decide
  exact Nat.pow_le_pow_right (by decide) (by simp only [shiftMost] at hs; omega)

/-- The ceiling on a CPU host is 2^30 ns on a word of 32 or 64 bits and 2^15 ns on a word of 16 bits. On a device
it is 2^20 ns. -/
theorem ceilings :
    limitNs shiftMost (kMost 64) = 1073741824 ∧ limitNs shiftMost (kMost 32) = 1073741824 ∧
      limitNs shiftMost (kMost 16) = 32768 ∧ limitNs 0 (kMost 64) = 1048576 := by
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

/-! ## The roll of the clock -/

/-- The elapsed time on the clock of w bits is its difference, and the roll of the clock does not change it. -/
theorem elapsed_rolls (start e : BitVec w) : (start + e) - start = e := by
  rw [BitVec.add_comm, BitVec.add_sub_cancel]

/-- Two reads of the clock modulo 2^w, `now` at a true time e after `start`, give e as their difference wherever e is
below 2^w. So the backoff reads the true time since its start until one roll passes. -/
theorem elapsed_true (start e : Nat) (he : e < 2 ^ w) :
    (BitVec.ofNat w (start + e) - BitVec.ofNat w start).toNat = e := by
  have := elapsed_rolls (BitVec.ofNat w start) (BitVec.ofNat w e)
  rw [← BitVec.ofNat_add] at this
  rw [this, BitVec.toNat_ofNat, Nat.mod_eq_of_lt he]

/-! ## The hosts' units -/

/-- The futex's `timespec`: the seconds and the rest. The rest is below one second, the two give back the limit, and
the seconds are 1 at max. -/
theorem timespec_fits (ns : Nat) (h : ns ≤ 2 ^ 30) :
    ns % 1000000000 < 1000000000 ∧ (ns / 1000000000) * 1000000000 + ns % 1000000000 = ns ∧ ns / 1000000000 ≤ 1 := by
  refine ⟨Nat.mod_lt _ (by decide), ?_, ?_⟩
  · rw [Nat.mul_comm]
    exact Nat.div_add_mod ns 1000000000
  · have : (2 : Nat) ^ 30 = 1073741824 := by decide
    omega

/-- `__ulock_wait` takes microseconds in 32 bits. The limit in microseconds fits them. The host rounds up in 64 bits,
and the sum fits them. -/
theorem microseconds_fit (ns : Nat) (h : ns ≤ 2 ^ 30) : (ns + 999) / 1000 < 2 ^ 32 ∧ ns + 999 < 2 ^ 64 := by
  have : (2 : Nat) ^ 30 = 1073741824 := by decide
  have : (2 : Nat) ^ 32 = 4294967296 := by decide
  have : (2 : Nat) ^ 64 = 18446744073709551616 := by decide
  omega

/-- `WaitOnAddress` takes milliseconds. The limit rounded up to whole milliseconds is 1074 at max, and a limit of
one ns or more is one millisecond at least. The host rounds up in 64 bits, and the sum fits them. -/
theorem milliseconds_fit (ns : Nat) (h : ns ≤ 2 ^ 30) (h1 : 1 ≤ ns) :
    (ns + 999999) / 1000000 ≤ 1074 ∧ 1 ≤ (ns + 999999) / 1000000 ∧ ns + 999999 < 2 ^ 64 := by
  have : (2 : Nat) ^ 30 = 1073741824 := by decide
  have : (2 : Nat) ^ 64 = 18446744073709551616 := by decide
  omega

end EmbeddedTypes.Manifold.Backoff
