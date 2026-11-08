/-!
# embedded_types, the drain of the manifold

A writer swaps a published pointer, then drains. The drain reads `open` after its fence, then each open lane. The
drain skips a lane whose read has busy clear or closed set: no pass holds such a lane. On each other lane it keeps
the generation it read, and waits until busy reads clear or the generation moves.

`Open.passes_below_open` puts each pass below `open`. This file holds one lane. Its steps are each operation on the
lane word: an entry, a leave, a contraction's claim, its close and its release, a grower's claim of a free lane, its
takeover of a closed lane, its adoption of the lane as its pass at its swap and its give-back, and the sleeper bit
set and cleared. A pass has an id, and the lane records it while the pass holds the lane.

- `held_until_end`: from a state where pass p holds the lane, each later state has p in the lane, busy set, closed
  clear and the generation unchanged, or has p ended.
- `over_means_ended`: so a drain that read p in the lane and later reads the lane over has seen p end.
- `drain_lane_complete`: the drain as the C runs it, its fence, then its read of the lane, then no wait where that
  read is free or closed and otherwise reads until the lane is over, ends with each pass of the fence ended.
-/

namespace EmbeddedTypes.Manifold.Drain

structure State where
  busy : Bool
  closed : Bool
  generation : Nat
  /-- The pass that holds the lane, none where it is free or a contraction or a grower holds it. -/
  holder : Option Nat
  /-- The passes that ended. -/
  ended : Nat → Bool

inductive Step : State → State → Prop where
  /-- An entry: the fetch-or takes the lane where busy was clear. -/
  | enter {s : State} (p : Nat) (h : s.busy = false) (fresh : s.ended p = false) :
      Step s { s with busy := true, holder := some p }
  /-- An entry, a claim or a compare-and-swap on a busy lane changes nothing. -/
  | refused {s : State} (h : s.busy = true) : Step s s
  /-- A leave: busy cleared, the generation counted, the pass ended. -/
  | leave {s : State} (p : Nat) (h : s.holder = some p) :
      Step s { s with busy := false, generation := s.generation + 1, holder := none,
                      ended := fun q => if q = p then true else s.ended q }
  /-- A contraction's claim, or a grower's claim, of a free lane. -/
  | claim {s : State} (h : s.busy = false) : Step s { s with busy := true, holder := none }
  /-- A contraction's close, or a grower's give-back of a closed lane: busy kept, closed set, the generation
  counted. -/
  | shut {s : State} (hb : s.busy = true) (h : s.holder = none) :
      Step s { s with closed := true, generation := s.generation + 1 }
  /-- A contraction's release, or a grower's give-back of a free lane: busy and closed cleared, the generation
  counted. -/
  | free {s : State} (hb : s.busy = true) (h : s.holder = none) :
      Step s { s with busy := false, closed := false, generation := s.generation + 1 }
  /-- A grower's takeover of a closed lane: busy kept, closed cleared, the generation counted. -/
  | takeover {s : State} (hc : s.closed = true) (h : s.holder = none) :
      Step s { s with closed := false, generation := s.generation + 1 }
  /-- A grower's swap of `open` lands, and the lane it holds is its pass. The word does not change. -/
  | adopt {s : State} (p : Nat) (hb : s.busy = true) (hc : s.closed = false) (h : s.holder = none)
      (fresh : s.ended p = false) : Step s { s with holder := some p }
  /-- The sleeper bit set or cleared: busy, closed and the generation kept. -/
  | sleeper {s : State} : Step s s

/-- The states that follow a state through zero or more steps. -/
inductive Follows : State → State → Prop where
  | refl (s : State) : Follows s s
  | step {s t u : State} : Follows s t → Step t u → Follows s u

/-- A lane with a holder is busy and not closed, and a closed lane is busy. -/
def Inv (s : State) : Prop := (s.holder.isSome → s.busy = true ∧ s.closed = false) ∧ (s.closed = true → s.busy = true)

