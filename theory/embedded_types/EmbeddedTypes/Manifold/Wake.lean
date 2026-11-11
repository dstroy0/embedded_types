/-!
# embedded_types, the sleeps on a lane word

Two threads sleep on a lane word. One is a drain that waits for the pass of the lane to end. The other is a worker
that found every lane busy with the manifold at its most. Each one reads the word, and sleeps only where the value
read has the busy bit. Then it sets the sleeper bit with a fetch-or.

The wait of the host sleeps while the word holds the value read with the bit set. The wait is the futex contract: the
compare and the sleep are one step, and a wake ends each sleep on the word.

Every leave, close, release and reopen is one fetch-add that changes the word. Where the old value has the sleeper
bit, the thread then clears the bit with a fetch-and and wakes the word. An entry on a busy word changes nothing. An
entry on a free word sets busy, and a sleeper never sleeps on a free value.

The theorem `no_lost_wake`. In each state that the steps reach, a thread asleep on a value has one of three things.
The word is still at that value, or a fetch-add owes it a clear and a wake, or the wake is out. So a sleeper sleeps
only while the pass that it waits for holds the lane, and the end of that pass wakes it.
-/

namespace EmbeddedTypes.Manifold.Wake

structure Word where
  busy : Bool
  generation : Nat
  flag : Bool
  deriving DecidableEq, Repr

inductive Sleeper where
  | idle
  | read (v : Word)
  | flagged (v : Word)
  | asleep (target : Word)
  | done
  deriving DecidableEq, Repr

structure State where
  word : Word
  sleeper : Sleeper
  /-- A fetch-add saw the bit set, and its clear and wake are to come. -/
  pending : Bool
  /-- A clear and wake came after the sleep began. -/
  owed : Bool

inductive Step : State → State → Prop where
  | read {s : State} (h : s.sleeper = .idle) : Step s { s with sleeper := .read s.word }
  /-- A value read free: the thread has nothing to wait for. -/
  | skip {s : State} {v : Word} (h : s.sleeper = .read v) (hb : v.busy = false) : Step s { s with sleeper := .done }
  | flag {s : State} {v : Word} (h : s.sleeper = .read v) (hb : v.busy = true) :
      Step s { s with word := { s.word with flag := true }, sleeper := .flagged v }
  | wait {s : State} {v : Word} (h : s.sleeper = .flagged v) :
      Step s { s with sleeper := if s.word = { v with flag := true } then .asleep s.word else .done }
  /-- A leave, a close, a release or a reopen: one fetch-add, the generation counted, busy as the operation leaves it,
  the bit kept. -/
  | change {s : State} (busy : Bool) :
      Step s { s with word := { s.word with busy := busy, generation := s.word.generation + 1 },
                      pending := s.pending || s.word.flag }
  /-- An entry on a free word. -/
  | enter {s : State} (h : s.word.busy = false) : Step s { s with word := { s.word with busy := true } }
  /-- The fetch-and clears the bit, and the wake follows. -/
  | clear {s : State} (h : s.pending = true) :
      Step s { s with word := { s.word with flag := false }, pending := false, owed := true }
  /-- Another sleeper's fetch-or. -/
  | other {s : State} : Step s { s with word := { s.word with flag := true } }
  /-- The wake ends the sleep. -/
  | woken {s : State} {t : Word} (h : s.sleeper = .asleep t) (ho : s.owed = true) : Step s { s with sleeper := .done }

inductive Reach : State → Prop where
  | rest (w : Word) : Reach ⟨w, .idle, false, false⟩
  | step {s t : State} : Reach s → Step s t → Reach t

/-- The invariant. A value read for a sleep has the busy bit. A sleeper asleep on a value has the word at that
value, or a wake to come. -/
def Inv (s : State) : Prop :=
  (∀ v, s.sleeper = .flagged v → v.busy = true) ∧
    ∀ t, s.sleeper = .asleep t → t.busy = true ∧ t.flag = true ∧ (s.word = t ∨ s.pending = true ∨ s.owed = true)

/-- A fetch-or of the bit on a word that has it set changes nothing. -/
theorem flag_kept (w : Word) (h : w.flag = true) : { w with flag := true } = w := by
  cases w
  simp_all

theorem step_inv {s t : State} (i : Inv s) (st : Step s t) : Inv t := by
  obtain ⟨hf, ha⟩ := i
  cases st with
  | read h => exact ⟨fun v hv => by simp at hv, fun t ht => by simp at ht⟩
  | skip h hb => exact ⟨fun v hv => by simp at hv, fun t ht => by simp at ht⟩
  | flag h hb =>
    refine ⟨fun v' hv => ?_, fun t ht => by simp at ht⟩
    simp only [Sleeper.flagged.injEq] at hv
    rw [← hv]
    exact hb
  | @wait v h =>
    refine ⟨fun v' hv => by simp only at hv; split at hv <;> simp at hv, fun t ht => ?_⟩
    simp only at ht
    split at ht
    · rename_i heq
      simp only [Sleeper.asleep.injEq] at ht
      subst ht
      rw [heq]
      exact ⟨hf v h, rfl, Or.inl rfl⟩
    · simp at ht
  | change busy =>
    refine ⟨hf, fun t ht => ?_⟩
    obtain ⟨hb, hfl, hw | hp | ho⟩ := ha t ht
    · subst hw
      refine ⟨hb, hfl, Or.inr (Or.inl ?_)⟩
      simp [hfl]
    · exact ⟨hb, hfl, Or.inr (Or.inl (by simp [hp]))⟩
    · exact ⟨hb, hfl, Or.inr (Or.inr ho)⟩
  | enter h =>
    refine ⟨hf, fun t ht => ?_⟩
    obtain ⟨hb, hfl, hw | hp | ho⟩ := ha t ht
    · subst hw
      rw [hb] at h
      cases h
    · exact ⟨hb, hfl, Or.inr (Or.inl hp)⟩
    · exact ⟨hb, hfl, Or.inr (Or.inr ho)⟩
  | clear h =>
    refine ⟨hf, fun t ht => ?_⟩
    obtain ⟨hb, hfl, _⟩ := ha t ht
    exact ⟨hb, hfl, Or.inr (Or.inr rfl)⟩
  | other =>
    refine ⟨hf, fun t ht => ?_⟩
    obtain ⟨hb, hfl, hw | hp | ho⟩ := ha t ht
    · subst hw
      exact ⟨hb, hfl, Or.inl (flag_kept _ hfl)⟩
    · exact ⟨hb, hfl, Or.inr (Or.inl hp)⟩
    · exact ⟨hb, hfl, Or.inr (Or.inr ho)⟩
  | woken h ho => exact ⟨fun v hv => by simp at hv, fun t ht => by simp at ht⟩

theorem reach_inv {s : State} (r : Reach s) : Inv s := by
  induction r with
  | rest w => exact ⟨fun v hv => by simp at hv, fun t ht => by simp at ht⟩
  | step _ st ih => exact step_inv ih st

/-- A thread asleep on a value has the word at that value, with the busy bit, so a pass still holds the lane. Or it
has a wake to come. -/
theorem no_lost_wake {s : State} {t : Word} (r : Reach s) (h : s.sleeper = .asleep t) :
    (s.word = t ∧ t.busy = true) ∨ s.pending = true ∨ s.owed = true := by
  obtain ⟨hb, _, hw | hp | ho⟩ := (reach_inv r).2 t h
  · exact Or.inl ⟨hw, hb⟩
  · exact Or.inr (Or.inl hp)
  · exact Or.inr (Or.inr ho)

end EmbeddedTypes.Manifold.Wake
