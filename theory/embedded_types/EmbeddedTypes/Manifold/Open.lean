/-!
# embedded_types, the open lanes of the manifold

The open count is n, the count of open lanes. The C holds n - 1, the place of the top open lane, in
`top_open_lane`. Arrivals only read it. A growth and a contraction write it, each with one compare-and-swap.

- An arrival starts at lane `(address >> 6) mod n`. On a lane with the busy bit, it spills to the next lane mod n.

- A contraction claims the top lane, n - 1, as an entry does. Where it holds the lane, it swaps the open count from
  n to n - 1. Where the swap lands, the lane is sealing until a fetch-add closes it. Where the swap fails, a
  fetch-add releases the lane.

- A growth takes lane n with a compare-and-swap on the lane word, from free or from closed. Then it swaps the open
  count from n to n + 1. Where the swap lands, the lane is the pass of the grower. Where it fails, the grower gives
  the lane back as it was. A sealing lane refuses the compare-and-swap. So no growth lands between the swap of a
  contraction and its close.

- Each of these is one atomic operation, and other steps can come between any two of them.

- The end of a pass on the top lane contracts again while each step closes a lane: the cascade.

An arrival can read a value of the open count that a later growth or contraction replaces. Each value that it reads
is at max `high`, the max lanes that were ever open. The model lets an arrival read any value from 1 to `high`, which
covers every value that it can read.

The theorems:

- `start_in_range` and `start_shift`. The start lane is below n, and the `(address >> 6) % n` of the C is it.

- `spill_covers`, `next_mod`, `spill_lane` and `spill_reaches`. The next lane of the C is 0 after the top and else
  the lane above. It visits `(first + j) % n` at step j, so a spill reaches each lane below n within n steps.

- `spill`, `spill_some`, `spill_none`, `spill_takes_first` and `spill_none_refused`. The spill of
  `embed_turnstile_spill` makes n tries at max, and try i is on lane `(start + i) % n`. The lane that it takes is
  the lane of its first try that takes. A spill that takes no lane made n tries, and none of them took.

- `cascade_bounded`. The cascade ends within c - 1 steps from c lanes and leaves one lane open at least.

- `leaveCascade`, `leaveCascade_empty` and `leaveCascade_bounded`. The loop of `embed_turnstile_leave` stops at 1
  lane, at a queue that is not empty, or at the first contraction that does not close. Where the queue is empty at
  each step, it is the cascade. It ends within c - 1 steps from c lanes and leaves one lane open at least.

- `passes_below_open`. In each state that the steps reach, each pass and each held lane is below the open count,
  and no closed lane is. So a drain that reads the open count after its fence reads each lane that a pass can hold.
  No lane below the open count stays closed.
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

/-! ## The spill

`embed_turnstile_spill` makes n tries at max. Try i is one fetch-or of busy on one lane, and `takes i` is true where
the old value of that fetch-or had busy clear. Other steps can come between two tries, so `takes` is free. The place
starts at the start lane and moves by `next` after each try that does not take. -/

/-- The spill of the C from try `index` on lane `place`, with `tries` tries left. It answers the try and the lane of
the first try that takes, or none. -/
def spill (takes : Nat → Bool) (n : Nat) (place index : Nat) : Nat → Option (Nat × Nat)
  | 0 => none
  | tries + 1 => if takes index then some (index, place) else spill takes n (next place n) (index + 1) tries

/-- A spill that takes a lane takes it at the first try that takes, and try i is on lane (start + i) % n. -/
theorem spill_some (takes : Nat → Bool) (n start : Nat) (hn : 0 < n) :
    ∀ (tries index place i lane : Nat), place = (start + index) % n →
      spill takes n place index tries = some (i, lane) →
      index ≤ i ∧ i < index + tries ∧ lane = (start + i) % n ∧ takes i = true ∧
        ∀ k, index ≤ k → k < i → takes k = false := by
  intro tries
  induction tries with
  | zero =>
    intro index place i lane _ h
    simp [spill] at h
  | succ tries ih =>
    intro index place i lane hp h
    simp only [spill] at h
    split at h
    · rename_i ht
      simp only [Option.some.injEq, Prod.mk.injEq] at h
      obtain ⟨rfl, rfl⟩ := h
      exact ⟨Nat.le_refl _, by omega, hp, ht, fun k h1 h2 => absurd h2 (by omega)⟩
    · rename_i ht
      have hnext : next place n = (start + (index + 1)) % n := by
        rw [next_mod _ _ (by rw [hp]; exact Nat.mod_lt _ hn), hp, Nat.mod_add_mod, Nat.add_assoc]
      obtain ⟨_, h2, h3, h4, h5⟩ := ih (index + 1) (next place n) i lane hnext h
      refine ⟨by omega, by omega, h3, h4, fun k hk1 hk2 => ?_⟩
      by_cases hk : k = index
      · subst hk
        simpa using ht
      · exact h5 k (by omega) hk2

