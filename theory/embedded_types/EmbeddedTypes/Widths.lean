/-!
# embedded_types, the widths the preprocessor derives

`include/embed_types.h`, `include/embed_lane.h`, `include/embed_turnstile.h` and `include/embed_dispatch_layout.h`
derive their widths in `#if` chains and hold them with `EMBED_STATIC_ASSERT`. Each definition here is one chain,
and `decide` checks each theorem on every arm of the chain.

- `word_bits`. `EMBED_WORD_BITS` is 64, 32 or 16 from `UINTPTR_MAX`, and every other value stops the build.

- `index_fits`. `EMBED_INDEX_BITS` fits the word on every arm, the assertion `sizeof(embed_index) <= sizeof(embed_word)`.

- `lane_holds`. On every arm, `EMBED_LANE_BITS` is whole 32-bit lanes, at least one, at most 512, and whole words.
  `EMBED_LANE_U32S` is 1, 2, 4, 8 or 16, the arms of `EMBED_LANE_WORDS`. The byte mask holds a bit for each byte.

- `lane_fields`, `generation_widths` and `lanes_fit`. At each word width, the busy, closed and sleeper bits of the
  lane word and its generation fill the word. The generation is 61 bits wide at 64, 29 at 32 and 13 at 16. The open
  count fits.
- `narg_counts`: `EMBED_NARG` gives n for each n from 1 to 24.
-/

namespace EmbeddedTypes.Widths

/-- `EMBED_WORD_BITS` from `UINTPTR_MAX`, or none where the chain ends at `#error`. -/
def wordBits (uintptrMax : Nat) : Option Nat :=
  if uintptrMax = 2 ^ 64 - 1 then some 64
  else if uintptrMax = 2 ^ 32 - 1 then some 32
  else if uintptrMax = 2 ^ 16 - 1 then some 16
  else none

/-- The chain gives a word only for the three pointer widths. -/
theorem word_bits (m : Nat) (w : Nat) (h : wordBits m = some w) : w = 64 ∨ w = 32 ∨ w = 16 := by
  unfold wordBits at h
  split at h
  · cases h; exact Or.inl rfl
  · split at h
    · cases h; exact Or.inr (Or.inl rfl)
    · split at h
      · cases h; exact Or.inr (Or.inr rfl)
      · cases h

/-- `EMBED_INDEX_BITS` where the build does not set it. -/
def indexBits (w : Nat) : Nat := if w = 16 then 16 else 32

/-- The index fits the word at each of the three widths. -/
theorem index_fits : ∀ w ∈ [64, 32, 16], indexBits w ≤ w ∧ (indexBits w = 16 ∨ indexBits w = 32) := by decide

/-- `EMBED_VECTOR_REGISTER_BITS`: the four arms of its chain. -/
def vectorArms : List Nat := [512, 256, 128, 0]

/-- `EMBED_LANE_BITS` where the build does not set it: 256 on a CUDA device, the vector register where there is
one, the word above 32 bits, and 32 below. -/
def laneBits (cuda : Bool) (vector w : Nat) : Nat :=
  if cuda then 256 else if vector ≠ 0 then vector else if w > 32 then w else 32

/-- The byte mask's width: 32 bits up to a 32-byte lane, 64 above. -/
def maskBits (laneBytes : Nat) : Nat := if laneBytes ≤ 32 then 32 else 64

/-- Every arm meets the assertions of `embed_lane.h` and lands on an arm of `EMBED_LANE_WORDS`. -/
theorem lane_holds : ∀ cuda ∈ [true, false], ∀ v ∈ vectorArms, ∀ w ∈ [64, 32, 16],
    let bits := laneBits cuda v w
    bits % 32 = 0 ∧ 32 ≤ bits ∧ bits ≤ 512 ∧ (bits / 8) % (w / 8) = 0 ∧
      (bits / 32 = 1 ∨ bits / 32 = 2 ∨ bits / 32 = 4 ∨ bits / 32 = 8 ∨ bits / 32 = 16) ∧
      bits / 8 ≤ maskBits (bits / 8) := by
  decide

/-- The lane word at a word width: the busy, closed and sleeper bits, and the generation's bits. -/
def generationBits (w : Nat) : Nat := w - 3

/-- The four fields fill the word at each width, and the generation is 61, 29 or 13 bits wide. -/
theorem lane_fields : ∀ w ∈ [64, 32, 16], 3 + generationBits w = w ∧ 0 < generationBits w := by decide

theorem generation_widths : generationBits 64 = 61 ∧ generationBits 32 = 29 ∧ generationBits 16 = 13 := by decide

/-- `EMBED_TURNSTILE_LANES` is from 1 to 64, and the open count less one fits each word width. -/
theorem lanes_fit : ∀ w ∈ [64, 32, 16], 64 - 1 < 2 ^ w := by decide

/-- `EMBED_NARG`: the caller's arguments, then the constants 24 down to 0; `EMBED_ARG_N` takes the 25th. -/
def narg (n : Nat) : Option Nat :=
  ((List.replicate n 0) ++ (List.range 25).reverse)[24]?

/-- The selected slot is the count of arguments for 1 to 24 arguments. -/
theorem narg_counts : ∀ n ∈ List.range' 1 24, narg n = some n := by decide

end EmbeddedTypes.Widths
