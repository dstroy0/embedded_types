import Std.Tactic.BVDecide
import EmbeddedTypes.Atomic
import EmbeddedTypes.Widths
import EmbeddedTypes.Manifold.LaneWord
import EmbeddedTypes.Manifold.Backoff
import EmbeddedTypes.Flags

/-!
# embedded_types, the device arm

The device arm of `include/embed_atomic.h` is arm 1: the atomics of CUDA in a device pass. Its primitives are
`atomicOr`, `atomicAnd`, `atomicAdd`, `atomicExch` and `atomicCAS`, and its fence is `__threadfence`. Each primitive
performs one read-modify-write on one word and returns the value before it. This file proves that the device arm
instantiates every model of this project with its own constants. So each theorem of the turnstile, the ring and the
flag matrix holds on the device as it holds on the host.

The device primitives are functions from the word to the pair of the new value and the old value. The theorems:

- `or_is_entered`, `or_is_flagged`, `and_is_unflagged`, `add_is_left`, `add_is_shut`. The device fetch-or, fetch-and
  and fetch-add on a lane word are the operations of `Manifold/LaneWord.lean`, and each returns the old value.

- `cas_is_cas`. The device compare-and-swap is the `cas` of `Atomic.lean`, so `cas_contract_32` and
  `cas_contract_64` hold on the device.

- `cas_is_transition`. A transition of the flag matrix on the device is one `atomicCAS` from the expected value to
  the new value. It succeeds where the word is the expected value.

- `raise_is_cas_loop`. One round of the raise loop on the device is `raiseRound` of `Atomic.lean`.

- `lane_is_256`, `lane_u32s_is_8`. The lane on a device is 256 bits, eight 32-bit registers of a thread, on every
  word width.

- `unit_shift_valid`, `ceiling_is_2_20`. The backoff unit shift on a device is 0, which is within the bound that
  `shift_defined` needs, and the ceiling of a wait is 2^20 ns.

The memory model of the device stays an axiom. The axiom is that `__threadfence` orders the writes before it ahead
of the reads after it, at device scope. The sequentially consistent fence does the same on the host. That is Axiom
A1 of the research paper, the same axiom that covers the host arms.
-/

namespace EmbeddedTypes.Device

/-! ## The primitives -/

/-- A device read-modify-write: the new value from the function, and the old value returned. -/
def rmw {w : Nat} (word : BitVec w) (f : BitVec w → BitVec w) : BitVec w × BitVec w := (f word, word)

/-- `atomicOr`: the word ored with the mask, and the word before. -/
def atomicOr {w : Nat} (word mask : BitVec w) : BitVec w × BitVec w := rmw word (· ||| mask)

/-- `atomicAnd`: the word anded with the mask, and the word before. -/
def atomicAnd {w : Nat} (word mask : BitVec w) : BitVec w × BitVec w := rmw word (· &&& mask)

/-- `atomicAdd`: the word plus the value with wraparound, and the word before. -/
def atomicAdd {w : Nat} (word value : BitVec w) : BitVec w × BitVec w := rmw word (· + value)

/-- `atomicExch`: the value, and the word before. -/
def atomicExch {w : Nat} (word value : BitVec w) : BitVec w × BitVec w := rmw word (fun _ => value)

/-- `atomicCAS`: the value where the word is the expected value, else the word, and the word before. -/
def atomicCAS {w : Nat} (word expected value : BitVec w) : BitVec w × BitVec w :=
  rmw word (fun x => if x = expected then value else x)

/-! ## The lane word on the device -/

/-- The entry on the device is `atomicOr` of the busy bit, which is `entered`, and it returns the old value. -/
theorem or_is_entered (w : Nat) (x : BitVec w) :
    atomicOr x (Manifold.LaneWord.busy w) = (Manifold.LaneWord.entered x, x) := by
  simp [atomicOr, rmw, Manifold.LaneWord.entered]

