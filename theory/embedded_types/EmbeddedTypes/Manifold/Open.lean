/-!
# embedded_types, the open lanes of the manifold

`open` holds n, the count of open lanes. Arrivals only read it. A growth and a contraction write it, each with one
compare-and-swap.

- An arrival starts at lane `(address >> 6) mod n`. On a lane with the busy bit, it spills to the next lane mod n.

- A contraction claims the top lane, n - 1, as an entry does. Where it holds the lane, it swaps `open` from n to
  n - 1. Where the swap lands, the lane is sealing until a fetch-add closes it. Where the swap fails, a fetch-add
  releases the lane.

- A growth takes lane n with a compare-and-swap on the lane word, from free or from closed. Then it swaps `open`
  from n to n + 1. Where the swap lands, the lane is the pass of the grower. Where it fails, the grower gives the
  lane back as it was. A sealing lane refuses the compare-and-swap. So no growth lands between the swap of a
  contraction and its close.

- Each of these is one atomic operation, and other steps can come between any two of them.

- The end of a pass on the top lane contracts again while each step closes a lane: the cascade.

An arrival can read a value of `open` that a later growth or contraction replaces. Each value that it reads is at
most `high`, the most lanes that were ever open. The model lets an arrival read any value from 1 to `high`, which
covers every value that it can read.

The theorems:

- `start_in_range` and `start_shift`. The start lane is below n, and the `(address >> 6) % n` of the C is it.

- `spill_covers`, `next_mod`, `spill_lane` and `spill_reaches`. The next lane of the C is 0 after the top and else
  the lane above. It visits `(first + j) % n` at step j, so a spill reaches each lane below n within n steps.

- `cascade_bounded`. The cascade ends within c - 1 steps from c lanes and leaves one lane open at least.

- `passes_below_open`. In each state that the steps reach, each pass and each held lane is below `open`, and no
  closed lane is. So a drain that reads `open` after its fence reads each lane that a pass can hold. No lane below
  `open` stays closed.
-/

namespace EmbeddedTypes.Manifold.Open

/-! ## Placement -/

/-- The start lane of an address: the cache line's index mod n. -/
def start (address n : Nat) : Nat := (address / 64) % n

theorem start_in_range (address n : Nat) (h : 0 < n) : start address n < n :=
  Nat.mod_lt _ h

/-- The spill from a start reaches each lane below n, at a step below n. -/
theorem spill_covers (first n lane : Nat) (h : 0 < n) (hl : lane < n) :
    ∃ j, j < n ∧ (first + j) % n = lane := by
  refine ⟨(lane + n - first % n) % n, Nat.mod_lt _ h, ?_⟩
  have hf : first % n < n := Nat.mod_lt _ h
  rw [Nat.add_mod, Nat.mod_mod, Nat.add_mod_mod]
  have : first % n + (lane + n - first % n) = lane + n := by omega
  rw [this, Nat.add_mod_right, Nat.mod_eq_of_lt hl]

/-- The C's start: the address shifted right by 6, mod n, is `start`. -/
theorem start_shift (address n : Nat) : (address >>> 6) % n = start address n := by
  simp [start, Nat.shiftRight_eq_div_pow]

/-- The C's next lane of a spill: lane 0 after the top lane, else the lane above. -/
def next (at_ n : Nat) : Nat := if at_ + 1 = n then 0 else at_ + 1

theorem next_mod (at_ n : Nat) (h : at_ < n) : next at_ n = (at_ + 1) % n := by
  unfold next
  split
  · rename_i e
    rw [e, Nat.mod_self]
  · exact (Nat.mod_eq_of_lt (by omega)).symm

/-- After j steps of the C's spill from a lane below n, the lane is `(first + j) % n`. -/
theorem spill_lane (first n j : Nat) (h : first < n) : Nat.repeat (fun a => next a n) j first = (first + j) % n := by
  induction j with
  | zero => simp [Nat.repeat, Nat.mod_eq_of_lt h]
  | succ j ih =>
    simp only [Nat.repeat, ih]
    rw [next_mod _ _ (Nat.mod_lt _ (by omega)), Nat.add_mod, Nat.mod_mod, ← Nat.add_mod, Nat.add_assoc]

