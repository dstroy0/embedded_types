import EmbeddedTypes.Manifold.Drain
import EmbeddedTypes.Flags

/-!
# embedded_types, the integrated algorithm

One model of the whole algorithm. It has the lane words of the turnstile and the slots of the ticket ring. It has
the workers that pass through the lanes and pop the slots. It also has the supervisor that kills a worker and frees
its lane.

The lane words are the states of `Manifold/Drain.lean`. Each step on a lane is a step of that file, so its theorems
apply to each lane here. The check of a job pointer is the check of `Flags.lean`, and a pop records whether the
pointer passed it.

The steps:

- `enter`. An idle worker takes a free lane. The lane word takes the entry step of the drain model.

- `grant`. A worker in a pass gives an empty or a released slot the next ticket of the counter of the grantor. Then
  the counter moves on.

- `push`. A worker in a pass pushes a job to a granted slot. The slot records whether its job pointer passes the
  check.

- `pop`. A worker in a pass pops a slot in the pushed state. The slot records the lane of the pass, and whether
  the job ran. The job runs only where its pointer passed the check. A pop of a slot in another state has no step:
  it fails.

- `replay`. A worker in a pass replays a popped slot, as a rewind does. The slot retains its record.

- `release`. The writer releases a popped or a replayed slot after its drain. The next grant at the index reuses
  it.

- `leave`. A worker in a pass ends it. The lane word takes the leave step of the drain model.

- `kill`. The supervisor kills a worker in a pass. The worker is dead on its lane, and the lane word does not change.

- `free`. The supervisor frees the lane of a dead worker, with the lane index that the popped word recorded. The
  lane word takes the leave step on behalf of the dead worker.

The theorems:

- `refused_never_runs`. A popped slot whose job pointer failed the check never ran its job. With
  `Flags.job_flip_changes_check`, a single-bit error in a job pointer never runs.

- `popped_moves` and `replayed_moves`. A popped slot moves on only to its replay or its release, and a replayed slot
  only to its release. Each keeps its stamp, its lane and its check. So a pop has one replay at max.

- `grant_once`. The stamps of the slots are below the counter of the grantor, and no two slots hold one. So the
  grantor grants each ticket once, which is that part of Axiom A3, as a theorem for a grantor that counts.

- `popped_never_pushed_again`. A ticket that a pop took is never granted or pushed again. So a pop takes the job of
  each ticket once at max.

- `pop_reachable`. The steps reach a popped slot from rest, so the theorems on popped slots hold of reachable
  states.

- `lane_identifies_worker`. A worker in a pass or dead holds the lane that its state names, and that lane names
  the worker back. Two workers never share a lane.

- `popped_lane_was_held`. The worker that popped a slot held the lane that the slot records, at the pop.

- `free_ends_pass`. The free of a dead worker ends its pass on the lane word. The lane reads free, and the pass of
  the worker has the ended mark. Any drain that read that lane held by the worker reads it over.

- `drain_completes`. In a state where every worker is idle or gone, no lane has a holder and no lane is busy. So
  every drain reads every lane over and ends.

- `lane_invariant`. Every lane word of a reachable state satisfies the invariant of the drain model, so
  `held_until_end` and `drain_lane_complete` apply to each lane of the integrated model.

- `reach_below` and `busy_below_count`. The model has n lanes, `EMBED_TURNSTILE_LANES` as n, and an entry takes a
  lane below n. Each lane that a worker holds and each busy lane stays below n. `Config.lean` sets n to the count
  that each target selects.
-/

namespace EmbeddedTypes.Integrated

/-- A worker: idle, in a pass on a lane, dead on a lane that it still holds, or gone after the supervisor freed
its lane. -/
inductive Worker where
  | idle
  | inPass (lane : Nat)
  | dead (lane : Nat)
  | gone
  deriving DecidableEq

/-- A slot, in the state sequence of `embed_ticket_ring.h`: grant, push, pop, replay, release. It is empty, or in
the state of its last transition. A granted slot holds a stamp, and a pushed slot also holds whether its job pointer
passes the check. A popped slot holds the lane of the pass that popped and whether the job ran. A replay and a
release retain that record, and the next grant at the index reuses a released slot. -/
inductive SlotState where
  | empty
  | granted (stamp : Nat)
  | pushed (stamp : Nat) (intact : Bool)
  | popped (stamp lane : Nat) (ran : Bool)
  | replayed (stamp lane : Nat) (ran : Bool)
  | released (stamp lane : Nat) (ran : Bool)
  deriving DecidableEq

/-- The stamp that a slot holds, none where it is empty. -/
def SlotState.stampOf : SlotState → Option Nat
  | .empty => none
  | .granted s => some s
  | .pushed s _ => some s
  | .popped s _ _ => some s
  | .replayed s _ _ => some s
  | .released s _ _ => some s

/-- The state: the lane words, the workers, the slots, and the counter of the grantor, which holds the next ticket. -/
structure State where
  lane : Nat → Manifold.Drain.State
  worker : Nat → Worker
  slot : Nat → SlotState
  next : Nat

