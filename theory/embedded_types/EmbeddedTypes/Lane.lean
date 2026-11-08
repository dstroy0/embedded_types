import Std.Tactic.BVDecide

/-!
# embedded_types, the lane's byte operations

The byte operations of `include/embed_lane.h`, one 32-bit lane at a time, written on `BitVec` as the C writes
them. `bv_decide` checks each theorem on every value.

- `zero_bytes`: the carry test of the portable arm sets bit 8k of its result where byte k of the word is zero, and
  leaves every other bit clear.
- `gather`: the multiply by `EMBED_LANE_NIBBLE_GATHER` takes a word whose bytes are each 0 or 1 to the nibble
  of those bits, the lowest byte the lowest bit.
- `equal_word`: the two together give the nibble of the equal bytes of two words, as
  `EMBED_LANE_EQUAL_WORD` computes it.
- `vector_word`: the vector arm without a byte mask builtin gives the same nibble from the compiler's byte
  compare, whose equal bytes are all ones and others zero.
- `assembled_16` and its narrower forms: the nibbles of the 32-bit lanes, shifted by four bits a lane and ored,
  give the mask with bit `4i + k` set where byte `k` of lane `i` is equal.
- `fill_byte`: `embed_lane_fill_byte` puts the byte in each byte of each 32-bit lane.
- `mask_first`: the fallback of `embed_lane_mask_first` meets the contract of `__builtin_ctzll`: a place where
  the bit is set, each bit below it clear.
-/

namespace EmbeddedTypes.Lane

/-- The byte `k` of a word. -/
def byteOf (w : BitVec 32) (k : Nat) : BitVec 32 := (w >>> (8 * k)) &&& 0xFF

/-- The carry test of `EMBED_LANE_ZERO_BYTES`: bit 8k set where byte k is zero. -/
def zeroBytes (w : BitVec 32) : BitVec 32 :=
  (~~~(((w &&& 0x7f7f7f7f) + 0x7f7f7f7f) ||| w ||| 0x7f7f7f7f)) >>> 7

/-- Each byte of the result is 1 where that byte of the word is zero, and 0 where it is not. -/
theorem zero_bytes (w : BitVec 32) :
    byteOf (zeroBytes w) 0 = (if byteOf w 0 = 0 then 1 else 0) ∧
      byteOf (zeroBytes w) 1 = (if byteOf w 1 = 0 then 1 else 0) ∧
      byteOf (zeroBytes w) 2 = (if byteOf w 2 = 0 then 1 else 0) ∧
      byteOf (zeroBytes w) 3 = (if byteOf w 3 = 0 then 1 else 0) := by
  simp only [byteOf, zeroBytes]
  bv_decide

/-- The gather of `EMBED_LANE_NIBBLE_GATHER`: the multiply, then the top byte. -/
def gather (bits : BitVec 32) : BitVec 32 := (bits * 0x01020408) >>> 24

/-- A word whose bytes are each 0 or 1 gathers into the nibble of those bits, the lowest byte the lowest bit. -/
theorem gather_nibble (bits : BitVec 32) (h : bits &&& 0xFEFEFEFE = 0) :
    gather bits = (bits &&& 1) ||| ((bits >>> 7) &&& 2) ||| ((bits >>> 14) &&& 4) ||| ((bits >>> 21) &&& 8) := by
  simp only [gather]
  bv_decide (config := { timeout := 600 })

/-- The nibble of the equal bytes of two words: bit k set where byte k of the two is equal. -/
def equalNibble (one other : BitVec 32) : BitVec 32 := gather (zeroBytes (one ^^^ other))

theorem equal_word (one other : BitVec 32) :
    equalNibble one other < 16 ∧
      ((equalNibble one other &&& 1 = 1) ↔ byteOf one 0 = byteOf other 0) ∧
      ((equalNibble one other &&& 2 = 2) ↔ byteOf one 1 = byteOf other 1) ∧
      ((equalNibble one other &&& 4 = 4) ↔ byteOf one 2 = byteOf other 2) ∧
      ((equalNibble one other &&& 8 = 8) ↔ byteOf one 3 = byteOf other 3) := by
  simp only [equalNibble, gather, zeroBytes, byteOf]
  bv_decide (config := { timeout := 600 })

/-- The compiler's byte compare of two words: each byte all ones where equal, zero where not. -/
def compareBytes (one other : BitVec 32) : BitVec 32 :=
  (if byteOf one 0 = byteOf other 0 then 0x000000FF else 0) |||
    (if byteOf one 1 = byteOf other 1 then 0x0000FF00 else 0) |||
    (if byteOf one 2 = byteOf other 2 then 0x00FF0000 else 0) |||
    (if byteOf one 3 = byteOf other 3 then 0xFF000000 else 0)

/-- The vector arm with no byte mask builtin: the compare masked to one bit a byte, then gathered, is the same
nibble as the portable arm's. -/
theorem vector_word (one other : BitVec 32) :
    gather (compareBytes one other &&& 0x01010101) = equalNibble one other := by
  simp only [gather, compareBytes, equalNibble, zeroBytes, byteOf]
  bv_decide (config := { timeout := 600 })

/-- The lane's mask from the nibbles of its first `n` 32-bit lanes, each shifted by four bits a lane. -/
def assemble (nibble : Nat → BitVec 64) : Nat → BitVec 64
  | 0 => 0
  | n + 1 => assemble nibble n ||| (nibble n <<< (4 * n))