/-- So the C's spill from the start reaches each lane below n within n steps. -/
theorem spill_reaches (address n lane : Nat) (h : 0 < n) (hl : lane < n) :
    ∃ j, j < n ∧ Nat.repeat (fun a => next a n) j ((address >>> 6) % n) = lane := by
  obtain ⟨j, hj, he⟩ := spill_covers ((address >>> 6) % n) n lane h hl
  exact ⟨j, hj, by rw [spill_lane _ _ _ (Nat.mod_lt _ h)]; exact he⟩

/-! ## The contraction's cascade

The end of a pass on the top lane contracts while each step closes a lane. `closes c` is whether the step from c
lanes closes the top lane. The cascade stops at 1 lane, or at the first step that does not close. -/

/-- The open count after the cascade from c lanes. -/
def cascade (closes : Nat → Bool) : Nat → Nat
  | 0 => 0
  | 1 => 1
  | c + 2 => if closes (c + 2) then cascade closes (c + 1) else c + 2

/-- The cascade ends, at most c - 1 steps from c lanes, and leaves one lane open at least. -/
theorem cascade_bounded (closes : Nat → Bool) (c : Nat) (h : 1 ≤ c) :
    1 ≤ cascade closes c ∧ cascade closes c ≤ c := by
  induction c with
  | zero => omega
  | succ c ih =>
    match c, ih with
    | 0, _ => simp [cascade]
    | c + 1, ih =>
      simp only [cascade]
      split
      · have := ih (by omega)
        omega
      · omega


/-! ## The lanes and the open count

Each step is one atomic operation of the C:

- The fetch-or of an entry takes a free lane below the count that it read.

- The fetch-add of a leave frees the lane of a pass.

- The fetch-or of a contraction claims a free top lane. Its swap of `open` down lands and leaves the lane sealing,
  or it fails and a fetch-add releases the lane. The fetch-add of a sealing lane closes it.

- The compare-and-swap of a growth on the lane word takes a free or closed lane. Its swap of `open` up lands, and
  the lane is the pass of the grower. Or it fails, and a fetch-add gives the lane back as it was.

A sealing lane is neither free nor closed, so a grower's compare-and-swap refuses it. -/

/-- A lane's state. -/
inductive Lane where
  | free
  | pass
  /-- A contraction holds the lane before its swap. -/
  | held
  /-- A contraction's swap landed, and its close is to come. -/
  | sealing
  | closed
  /-- A grower holds the lane before its swap, and it took the lane from closed or from free. -/
  | growing (wasClosed : Bool)
  deriving DecidableEq, Repr

structure State where
  /-- The value of `open`. -/
  count : Nat
  /-- The most lanes that were ever open. -/
  high : Nat
  lane : Nat → Lane

/-- At rest: one lane open, and each lane free. -/
def rest : State := { count := 1, high := 1, lane := fun _ => .free }

/-- The state with one lane changed. -/
def State.set (s : State) (at_ : Nat) (value : Lane) : State :=
  ⟨s.count, s.high, fun l => if l = at_ then value else s.lane l⟩

/-- The state with one lane changed, and `open` and `high` given. -/
def State.reset (s : State) (at_ : Nat) (value : Lane) (count high : Nat) : State :=
  ⟨count, high, fun l => if l = at_ then value else s.lane l⟩