/-- A spill that takes no lane made each of its tries, and none of them took. -/
theorem spill_none (takes : Nat → Bool) (n : Nat) :
    ∀ (tries index place : Nat), spill takes n place index tries = none →
      ∀ k, index ≤ k → k < index + tries → takes k = false := by
  intro tries
  induction tries with
  | zero =>
    intro index place _ k h1 h2
    omega
  | succ tries ih =>
    intro index place h k hk1 hk2
    simp only [spill] at h
    split at h
    · simp at h
    · rename_i ht
      by_cases hk : k = index
      · subst hk
        simpa using ht
      · exact ih (index + 1) (next place n) h k (by omega) (by omega)

/-- The spill of the C from the start lane over n lanes. It takes the lane of the first of its n tries that takes,
and try i is on lane (start + i) % n. -/
theorem spill_takes_first (takes : Nat → Bool) (n start i lane : Nat) (hn : 0 < n) (hs : start < n)
    (h : spill takes n start 0 n = some (i, lane)) :
    i < n ∧ lane = (start + i) % n ∧ takes i = true ∧ ∀ k, k < i → takes k = false := by
  obtain ⟨_, h2, h3, h4, h5⟩ :=
    spill_some takes n start hn n 0 start i lane (by simp [Nat.mod_eq_of_lt hs]) h
  exact ⟨by omega, h3, h4, fun k hk => h5 k (Nat.zero_le _) hk⟩

/-- A spill of the C that takes no lane made n tries, and none of them took. By `spill_covers`, those tries are on
each lane below n. -/
theorem spill_none_refused (takes : Nat → Bool) (n start : Nat) (h : spill takes n start 0 n = none) :
    ∀ k, k < n → takes k = false :=
  fun k hk => spill_none takes n n 0 start h k (Nat.zero_le _) (by omega)

/-! ## The contraction's cascade

The end of a pass on the top lane contracts while each step closes a lane. `closes c` is whether the step from c
lanes closes the top lane. The cascade stops at 1 lane, or at the first step that does not close. -/

/-- The open count after the cascade from c lanes. -/
def cascade (closes : Nat → Bool) : Nat → Nat
  | 0 => 0
  | 1 => 1
  | c + 2 => if closes (c + 2) then cascade closes (c + 1) else c + 2

/-- The cascade ends, at max c - 1 steps from c lanes, and leaves one lane open at least. -/
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

/-! ## The loop of the leave

The leave of the C contracts while more than one lane is open and the queue reads empty. It stops at the first
contraction that does not close. `empty c` is true where the queue read before the step from c lanes is empty. -/

/-- The open count after the loop of the leave from c lanes. -/
def leaveCascade (empty closes : Nat → Bool) : Nat → Nat
  | 0 => 0
  | 1 => 1
  | c + 2 => if empty (c + 2) then (if closes (c + 2) then leaveCascade empty closes (c + 1) else c + 2) else c + 2

/-- Where the queue reads empty at each step, the loop of the leave is the cascade. -/
theorem leaveCascade_empty (closes : Nat → Bool) : ∀ c, leaveCascade (fun _ => true) closes c = cascade closes c
  | 0 => rfl
  | 1 => rfl
  | c + 2 => by simp [leaveCascade, cascade, leaveCascade_empty closes (c + 1)]

/-- The loop of the leave ends, at max c - 1 steps from c lanes, and leaves one lane open at least. -/
theorem leaveCascade_bounded (empty closes : Nat → Bool) (c : Nat) (h : 1 ≤ c) :
    1 ≤ leaveCascade empty closes c ∧ leaveCascade empty closes c ≤ c := by
  induction c with
  | zero => omega
  | succ c ih =>
    match c, ih with
    | 0, _ => simp [leaveCascade]
    | c + 1, ih =>
      simp only [leaveCascade]
      split
      · split
        · have := ih (by omega)
          omega
        · omega
      · omega


/-! ## The lanes and the open count

Each step is one atomic operation of the C:

- The fetch-or of an entry takes a free lane below the count that it read.

- The fetch-add of a leave frees the lane of a pass.

- The fetch-or of a contraction claims a free top lane. Its swap of the open count down lands and leaves the lane
  sealing, or it fails and a fetch-add releases the lane. The fetch-add of a sealing lane closes it.

