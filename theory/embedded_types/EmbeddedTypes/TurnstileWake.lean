/-!
# embedded_types, the turnstile's sleeps and wakes

Two sleeps of `include/embed_turnstile.h` rest on `embed_turnstile_backed_off`: one fetch-or sets a sleeper flag on
a word, and the host's wait sleeps while the word holds the value read with that flag set. The wait is the futex
contract: the compare and the sleep are one step, a wake ends each sleep on the word, and each sleep has a time
limit. A word here is its flag and a version that each other change to the word raises.

`embed_turnstile_enter` sleeps on `busy` with the flag `EMBED_TURNSTILE_SLEEPERS`, where its badge-in found no lane
and every open lane busy. `embed_turnstile_leave` clears its lane's bit and the flag with one fetch-and, and wakes
the word where the flag was set. A claim sets a bit with a fetch-or and keeps the flag.

`embed_turnstiles_drain` sleeps on a lane's badge with the flag `EMBED_TURNSTILE_SLEEPER`. The end of a pass clears
the badge's state bits with one fetch-and, and wakes the badge where the flag was set. These steps on a badge are
the steps of `Step` with no claim, and `busy_woken` holds them too. The badge write of the next pass through the lane
is a store: it puts a new badge with the flag clear, and it wakes nothing.

The theorems:

- `busy_woken`: a sleeper asleep on its word has the flag on the word or a wake owed to it. Thus each pass that
  ends while the sleeper sleeps owes it a wake (`leave_owes`).
- `badge_write_sleeps`: a badge write while the drain sleeps leaves the drain asleep, its flag gone, and no wake
  owed. The drain sleeps until its time limit: at most 64 ms, by `TurnstileBits.step_bounded`.
-/

namespace EmbeddedTypes.TurnstileWake

/-- A word: its version, which each change but the flag's raises, and its sleeper flag. -/
structure Word where
  version : Nat
  flag : Bool
  deriving DecidableEq, Repr

/-- The sleeper: before its read, with the value read, with the flag set, asleep on a value, or returned. -/
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
  owed : Bool
  deriving DecidableEq, Repr

/-- The steps: the sleeper's three, a claim, a pass's end, its wake, and the time limit. -/
inductive Step : State → State → Prop where
  | read {s : State} (h : s.sleeper = .idle) : Step s { s with sleeper := .read s.word }
  | flag {s : State} {v : Word} (h : s.sleeper = .read v) :
      Step s { s with word := { s.word with flag := true }, sleeper := .flagged v }
  | wait {s : State} {v : Word} (h : s.sleeper = .flagged v) :
      Step s { s with sleeper := if s.word = { v with flag := true } then .asleep s.word else .done }
  /-- A claim's fetch-or: a lane's bit set, the flag kept. -/
  | claim {s : State} : Step s { s with word := { s.word with version := s.word.version + 1 } }
  /-- A pass's end: the fetch-and clears its bit and the flag, and a wake is owed where the flag was set. -/
  | leave {s : State} :
      Step s { s with word := ⟨s.word.version + 1, false⟩, owed := s.owed || s.word.flag }
  /-- The wake: each sleep on the word ends. -/
  | wake {s : State} (h : s.owed = true) :
      Step s { s with owed := false, sleeper := match s.sleeper with | .asleep _ => .done | x => x }
  /-- The time limit of the sleep. -/
  | timeout {s : State} {t : Word} (h : s.sleeper = .asleep t) : Step s { s with sleeper := .done }

def start : State := ⟨⟨0, false⟩, .idle, false⟩

inductive Reach : State → Prop where
  | start : Reach start
  | step {s s' : State} : Reach s → Step s s' → Reach s'

/-- The invariant: an asleep sleeper has the flag on the word, or a wake owed. -/
def Inv (s : State) : Prop := ∀ t, s.sleeper = .asleep t → s.word.flag = true ∨ s.owed = true

theorem inv_step {s s' : State} (hs : Inv s) (st : Step s s') : Inv s' := by
  cases st with
  | read h => intro t ht; simp at ht
  | flag h => intro t ht; simp at ht
  | @wait v h =>
    intro t ht
    simp only at ht
    by_cases he : s.word = { v with flag := true }
    · left; rw [he]
    · simp [he] at ht
  | claim => intro t ht; exact hs t ht
  | leave =>
    intro t ht
    rcases hs t ht with hf | ho
    · right; simp [hf]
    · right; simp [ho]
  | wake h =>
    intro t ht
    simp only at ht
    cases hsl : s.sleeper <;> rw [hsl] at ht <;> simp at ht
  | timeout h => intro t ht; simp at ht

theorem inv_reach {s : State} (h : Reach s) : Inv s := by
  induction h with
  | start => intro t ht; simp [start] at ht
  | step _ st ih => exact inv_step ih st

/-- A sleeper asleep on its word has the flag on the word or a wake owed to it. -/
theorem busy_woken {s : State} (h : Reach s) {t : Word} (ht : s.sleeper = .asleep t) :
    s.word.flag = true ∨ s.owed = true :=
  inv_reach h t ht

/-- Each pass that ends while the sleeper sleeps owes it a wake. -/
theorem leave_owes {s : State} (h : Reach s) {t : Word} (ht : s.sleeper = .asleep t) :
    (s.owed || s.word.flag) = true := by
  rcases busy_woken h ht with hf | ho
  · simp [hf]
  · simp [ho]

/-! ## The badge write of the next pass -/

/-- The steps on a badge: those of `Step`, and the next pass's badge write, a store with the flag clear and no wake. -/
inductive BadgeStep : State → State → Prop where
  | step {s s' : State} : Step s s' → BadgeStep s s'
  | write {s : State} : BadgeStep s { s with word := ⟨s.word.version + 1, false⟩ }

inductive BadgeReach : State → Prop where
  | start : BadgeReach start
  | step {s s' : State} : BadgeReach s → BadgeStep s s' → BadgeReach s'

/-- The drain flags the badge and sleeps; the next pass's badge write then leaves the drain asleep with no flag on
the badge and no wake owed. -/
theorem badge_write_sleeps :
    ∃ s, BadgeReach s ∧ (∃ t, s.sleeper = .asleep t) ∧ s.word.flag = false ∧ s.owed = false := by
  let s1 : State := ⟨⟨0, false⟩, .read ⟨0, false⟩, false⟩
  let s2 : State := ⟨⟨0, true⟩, .flagged ⟨0, false⟩, false⟩
  let s3 : State := ⟨⟨0, true⟩, .asleep ⟨0, true⟩, false⟩
  let s4 : State := ⟨⟨1, false⟩, .asleep ⟨0, true⟩, false⟩
  have h1 : BadgeReach s1 := BadgeReach.step BadgeReach.start (BadgeStep.step (Step.read (s := start) rfl))
  have h2 : BadgeReach s2 :=
    BadgeReach.step h1 (BadgeStep.step (Step.flag (s := s1) (v := ⟨0, false⟩) rfl))
  have h3 : BadgeReach s3 := by
    have hw := BadgeReach.step h2 (BadgeStep.step (Step.wait (s := s2) (v := ⟨0, false⟩) rfl))
    have he : (if s2.word = { (⟨0, false⟩ : Word) with flag := true } then Sleeper.asleep s2.word else Sleeper.done)
        = Sleeper.asleep ⟨0, true⟩ := by decide
    rw [he] at hw
    exact hw
  have h4 : BadgeReach s4 := BadgeReach.step h3 (BadgeStep.write (s := s3))
  exact ⟨s4, h4, ⟨⟨0, true⟩, rfl⟩, rfl, rfl⟩

end EmbeddedTypes.TurnstileWake
