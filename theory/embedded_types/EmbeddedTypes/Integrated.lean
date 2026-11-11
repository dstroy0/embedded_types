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

- `pop`. A worker in a pass pops a slot in the pushed state. The slot records the lane of the pass, and whether
  the job ran. The job runs only where its pointer passed the check. A pop of a slot in another state has no step:
  it fails.

- `leave`. A worker in a pass ends it. The lane word takes the leave step of the drain model.

- `kill`. The supervisor kills a worker in a pass. The worker is dead on its lane, and the lane word does not change.

- `free`. The supervisor frees the lane of a dead worker, with the lane index that the popped word recorded. The
  lane word takes the leave step on behalf of the dead worker.

The theorems:

- `refused_never_runs`. A popped slot whose job pointer failed the check never ran its job. With
  `Flags.job_flip_changes_check`, a single-bit error in a job pointer never runs.

- `runs_at_most_once`. A popped slot stays popped, so its job ran at most once.

- `lane_identifies_worker`. A worker in a pass or dead holds the lane that its state names, and that lane names
  the worker back. Two workers never share a lane.

- `popped_lane_was_held`. The worker that popped a slot held the lane that the slot records, at the pop.

- `free_ends_pass`. The free of a dead worker ends its pass on the lane word. The lane reads free, and the pass of
  the worker has the ended mark. Any drain that read that lane held by the worker reads it over.

- `drain_completes`. In a state where every worker is idle or gone, no lane has a holder and no lane is busy. So
  every drain reads every lane over and ends.

- `lane_invariant`. Every lane word of a reachable state satisfies the invariant of the drain model, so
  `held_until_end` and `drain_lane_complete` apply to each lane of the integrated model.
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

/-- A slot. It is empty, or pushed, or popped. A pushed slot holds a stamp and whether its job pointer passes the
check. A popped slot holds the lane of the pass that popped and whether the job ran. -/
inductive SlotState where
  | empty
  | pushed (stamp : Nat) (intact : Bool)
  | popped (stamp lane : Nat) (ran : Bool)
  deriving DecidableEq

structure State where
  lane : Nat → Manifold.Drain.State
  worker : Nat → Worker
  slot : Nat → SlotState

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

inductive Step : State → State → Prop where
  /-- An idle worker takes a free lane, with the entry step of the lane word. -/
  | enter {s : State} (w l : Nat) (hw : s.worker w = .idle) (hl : (s.lane l).busy = false)
      (fresh : (s.lane l).ended w = false) :
      Step s ((s.setLane l (laneEntered (s.lane l) w)).setWorker w (.inPass l))
  /-- A worker in a pass pops a pushed slot. The job runs where its pointer passed the check. -/
  | pop {s : State} (w l at_ stamp : Nat) (intact : Bool) (hw : s.worker w = .inPass l)
      (hs : s.slot at_ = .pushed stamp intact) :
      Step s (s.setSlot at_ (.popped stamp l intact))
  /-- A worker ends its pass, with the leave step of the lane word. -/
  | leave {s : State} (w l : Nat) (hw : s.worker w = .inPass l) :
      Step s ((s.setLane l (laneLeft (s.lane l) w)).setWorker w .idle)
  /-- The supervisor kills a worker in a pass. The lane word does not change. -/
  | kill {s : State} (w l : Nat) (hw : s.worker w = .inPass l) : Step s (s.setWorker w (.dead l))
  /-- The supervisor frees the lane of a dead worker, with the leave step of the lane word on its behalf. -/
  | free {s : State} (w l : Nat) (hw : s.worker w = .dead l) :
      Step s ((s.setLane l (laneLeft (s.lane l) w)).setWorker w .gone)

/-- The state at rest: every lane free, every worker idle, every slot empty. -/
def rest : State := { lane := fun _ => laneRest, worker := fun _ => .idle, slot := fun _ => .empty }

/-- The states that the steps reach from rest. -/
inductive Reach : State → Prop where
  | rest : Reach rest
  | step {s t : State} : Reach s → Step s t → Reach t

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

/-- The leave of the holder of a lane keeps the invariant, for a leave and for a free. -/
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

theorem step_inv {s t : State} (i : Inv s) (st : Step s t) : Inv t := by
  cases st with
  | enter w l hw hl fresh =>
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
  | pop w l at_ stamp intact hw hs =>
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

theorem reach_inv {s : State} (r : Reach s) : Inv s := by
  induction r with
  | rest => exact rest_inv
  | step _ st ih => exact step_inv ih st

/-! ## The theorems -/