- The compare-and-swap of a growth on the lane word takes a free or closed lane. Its swap of the open count up lands,
  and the lane is the pass of the grower. Or it fails, and a fetch-add gives the lane back as it was.

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
  /-- The open count, which the C holds less one in `top_open_lane`. -/
  count : Nat
  /-- The max lanes that were ever open. -/
  high : Nat
  lane : Nat → Lane

/-- At rest: one lane open, and each lane free. -/
def rest : State := { count := 1, high := 1, lane := fun _ => .free }

/-- The state with one lane changed. -/
def State.set (s : State) (at_ : Nat) (value : Lane) : State :=
  ⟨s.count, s.high, fun l => if l = at_ then value else s.lane l⟩

/-- The state with one lane changed, and the open count and `high` given. -/
def State.reset (s : State) (at_ : Nat) (value : Lane) (count high : Nat) : State :=
  ⟨count, high, fun l => if l = at_ then value else s.lane l⟩

/-- The steps, on a manifold of `max` lanes. -/
inductive Step (max : Nat) : State → State → Prop where
  /-- An arrival that read r lanes open takes a free lane below r. -/
  | enter {s : State} (r l : Nat) (hr : 1 ≤ r ∧ r ≤ s.high) (hl : l < r) (hf : s.lane l = .free) :
      Step max s (s.set l .pass)
  | leave {s : State} (l : Nat) (h : s.lane l = .pass) : Step max s (s.set l .free)
  /-- A contraction that read n lanes open, n above 1, claims lane n - 1. -/
  | claim {s : State} (n : Nat) (hn : 1 < n ∧ n ≤ s.high) (hf : s.lane (n - 1) = .free) :
      Step max s (s.set (n - 1) .held)
  /-- The swap of the open count from l + 1 to l lands. -/
  | shut {s : State} (l : Nat) (h : s.lane l = .held) (ho : s.count = l + 1) :
      Step max s (s.reset l .sealing l s.high)
  /-- The swap fails, and the contraction releases its lane. -/
  | release {s : State} (l : Nat) (h : s.lane l = .held) (ho : s.count ≠ l + 1) : Step max s (s.set l .free)
  /-- The fetch-add closes a sealing lane. -/
  | finish {s : State} (l : Nat) (h : s.lane l = .sealing) : Step max s (s.set l .closed)
  /-- A grower's compare-and-swap takes a free lane. -/
  | takeFree {s : State} (l : Nat) (h : s.lane l = .free) : Step max s (s.set l (.growing false))
  /-- A grower's compare-and-swap takes a closed lane, and counts the reopen. -/
  | takeClosed {s : State} (l : Nat) (h : s.lane l = .closed) : Step max s (s.set l (.growing true))
  /-- The swap of the open count from l to l + 1 lands, and the lane is the grower's pass. -/
  | grow {s : State} (l : Nat) (b : Bool) (h : s.lane l = .growing b) (ho : s.count = l) (hm : l < max) :
      Step max s (s.reset l .pass (l + 1) (Max.max s.high (l + 1)))
  /-- The swap fails, and the grower gives the lane back as it was. -/
  | giveBack {s : State} (l : Nat) (b : Bool) (h : s.lane l = .growing b) (ho : s.count ≠ l) :
      Step max s (s.set l (if b then .closed else .free))

/-- The states the steps reach from rest. -/
inductive Reach (max : Nat) : State → Prop where
  | rest : Reach max rest
  | step {s t : State} : Reach max s → Step max s t → Reach max t

/-- The invariant. -/
structure Inv (max : Nat) (s : State) : Prop where
  count_pos : 1 ≤ s.count
  count_high : s.count ≤ s.high
  high_max : s.high ≤ max
  /-- Each lane at or above `high` is free, or a grower holds it from free. -/
  above : ∀ l, s.high ≤ l → s.lane l = .free ∨ s.lane l = .growing false
  /-- Each lane from the open count up to `high` has the closed state or the sealing state, or a grower holds it
  from closed. -/
  between : ∀ l, s.count ≤ l → l < s.high →
    s.lane l = .closed ∨ s.lane l = .sealing ∨ s.lane l = .growing true
  held_pos : ∀ l, s.lane l = .held → 1 ≤ l
  closed_range : ∀ l, s.lane l = .closed → s.count ≤ l ∧ l < s.high
  sealing_range : ∀ l, s.lane l = .sealing → s.count ≤ l ∧ l < s.high
  growing_true : ∀ l, s.lane l = .growing true → s.count ≤ l ∧ l < s.high
  growing_false : ∀ l, s.lane l = .growing false → l < s.count ∨ s.high ≤ l