/-- The steps, on a manifold of `most` lanes. -/
inductive Step (most : Nat) : State → State → Prop where
  /-- An arrival that read r lanes open takes a free lane below r. -/
  | enter {s : State} (r l : Nat) (hr : 1 ≤ r ∧ r ≤ s.high) (hl : l < r) (hf : s.lane l = .free) :
      Step most s (s.set l .pass)
  | leave {s : State} (l : Nat) (h : s.lane l = .pass) : Step most s (s.set l .free)
  /-- A contraction that read n lanes open, n above 1, claims lane n - 1. -/
  | claim {s : State} (n : Nat) (hn : 1 < n ∧ n ≤ s.high) (hf : s.lane (n - 1) = .free) :
      Step most s (s.set (n - 1) .held)
  /-- The swap of `open` from l + 1 to l lands. -/
  | shut {s : State} (l : Nat) (h : s.lane l = .held) (ho : s.count = l + 1) :
      Step most s (s.reset l .sealing l s.high)
  /-- The swap fails, and the contraction releases its lane. -/
  | release {s : State} (l : Nat) (h : s.lane l = .held) (ho : s.count ≠ l + 1) : Step most s (s.set l .free)
  /-- The fetch-add closes a sealing lane. -/
  | finish {s : State} (l : Nat) (h : s.lane l = .sealing) : Step most s (s.set l .closed)
  /-- A grower's compare-and-swap takes a free lane. -/
  | takeFree {s : State} (l : Nat) (h : s.lane l = .free) : Step most s (s.set l (.growing false))
  /-- A grower's compare-and-swap takes a closed lane, and counts the reopen. -/
  | takeClosed {s : State} (l : Nat) (h : s.lane l = .closed) : Step most s (s.set l (.growing true))
  /-- The swap of `open` from l to l + 1 lands, and the lane is the grower's pass. -/
  | grow {s : State} (l : Nat) (b : Bool) (h : s.lane l = .growing b) (ho : s.count = l) (hm : l < most) :
      Step most s (s.reset l .pass (l + 1) (max s.high (l + 1)))
  /-- The swap fails, and the grower gives the lane back as it was. -/
  | giveBack {s : State} (l : Nat) (b : Bool) (h : s.lane l = .growing b) (ho : s.count ≠ l) :
      Step most s (s.set l (if b then .closed else .free))

/-- The states the steps reach from rest. -/
inductive Reach (most : Nat) : State → Prop where
  | rest : Reach most rest
  | step {s t : State} : Reach most s → Step most s t → Reach most t

/-- The invariant. -/
structure Inv (most : Nat) (s : State) : Prop where
  count_pos : 1 ≤ s.count
  count_high : s.count ≤ s.high
  high_most : s.high ≤ most
  /-- Each lane at or above `high` is free, or a grower holds it from free. -/
  above : ∀ l, s.high ≤ l → s.lane l = .free ∨ s.lane l = .growing false
  /-- Each lane from `open` up to `high` has the closed state or the sealing state, or a grower holds it from closed. -/
  between : ∀ l, s.count ≤ l → l < s.high →
    s.lane l = .closed ∨ s.lane l = .sealing ∨ s.lane l = .growing true
  held_pos : ∀ l, s.lane l = .held → 1 ≤ l
  closed_range : ∀ l, s.lane l = .closed → s.count ≤ l ∧ l < s.high
  sealing_range : ∀ l, s.lane l = .sealing → s.count ≤ l ∧ l < s.high
  growing_true : ∀ l, s.lane l = .growing true → s.count ≤ l ∧ l < s.high
  growing_false : ∀ l, s.lane l = .growing false → l < s.count ∨ s.high ≤ l

/-- A lane that is free, a pass or held is below `open` where it is below `high`, and a pass or a held lane is below
`open`. -/
theorem below_open {most : Nat} {s : State} (i : Inv most s) (l : Nat)
    (h : s.lane l = .pass ∨ s.lane l = .held) : l < s.count := by
  by_cases hc : s.count ≤ l
  · by_cases hh : l < s.high
    · rcases i.between l hc hh with e | e | e <;> rcases h with h | h <;> rw [h] at e <;> cases e
    · rcases i.above l (by omega) with e | e <;> rcases h with h | h <;> rw [h] at e <;> cases e
  · omega

