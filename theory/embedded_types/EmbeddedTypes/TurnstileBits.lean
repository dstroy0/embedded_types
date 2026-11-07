import Std.Tactic.BVDecide

/-!
# embedded_types, the bit arithmetic of the turnstile

Each definition here is one expression of `include/embed_turnstile.h`, written on `BitVec` at the width the C
gives it. `bv_decide` checks each theorem on every value of that width.

- `lowest_fallback` and `count_fallback`: the fallback arms of `embed_turnstile_lowest` and
  `embed_turnstile_count` meet the contract of the builtins `__builtin_ctzll` and `__builtin_popcountll`.
- `layout_64` and `layout_32`: the fields of the badge word do not overlap, and together they fill the word.
- `claim_answer`: `embed_turnstile_claim` answers its lane where its fetch-or set the bit, and
  `EMBED_TURNSTILE_NONE` where it did not.
- `claim_in_range`: a lane that a claim answers is below `EMBED_TURNSTILE_LANES_MOST`.
- `spread_mask`: the spread of `embed_turnstile_leave` is the low mask up to the highest busy lane.
- `trim_keeps_busy`: the trim keeps open each lane that the leave read as busy.
- `step_bounded`: a backoff step stays at or below `EMBED_TURNSTILE_STEP_MOST`, and its sleep is at most 64 ms.
-/

namespace EmbeddedTypes.TurnstileBits

/-! ## The lowest set bit and the count of set bits -/

/-- The fallback arm of `embed_turnstile_lowest`: the lowest bit isolated, then placed by six masked steps. -/
def lowestFallback (mask : BitVec 64) : BitVec 64 :=
  let low := mask &&& (0 - mask)
  let step (m : BitVec 64) (w : BitVec 64) : BitVec 64 := if low &&& m != 0 then w else 0
  step 0xFFFFFFFF00000000 32 + step 0xFFFF0000FFFF0000 16 + step 0xFF00FF00FF00FF00 8 +
    step 0xF0F0F0F0F0F0F0F0 4 + step 0xCCCCCCCCCCCCCCCC 2 + step 0xAAAAAAAAAAAAAAAA 1

/-- On a value with a bit set, the fallback gives a place where the bit is set, and each bit below is clear. This
is the contract of `__builtin_ctzll`. -/
theorem lowest_fallback (mask : BitVec 64) (h : mask ≠ 0) :
    (mask >>> lowestFallback mask) &&& 1 = 1 ∧
      mask &&& (((1 : BitVec 64) <<< lowestFallback mask) - 1) = 0 := by
  unfold lowestFallback
  bv_decide

/-- The fallback arm of `embed_turnstile_count`: the SWAR count. -/
def countFallback (mask : BitVec 64) : BitVec 64 :=
  let b1 := mask - ((mask >>> 1) &&& 0x5555555555555555)
  let b2 := (b1 &&& 0x3333333333333333) + ((b1 >>> 2) &&& 0x3333333333333333)
  let b3 := (b2 + (b2 >>> 4)) &&& 0x0F0F0F0F0F0F0F0F
  (b3 * 0x0101010101010101) >>> 56

/-- The count bit by bit: bit `i` of the value added `n` times from the top. -/
def countReference : Nat → BitVec 64 → BitVec 64
  | 0, _ => 0
  | n + 1, mask => ((mask >>> n) &&& 1) + countReference n mask

/-- The multiply of the last SWAR step is the sum of eight shifts by whole bytes. -/
theorem bytes_summed (x : BitVec 64) :
    x * 0x0101010101010101 =
      x + (x <<< (8 : BitVec 64)) + (x <<< (16 : BitVec 64)) + (x <<< (24 : BitVec 64)) +
        (x <<< (32 : BitVec 64)) + (x <<< (40 : BitVec 64)) + (x <<< (48 : BitVec 64)) +
        (x <<< (56 : BitVec 64)) := by
  bv_decide (config := { timeout := 600 })

/-- The SWAR count is the number of set bits. This is the contract of `__builtin_popcountll`. -/
theorem count_fallback (mask : BitVec 64) : countFallback mask = countReference 64 mask := by
  simp only [countFallback, bytes_summed, countReference]
  bv_decide (config := { timeout := 600 })

/-! ## The badge word -/

/-- The badge fields at a word of `bits` bits, with `idBits` bits of id, as the header defines them. -/
structure Layout (w : Nat) where
  idMask : BitVec w
  complete : BitVec w
  sleeper : BitVec w
  timeMask : BitVec w