/-- The sleeper bit set on the device is `atomicOr` of the bit, which is `flagged`. -/
theorem or_is_flagged (w : Nat) (x : BitVec w) :
    atomicOr x (Manifold.LaneWord.sleeper w) = (Manifold.LaneWord.flagged x, x) := by
  simp [atomicOr, rmw, Manifold.LaneWord.flagged]

/-- The sleeper bit cleared on the device is `atomicAnd` of its complement, which is `unflagged`. -/
theorem and_is_unflagged (w : Nat) (x : BitVec w) :
    atomicAnd x (~~~(Manifold.LaneWord.sleeper w)) = (Manifold.LaneWord.unflagged x, x) := by
  simp [atomicAnd, rmw, Manifold.LaneWord.unflagged]

/-- The leave on the device is `atomicAdd` of the unit less the busy bit, which is `left`. -/
theorem add_is_left (w : Nat) (x : BitVec w) :
    atomicAdd x (Manifold.LaneWord.unit w - Manifold.LaneWord.busy w) = (Manifold.LaneWord.left x, x) := by
  simp [atomicAdd, rmw, Manifold.LaneWord.left]

/-- The close on the device is `atomicAdd` of the unit plus the closed bit, which is `shut`. -/
theorem add_is_shut (w : Nat) (x : BitVec w) :
    atomicAdd x (Manifold.LaneWord.unit w + Manifold.LaneWord.closed w) = (Manifold.LaneWord.shut x, x) := by
  simp [atomicAdd, rmw, Manifold.LaneWord.shut]

/-! ## The compare-and-swap on the device -/

/-- The device compare-and-swap is the `cas` of `Atomic.lean`: the same word after, the same old value, and the
answer is whether the word was the expected value. -/
theorem cas_is_cas (w : Nat) (word expected value : BitVec w) :
    (atomicCAS word expected value).1 = (Atomic.cas word expected value).1 ∧
      (atomicCAS word expected value).2 = (Atomic.cas word expected value).2.1 := by
  simp [atomicCAS, rmw, Atomic.cas]

/-- A transition of the flag matrix on the device is one `atomicCAS` from the expected value to the new value. It
writes the new value where the word is the expected value, and leaves the word alone elsewhere. -/
theorem cas_is_transition (word expected flag : BitVec 64) :
    (atomicCAS word expected (Flags.transition expected flag)).1 =
      (if word = expected then Flags.transition expected flag else word) := by
  simp [atomicCAS, rmw]

/-- One round of the raise loop on the device: the test on the value found, then `atomicCAS`. It is `raiseRound`
of `Atomic.lean`. -/
theorem raise_is_cas_loop (w : Nat) (word found value : BitVec w) :
    (if found < value then
        ((atomicCAS word found value).1, (atomicCAS word found value).2, decide (word = found))
      else (word, found, true)) = Atomic.raiseRound word found value := by
  simp [Atomic.raiseRound, Atomic.cas, atomicCAS, rmw]

/-! ## The constants of the device -/

/-- The lane on a device is 256 bits on every word width, whatever vector register the host has. -/
theorem lane_is_256 (vector w : Nat) : Widths.laneBits true vector w = 256 := by
  simp [Widths.laneBits]

/-- The lane on a device is eight 32-bit registers. -/
theorem lane_u32s_is_8 : 256 / 32 = 8 := by decide

/-- The backoff unit shift on a device is 0, within the bound that `shift_defined` needs. -/
theorem unit_shift_valid : 0 ≤ Manifold.Backoff.shiftMost := by
  simp [Manifold.Backoff.shiftMost]

/-- The ceiling of a wait on a device is 2^20 ns. -/
theorem ceiling_is_2_20 : Manifold.Backoff.limitNs 0 Manifold.Backoff.kMost = 2 ^ 20 := by
  simp [Manifold.Backoff.limitNs, Manifold.Backoff.kMost]

end EmbeddedTypes.Device
