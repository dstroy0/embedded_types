import Std.Tactic.BVDecide

/-!
# embedded_types, the widths the preprocessor derives

`include/embed_types.h`, `include/embed_lane.h`, `include/embed_turnstile.h` and `include/embed_dispatch_layout.h`
derive their widths in `#if` chains and hold them with `EMBED_STATIC_ASSERT`. Each definition here is one chain,
and `decide` checks each theorem on every arm of the chain.

- `word_bits`. `EMBED_WORD_BITS` is 64, 32 or 16 from `UINTPTR_MAX`, and every other value stops the build.

- `index_fits`. `EMBED_INDEX_BITS` fits the word on every arm, the assertion `sizeof(embed_index) <= sizeof(embed_word)`.

- `lane_holds`. On every arm, `EMBED_LANE_BITS` is whole 32-bit lanes, at least one, at max 512, and whole words.
  `EMBED_LANE_U32S` is 1, 2, 4, 8 or 16, the arms of `EMBED_LANE_WORDS`. The byte mask holds a bit for each byte.

- `lane_fields`, `generation_widths` and `lanes_fit`. The lane word is an `embed_atomic_word`, 64 bits on a word of
  64 bits and 32 bits below it. At each word width, the busy, closed and sleeper bits of the lane word and its
  generation fill that atomic word. The generation is 61 bits wide at 64, and 29 at 32 and 16. The open count fits.

- `target_classes`. `EMBED_VECTOR_REGISTER_BITS` and the CUDA device pass select `EMBED_TARGET`: a device in the
  device pass of CUDA, a host with a vector register, and embedded with none.

- `turnstile_lanes_hold` and `targets_select`. `EMBED_TARGET` selects `EMBED_TURNSTILE_LANES` and
  `EMBED_TURNSTILE_UNIT_SHIFT`. An embedded target gets 4 lanes and the shift 0, a host 64 and 10, and a device 64
  and 0. Every arm meets the assertions of the turnstile and the ticket ring.

- `stored_round_trip`, `stored_set_is_split`, `stored_set_get` and `stored_upper_kept`. On an embedded target,
  `embed_stored_u64` holds a count as its lower word and its upper word. A read after a set gives the count back.
  The set writes the upper word only where it changes.

- `derived_builds` and `wide_without_stops`. `EMBED_ATOMIC_HAS_64` is 0 on MSVC below a pointer of 64 bits. There,
  the word, the size and the pointer that the header derives are 32 bits, and each gets the 32-bit atomic word. A
  width of 64 on that target stops the build.

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

/-- The width of `embed_atomic_word` at a word width: 64 bits on a word of 64 bits, and 32 bits below it. No arm
has an atomic word of 16 bits. -/
def atomicBits (w : Nat) : Nat := if w = 64 then 64 else 32

/-- The lane word at a word width is an `embed_atomic_word`: the busy, closed and sleeper bits, and the generation's
bits. -/
def generationBits (w : Nat) : Nat := atomicBits w - 3

/-- The four fields fill the atomic word at each width. The generation is 61 bits wide at 64, and 29 at 32 and 16. -/
theorem lane_fields : ∀ w ∈ [64, 32, 16], 3 + generationBits w = atomicBits w ∧ 0 < generationBits w := by decide

theorem generation_widths : generationBits 64 = 61 ∧ generationBits 32 = 29 ∧ generationBits 16 = 29 := by decide

/-- `EMBED_ATOMIC_HAS_64`: 0 on the MSVC arm on a target whose pointer is below 64 bits, and 1 on every other arm
and target. -/
def has64 (msvc : Bool) (pointerBits : Nat) : Bool := !msvc || pointerBits = 64

/-- The atomic word that `embed_atomic.h` gives a width: the 64-bit word at 64, where the arm has one, and the
32-bit word below. None is the `#error` of a width of 64 on an arm with no 64-bit word. -/
def atomicFor (has : Bool) (bits : Nat) : Option Nat :=
  if bits = 64 then (if has then some 64 else none) else some 32

/-- Take each arm and each pointer width. The word that the header derives from the pointer, the size and the pointer
each get an atomic word. So no derived build stops at the `#error`. On MSVC, `size_t` is as wide as the pointer. -/
theorem derived_builds : ∀ msvc ∈ [true, false], ∀ p ∈ [16, 32, 64],
    ((wordBits (2 ^ p - 1)).bind (atomicFor (has64 msvc p))).isSome ∧ (atomicFor (has64 msvc p) p).isSome := by
  decide

/-- A width of 64 on an arm with no 64-bit word stops the build. -/
theorem wide_without_stops : atomicFor (has64 true 32) 64 = none := by decide

/-- `EMBED_TURNSTILE_LANES` is from 1 to 64, and the open count less one fits each word width. -/
theorem lanes_fit : ∀ w ∈ [64, 32, 16], 64 - 1 < 2 ^ w := by decide

/-- `EMBED_TARGET`: the class of the target. -/
inductive Target where
  | host
  | device
  | embedded
  deriving DecidableEq, Repr