/-- The fields at a 64-bit word: `EMBED_TURNSTILE_ID_BITS` is 16. -/
def layout64 : Layout 64 :=
  let idShift : BitVec 64 := 64 - 16
  { idMask := (((1 : BitVec 64) <<< (16 : BitVec 64)) - 1) <<< idShift
    complete := (1 : BitVec 64) <<< (idShift - 1)
    sleeper := (1 : BitVec 64) <<< (idShift - 2)
    timeMask := ((1 : BitVec 64) <<< (idShift - 2)) - 1 }

/-- The fields at a 32-bit word: `EMBED_TURNSTILE_ID_BITS` is 10. -/
def layout32 : Layout 32 :=
  let idShift : BitVec 32 := 32 - 10
  { idMask := (((1 : BitVec 32) <<< (10 : BitVec 32)) - 1) <<< idShift
    complete := (1 : BitVec 32) <<< (idShift - 1)
    sleeper := (1 : BitVec 32) <<< (idShift - 2)
    timeMask := ((1 : BitVec 32) <<< (idShift - 2)) - 1 }

/-- At 64 bits the four fields are disjoint, and together they fill the word. -/
theorem layout_64 :
    layout64.idMask &&& layout64.complete = 0 ∧ layout64.idMask &&& layout64.sleeper = 0 ∧
      layout64.idMask &&& layout64.timeMask = 0 ∧ layout64.complete &&& layout64.sleeper = 0 ∧
      layout64.complete &&& layout64.timeMask = 0 ∧ layout64.sleeper &&& layout64.timeMask = 0 ∧
      (layout64.idMask ||| layout64.complete ||| layout64.sleeper ||| layout64.timeMask) = -1 := by
  simp only [layout64]
  bv_decide

/-- At 32 bits the four fields are disjoint, and together they fill the word. -/
theorem layout_32 :
    layout32.idMask &&& layout32.complete = 0 ∧ layout32.idMask &&& layout32.sleeper = 0 ∧
      layout32.idMask &&& layout32.timeMask = 0 ∧ layout32.complete &&& layout32.sleeper = 0 ∧
      layout32.complete &&& layout32.timeMask = 0 ∧ layout32.sleeper &&& layout32.timeMask = 0 ∧
      (layout32.idMask ||| layout32.complete ||| layout32.sleeper ||| layout32.timeMask) = -1 := by
  simp only [layout32]
  bv_decide

/-- The end of a pass clears the state bits and keeps the id and the stamp: a drain tells this pass from the next
by them. -/
theorem leave_keeps_pass (badge : BitVec 64) :
    let state := layout64.complete ||| layout64.sleeper
    let pass := layout64.idMask ||| layout64.timeMask
    (badge &&& ~~~state) &&& pass = badge &&& pass ∧ (badge &&& ~~~state) &&& state = 0 := by
  simp only [layout64]
  bv_decide

/-! ## The claim of a lane

The word is 64 bits. `EMBED_TURNSTILE_LANES_MOST` is 63, the lane bits are bits 0 to 62, and the sleeper flag is
bit 63. -/

/-- The lane bits and the sleeper flag of `busy` at 64 bits. -/
def laneBits : BitVec 64 := ((1 : BitVec 64) <<< (63 : BitVec 64)) - 1
def sleepers : BitVec 64 := (1 : BitVec 64) <<< (63 : BitVec 64)

/-- The lane a claim picks: the lowest free lane, or the sleeper flag's place where no lane is free. -/
def claimPlace (busy openRead : BitVec 64) : BitVec 64 :=
  lowestFallback (((openRead ||| 1) &&& ~~~busy &&& laneBits) ||| sleepers)

/-- The bit the claim's fetch-or sets: the picked lane's bit, nothing on the sentinel or where not permitted. -/
def claimBit (busy openRead permit : BitVec 64) : BitVec 64 :=
  ((1 : BitVec 64) <<< claimPlace busy openRead) &&& laneBits &&& permit

/-- The answer of the claim, with `before` the value that its fetch-or returned. -/
def claimAnswer (busy openRead permit before : BitVec 64) : BitVec 64 :=
  claimPlace busy openRead ||| (0 - (if claimBit busy openRead permit &&& ~~~before = 0 then 1 else 0))

/-- The claim answers its lane where its fetch-or set the bit, and every bit set, `EMBED_TURNSTILE_NONE`, where the
bit was set before or was masked to nothing. -/
theorem claim_answer (busy openRead permit before : BitVec 64) :
    (claimBit busy openRead permit &&& ~~~before ≠ 0 →
      claimAnswer busy openRead permit before = claimPlace busy openRead) ∧
    (claimBit busy openRead permit &&& ~~~before = 0 → claimAnswer busy openRead permit before = -1) := by
  simp only [claimAnswer, claimBit, claimPlace, lowestFallback, laneBits, sleepers]
  bv_decide