/-- The lane word at rest: free, not closed, no holder, no pass ended. -/
def laneRest : Manifold.Drain.State :=
  { busy := false, closed := false, generation := 0, holder := none, ended := fun _ => false }

/-- The lane word after an entry by worker w. -/
def laneEntered (l : Manifold.Drain.State) (w : Nat) : Manifold.Drain.State :=
  { l with busy := true, holder := some w }

/-- The lane word after the leave of worker w. -/
def laneLeft (l : Manifold.Drain.State) (w : Nat) : Manifold.Drain.State :=
  { l with busy := false, generation := l.generation + 1, holder := none,
           ended := fun q => if q = w then true else l.ended q }

def State.setLane (s : State) (at_ : Nat) (v : Manifold.Drain.State) : State :=
  { s with lane := fun i => if i = at_ then v else s.lane i }

def State.setWorker (s : State) (w : Nat) (v : Worker) : State :=
  { s with worker := fun i => if i = w then v else s.worker i }

def State.setSlot (s : State) (at_ : Nat) (v : SlotState) : State :=
  { s with slot := fun i => if i = at_ then v else s.slot i }

/-- The steps of a set of n lanes, `EMBED_TURNSTILE_LANES` as n. An entry takes a lane below n, as the spill of
`Manifold/Open.lean` does. -/
inductive Step (n : Nat) : State → State → Prop where
  /-- An idle worker takes a free lane below the lane count, with the entry step of the lane word. -/
  | enter {s : State} (w l : Nat) (hn : l < n) (hw : s.worker w = .idle) (hl : (s.lane l).busy = false)
      (fresh : (s.lane l).ended w = false) :
      Step n s ((s.setLane l (laneEntered (s.lane l) w)).setWorker w (.inPass l))
  /-- A worker in a pass grants an empty or a released slot the next ticket of the counter, and the counter moves
  on. So the grantor grants each ticket once. -/
  | grant {s : State} (w l at_ : Nat) (hw : s.worker w = .inPass l)
      (hs : s.slot at_ = .empty ∨ ∃ st l' r, s.slot at_ = .released st l' r) :
      Step n s { s.setSlot at_ (.granted s.next) with next := s.next + 1 }
  /-- A worker in a pass pushes a job to a granted slot. `intact` is whether its job pointer passes the check. -/
  | push {s : State} (w l at_ stamp : Nat) (intact : Bool) (hw : s.worker w = .inPass l)
      (hs : s.slot at_ = .granted stamp) :
      Step n s (s.setSlot at_ (.pushed stamp intact))
  /-- A worker in a pass pops a pushed slot. The job runs where its pointer passed the check. -/
  | pop {s : State} (w l at_ stamp : Nat) (intact : Bool) (hw : s.worker w = .inPass l)
      (hs : s.slot at_ = .pushed stamp intact) :
      Step n s (s.setSlot at_ (.popped stamp l intact))
  /-- A worker in a pass replays a popped slot: a rewind reads the job again, with the same check as at the pop. -/
  | replay {s : State} (w l at_ stamp l' : Nat) (ran : Bool) (hw : s.worker w = .inPass l)
      (hs : s.slot at_ = .popped stamp l' ran) :
      Step n s (s.setSlot at_ (.replayed stamp l' ran))
  /-- The writer releases a popped or a replayed slot after its drain. -/
  | release {s : State} (at_ stamp l : Nat) (ran : Bool)
      (hs : s.slot at_ = .popped stamp l ran ∨ s.slot at_ = .replayed stamp l ran) :
      Step n s (s.setSlot at_ (.released stamp l ran))
  /-- A worker ends its pass, with the leave step of the lane word. -/
  | leave {s : State} (w l : Nat) (hw : s.worker w = .inPass l) :
      Step n s ((s.setLane l (laneLeft (s.lane l) w)).setWorker w .idle)
  /-- The supervisor kills a worker in a pass. The lane word does not change. -/
  | kill {s : State} (w l : Nat) (hw : s.worker w = .inPass l) : Step n s (s.setWorker w (.dead l))
  /-- The supervisor frees the lane of a dead worker, with the leave step of the lane word on its behalf. -/
  | free {s : State} (w l : Nat) (hw : s.worker w = .dead l) :
      Step n s ((s.setLane l (laneLeft (s.lane l) w)).setWorker w .gone)

/-- The state at rest: every lane free, every worker idle, every slot empty, and the counter at 0. -/
def rest : State := { lane := fun _ => laneRest, worker := fun _ => .idle, slot := fun _ => .empty, next := 0 }

/-- The states that the steps of n lanes reach from rest. -/
inductive Reach (n : Nat) : State → Prop where
  | rest : Reach n rest
  | step {s t : State} : Reach n s → Step n s t → Reach n t

/-! ## The invariant -/

/-- The invariant of a state. A worker in a pass or dead on a lane holds that lane: the lane is busy and its holder
is the worker. A lane with a holder has that holder in a pass or dead on it. A busy lane has a holder. Each lane word
satisfies the invariant of the drain model. -/
structure Inv (s : State) : Prop where
  holds : ∀ w l, (s.worker w = .inPass l ∨ s.worker w = .dead l) →
    (s.lane l).busy = true ∧ (s.lane l).holder = some w
  holder : ∀ l w, (s.lane l).holder = some w → s.worker w = .inPass l ∨ s.worker w = .dead l
  busy : ∀ l, (s.lane l).busy = true → (s.lane l).holder ≠ none
  lane : ∀ l, Manifold.Drain.Inv (s.lane l)

theorem rest_inv : Inv rest where
  holds := by intro w l h; rcases h with h | h <;> simp [rest] at h
  holder := by intro l w h; simp [rest, laneRest] at h
  busy := by intro l h; simp [rest, laneRest] at h
  lane := by intro l; exact ⟨fun h => by simp [rest, laneRest] at h, fun h => by simp [rest, laneRest] at h⟩

/-- Each lane step of the integrated model is a step of the drain model. -/
theorem entered_is_step (l : Manifold.Drain.State) (w : Nat) (hb : l.busy = false) (fresh : l.ended w = false) :
    Manifold.Drain.Step l (laneEntered l w) :=
  Manifold.Drain.Step.enter w hb fresh

theorem left_is_step (l : Manifold.Drain.State) (w : Nat) (h : l.holder = some w) :
    Manifold.Drain.Step l (laneLeft l w) :=
  Manifold.Drain.Step.leave w h

/-- The leave of the holder of a lane retains the invariant, for a leave and for a free. -/
theorem left_inv {s : State} (i : Inv s) (w l : Nat) (hw : s.worker w = .inPass l ∨ s.worker w = .dead l)
    (hold : (s.lane l).busy = true ∧ (s.lane l).holder = some w)
    (v : Worker) (hv : ∀ l', v ≠ .inPass l' ∧ v ≠ .dead l') :
    Inv ((s.setLane l (laneLeft (s.lane l) w)).setWorker w v) := by
  refine ⟨?_, ?_, ?_, ?_⟩
  · intro w' l' h
    simp only [State.setWorker, State.setLane] at h ⊢
    by_cases hww : w' = w
    · subst hww
      rw [ite_eq_left rfl] at h
      rcases h with h | h
      · exact absurd h (hv l').1
      · exact absurd h (hv l').2
    · rw [ite_eq_right hww] at h
      obtain ⟨hb, hh⟩ := i.holds w' l' h
      by_cases hll : l' = l
      · subst hll
        rw [hold.2] at hh
        simp only [Option.some.injEq] at hh
        exact absurd hh.symm hww
      · rw [ite_eq_right hll]
        exact ⟨hb, hh⟩
  · intro l' w' h
    simp only [State.setWorker, State.setLane] at h ⊢
    by_cases hll : l' = l
    · subst hll
      rw [ite_eq_left rfl] at h
      simp [laneLeft] at h
    · rw [ite_eq_right hll] at h
      have := i.holder l' w' h
      by_cases hww : w' = w
      · subst hww
        rcases this with this | this <;> rcases hw with hw | hw <;> rw [hw] at this <;> cases this <;>
          exact absurd rfl hll
      · rw [ite_eq_right hww]
        exact this
  · intro l' h
    simp only [State.setWorker, State.setLane] at h ⊢
    by_cases hll : l' = l
    · subst hll
      rw [ite_eq_left rfl] at h
      simp [laneLeft] at h
    · rw [ite_eq_right hll] at h ⊢
      exact i.busy l' h
  · intro l'
    simp only [State.setWorker, State.setLane]
    by_cases hll : l' = l
    · subst hll
      rw [ite_eq_left rfl]
      exact Manifold.Drain.step_inv (i.lane l') (left_is_step _ _ hold.2)
    · rw [ite_eq_right hll]
      exact i.lane l'

theorem step_inv {n : Nat} {s t : State} (i : Inv s) (st : Step n s t) : Inv t := by
  cases st with
  | enter w l hn hw hl fresh =>
    refine ⟨?_, ?_, ?_, ?_⟩
    · intro w' l' h
      simp only [State.setWorker, State.setLane] at h ⊢
      by_cases hww : w' = w
      · subst hww
        rw [ite_eq_left rfl] at h
        rcases h with h | h
        · simp only [Worker.inPass.injEq] at h
          subst h
          rw [ite_eq_left rfl]
          simp [laneEntered]
        · cases h
      · rw [ite_eq_right hww] at h
        obtain ⟨hb, hh⟩ := i.holds w' l' h
        by_cases hll : l' = l
        · subst hll
          rw [hb] at hl
          cases hl
        · rw [ite_eq_right hll]
          exact ⟨hb, hh⟩
    · intro l' w' h
      simp only [State.setWorker, State.setLane] at h ⊢
      by_cases hll : l' = l
      · subst hll
        rw [ite_eq_left rfl] at h
        simp only [laneEntered, Option.some.injEq] at h
        subst h
        rw [ite_eq_left rfl]
        exact Or.inl rfl
      · rw [ite_eq_right hll] at h
        have := i.holder l' w' h
        by_cases hww : w' = w
        · subst hww
          rcases this with this | this <;> rw [hw] at this <;> cases this
        · rw [ite_eq_right hww]
          exact this
    · intro l' h
      simp only [State.setWorker, State.setLane] at h ⊢
      by_cases hll : l' = l
      · subst hll
        rw [ite_eq_left rfl]
        simp [laneEntered]
      · rw [ite_eq_right hll] at h ⊢
        exact i.busy l' h
    · intro l'
      simp only [State.setWorker, State.setLane]
      by_cases hll : l' = l
      · subst hll
        rw [ite_eq_left rfl]
        exact Manifold.Drain.step_inv (i.lane l') (entered_is_step _ _ hl fresh)
      · rw [ite_eq_right hll]
        exact i.lane l'
  | grant w l at_ hw hs =>
    exact ⟨fun w' l' h => i.holds w' l' h, fun l' w' h => i.holder l' w' h, fun l' h => i.busy l' h,
      fun l' => i.lane l'⟩
  | push w l at_ stamp intact hw hs =>
    exact ⟨fun w' l' h => i.holds w' l' h, fun l' w' h => i.holder l' w' h, fun l' h => i.busy l' h,
      fun l' => i.lane l'⟩
  | pop w l at_ stamp intact hw hs =>
    exact ⟨fun w' l' h => i.holds w' l' h, fun l' w' h => i.holder l' w' h, fun l' h => i.busy l' h,
      fun l' => i.lane l'⟩
  | replay w l at_ stamp l' ran hw hs =>
    exact ⟨fun w' l' h => i.holds w' l' h, fun l' w' h => i.holder l' w' h, fun l' h => i.busy l' h,
      fun l' => i.lane l'⟩
  | release at_ stamp l ran hs =>
    exact ⟨fun w' l' h => i.holds w' l' h, fun l' w' h => i.holder l' w' h, fun l' h => i.busy l' h,
      fun l' => i.lane l'⟩
  | leave w l hw =>
    exact left_inv i w l (Or.inl hw) (i.holds w l (Or.inl hw)) .idle (fun _ => ⟨by simp, by simp⟩)
  | kill w l hw =>
    refine ⟨?_, ?_, fun l' h => i.busy l' h, fun l' => i.lane l'⟩
    · intro w' l' h
      simp only [State.setWorker] at h ⊢
      by_cases hww : w' = w
      · subst hww
        rw [ite_eq_left rfl] at h
        rcases h with h | h
        · cases h
        · simp only [Worker.dead.injEq] at h
          subst h
          exact i.holds _ _ (Or.inl hw)
      · rw [ite_eq_right hww] at h
        exact i.holds w' l' h
    · intro l' w' h
      simp only [State.setWorker] at h ⊢
      have := i.holder l' w' h
      by_cases hww : w' = w
      · subst hww
        rw [ite_eq_left rfl]
        rcases this with this | this <;> rw [hw] at this
        · simp only [Worker.inPass.injEq] at this
          subst this
          exact Or.inr rfl
        · cases this
      · rw [ite_eq_right hww]
        exact this
  | free w l hw =>
    exact left_inv i w l (Or.inr hw) (i.holds w l (Or.inr hw)) .gone (fun _ => ⟨by simp, by simp⟩)

theorem reach_inv {n : Nat} {s : State} (r : Reach n s) : Inv s := by
  induction r with
  | rest => exact rest_inv
  | step _ st ih => exact step_inv ih st

/-! ## The lane count -/

/-- Each lane that a worker holds, in a pass or dead, is below the lane count n. -/
def Below (n : Nat) (s : State) : Prop := ∀ w l, (s.worker w = .inPass l ∨ s.worker w = .dead l) → l < n

theorem rest_below (n : Nat) : Below n rest := by
  intro w l h
  rcases h with h | h <;> simp [rest] at h

theorem step_below {n : Nat} {s t : State} (b : Below n s) (st : Step n s t) : Below n t := by
  cases st with
  | enter w l hn hw hl fresh =>
    intro w' l' h
    simp only [State.setWorker, State.setLane] at h
    by_cases hww : w' = w
    · subst hww
      rw [ite_eq_left rfl] at h
      rcases h with h | h
      · simp only [Worker.inPass.injEq] at h
        subst h
        exact hn
      · cases h
    · rw [ite_eq_right hww] at h
      exact b w' l' h
  | grant w l at_ hw hs => exact fun w' l' h => b w' l' h
  | push w l at_ stamp intact hw hs => exact fun w' l' h => b w' l' h
  | pop w l at_ stamp intact hw hs =>
    intro w' l' h
    exact b w' l' h
  | replay w l at_ stamp l' ran hw hs => exact fun w' l'' h => b w' l'' h
  | release at_ stamp l ran hs => exact fun w' l' h => b w' l' h
  | leave w l hw =>
    intro w' l' h
    simp only [State.setWorker, State.setLane] at h
    by_cases hww : w' = w
    · subst hww
      rw [ite_eq_left rfl] at h
      rcases h with h | h <;> cases h
    · rw [ite_eq_right hww] at h
      exact b w' l' h
  | kill w l hw =>
    intro w' l' h
    simp only [State.setWorker] at h
    by_cases hww : w' = w
    · subst hww
      rw [ite_eq_left rfl] at h
      rcases h with h | h
      · cases h
      · simp only [Worker.dead.injEq] at h
        subst h
        exact b _ _ (Or.inl hw)
    · rw [ite_eq_right hww] at h
      exact b w' l' h
  | free w l hw =>
    intro w' l' h
    simp only [State.setWorker, State.setLane] at h
    by_cases hww : w' = w
    · subst hww
      rw [ite_eq_left rfl] at h
      rcases h with h | h <;> cases h
    · rw [ite_eq_right hww] at h
      exact b w' l' h

theorem reach_below {n : Nat} {s : State} (r : Reach n s) : Below n s := by
  induction r with
  | rest => exact rest_below n
  | step _ st ih => exact step_below ih st

/-- In each state that the steps of n lanes reach, each busy lane is below n. So each lane that a drain reads over
and each lane index that a pop records is below `EMBED_TURNSTILE_LANES`. -/
theorem busy_below_count {n : Nat} {s : State} (r : Reach n s) (l : Nat) (h : (s.lane l).busy = true) : l < n := by
  have i := reach_inv r
  cases hh : (s.lane l).holder with
  | none => exact absurd hh (i.busy l h)
  | some w => exact reach_below r w l (i.holder l w hh)

/-! ## The theorems -/

/-- What a step does to the slot at `a`. It leaves the slot as it was, or it is one of the five transitions of the
slot at `a`. Each transition starts from the slot that it needs. -/
theorem slot_after {n : Nat} {s t : State} (st : Step n s t) (a : Nat) :
    t.slot a = s.slot a ∨
      ((s.slot a = .empty ∨ ∃ st l' r, s.slot a = .released st l' r) ∧ t.slot a = .granted s.next) ∨
      (∃ stamp intact, s.slot a = .granted stamp ∧ t.slot a = .pushed stamp intact) ∨
      (∃ w l stamp intact, s.worker w = .inPass l ∧ s.slot a = .pushed stamp intact ∧
        t.slot a = .popped stamp l intact) ∨
      (∃ stamp l ran, s.slot a = .popped stamp l ran ∧ t.slot a = .replayed stamp l ran) ∨
      (∃ stamp l ran, (s.slot a = .popped stamp l ran ∨ s.slot a = .replayed stamp l ran) ∧
        t.slot a = .released stamp l ran) := by
  cases st with
  | grant w l at' hw hs =>
    by_cases ha : a = at'
    · subst ha; right; left; exact ⟨hs, by simp [State.setSlot]⟩
    · left; simp [State.setSlot, ha]
  | push w l at' stamp intact hw hs =>
    by_cases ha : a = at'
    · subst ha; right; right; left; exact ⟨stamp, intact, hs, by simp [State.setSlot]⟩
    · left; simp [State.setSlot, ha]
  | pop w l at' stamp intact hw hs =>
    by_cases ha : a = at'
    · subst ha; right; right; right; left; exact ⟨w, l, stamp, intact, hw, hs, by simp [State.setSlot]⟩
    · left; simp [State.setSlot, ha]
  | replay w l at' stamp l' ran hw hs =>
    by_cases ha : a = at'
    · subst ha; right; right; right; right; left; exact ⟨stamp, l', ran, hs, by simp [State.setSlot]⟩
    · left; simp [State.setSlot, ha]
  | release at' stamp l ran hs =>
    by_cases ha : a = at'
    · subst ha; right; right; right; right; right; exact ⟨stamp, l, ran, hs, by simp [State.setSlot]⟩
    · left; simp [State.setSlot, ha]
  | enter _ _ _ _ _ _ => left; simp [State.setLane, State.setWorker]
  | leave _ _ _ => left; simp [State.setLane, State.setWorker]
  | kill _ _ _ => left; simp [State.setWorker]
  | free _ _ _ => left; simp [State.setLane, State.setWorker]

/-- The counter of the grantor moves on at a grant alone, by one. -/
theorem next_after {n : Nat} {s t : State} (st : Step n s t) : t.next = s.next ∨ t.next = s.next + 1 := by
  cases st <;> simp [State.setSlot, State.setLane, State.setWorker]

/-- A popped slot whose job pointer failed the check never ran its job: the pop step records `ran` as the check. -/
theorem refused_never_runs {n : Nat} {s t : State} (st : Step n s t) (at_ stamp l : Nat) (ran : Bool)
    (hs : s.slot at_ ≠ .popped stamp l ran) (ht : t.slot at_ = .popped stamp l ran) :
    s.slot at_ = .pushed stamp ran := by
  rcases slot_after st at_ with h | ⟨_, h⟩ | ⟨_, _, _, h⟩ | ⟨_, _, _, _, _, h0, h⟩ | ⟨_, _, _, _, h⟩ | ⟨_, _, _, _, h⟩
  · rw [h] at ht; exact absurd ht hs
  · rw [h] at ht; cases ht
  · rw [h] at ht; cases ht
  · rw [h] at ht; cases ht; exact h0
  · rw [h] at ht; cases ht
  · rw [h] at ht; cases ht

/-- A single-bit error in the job pointer fails the check of `Flags.lean`. So the pop of that slot records `ran` as
false, and by `refused_never_runs` the job never runs. -/
theorem single_flip_never_runs (p : Nat) (hp : p ∈ Flags.pointers) (job i : BitVec p)
    (hi : i < BitVec.ofNat p p) :
    decide (Flags.check (job ^^^ ((1 : BitVec p) <<< i)) = Flags.check job) = false :=
  decide_eq_false (Flags.job_flip_changes_check p hp job i hi)

/-- A popped slot moves on only to its replay or its release, with its stamp, its lane and its check as they were. -/
theorem popped_moves {n : Nat} {s t : State} (st : Step n s t) (at_ stamp l : Nat) (ran : Bool)
    (hs : s.slot at_ = .popped stamp l ran) :
    t.slot at_ = .popped stamp l ran ∨ t.slot at_ = .replayed stamp l ran ∨ t.slot at_ = .released stamp l ran := by
  rcases slot_after st at_ with h | ⟨h0, h⟩ | ⟨_, _, h0, h⟩ | ⟨_, _, _, _, _, h0, h⟩ | ⟨_, _, _, h0, h⟩ |
      ⟨_, _, _, h0, h⟩
  · exact Or.inl (h.trans hs)
  · rw [hs] at h0; rcases h0 with h0 | ⟨_, _, _, h0⟩ <;> cases h0
  · rw [hs] at h0; cases h0
  · rw [hs] at h0; cases h0
  · rw [hs] at h0; cases h0; exact Or.inr (Or.inl h)
  · rw [hs] at h0; rcases h0 with h0 | h0 <;> cases h0; exact Or.inr (Or.inr h)

/-- A replayed slot moves on only to its release. So a pop has one replay at max. -/
theorem replayed_moves {n : Nat} {s t : State} (st : Step n s t) (at_ stamp l : Nat) (ran : Bool)
    (hs : s.slot at_ = .replayed stamp l ran) :
    t.slot at_ = .replayed stamp l ran ∨ t.slot at_ = .released stamp l ran := by
  rcases slot_after st at_ with h | ⟨h0, h⟩ | ⟨_, _, h0, h⟩ | ⟨_, _, _, _, _, h0, h⟩ | ⟨_, _, _, h0, h⟩ |
      ⟨_, _, _, h0, h⟩
  · exact Or.inl (h.trans hs)
  · rw [hs] at h0; rcases h0 with h0 | ⟨_, _, _, h0⟩ <;> cases h0
  · rw [hs] at h0; cases h0
  · rw [hs] at h0; cases h0
  · rw [hs] at h0; cases h0
  · rw [hs] at h0; rcases h0 with h0 | h0 <;> cases h0; exact Or.inr h

/-- A worker in a pass or dead holds its lane, the lane names the worker back, and two workers never share a
lane. -/
theorem lane_identifies_worker {n : Nat} {s : State} (r : Reach n s) (w l : Nat)
    (h : s.worker w = .inPass l ∨ s.worker w = .dead l) :
    (s.lane l).busy = true ∧ (s.lane l).holder = some w ∧
      ∀ w', (s.worker w' = .inPass l ∨ s.worker w' = .dead l) → w' = w := by
  have i := reach_inv r
  obtain ⟨hb, hh⟩ := i.holds w l h
  refine ⟨hb, hh, fun w' h' => ?_⟩
  have := (i.holds w' l h').2
  rw [hh] at this
  exact ((Option.some.injEq _ _).mp this).symm

/-- The worker that popped a slot held the lane that the slot records, at the pop. -/
theorem popped_lane_was_held {n : Nat} {s t : State} (r : Reach n s) (st : Step n s t) (at_ stamp l : Nat)
    (ran : Bool)
    (hs : s.slot at_ ≠ .popped stamp l ran) (ht : t.slot at_ = .popped stamp l ran) :
    ∃ w, s.worker w = .inPass l ∧ (s.lane l).holder = some w := by
  rcases slot_after st at_ with h | ⟨_, h⟩ | ⟨_, _, _, h⟩ | ⟨w, l', _, _, hw, _, h⟩ | ⟨_, _, _, _, h⟩ | ⟨_, _, _, _, h⟩
  · rw [h] at ht; exact absurd ht hs
  · rw [h] at ht; cases ht
  · rw [h] at ht; cases ht
  · rw [h] at ht
    cases ht
    exact ⟨w, hw, ((reach_inv r).holds w l (Or.inl hw)).2⟩
  · rw [h] at ht; cases ht
  · rw [h] at ht; cases ht

/-- A pop that the steps reach from rest. On one lane, worker 0 enters lane 0, grants slot 0 the ticket 0, pushes
a job that passes its check, and pops it. So the theorems on popped slots hold of states that the steps reach. -/
theorem pop_reachable : ∃ s, Reach 1 s ∧ s.slot 0 = .popped 0 0 true := by
  let s1 := (rest.setLane 0 (laneEntered (rest.lane 0) 0)).setWorker 0 (.inPass 0)
  have r1 : Reach 1 s1 := .step .rest (.enter 0 0 (by decide) rfl rfl rfl)
  let s2 : State := { s1.setSlot 0 (.granted s1.next) with next := s1.next + 1 }
  have r2 : Reach 1 s2 := .step r1 (.grant 0 0 0 rfl (Or.inl rfl))
  let s3 := s2.setSlot 0 (.pushed 0 true)
  have r3 : Reach 1 s3 := .step r2 (.push 0 0 0 0 true rfl rfl)
  let s4 := s3.setSlot 0 (.popped 0 0 true)
  have r4 : Reach 1 s4 := .step r3 (.pop 0 0 0 0 true rfl rfl)
  exact ⟨s4, r4, rfl⟩

/-! ## The tickets of the grantor -/

/-- The invariant of the slots. Each stamp that a slot holds is below the counter of the grantor, and no two slots
hold one stamp. -/
structure SlotInv (s : State) : Prop where
  below : ∀ a st, (s.slot a).stampOf = some st → st < s.next
  apart : ∀ a b st, (s.slot a).stampOf = some st → (s.slot b).stampOf = some st → a = b

theorem rest_slotInv : SlotInv rest :=
  ⟨fun _ _ h => by simp [rest, SlotState.stampOf] at h, fun _ _ _ h => by simp [rest, SlotState.stampOf] at h⟩

/-- A step that leaves the stamp of each slot and the counter as they were keeps the invariant of the slots. -/
theorem slotInv_same {s t : State} (i : SlotInv s) (hn : t.next = s.next)
    (hst : ∀ a, (t.slot a).stampOf = (s.slot a).stampOf) : SlotInv t :=
  ⟨fun a st h => by rw [hst] at h; rw [hn]; exact i.below a st h,
    fun a b st ha hb => by rw [hst] at ha hb; exact i.apart a b st ha hb⟩

theorem step_slotInv {n : Nat} {s t : State} (i : SlotInv s) (st : Step n s t) : SlotInv t := by
  cases st with
  | grant w l at' hw hs =>
    refine ⟨?_, ?_⟩
    · intro a st h
      simp only [State.setSlot] at h ⊢
      by_cases ha : a = at'
      · rw [ite_eq_left ha] at h; simp [SlotState.stampOf] at h; omega
      · rw [ite_eq_right ha] at h; have := i.below a st h; omega
    · intro a b st ha hb
      simp only [State.setSlot] at ha hb
      by_cases h1 : a = at' <;> by_cases h2 : b = at'
      · rw [h1, h2]
      · rw [ite_eq_left h1] at ha
        rw [ite_eq_right h2] at hb
        simp [SlotState.stampOf] at ha
        have := i.below b st hb
        omega
      · rw [ite_eq_right h1] at ha
        rw [ite_eq_left h2] at hb
        simp [SlotState.stampOf] at hb
        have := i.below a st ha
        omega
      · rw [ite_eq_right h1] at ha
        rw [ite_eq_right h2] at hb
        exact i.apart a b st ha hb
  | push w l at' stamp intact hw hs =>
    refine slotInv_same i rfl fun a => ?_
    simp only [State.setSlot]
    by_cases ha : a = at'
    · rw [ite_eq_left ha, ha, hs]; rfl
    · rw [ite_eq_right ha]
  | pop w l at' stamp intact hw hs =>
    refine slotInv_same i rfl fun a => ?_
    simp only [State.setSlot]
    by_cases ha : a = at'
    · rw [ite_eq_left ha, ha, hs]; rfl
    · rw [ite_eq_right ha]
  | replay w l at' stamp l' ran hw hs =>
    refine slotInv_same i rfl fun a => ?_
    simp only [State.setSlot]
    by_cases ha : a = at'
    · rw [ite_eq_left ha, ha, hs]; rfl
    · rw [ite_eq_right ha]
  | release at' stamp l ran hs =>
    refine slotInv_same i rfl fun a => ?_
    simp only [State.setSlot]
    by_cases ha : a = at'
    · rw [ite_eq_left ha, ha]
      rcases hs with hs | hs <;> rw [hs] <;> rfl
    · rw [ite_eq_right ha]
  | enter _ _ _ _ _ _ => exact slotInv_same i rfl fun _ => rfl
  | leave _ _ _ => exact slotInv_same i rfl fun _ => rfl
  | kill _ _ _ => exact slotInv_same i rfl fun _ => rfl
  | free _ _ _ => exact slotInv_same i rfl fun _ => rfl

/-- The grantor grants each ticket once. Take a state that the steps reach. The stamps of its slots are below the
counter, no two slots hold one, and each grant takes the counter. This is the part of Axiom A3 that names the grant
of each ticket once, as a theorem for a grantor that counts. -/
theorem grant_once {n : Nat} {s : State} (r : Reach n s) : SlotInv s := by
  induction r with
  | rest => exact rest_slotInv
  | step _ st ih => exact step_slotInv ih st

/-- The states that follow a state through zero or more steps. -/
inductive Follows (n : Nat) : State → State → Prop where
  | refl (s : State) : Follows n s s
  | step {s t u : State} : Follows n s t → Step n t u → Follows n s u

/-- A ticket that a pop took is never pushed again. Each state after the pop has no slot that holds the ticket as
granted or as pushed. So a pop takes the job of each ticket once at max. -/
theorem popped_never_pushed_again {n : Nat} {s u : State} (r : Reach n s) (a stamp l : Nat) (ran : Bool)
    (hs : s.slot a = .popped stamp l ran ∨ s.slot a = .replayed stamp l ran ∨ s.slot a = .released stamp l ran)
    (f : Follows n s u) :
    stamp < u.next ∧ ∀ b, u.slot b ≠ .granted stamp ∧ ∀ intact, u.slot b ≠ .pushed stamp intact := by
  have i := grant_once r
  have hst : (s.slot a).stampOf = some stamp := by
    rcases hs with h | h | h <;> rw [h] <;> rfl
  induction f with
  | refl =>
    refine ⟨i.below a stamp hst, fun b => ⟨fun hb => ?_, fun intact hb => ?_⟩⟩
    · have hba := i.apart b a stamp (by rw [hb]; rfl) hst
      subst hba
      rcases hs with h | h | h <;> rw [hb] at h <;> cases h
    · have hba := i.apart b a stamp (by rw [hb]; rfl) hst
      subst hba
      rcases hs with h | h | h <;> rw [hb] at h <;> cases h
  | step _ st ih =>
    obtain ⟨hlt, hno⟩ := ih
    refine ⟨by rcases next_after st with h | h <;> omega, fun b => ⟨fun hb => ?_, fun intact hb => ?_⟩⟩
    · rcases slot_after st b with h | ⟨_, h⟩ | ⟨_, _, _, h⟩ | ⟨_, _, _, _, _, _, h⟩ | ⟨_, _, _, _, h⟩ |
          ⟨_, _, _, _, h⟩
      · rw [h] at hb; exact (hno b).1 hb
      · rw [h] at hb; cases hb; omega
      · rw [h] at hb; cases hb
      · rw [h] at hb; cases hb
      · rw [h] at hb; cases hb
      · rw [h] at hb; cases hb
    · rcases slot_after st b with h | ⟨_, h⟩ | ⟨_, _, h0, h⟩ | ⟨_, _, _, _, _, _, h⟩ | ⟨_, _, _, _, h⟩ |
          ⟨_, _, _, _, h⟩
      · rw [h] at hb; exact (hno b).2 intact hb
      · rw [h] at hb; cases hb
      · rw [h] at hb; cases hb; exact (hno b).1 h0
      · rw [h] at hb; cases hb
      · rw [h] at hb; cases hb
      · rw [h] at hb; cases hb

/-- The free of a dead worker ends its pass on the lane word. The lane reads free, and the pass of the worker has
the ended mark. A drain that read the lane held by that worker reads it over. -/
theorem free_ends_pass (s : State) (w l : Nat) :
    let t := (s.setLane l (laneLeft (s.lane l) w)).setWorker w .gone
    (t.lane l).busy = false ∧ (t.lane l).ended w = true ∧ t.worker w = .gone ∧
      Manifold.Drain.over (s.lane l) (t.lane l) = true := by
  simp [State.setLane, State.setWorker, laneLeft, Manifold.Drain.over]

/-- In a state where every worker is idle or gone, no lane has a holder and no lane is busy. So every drain reads
every lane over, and ends. -/
theorem drain_completes {n : Nat} {s : State} (r : Reach n s)
    (quiet : ∀ w, s.worker w = .idle ∨ s.worker w = .gone) (l : Nat) :
    (s.lane l).holder = none ∧ (s.lane l).busy = false := by
  have i := reach_inv r
  have hn : (s.lane l).holder = none := by
    cases hh : (s.lane l).holder with
    | none => rfl
    | some w =>
      rcases i.holder l w hh with h | h <;> rcases quiet w with q | q <;> rw [h] at q <;> cases q
  refine ⟨hn, ?_⟩
  cases hb : (s.lane l).busy with
  | false => rfl
  | true => exact absurd hn (i.busy l hb)

/-- Every lane word of a reachable state satisfies the invariant of the drain model. -/
theorem lane_invariant {n : Nat} {s : State} (r : Reach n s) (l : Nat) : Manifold.Drain.Inv (s.lane l) :=
  (reach_inv r).lane l

end EmbeddedTypes.Integrated