theorem free_below_open {most : Nat} {s : State} (i : Inv most s) (l : Nat) (hf : s.lane l = .free)
    (hh : l < s.high) : l < s.count := by
  by_cases hc : s.count ≤ l
  · rcases i.between l hc hh with e | e | e <;> rw [hf] at e <;> cases e
  · omega

theorem rest_inv (most : Nat) (h : 1 ≤ most) : Inv most rest := by
  refine ⟨by simp [rest], by simp [rest], by simpa [rest] using h, fun _ _ => Or.inl rfl, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;>
    intro l <;> simp [rest] <;> omega

/-- A lane set to a value, the rest of the state kept: each field of the invariant at another lane is the old one. -/
theorem set_other (s : State) (l m : Nat) (v : Lane) (h : m ≠ l) : (s.set l v).lane m = s.lane m := by
  simp [State.set, h]

theorem set_same (s : State) (l : Nat) (v : Lane) : (s.set l v).lane l = v := by
  simp [State.set]

theorem reset_other (s : State) (l m : Nat) (v : Lane) (c h : Nat) (hm : m ≠ l) : (s.reset l v c h).lane m = s.lane m := by
  simp [State.reset, hm]

theorem reset_same (s : State) (l : Nat) (v : Lane) (c h : Nat) : (s.reset l v c h).lane l = v := by
  simp [State.reset]

/-- A lane below `open` set to free, a pass, held above lane 0, or a grower's from free keeps the invariant. -/
theorem set_inv {most : Nat} {s : State} (i : Inv most s) (l : Nat) (v : Lane) (hl : l < s.count)
    (hv : v = .free ∨ v = .pass ∨ (v = .held ∧ 1 ≤ l) ∨ v = .growing false) : Inv most (s.set l v) := by
  have hch := i.count_high
  refine ⟨i.count_pos, i.count_high, i.high_most, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro m hm
    rw [set_other _ _ _ _ (by simp only [State.set] at hm; omega)]
    exact i.above m hm
  · intro m h1 h2
    rw [set_other _ _ _ _ (by simp only [State.set] at h1; omega)]
    exact i.between m h1 h2
  · intro m hm
    by_cases e : m = l
    · subst e
      rw [set_same] at hm
      rcases hv with hv | hv | ⟨_, hp⟩ | hv <;> first | exact hp | (subst hv; cases hm)
    · rw [set_other _ _ _ _ e] at hm
      exact i.held_pos m hm
  · intro m hm
    by_cases e : m = l
    · subst e
      rw [set_same] at hm
      rcases hv with hv | hv | ⟨hv, _⟩ | hv <;> subst hv <;> cases hm
    · rw [set_other _ _ _ _ e] at hm
      exact i.closed_range m hm
  · intro m hm
    by_cases e : m = l
    · subst e
      rw [set_same] at hm
      rcases hv with hv | hv | ⟨hv, _⟩ | hv <;> subst hv <;> cases hm
    · rw [set_other _ _ _ _ e] at hm
      exact i.sealing_range m hm
  · intro m hm
    by_cases e : m = l
    · subst e
      rw [set_same] at hm
      rcases hv with hv | hv | ⟨hv, _⟩ | hv <;> subst hv <;> cases hm
    · rw [set_other _ _ _ _ e] at hm
      exact i.growing_true m hm
  · intro m hm
    by_cases e : m = l
    · subst e
      exact Or.inl hl
    · rw [set_other _ _ _ _ e] at hm
      exact i.growing_false m hm