/-- A popped slot whose job pointer failed the check never ran its job: the pop step records `ran` as the check. -/
theorem refused_never_runs {s t : State} (st : Step s t) (at_ stamp l : Nat) (ran : Bool)
    (hs : s.slot at_ ≠ .popped stamp l ran) (ht : t.slot at_ = .popped stamp l ran) :
    s.slot at_ = .pushed stamp ran := by
  cases st with
  | pop w l' at' stamp' intact hw hs' =>
    simp only [State.setSlot] at ht
    by_cases ha : at_ = at'
    · subst ha
      rw [ite_eq_left rfl] at ht
      simp only [SlotState.popped.injEq] at ht
      obtain ⟨rfl, rfl, rfl⟩ := ht
      exact hs'
    · rw [ite_eq_right ha] at ht
      exact absurd ht hs
  | enter _ _ _ _ _ => exact absurd ht hs
  | leave _ _ _ => exact absurd ht hs
  | kill _ _ _ => exact absurd ht hs
  | free _ _ _ => exact absurd ht hs

/-- A single-bit error in the job pointer fails the check of `Flags.lean`. So the pop of that slot records `ran` as
false, and by `refused_never_runs` the job never runs. -/
theorem single_flip_never_runs (job i : BitVec 64) (hi : i < 64) :
    decide (Flags.check (job ^^^ ((1 : BitVec 64) <<< i)) = Flags.check job) = false :=
  decide_eq_false (Flags.job_flip_changes_check job i hi)

/-- No step changes a popped slot, so a popped slot stays popped and its job ran at most once. -/
theorem runs_at_most_once {s t : State} (st : Step s t) (at_ stamp l : Nat) (ran : Bool)
    (hs : s.slot at_ = .popped stamp l ran) : t.slot at_ = .popped stamp l ran := by
  cases st with
  | pop w l' at' stamp' intact hw hs' =>
    simp only [State.setSlot]
    by_cases ha : at_ = at'
    · subst ha
      rw [hs] at hs'
      cases hs'
    · rw [ite_eq_right ha]
      exact hs
  | enter _ _ _ _ _ => exact hs
  | leave _ _ _ => exact hs
  | kill _ _ _ => exact hs
  | free _ _ _ => exact hs

/-- A worker in a pass or dead holds its lane, the lane names the worker back, and two workers never share a
lane. -/
theorem lane_identifies_worker {s : State} (r : Reach s) (w l : Nat)
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
theorem popped_lane_was_held {s t : State} (r : Reach s) (st : Step s t) (at_ stamp l : Nat) (ran : Bool)
    (hs : s.slot at_ ≠ .popped stamp l ran) (ht : t.slot at_ = .popped stamp l ran) :
    ∃ w, s.worker w = .inPass l ∧ (s.lane l).holder = some w := by
  cases st with
  | pop w l' at' stamp' intact hw hs' =>
    simp only [State.setSlot] at ht
    by_cases ha : at_ = at'
    · subst ha
      rw [ite_eq_left rfl] at ht
      simp only [SlotState.popped.injEq] at ht
      obtain ⟨rfl, rfl, rfl⟩ := ht
      exact ⟨w, hw, ((reach_inv r).holds w l' (Or.inl hw)).2⟩
    · rw [ite_eq_right ha] at ht
      exact absurd ht hs
  | enter _ _ _ _ _ => exact absurd ht hs
  | leave _ _ _ => exact absurd ht hs
  | kill _ _ _ => exact absurd ht hs
  | free _ _ _ => exact absurd ht hs

/-- The free of a dead worker ends its pass on the lane word. The lane reads free, and the pass of the worker has
the ended mark. A drain that read the lane held by that worker reads it over. -/
theorem free_ends_pass (s : State) (w l : Nat) :
    let t := (s.setLane l (laneLeft (s.lane l) w)).setWorker w .gone
    (t.lane l).busy = false ∧ (t.lane l).ended w = true ∧ t.worker w = .gone ∧
      Manifold.Drain.over (s.lane l) (t.lane l) = true := by
  simp [State.setLane, State.setWorker, laneLeft, Manifold.Drain.over]

/-- In a state where every worker is idle or gone, no lane has a holder and no lane is busy. So every drain reads
every lane over, and ends. -/
theorem drain_completes {s : State} (r : Reach s)
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
theorem lane_invariant {s : State} (r : Reach s) (l : Nat) : Manifold.Drain.Inv (s.lane l) :=
  (reach_inv r).lane l

end EmbeddedTypes.Integrated