/-- With each nibble below 16, the mask of 16 lanes holds each lane's nibble at four bits a lane. -/
theorem assembled_16 (nibble : Nat → BitVec 64) (h : ∀ i, nibble i < 16) :
    ∀ i, i < 16 → (assemble nibble 16 >>> (4 * i)) &&& 0xF = nibble i := by
  have h0 := h 0; have h1 := h 1; have h2 := h 2; have h3 := h 3
  have h4 := h 4; have h5 := h 5; have h6 := h 6; have h7 := h 7
  have h8 := h 8; have h9 := h 9; have h10 := h 10; have h11 := h 11
  have h12 := h 12; have h13 := h 13; have h14 := h 14; have h15 := h 15
  intro i hi
  simp only [assemble]
  have : i = 0 ∨ i = 1 ∨ i = 2 ∨ i = 3 ∨ i = 4 ∨ i = 5 ∨ i = 6 ∨ i = 7 ∨ i = 8 ∨ i = 9 ∨ i = 10 ∨ i = 11 ∨
      i = 12 ∨ i = 13 ∨ i = 14 ∨ i = 15 := by omega
  rcases this with rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl | rfl <;>
    bv_decide

/-- The nibbles of the first `n` lanes, and zero past them. -/
def cut (nibble : Nat → BitVec 64) (n : Nat) : Nat → BitVec 64 := fun i => if i < n then nibble i else 0

/-- A lane of `n` words assembles the mask of 16 words whose words past `n` are zero. -/
theorem assemble_cut (nibble : Nat → BitVec 64) (n : Nat) :
    ∀ m, assemble (cut nibble n) m = assemble nibble (min m n) := by
  intro m
  induction m with
  | zero => simp [assemble]
  | succ m ih =>
    simp only [assemble, ih, cut]
    by_cases hm : m < n
    · simp only [hm, ↓reduceIte]
      rw [Nat.min_eq_left (Nat.succ_le_of_lt hm), Nat.min_eq_left (Nat.le_of_lt hm)]
      rfl
    · simp only [hm, ↓reduceIte]
      have h1 : min (m + 1) n = n := Nat.min_eq_right (by omega)
      have h2 : min m n = n := Nat.min_eq_right (by omega)
      rw [h1, h2]
      simp

/-- The lanes the header allows, 1 to 16 words: the mask holds each word's nibble at four bits a word, and zero
in each place past the last word. -/
theorem assembled (nibble : Nat → BitVec 64) (h : ∀ i, nibble i < 16) (n : Nat) (hn : n ≤ 16) :
    (∀ i, i < n → (assemble nibble n >>> (4 * i)) &&& 0xF = nibble i) ∧
      (∀ i, n ≤ i → i < 16 → (assemble nibble n >>> (4 * i)) &&& 0xF = 0) := by
  have hc : ∀ i, cut nibble n i < 16 := by
    intro i; simp only [cut]; split
    · exact h i
    · decide
  have he : assemble nibble n = assemble (cut nibble n) 16 := by
    rw [assemble_cut, Nat.min_eq_right hn]
  refine ⟨fun i hi => ?_, fun i hni hi => ?_⟩
  · rw [he, assembled_16 _ hc i (by omega)]; simp [cut, hi]
  · rw [he, assembled_16 _ hc i hi]; simp [cut]; omega

/-- The multiply of `embed_lane_fill_byte` is the sum of four shifts by whole bytes. -/
theorem fill_summed (v : BitVec 32) :
    v * 0x01010101 = v + (v <<< (8 : BitVec 32)) + (v <<< (16 : BitVec 32)) + (v <<< (24 : BitVec 32)) := by
  bv_decide (config := { timeout := 600 })

/-- `embed_lane_fill_byte`: the byte times 0x01010101 has the byte in each of its four bytes. -/
theorem fill_byte (v : BitVec 32) (h : v < 256) :
    byteOf (v * 0x01010101) 0 = v ∧ byteOf (v * 0x01010101) 1 = v ∧ byteOf (v * 0x01010101) 2 = v ∧
      byteOf (v * 0x01010101) 3 = v := by
  rw [fill_summed]
  simp only [byteOf, Nat.mul_zero, Nat.mul_one]
  bv_decide (config := { timeout := 600 })

/-- The fallback arm of `embed_lane_mask_first`: the lowest bit isolated, then placed by six masked steps. -/
def lowestFallback (mask : BitVec 64) : BitVec 64 :=
  let low := mask &&& (0 - mask)
  let step (m : BitVec 64) (w : BitVec 64) : BitVec 64 := if low &&& m != 0 then w else 0
  step 0xFFFFFFFF00000000 32 + step 0xFFFF0000FFFF0000 16 + step 0xFF00FF00FF00FF00 8 +
    step 0xF0F0F0F0F0F0F0F0 4 + step 0xCCCCCCCCCCCCCCCC 2 + step 0xAAAAAAAAAAAAAAAA 1

/-- On a value with a bit set, the fallback gives a place where the bit is set, and each bit below is clear. This
is the contract of `__builtin_ctzll`. -/
theorem mask_first (mask : BitVec 64) (h : mask ≠ 0) :
    (mask >>> lowestFallback mask) &&& 1 = 1 ∧ mask &&& (((1 : BitVec 64) <<< lowestFallback mask) - 1) = 0 := by
  unfold lowestFallback
  bv_decide

end EmbeddedTypes.Lane