/-- A lane from `open` up to `high` set to closed, sealing or a grower's from closed keeps the invariant. -/
theorem set_inv_between {most : Nat} {s : State} (i : Inv most s) (l : Nat) (v : Lane) (h1 : s.count ≤ l)
    (h2 : l < s.high) (hv : v = .closed ∨ v = .sealing ∨ v = .growing true) : Inv most (s.set l v) := by
  refine ⟨i.count_pos, i.count_high, i.high_most, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
  · intro m hm
    rw [set_other _ _ _ _ (by simp only [State.set] at hm; omega)]
    exact i.above m hm
  · intro m hm1 hm2
    by_cases e : m = l
    · subst e
      rw [set_same]
      rcases hv with hv | hv | hv <;> subst hv <;> simp
    · rw [set_other _ _ _ _ e]
      exact i.between m hm1 hm2
  · intro m hm
    by_cases e : m = l
    · subst e
      rw [set_same] at hm
      rcases hv with hv | hv | hv <;> subst hv <;> cases hm
    · rw [set_other _ _ _ _ e] at hm
      exact i.held_pos m hm
  all_goals
    intro m hm
    by_cases e : m = l
    · subst e
      rw [set_same] at hm
      first
        | exact ⟨h1, h2⟩
        | (rcases hv with hv | hv | hv <;> subst hv <;> cases hm)
        | (rcases hv with hv | hv | hv <;> subst hv <;> first | exact ⟨h1, h2⟩ | cases hm)
    · rw [set_other _ _ _ _ e] at hm
      first
        | exact i.closed_range m hm
        | exact i.sealing_range m hm
        | exact i.growing_true m hm
        | exact i.growing_false m hm