/-- `EMBED_TARGET` where the build does not set it. The device pass of CUDA is a device. A target with a vector
register is a host, and a target with none is an embedded target. -/
def target (cuda : Bool) (vector : Nat) : Target :=
  if cuda then .device else if vector ≠ 0 then .host else .embedded

/-- The lane count of each class: 4 on an embedded target, and 64 on a host and on a device. -/
def lanesOf : Target → Nat
  | .embedded => 4
  | _ => 64

/-- The unit shift of each class: 10 on a host, and 0 on a device and on an embedded target. -/
def shiftOf : Target → Nat
  | .host => 10
  | _ => 0

/-- `EMBED_TURNSTILE_LANES` where the build does not set it, from the class of the target. -/
def turnstileLanes (cuda : Bool) (vector : Nat) : Nat := lanesOf (target cuda vector)

/-- `EMBED_TURNSTILE_UNIT_SHIFT` where the build does not set it, from the class of the target. -/
def unitShift (cuda : Bool) (vector : Nat) : Nat := shiftOf (target cuda vector)

/-- The class of each target. The device pass of CUDA is a device on every arm. A vector register makes a host, and
no vector register makes an embedded target. -/
theorem target_classes :
    (∀ v ∈ vectorArms, target true v = .device) ∧ (∀ v ∈ [512, 256, 128], target false v = .host) ∧
      target false 0 = .embedded := by
  decide

/-- On every arm, the lane count meets the assertion of `embed_turnstile.h`, from 1 to 64, and the assertion of
`embed_ticket_ring.h`, at max 128. The open count less one fits each word width, and the unit shift is 10 at max. -/
theorem turnstile_lanes_hold : ∀ cuda ∈ [true, false], ∀ v ∈ vectorArms,
    1 ≤ turnstileLanes cuda v ∧ turnstileLanes cuda v ≤ 64 ∧ turnstileLanes cuda v ≤ 128 ∧
      (∀ w ∈ [64, 32, 16], turnstileLanes cuda v - 1 < 2 ^ w) ∧ unitShift cuda v ≤ 10 := by
  decide

/-- A microcontroller, with no vector register and no CUDA, gets 4 lanes and the unit shift 0. A CPU host gets 64
lanes and 10. A CUDA device gets 64 lanes and 0. -/
theorem targets_select :
    (turnstileLanes false 0 = 4 ∧ unitShift false 0 = 0) ∧
      (∀ v ∈ [512, 256, 128], turnstileLanes false v = 64 ∧ unitShift false v = 10) ∧
      (∀ v ∈ vectorArms, turnstileLanes true v = 64 ∧ unitShift true v = 0) := by
  decide

/-- `embed_stored_u64` on an embedded target: the lower word and the upper word of the count. -/
def storedSplit (count : BitVec 64) : BitVec 32 × BitVec 32 := (count.setWidth 32, (count >>> 32).setWidth 32)

/-- `embed_stored_u64_get` on an embedded target: the upper word moves to bits 32 to 63, under the lower word. -/
def storedGet (halves : BitVec 32 × BitVec 32) : BitVec 64 := (halves.2.setWidth 64 <<< 32) ||| halves.1.setWidth 64

/-- `embed_stored_u64_set` on an embedded target: the lower word always, and the upper word where it changes. -/
def storedSet (old : BitVec 32 × BitVec 32) (count : BitVec 64) : BitVec 32 × BitVec 32 :=
  ((storedSplit count).1, if old.2 ≠ (storedSplit count).2 then (storedSplit count).2 else old.2)

/-- The halves hold the count: a read after a split gives the count back. -/
theorem stored_round_trip (count : BitVec 64) : storedGet (storedSplit count) = count := by
  simp only [storedGet, storedSplit]
  bv_decide

/-- The set that writes the upper word only where it changes leaves the same halves as a write of both. -/
theorem stored_set_is_split (old : BitVec 32 × BitVec 32) (count : BitVec 64) :
    storedSet old count = storedSplit count := by
  simp only [storedSet]
  split <;> simp_all

/-- So a read after each set gives the count that the set wrote. -/
theorem stored_set_get (old : BitVec 32 × BitVec 32) (count : BitVec 64) : storedGet (storedSet old count) = count := by
  rw [stored_set_is_split, stored_round_trip]

/-- A count whose upper word is the upper word already stored leaves the upper word unwritten. -/
theorem stored_upper_kept (old : BitVec 32 × BitVec 32) (count : BitVec 64) (h : old.2 = (storedSplit count).2) :
    (storedSet old count).2 = old.2 := by
  simp [storedSet, h]

/-- `EMBED_NARG`: the caller's arguments, then the constants 24 down to 0; `EMBED_ARG_N` takes the 25th. -/
def narg (n : Nat) : Option Nat :=
  ((List.replicate n 0) ++ (List.range 25).reverse)[24]?

/-- The selected slot is the count of arguments for 1 to 24 arguments. -/
theorem narg_counts : ∀ n ∈ List.range' 1 24, narg n = some n := by decide

end EmbeddedTypes.Widths
