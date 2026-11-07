/-!
# embedded_types, the C11 orders of the turnstile

`Turnstile.lean` interleaves whole steps. `include/embed_atomic.h` lowers them to C11: a load is `acquire`, a store
is `release`, a fetch-or and a fetch-and are `acq_rel`, and `EMBED_ATOMIC_ORDERED` is a `seq_cst` fence. Two facts
of C11 ([atomics.order], [intro.races]) carry the model's theorem to that code.

The fences. A badge-in claims its lane with a fetch-or on `busy` and writes its badge, then a fence, then reads what
the writer publishes (`embed_turnstile_enter`). A writer swaps its published pointer, then a fence, then loads `busy`
for the drain's snapshot (`embed_turnstiles_drain`). The fence rule for two `seq_cst` fences gives the
disjunction `drain_or_new`: the pass reads the new pointer, or the snapshot holds the pass's lane.

The chain. Where the drain takes a lane off its snapshot, its acquire load read one of two writes: the fetch-and of
the end of the pass, or the badge store of a later pass through the lane. That later pass claimed the lane with a
fetch-or, which read the lane's bit clear: the fetch-and of the end of the pass, or a write after it in the
modification order of `busy`, each a read-modify-write. A read-modify-write reads the last value before its own
write, and an `acq_rel` one that reads a `release` write synchronizes with it. Thus each link of the chain is a
happens-before edge, and `drained_after` gives the result: each read of the old pass happens before the drain's
return.
-/

namespace EmbeddedTypes.TurnstileOrder

/-- One pass and one writer as the two fences order them in the total order of `seq_cst` fences. -/
structure FenceExec where
  passFence : Nat
  writerFence : Nat
  distinct : passFence ≠ writerFence
  /-- The pass's read of the published pointer reads the writer's swap, or a later write. -/
  passReadsNew : Bool
  /-- The snapshot's load of `busy` reads the pass's claim, or a later write that keeps its bit. -/
  snapshotHasLane : Bool
  /-- The fence rule, the pass's fence first: the claim before it, the snapshot's load after the writer's fence. -/
  passFirst : passFence < writerFence → snapshotHasLane = true
  /-- The fence rule, the writer's fence first: the swap before it, the pass's read after the pass's fence. -/
  writerFirst : writerFence < passFence → passReadsNew = true

/-- The pass reads the new pointer, or the drain's snapshot holds its lane and the drain waits it out. -/
theorem drain_or_new (e : FenceExec) : e.passReadsNew = true ∨ e.snapshotHasLane = true := by
  rcases Nat.lt_or_gt_of_ne e.distinct with h | h
  · exact Or.inr (e.passFirst h)
  · exact Or.inl (e.writerFirst h)

/-- The events of one lane: the old pass's last read of what the writer releases, its two fetch-ands, the next
pass's claim and badge store, and the drain's load and return. `hb` is happens-before. -/
structure ChainExec where
  hb : Nat → Nat → Prop
  trans : ∀ a b c, hb a b → hb b c → hb a c
  /-- The events. -/
  lastRead : Nat
  clearBusy : Nat
  nextClaim : Nat
  nextBadge : Nat
  drainLoad : Nat
  drainReturn : Nat
  /-- Sequenced before: the old pass reads, then ends; the next pass claims, then writes its badge; the drain
  loads, then returns. -/
  readBeforeClear : hb lastRead clearBusy
  claimBeforeBadge : hb nextClaim nextBadge
  loadBeforeReturn : hb drainLoad drainReturn
  /-- Synchronizes with: the next claim, an `acq_rel` fetch-or, read the old pass's `release` fetch-and through a
  release sequence of read-modify-writes on `busy`. -/
  claimSyncs : hb clearBusy nextClaim
  /-- What the drain's acquire load read: the old pass's fetch-and of `busy`, or the next pass's badge store. -/
  loadReadsClear : Bool
  clearSyncs : loadReadsClear = true → hb clearBusy drainLoad
  badgeSyncs : loadReadsClear = false → hb nextBadge drainLoad

/-- Each read of the old pass happens before the drain's return, by either write that the drain's load read. -/
theorem drained_after (e : ChainExec) : e.hb e.lastRead e.drainReturn := by
  cases h : e.loadReadsClear with
  | true =>
    exact e.trans _ _ _ (e.trans _ _ _ e.readBeforeClear (e.clearSyncs h)) e.loadBeforeReturn
  | false =>
    have hclaim := e.trans _ _ _ e.readBeforeClear e.claimSyncs
    have hbadge := e.trans _ _ _ hclaim e.claimBeforeBadge
    exact e.trans _ _ _ (e.trans _ _ _ hbadge (e.badgeSyncs h)) e.loadBeforeReturn

end EmbeddedTypes.TurnstileOrder