/-- A lane that is free, a pass or held is below the open count where it is below `high`. A pass or a held lane is
below the open count. -/
theorem below_open {max : Nat} {s : State} (i : Inv max s) (l : Nat)
    (h : s.lane l = .pass ∨ s.lane l = .held) : l < s.count := by
  by_cases hc : s.count ≤ l
  · by_cases hh : l < s.high
    · rcases i.between l hc hh with e | e | e <;> rcases h with h | h <;> rw [h] at e <;> cases e
    · rcases i.above l (by omega) with e | e <;> rcases h with h | h <;> rw [h] at e <;> cases e
  · omega

theorem free_below_open {max : Nat} {s : State} (i : Inv max s) (l : Nat) (hf : s.lane l = .free)
    (hh : l < s.high) : l < s.count := by
  by_cases hc : s.count ≤ l
  · rcases i.between l hc hh with e | e | e <;> rw [hf] at e <;> cases e
  · omega

theorem rest_inv (max : Nat) (h : 1 ≤ max) : Inv max rest := by
  refine ⟨by simp [rest], by simp [rest], by simpa [rest] using h, fun _ _ => Or.inl rfl, ?_, ?_, ?_, ?_, ?_, ?_⟩ <;>
    intro l <;> simp [rest] <;> omega

/-- A lane set to a value, the rest of the state retained: each field of the invariant at another lane is the old one. -/
theorem set_other (s : State) (l m : Nat) (v : Lane) (h : m ≠ l) : (s.set l v).lane m = s.lane m := by
  simp [State.set, h]

theorem set_same (s : State) (l : Nat) (v : Lane) : (s.set l v).lane l = v := by
  simp [State.set]

theorem reset_other (s : State) (l m : Nat) (v : Lane) (c h : Nat) (hm : m ≠ l) : (s.reset l v c h).lane m = s.lane m := by
  simp [State.reset, hm]

theorem reset_same (s : State) (l : Nat) (v : Lane) (c h : Nat) : (s.reset l v c h).lane l = v := by
  simp [State.reset]

/-- A lane below the open count set to free, a pass, held above lane 0, or a grower's from free retains the
invariant. -/
theorem set_inv {max : Nat} {s : State} (i : Inv max s) (l : Nat) (v : Lane) (hl : l < s.count)
    (hv : v = .free ∨ v = .pass ∨ (v = .held ∧ 1 ≤ l) ∨ v = .growing false) : Inv max (s.set l v) := by
  have hch := i.count_high
  refine ⟨i.count_pos, i.count_high, i.high_max, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
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

/-- A lane from the open count up to `high` set to closed, sealing or a grower's from closed retains the invariant. -/
theorem set_inv_between {max : Nat} {s : State} (i : Inv max s) (l : Nat) (v : Lane) (h1 : s.count ≤ l)
    (h2 : l < s.high) (hv : v = .closed ∨ v = .sealing ∨ v = .growing true) : Inv max (s.set l v) := by
  refine ⟨i.count_pos, i.count_high, i.high_max, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
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

theorem step_inv {max : Nat} {s t : State} (i : Inv max s) (st : Step max s t) : Inv max t := by
  have hch := i.count_high
  have hhm := i.high_max
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
      refine ⟨i.count_pos, i.count_high, i.high_max, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
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
    refine ⟨by simp only [State.reset]; omega, by simp only [State.reset]; omega, i.high_max, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
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
      · refine ⟨i.count_pos, i.count_high, i.high_max, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
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

theorem reach_inv (max : Nat) (h : 1 ≤ max) {s : State} (r : Reach max s) : Inv max s := by
  induction r with
  | rest => exact rest_inv max h
  | step _ st ih => exact step_inv ih st

/-- In each state that the steps reach, each pass and each held lane is below the open count, and no closed lane is.
The open count is from 1 to the lane count. So a drain that reads the open count after its fence reads each lane
that a pass can hold. No lane below the open count stays closed. -/
theorem passes_below_open (max : Nat) (h : 1 ≤ max) {s : State} (r : Reach max s) :
    (∀ l, s.lane l = .pass → l < s.count) ∧ (∀ l, s.lane l = .held → l < s.count) ∧
      (∀ l, s.lane l = .closed → s.count ≤ l) ∧ 1 ≤ s.count ∧ s.count ≤ max := by
  have i := reach_inv max h r
  refine ⟨fun l hl => below_open i l (Or.inl hl), fun l hl => below_open i l (Or.inr hl),
    fun l hl => (i.closed_range l hl).1, i.count_pos, ?_⟩
  have := i.count_high
  have := i.high_max
  omega

end EmbeddedTypes.Manifold.Open