/-- A claim whose fetch-or set a bit answers a lane below 63, and the bit it set is that lane's. -/
theorem claim_in_range (busy openRead permit before : BitVec 64)
    (h : claimBit busy openRead permit &&& ~~~before ≠ 0) :
    claimPlace busy openRead < 63 ∧
      claimBit busy openRead permit = (1 : BitVec 64) <<< claimPlace busy openRead := by
  simp only [claimBit, claimPlace, lowestFallback, laneBits, sleepers] at h ⊢
  bv_decide

/-- Two claims that read any values and whose fetch-ors run one after the other do not both set the same bit: the
second fetch-or returns a value with the first one's bit set. -/
theorem claims_exclusive (busy₁ open₁ busy₂ open₂ before : BitVec 64)
    (h₁ : claimBit busy₁ open₁ (-1) &&& ~~~before ≠ 0)
    (h₂ : claimBit busy₂ open₂ (-1) &&& ~~~(before ||| claimBit busy₁ open₁ (-1)) ≠ 0) :
    claimBit busy₁ open₁ (-1) ≠ claimBit busy₂ open₂ (-1) := by
  intro he
  rw [he] at h₂
  simp only [claimBit, claimPlace, lowestFallback, laneBits, sleepers] at h₂
  bv_decide

/-! ## The end of a pass: the spread and the trim -/

/-- The spread of `embed_turnstile_leave` before the queue's term: the busy lanes but the one leaving, bit 0 set,
and every bit below the highest set bit filled by six shifts. -/
def spread (busy bit : BitVec 64) : BitVec 64 :=
  let s0 := ((busy &&& ~~~bit) &&& laneBits) ||| 1
  let s1 := s0 ||| (s0 >>> 1)
  let s2 := s1 ||| (s1 >>> 2)
  let s3 := s2 ||| (s2 >>> 4)
  let s4 := s3 ||| (s3 >>> 8)
  let s5 := s4 ||| (s4 >>> 16)
  s5 ||| (s5 >>> 32)

/-- The spread is a low mask, it covers each busy lane but the one leaving, and it has no bit above the highest
of those lanes. -/
theorem spread_mask (busy bit : BitVec 64) :
    let s0 := ((busy &&& ~~~bit) &&& laneBits) ||| 1
    spread busy bit &&& (spread busy bit + 1) = 0 ∧ s0 &&& ~~~spread busy bit = 0 ∧
      spread busy bit >>> 1 < s0 := by
  simp only [spread, laneBits]
  bv_decide

/-- The trim: `open` cut by the spread, or by every bit where the queue is not empty. It keeps open each lane that
the leave read as busy, other than the one leaving. -/
theorem trim_keeps_busy (busy bit openRead : BitVec 64) (waiting : Bool) :
    let full : BitVec 64 := if waiting then -1 else 0
    ((busy &&& ~~~bit) &&& laneBits &&& openRead) &&& ~~~(openRead &&& (spread busy bit ||| full)) = 0 := by
  cases waiting <;> simp only [spread, laneBits] <;> bv_decide

/-! ## The backoff step

The step is a 32-bit `unsigned int`. `EMBED_TURNSTILE_SLEEP_SHIFT` is 7 and `EMBED_TURNSTILE_STEP_MOST` is
64 shifted left by 7, 8192. -/

/-- The step after one backoff: doubled where `on` is 1, kept where it is 0, and held at 8192. -/
def stepAfter (step on : BitVec 32) : BitVec 32 :=
  let doubled := step <<< on
  doubled ^^^ ((doubled ^^^ 8192) &&& (0 - (if doubled > 8192 then 1 else 0)))

/-- The sleep of one step in milliseconds: the step shifted right by 7, masked to 0 where `on` is 0. -/
def sleepOf (step on : BitVec 32) : BitVec 32 := (step >>> 7) &&& (0 - on)

/-- From a step at or below 8192 with `on` at 0 or 1, the next step is at or below 8192, it is the doubled step
held at 8192 where `on` is 1, the same step where `on` is 0, and the sleep is at most 64 ms. -/
theorem step_bounded (step on : BitVec 32) (hs : step ≤ 8192) (ho : on ≤ 1) :
    stepAfter step on ≤ 8192 ∧ sleepOf step on ≤ 64 ∧
      (on = 0 → stepAfter step on = step ∧ sleepOf step on = 0) ∧
      (on = 1 → stepAfter step on = (if step * 2 > 8192 then 8192 else step * 2)) := by
  simp only [stepAfter, sleepOf]
  bv_decide

end EmbeddedTypes.TurnstileBits