theorem step_inv {s t : State} (i : Inv s) (st : Step s t) : Inv t := by
  obtain ⟨ih, ic⟩ := i
  cases st with
  | enter p h =>
    refine ⟨fun _ => ⟨rfl, ?_⟩, fun _ => rfl⟩
    cases e : s.closed
    · rfl
    · rw [ic e] at h; cases h
  | refused => exact ⟨ih, ic⟩
  | leave => exact ⟨fun h => by simp at h, fun h => by simp at *; exact absurd h (by
      obtain ⟨_, hc⟩ := ih (by simp [*]); simp [hc])⟩
  | claim => exact ⟨fun h => by simp at h, fun _ => rfl⟩
  | shut hb h => exact ⟨fun hh => by simp [h] at hh, fun _ => hb⟩
  | free hb h => exact ⟨fun hh => by simp [h] at hh, fun hh => by simp at hh⟩
  | takeover hc h => exact ⟨fun hh => by simp [h] at hh, fun hh => by simp at hh⟩
  | adopt p hb hc h => exact ⟨fun _ => ⟨hb, hc⟩, fun hh => by simp [hc] at hh⟩
  | sleeper => exact ⟨ih, ic⟩

/-- Where pass p holds the lane, each later state has p in the lane, busy, not closed, at the same generation, or
has p ended. -/
theorem held_until_end {s u : State} (p : Nat) (i : Inv s) (hp : s.holder = some p) (f : Follows s u) :
    Inv u ∧ ((u.holder = some p ∧ u.busy = true ∧ u.closed = false ∧ u.generation = s.generation) ∨
      u.ended p = true) := by
  induction f with
  | refl => exact ⟨i, Or.inl ⟨hp, (i.1 (by simp [hp])).1, (i.1 (by simp [hp])).2, rfl⟩⟩
  | step _ st ih =>
    obtain ⟨it, held | done⟩ := ih
    · obtain ⟨hh, hb, hc, hg⟩ := held
      refine ⟨step_inv it st, ?_⟩
      cases st with
      | enter q h => rw [hb] at h; cases h
      | refused => exact Or.inl ⟨hh, hb, hc, hg⟩
      | leave q h =>
        rw [hh] at h
        cases h
        right
        simp
      | claim h => rw [hb] at h; cases h
      | shut _ h => rw [hh] at h; cases h
      | free _ h => rw [hh] at h; cases h
      | takeover _ h => rw [hh] at h; cases h
      | adopt _ _ _ h => rw [hh] at h; cases h
      | sleeper => exact Or.inl ⟨hh, hb, hc, hg⟩
    · refine ⟨step_inv it st, Or.inr ?_⟩
      cases st with
      | leave q h =>
        simp only
        split
        · rfl
        · exact done
      | _ => exact done

/-- The drain's test: busy clear, or the generation moved from the one it read. -/
def over (snapshot now : State) : Bool := !now.busy || now.generation != snapshot.generation

/-- A drain that read pass p in the lane, and reads the lane over later, has seen p end. -/
theorem over_means_ended {s u : State} (p : Nat) (i : Inv s) (hp : s.holder = some p) (f : Follows s u)
    (ho : over s u = true) : u.ended p = true := by
  obtain ⟨_, held | done⟩ := held_until_end p i hp f
  · obtain ⟨_, hb, _, hg⟩ := held
    simp [over, hb, hg] at ho
  · exact done

/-- An ended pass stays ended. -/
theorem ended_stays {s u : State} (p : Nat) (f : Follows s u) (h : s.ended p = true) : u.ended p = true := by
  induction f with
  | refl => exact h
  | step _ st ih =>
    cases st with
    | leave q _ =>
      simp only
      split
      · rfl
      · exact ih
    | _ => exact ih

/-- The drain on one lane, as the C runs it. Pass p holds the lane at the drain's fence, state s. The drain reads
the lane later, at state t. Where that read has busy clear or closed set, the drain does not wait. Otherwise it reads
the lane until it is over, at state u. In each case p has ended at u. With `Open.passes_below_open`, each pass at
the fence is below the open count that the drain reads, so the drain's loop over the open lanes ends with each pass
of the fence ended. -/
theorem drain_lane_complete {s t u : State} (p : Nat) (i : Inv s) (hp : s.holder = some p) (ft : Follows s t)
    (fu : Follows t u) (hdone : t.busy = false ∨ t.closed = true ∨ over t u = true) : u.ended p = true := by
  obtain ⟨it, held | done⟩ := held_until_end p i hp ft
  · obtain ⟨hh, hb, hc, _⟩ := held
    rcases hdone with hfree | hclosed | hover
    · rw [hb] at hfree
      cases hfree
    · rw [hc] at hclosed
      cases hclosed
    · exact over_means_ended p it hh fu hover
  · exact ended_stays p fu done

end EmbeddedTypes.Manifold.Drain