theorem step_inv {most : Nat} {s t : State} (i : Inv most s) (st : Step most s t) : Inv most t := by
  have hch := i.count_high
  have hhm := i.high_most
  cases st with
  | enter r l hr hl hf => exact set_inv i l .pass (free_below_open i l hf (by omega)) (Or.inr (Or.inl rfl))
  | leave l h => exact set_inv i l .free (below_open i l (Or.inl h)) (Or.inl rfl)
  | claim n hn hf =>
    exact set_inv i (n - 1) .held (free_below_open i (n - 1) hf (by omega)) (Or.inr (Or.inr (Or.inl ⟨rfl, by omega⟩)))
  | release l h ho => exact set_inv i l .free (below_open i l (Or.inr h)) (Or.inl rfl)
  | finish l h =>
    have := i.sealing_range l h
    exact set_inv_between i l .closed this.1 this.2 (Or.inl rfl)
  | takeFree l h =>
    by_cases hh : l < s.high
    · exact set_inv i l (.growing false) (free_below_open i l h hh) (Or.inr (Or.inr (Or.inr rfl)))
    · -- a lane at or above `high`: the invariant holds it free or the grower's from free
      refine ⟨i.count_pos, i.count_high, i.high_most, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
      · intro m hm
        by_cases e : m = l
        · subst e; rw [set_same]; exact Or.inr rfl
        · rw [set_other _ _ _ _ e]; exact i.above m hm
      · intro m h1 h2
        rw [set_other _ _ _ _ (by simp only [State.set] at h2; omega)]
        exact i.between m h1 h2
      all_goals
        intro m hm
        by_cases e : m = l
        · subst e
          rw [set_same] at hm
          first | exact Or.inr (by simp only [State.set]; omega) | cases hm
        · rw [set_other _ _ _ _ e] at hm
          first
            | exact i.held_pos m hm
            | exact i.closed_range m hm
            | exact i.sealing_range m hm
            | exact i.growing_true m hm
            | exact i.growing_false m hm
  | takeClosed l h =>
    have := i.closed_range l h
    exact set_inv_between i l (.growing true) this.1 this.2 (Or.inr (Or.inr rfl))
  | shut l h ho =>
    have hpos := i.held_pos l h
    refine ⟨by simp only [State.reset]; omega, by simp only [State.reset]; omega, i.high_most, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro m hm
      simp only [State.reset] at hm
      rw [reset_other _ _ _ _ _ _ (by omega)]
      exact i.above m hm
    · intro m h1 h2
      simp only [State.reset] at h1 h2
      by_cases e : m = l
      · subst e; rw [reset_same]; exact Or.inr (Or.inl rfl)
      · rw [reset_other _ _ _ _ _ _ e]; exact i.between m (by omega) h2
    all_goals
      intro m hm
      by_cases e : m = l
      · subst e
        rw [reset_same] at hm
        first
          | exact ⟨by simp only [State.reset]; omega, by simp only [State.reset]; omega⟩
          | cases hm
      · rw [reset_other _ _ _ _ _ _ e] at hm
        try simp only [State.reset]
        first
          | exact i.held_pos m hm
          | (have := i.closed_range m hm; omega)
          | (have := i.sealing_range m hm; omega)
          | (have := i.growing_true m hm; omega)
          | (have := i.growing_false m hm
             rcases this with h3 | h3
             · left
               have : m ≠ l := e
               omega
             · right; exact h3)
  | grow l b h ho hm =>
    refine ⟨by simp only [State.reset]; omega, by simp only [State.reset]; omega,
      by simp only [State.reset]; omega, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro m hm'
      simp only [State.reset] at hm'
      rw [reset_other _ _ _ _ _ _ (by omega)]
      exact i.above m (by omega)
    · intro m h1 h2
      simp only [State.reset] at h1 h2
      rw [reset_other _ _ _ _ _ _ (by omega)]
      have hmh : m < s.high := by
        rcases Nat.lt_or_ge m s.high with h3 | h3
        · exact h3
        · omega
      exact i.between m (by omega) hmh
    all_goals
      intro m hm'
      by_cases e : m = l
      · subst e
        rw [reset_same] at hm'
        cases hm'
      · rw [reset_other _ _ _ _ _ _ e] at hm'
        try simp only [State.reset]
        first
          | exact i.held_pos m hm'
          | (have := i.closed_range m hm'; omega)
          | (have := i.sealing_range m hm'; omega)
          | (have := i.growing_true m hm'; omega)
          | (have := i.growing_false m hm'
             rcases this with h3 | h3
             · left; omega
             · right; omega)
  | giveBack l b h ho =>
    cases b with
    | true =>
      have := i.growing_true l h
      exact set_inv_between i l .closed this.1 this.2 (Or.inl rfl)
    | false =>
      rcases i.growing_false l h with hl | hl
      · exact set_inv i l .free hl (Or.inl rfl)
      · refine ⟨i.count_pos, i.count_high, i.high_most, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
        · intro m hm
          by_cases e : m = l
          · subst e; rw [set_same]; exact Or.inl rfl
          · rw [set_other _ _ _ _ e]; exact i.above m hm
        · intro m h1 h2
          rw [set_other _ _ _ _ (by simp only [State.set] at h2; omega)]
          exact i.between m h1 h2
        all_goals
          intro m hm
          by_cases e : m = l
          · subst e
            rw [set_same] at hm
            cases hm
          · rw [set_other _ _ _ _ e] at hm
            first
              | exact i.held_pos m hm
              | exact i.closed_range m hm
              | exact i.sealing_range m hm
              | exact i.growing_true m hm
              | exact i.growing_false m hm

theorem reach_inv (most : Nat) (h : 1 ≤ most) {s : State} (r : Reach most s) : Inv most s := by
  induction r with
  | rest => exact rest_inv most h
  | step _ st ih => exact step_inv ih st

/-- In each state that the steps reach, each pass and each held lane is below `open`, and no closed lane is. `open`
is from 1 to the lane count. So a drain that reads `open` after its fence reads each lane that a pass can hold, and
no lane below `open` stays closed. -/
theorem passes_below_open (most : Nat) (h : 1 ≤ most) {s : State} (r : Reach most s) :
    (∀ l, s.lane l = .pass → l < s.count) ∧ (∀ l, s.lane l = .held → l < s.count) ∧
      (∀ l, s.lane l = .closed → s.count ≤ l) ∧ 1 ≤ s.count ∧ s.count ≤ most := by
  have i := reach_inv most h r
  refine ⟨fun l hl => below_open i l (Or.inl hl), fun l hl => below_open i l (Or.inr hl),
    fun l hl => (i.closed_range l hl).1, i.count_pos, ?_⟩
  have := i.count_high
  have := i.high_most
  omega

end EmbeddedTypes.Manifold.Open
