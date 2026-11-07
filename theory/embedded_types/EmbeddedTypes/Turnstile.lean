/-!
# embedded_types, the turnstile: the drain waits out each pass of its snapshot

The model of `embed_turnstile_enter`, `embed_turnstile_leave` and `embed_turnstiles_drain` in
`include/embed_turnstile.h`, one lane at a time. Each step is one access to a word, and the model is sequentially
consistent. `TurnstileOrder.lean` carries the result to the fences of C11.

A lane is free, claimed by a pass that has not written its badge, passing with its badge live, or leaving with its
badge's state bits cleared and its `busy` bit still set. A pass has a serial number, and the token of its badge is
that number. Thus two passes have two tokens. The C gives a pass its id and its stamp, and the `@warning` of the
header names the one case where two passes on one lane share both: a stamp that rolls over. The model leaves that
case out.

A claim takes any free lane. The C takes the lowest free lane of `open`, and the model allows more. Thus `open`,
its growth and its trim have no part in the theorem: a trim or a growth at any time leaves the theorem true.
`TurnstileBits.lean` holds the bit arithmetic: one fetch-or sets each lane's bit for one claim only.

The drain takes its snapshot of `busy` and the lanes' occupants, then runs rounds. A round reads one pending lane's
badge, then reads the lane's `busy` bit, and decides. The steps of the passes run between the two reads.

The theorem `drained`: when the drain has no lane pending, each pass of its snapshot has ended.
-/

namespace EmbeddedTypes.Turnstile

/-- A badge word: the token of its pass, the completion bit, and the sleeper bit. -/
structure Badge where
  token : Nat
  live : Bool
  sleeper : Bool
  deriving DecidableEq, Repr

/-- Where a lane stands, with the serial number of its pass. -/
inductive Phase where
  | free
  | claimed (n : Nat)
  | passing (n : Nat)
  | leaving (n : Nat)
  deriving DecidableEq, Repr

/-- The pass on a lane, where one is. -/
def Phase.serial : Phase → Option Nat
  | .free => none
  | .claimed n => some n
  | .passing n => some n
  | .leaving n => some n

/-- Where the drain stands: before its snapshot, in its rounds, or between the two reads of a round. -/
inductive Drain where
  | idle
  | rounds
  | read (lane : Nat) (r : Badge)
  deriving DecidableEq, Repr

structure State where
  phase : Nat → Phase
  badge : Nat → Badge
  next : Nat
  drain : Drain
  pending : Nat → Bool
  snap : Nat → Option Nat
  first : Badge
  held : Bool
  seen : Option Nat

/-- One lane's value changed. -/
def set {α : Type} (f : Nat → α) (l : Nat) (a : α) : Nat → α := fun k => if k = l then a else f k

/-- A pass has ended: its serial was given out, and no lane holds it. A claim takes the next serial, so a pass that
has ended stays ended. -/
def Ended (s : State) (n : Nat) : Prop := n < s.next ∧ ∀ l, (s.phase l).serial ≠ some n

/-- The steps. -/
inductive Step : State → State → Prop where
  /-- A claim: the fetch-or sets a free lane's `busy` bit. The pass takes the next serial number. -/
  | claim {s : State} (l : Nat) (h : s.phase l = .free) :
      Step s { s with phase := set s.phase l (.claimed s.next), next := s.next + 1 }
  /-- The badge written: its token, the completion bit set, the sleeper bit clear. -/
  | write {s : State} (l n : Nat) (h : s.phase l = .claimed n) :
      Step s { s with phase := set s.phase l (.passing n), badge := set s.badge l ⟨n, true, false⟩ }
  /-- The first fetch-and of the end of a pass: the badge's state bits cleared, the token kept. -/
  | clearBadge {s : State} (l n : Nat) (h : s.phase l = .passing n) :
      Step s { s with phase := set s.phase l (.leaving n),
                      badge := set s.badge l { s.badge l with live := false, sleeper := false } }
  /-- The second fetch-and: the lane's `busy` bit cleared. -/
  | clearBusy {s : State} (l n : Nat) (h : s.phase l = .leaving n) :
      Step s { s with phase := set s.phase l .free }
  /-- The drain's backoff sets the sleeper bit on a lane's badge with one fetch-or. -/
  | flag {s : State} (l : Nat) :
      Step s { s with badge := set s.badge l { s.badge l with sleeper := true } }
  /-- The snapshot: the lanes with their `busy` bit set, and the passes that hold them. -/
  | snapshot {s : State} (h : s.drain = .idle) :
      Step s { s with drain := .rounds, pending := fun l => (s.phase l).serial.isSome,
                      snap := fun l => (s.phase l).serial, held := false, seen := none }
  /-- A round's first read: a pending lane's badge. -/
  | readBadge {s : State} (l : Nat) (h : s.drain = .rounds) (hp : s.pending l = true) :
      Step s { s with drain := .read l (s.badge l) }
  /-- A round's second read and its decision. The lane is over where its `busy` bit is clear, or where a live
  badge is held and the badge read moved off it in token. -/
  | decide {s : State} (l : Nat) (r : Badge) (h : s.drain = .read l r) :
      Step s
        (let take := s.seen ≠ some l ∨ s.held = false
         let first := if take then r else s.first
         let held := if take then r.live else s.held
         let over := (s.phase l).serial = none ∨ (held = true ∧ r.token ≠ first.token)
         { s with drain := .rounds, first := first, held := held, seen := some l,
                  pending := set s.pending l (s.pending l && !decide over) })

def start : State :=
  ⟨fun _ => .free, fun _ => ⟨0, false, false⟩, 0, .idle, fun _ => false, fun _ => none, ⟨0, false, false⟩, false,
    none⟩

inductive Reach : State → Prop where
  | start : Reach start
  | step {s s' : State} : Reach s → Step s s' → Reach s'

/-- The invariant.

- `fresh`: each serial on a lane is below `next`.
- `single`: a serial is on one lane at most.
- `liveOwned`: a live badge is the badge of the pass on its lane, which is passing.
- `tokenOwned`: a lane passing or leaving has its pass's token in its badge.
- `snapFresh`: each serial of the snapshot is below `next`.
- `snapLane`: a pass of the snapshot that is still on a lane is on its lane of the snapshot.
- `pendingSnap`: a pending lane had a pass at the snapshot.
- `doneEnded`: a lane off the pending set had no pass at the snapshot, or that pass has ended.
- `heldNext`: a live badge held for a lane is the badge of the snapshot's pass, or of a pass after that one ended.
- `heldOwned`: the pass of a held badge is on the lane, passing or leaving, or it has ended.
- `readNext` and `readOwned`: the same two facts for the badge that a round read, where it is live.
- `readMoved`: where a held badge's token differs from the token that a round read, the held pass has ended.
-/
structure Inv (s : State) : Prop where
  fresh : ∀ l n, (s.phase l).serial = some n → n < s.next
  single : ∀ l l' n, (s.phase l).serial = some n → (s.phase l').serial = some n → l = l'
  liveOwned : ∀ l, (s.badge l).live = true → s.phase l = .passing (s.badge l).token
  tokenOwned : ∀ l n, (s.phase l = .passing n ∨ s.phase l = .leaving n) → (s.badge l).token = n
  snapFresh : s.drain ≠ .idle → ∀ l n, s.snap l = some n → n < s.next
  snapLane : s.drain ≠ .idle → ∀ l l' n, s.snap l = some n → (s.phase l').serial = some n → l' = l
  pendingSnap : s.drain ≠ .idle → ∀ l, s.pending l = true → (s.snap l).isSome
  doneEnded : s.drain ≠ .idle → ∀ l n, s.pending l = false → s.snap l = some n → Ended s n
  heldNext : s.drain ≠ .idle → ∀ l n, s.seen = some l → s.held = true → s.snap l = some n →
    s.first.token = n ∨ Ended s n
  heldOwned : s.drain ≠ .idle → ∀ l, s.seen = some l → s.held = true →
    s.phase l = .passing s.first.token ∨ s.phase l = .leaving s.first.token ∨ Ended s s.first.token
  readNext : ∀ l r n, s.drain = .read l r → r.live = true → s.snap l = some n → r.token = n ∨ Ended s n
  readOwned : ∀ l r, s.drain = .read l r → r.live = true →
    s.phase l = .passing r.token ∨ s.phase l = .leaving r.token ∨ Ended s r.token
  readMoved : ∀ l r, s.drain = .read l r → s.seen = some l → s.held = true → r.token ≠ s.first.token →
    Ended s s.first.token

theorem set_same {α : Type} (f : Nat → α) (l : Nat) (a : α) : set f l a l = a := by simp [set]

theorem set_other {α : Type} (f : Nat → α) (l k : Nat) (a : α) (h : k ≠ l) : set f l a k = f k := by
  simp [set, h]

/-- A pass that has ended stays ended: only a claim puts a serial on a lane, and it is the next one. -/
theorem ended_step {s s' : State} {n : Nat} (st : Step s s') (he : Ended s n) : Ended s' n := by
  obtain ⟨hn, hl⟩ := he
  cases st with
  | claim l h =>
    refine ⟨Nat.lt_succ_of_lt hn, fun k => ?_⟩
    by_cases hk : k = l
    · subst hk; simp [set_same, Phase.serial]; omega
    · simp only [set_other _ _ _ _ hk]; exact hl k
  | write l m h =>
    refine ⟨hn, fun k => ?_⟩
    by_cases hk : k = l
    · subst hk; have := hl k; rw [h] at this; simpa [set_same, Phase.serial] using this
    · simp only [set_other _ _ _ _ hk]; exact hl k
  | clearBadge l m h =>
    refine ⟨hn, fun k => ?_⟩
    by_cases hk : k = l
    · subst hk; have := hl k; rw [h] at this; simpa [set_same, Phase.serial] using this
    · simp only [set_other _ _ _ _ hk]; exact hl k
  | clearBusy l m h =>
    refine ⟨hn, fun k => ?_⟩
    by_cases hk : k = l
    · subst hk; simp [set_same, Phase.serial]
    · simp only [set_other _ _ _ _ hk]; exact hl k
  | flag l => exact ⟨hn, hl⟩
  | snapshot h => exact ⟨hn, hl⟩
  | readBadge l h hp => exact ⟨hn, hl⟩
  | decide l r h => exact ⟨hn, hl⟩

/-- A pass that holds no lane and whose serial was given out has ended. -/
theorem ended_of {s : State} {n : Nat} (hn : n < s.next) (hl : ∀ l, (s.phase l).serial ≠ some n) : Ended s n :=
  ⟨hn, hl⟩

theorem inv_start : Inv start where
  fresh := by intro l n h; simp [start, Phase.serial] at h
  single := by intro l l' n h; simp [start, Phase.serial] at h
  liveOwned := by intro l h; simp [start] at h
  tokenOwned := by intro l n h; simp [start] at h
  snapFresh := by intro h; simp [start] at h
  snapLane := by intro h; simp [start] at h
  pendingSnap := by intro h; simp [start] at h
  doneEnded := by intro h; simp [start] at h
  heldNext := by intro h; simp [start] at h
  heldOwned := by intro h; simp [start] at h
  readNext := by intro l r n h; simp [start] at h
  readOwned := by intro l r h; simp [start] at h
  readMoved := by intro l r h; simp [start] at h

/-- A lane's serial after a step that changed only that lane's phase, read back at another lane. -/
theorem serial_other {s : State} {l k : Nat} {p : Phase} (h : k ≠ l) :
    ((set s.phase l p) k).serial = (s.phase k).serial := by
  simp [set_other _ _ _ _ h]

/-- The steps of a pass: each changes one lane's phase and keeps the drain's fields. The facts of the drain that
name a pass on a lane follow from the facts before the step, case by case. -/
theorem inv_step {s s' : State} (hs : Inv s) (st : Step s s') : Inv s' := by
  have hend : ∀ n, Ended s n → Ended s' n := fun n he => ended_step st he
  cases st with
  | claim l h =>
    have hfree : (s.phase l).serial = none := by rw [h]; rfl
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro k n hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same, Phase.serial] at hk; dsimp only; omega
      · rw [serial_other hkl] at hk; dsimp only; exact Nat.lt_succ_of_lt (hs.fresh k n hk)
    · intro a b n ha hb
      by_cases hal : a = l
      · by_cases hbl : b = l
        · rw [hal, hbl]
        · subst hal; simp [set_same, Phase.serial] at ha; rw [serial_other hbl] at hb
          have := hs.fresh b n hb; omega
      · by_cases hbl : b = l
        · subst hbl; simp [set_same, Phase.serial] at hb; rw [serial_other hal] at ha
          have := hs.fresh a n ha; omega
        · rw [serial_other hal] at ha; rw [serial_other hbl] at hb; exact hs.single a b n ha hb
    · intro k hk
      have hl := hs.liveOwned k hk
      by_cases hkl : k = l
      · subst hkl; rw [h] at hl; cases hl
      · simp only [set_other _ _ _ _ hkl]; exact hl
    · intro k n hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same] at hk
      · simp only [set_other _ _ _ _ hkl] at hk; exact hs.tokenOwned k n hk
    · intro hd k n hk; exact Nat.lt_succ_of_lt (hs.snapFresh hd k n hk)
    · intro hd a b n ha hb
      by_cases hbl : b = l
      · subst hbl; simp [set_same, Phase.serial] at hb; have := hs.snapFresh hd a n ha; omega
      · rw [serial_other hbl] at hb; exact hs.snapLane hd a b n ha hb
    · exact hs.pendingSnap
    · intro hd k n hp hk; exact hend n (hs.doneEnded hd k n hp hk)
    · intro hd k n he hh hk
      rcases hs.heldNext hd k n he hh hk with ht | ht
      · exact Or.inl ht
      · exact Or.inr (hend n ht)
    · intro hd k he hh
      rcases hs.heldOwned hd k he hh with hp | hp | hp
      · by_cases hkl : k = l
        · subst hkl; rw [h] at hp; cases hp
        · exact Or.inl (by simp only [set_other _ _ _ _ hkl]; exact hp)
      · by_cases hkl : k = l
        · subst hkl; rw [h] at hp; cases hp
        · exact Or.inr (Or.inl (by simp only [set_other _ _ _ _ hkl]; exact hp))
      · exact Or.inr (Or.inr (hend _ hp))
    · intro k r n hd hr hk
      rcases hs.readNext k r n hd hr hk with ht | ht
      · exact Or.inl ht
      · exact Or.inr (hend n ht)
    · intro k r hd hr
      rcases hs.readOwned k r hd hr with hp | hp | hp
      · by_cases hkl : k = l
        · subst hkl; rw [h] at hp; cases hp
        · exact Or.inl (by simp only [set_other _ _ _ _ hkl]; exact hp)
      · by_cases hkl : k = l
        · subst hkl; rw [h] at hp; cases hp
        · exact Or.inr (Or.inl (by simp only [set_other _ _ _ _ hkl]; exact hp))
      · exact Or.inr (Or.inr (hend _ hp))
    · intro k r hd he hh hne; exact hend _ (hs.readMoved k r hd he hh hne)
  | write l m h =>
    have hm : (s.phase l).serial = some m := by rw [h]; rfl
    have hser : ∀ k, ((set s.phase l (.passing m)) k).serial = (s.phase k).serial := by
      intro k; by_cases hkl : k = l
      · subst hkl; rw [set_same, h]; rfl
      · exact serial_other hkl
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro k n hk; rw [hser] at hk; exact hs.fresh k n hk
    · intro a b n ha hb; rw [hser] at ha hb; exact hs.single a b n ha hb
    · intro k hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same]
      · simp only [set_other _ _ _ _ hkl] at hk ⊢; exact hs.liveOwned k hk
    · intro k n hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same] at hk ⊢; omega
      · simp only [set_other _ _ _ _ hkl] at hk ⊢; exact hs.tokenOwned k n hk
    · exact hs.snapFresh
    · intro hd a b n ha hb; rw [hser] at hb; exact hs.snapLane hd a b n ha hb
    · exact hs.pendingSnap
    · intro hd k n hp hk; exact hend n (hs.doneEnded hd k n hp hk)
    · intro hd k n he hh hk
      rcases hs.heldNext hd k n he hh hk with ht | ht
      · exact Or.inl ht
      · exact Or.inr (hend n ht)
    · intro hd k he hh
      rcases hs.heldOwned hd k he hh with hp | hp | hp
      · by_cases hkl : k = l
        · subst hkl; rw [h] at hp; cases hp
        · exact Or.inl (by simp only [set_other _ _ _ _ hkl]; exact hp)
      · by_cases hkl : k = l
        · subst hkl; rw [h] at hp; cases hp
        · exact Or.inr (Or.inl (by simp only [set_other _ _ _ _ hkl]; exact hp))
      · exact Or.inr (Or.inr (hend _ hp))
    · intro k r n hd hr hk
      rcases hs.readNext k r n hd hr hk with ht | ht
      · exact Or.inl ht
      · exact Or.inr (hend n ht)
    · intro k r hd hr
      rcases hs.readOwned k r hd hr with hp | hp | hp
      · by_cases hkl : k = l
        · subst hkl; rw [h] at hp; cases hp
        · exact Or.inl (by simp only [set_other _ _ _ _ hkl]; exact hp)
      · by_cases hkl : k = l
        · subst hkl; rw [h] at hp; cases hp
        · exact Or.inr (Or.inl (by simp only [set_other _ _ _ _ hkl]; exact hp))
      · exact Or.inr (Or.inr (hend _ hp))
    · intro k r hd he hh hne; exact hend _ (hs.readMoved k r hd he hh hne)
  | clearBadge l m h =>
    have hser : ∀ k, ((set s.phase l (.leaving m)) k).serial = (s.phase k).serial := by
      intro k; by_cases hkl : k = l
      · subst hkl; rw [set_same, h]; rfl
      · exact serial_other hkl
    have htok : (s.badge l).token = m := hs.tokenOwned l m (Or.inl h)
    -- a pass that was passing on the lane is leaving there now, and every other lane keeps its phase
    have hmove : ∀ k q, (s.phase k = .passing q ∨ s.phase k = .leaving q) →
        ((set s.phase l (.leaving m)) k = .passing q ∨ (set s.phase l (.leaving m)) k = .leaving q) := by
      intro k q hk
      by_cases hkl : k = l
      · subst hkl; rw [h] at hk; simp [set_same]
        rcases hk with hk | hk <;> cases hk <;> rfl
      · simp only [set_other _ _ _ _ hkl]; exact hk
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro k n hk; rw [hser] at hk; exact hs.fresh k n hk
    · intro a b n ha hb; rw [hser] at ha hb; exact hs.single a b n ha hb
    · intro k hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same] at hk
      · simp only [set_other _ _ _ _ hkl] at hk ⊢; exact hs.liveOwned k hk
    · intro k n hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same] at hk ⊢; omega
      · simp only [set_other _ _ _ _ hkl] at hk ⊢; exact hs.tokenOwned k n hk
    · exact hs.snapFresh
    · intro hd a b n ha hb; rw [hser] at hb; exact hs.snapLane hd a b n ha hb
    · exact hs.pendingSnap
    · intro hd k n hp hk; exact hend n (hs.doneEnded hd k n hp hk)
    · intro hd k n he hh hk
      rcases hs.heldNext hd k n he hh hk with ht | ht
      · exact Or.inl ht
      · exact Or.inr (hend n ht)
    · intro hd k he hh
      rcases hs.heldOwned hd k he hh with hp | hp | hp
      · exact (hmove k _ (Or.inl hp)).elim Or.inl (fun x => Or.inr (Or.inl x))
      · exact (hmove k _ (Or.inr hp)).elim Or.inl (fun x => Or.inr (Or.inl x))
      · exact Or.inr (Or.inr (hend _ hp))
    · intro k r n hd hr hk
      rcases hs.readNext k r n hd hr hk with ht | ht
      · exact Or.inl ht
      · exact Or.inr (hend n ht)
    · intro k r hd hr
      rcases hs.readOwned k r hd hr with hp | hp | hp
      · exact (hmove k _ (Or.inl hp)).elim Or.inl (fun x => Or.inr (Or.inl x))
      · exact (hmove k _ (Or.inr hp)).elim Or.inl (fun x => Or.inr (Or.inl x))
      · exact Or.inr (Or.inr (hend _ hp))
    · intro k r hd he hh hne; exact hend _ (hs.readMoved k r hd he hh hne)
  | clearBusy l m h =>
    have hm : (s.phase l).serial = some m := by rw [h]; rfl
    -- the pass leaving the lane ends here: its serial is on no lane after the step
    have hgone : Ended { s with phase := set s.phase l .free } m := by
      refine ⟨hs.fresh l m hm, fun k hk => ?_⟩
      by_cases hkl : k = l
      · subst hkl; simp [set_same, Phase.serial] at hk
      · rw [serial_other hkl] at hk; exact hkl (hs.single k l m hk hm)
    have hkeep : ∀ k q, (s.phase k = .passing q ∨ s.phase k = .leaving q) →
        ((set s.phase l .free) k = .passing q ∨ (set s.phase l .free) k = .leaving q ∨
          Ended { s with phase := set s.phase l .free } q) := by
      intro k q hk
      by_cases hkl : k = l
      · subst hkl; rw [h] at hk
        rcases hk with hk | hk <;> cases hk; exact Or.inr (Or.inr hgone)
      · simp only [set_other _ _ _ _ hkl]
        rcases hk with hk | hk
        · exact Or.inl hk
        · exact Or.inr (Or.inl hk)
    refine ⟨?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro k n hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same, Phase.serial] at hk
      · rw [serial_other hkl] at hk; exact hs.fresh k n hk
    · intro a b n ha hb
      by_cases hal : a = l
      · subst hal; simp [set_same, Phase.serial] at ha
      · by_cases hbl : b = l
        · subst hbl; simp [set_same, Phase.serial] at hb
        · rw [serial_other hal] at ha; rw [serial_other hbl] at hb; exact hs.single a b n ha hb
    · intro k hk
      have hl := hs.liveOwned k hk
      by_cases hkl : k = l
      · subst hkl; rw [h] at hl; cases hl
      · simp only [set_other _ _ _ _ hkl]; exact hl
    · intro k n hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same] at hk
      · simp only [set_other _ _ _ _ hkl] at hk; exact hs.tokenOwned k n hk
    · exact hs.snapFresh
    · intro hd a b n ha hb
      by_cases hbl : b = l
      · subst hbl; simp [set_same, Phase.serial] at hb
      · rw [serial_other hbl] at hb; exact hs.snapLane hd a b n ha hb
    · exact hs.pendingSnap
    · intro hd k n hp hk; exact hend n (hs.doneEnded hd k n hp hk)
    · intro hd k n he hh hk
      rcases hs.heldNext hd k n he hh hk with ht | ht
      · exact Or.inl ht
      · exact Or.inr (hend n ht)
    · intro hd k he hh
      rcases hs.heldOwned hd k he hh with hp | hp | hp
      · exact hkeep k _ (Or.inl hp)
      · exact hkeep k _ (Or.inr hp)
      · exact Or.inr (Or.inr (hend _ hp))
    · intro k r n hd hr hk
      rcases hs.readNext k r n hd hr hk with ht | ht
      · exact Or.inl ht
      · exact Or.inr (hend n ht)
    · intro k r hd hr
      rcases hs.readOwned k r hd hr with hp | hp | hp
      · exact hkeep k _ (Or.inl hp)
      · exact hkeep k _ (Or.inr hp)
      · exact Or.inr (Or.inr (hend _ hp))
    · intro k r hd he hh hne; exact hend _ (hs.readMoved k r hd he hh hne)
  | flag l =>
    refine ⟨hs.fresh, hs.single, ?_, ?_, hs.snapFresh, hs.snapLane, hs.pendingSnap, hs.doneEnded, hs.heldNext,
      hs.heldOwned, hs.readNext, hs.readOwned, hs.readMoved⟩
    · intro k hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same] at hk ⊢; exact hs.liveOwned k hk
      · simp only [set_other _ _ _ _ hkl] at hk ⊢; exact hs.liveOwned k hk
    · intro k n hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same]; exact hs.tokenOwned k n hk
      · simp only [set_other _ _ _ _ hkl]; exact hs.tokenOwned k n hk
  | snapshot h =>
    refine ⟨hs.fresh, hs.single, hs.liveOwned, hs.tokenOwned, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro _ k n hk; exact hs.fresh k n hk
    · intro _ a b n ha hb; exact hs.single b a n hb ha
    · intro _ k hk; exact hk
    · intro _ k n hp hk; simp at hp hk; rw [hk] at hp; cases hp
    · intro _ k n he; simp at he
    · intro _ k he; simp at he
    · intro k r n hd; simp at hd
    · intro k r hd; simp at hd
    · intro k r hd; simp at hd
  | readBadge l h hp =>
    have hd : s.drain ≠ .idle := by rw [h]; intro e; cases e
    refine ⟨hs.fresh, hs.single, hs.liveOwned, hs.tokenOwned, fun _ => hs.snapFresh hd, fun _ => hs.snapLane hd,
      fun _ => hs.pendingSnap hd, fun _ => hs.doneEnded hd, fun _ => hs.heldNext hd, fun _ => hs.heldOwned hd,
      ?_, ?_, ?_⟩
    · intro k r n hr hl hk
      simp only [Drain.read.injEq] at hr
      obtain ⟨hlk, hrr⟩ := hr
      subst hlk; subst hrr
      have hpass := hs.liveOwned l hl
      by_cases he : Ended s n
      · exact Or.inr he
      · -- the pass of the snapshot is on a lane, so it is on this lane, which the badge's pass holds
        have hn := hs.snapFresh hd l n hk
        have : ∃ k', (s.phase k').serial = some n := by
          apply Classical.byContradiction
          intro hno
          exact he ⟨hn, fun k' hk' => hno ⟨k', hk'⟩⟩
        obtain ⟨k', hk'⟩ := this
        have hkk := hs.snapLane hd l k' n hk hk'
        subst hkk
        rw [hpass] at hk'
        exact Or.inl (by simp [Phase.serial] at hk'; exact hk')
    · intro k r hr hl
      simp only [Drain.read.injEq] at hr
      obtain ⟨hlk, hrr⟩ := hr
      subst hlk; subst hrr
      exact Or.inl (hs.liveOwned l hl)
    · intro k r hr he hh hne
      simp only [Drain.read.injEq] at hr
      obtain ⟨hlk, hrr⟩ := hr
      subst hlk; subst hrr
      rcases hs.heldOwned hd l he hh with hq | hq | hq
      · exact (hne (hs.tokenOwned l _ (Or.inl hq))).elim
      · exact (hne (hs.tokenOwned l _ (Or.inr hq))).elim
      · exact hq
  | decide l r h =>
    have hd : s.drain ≠ .idle := by rw [h]; intro e; cases e
    have hpl : s.pending l = true → (s.snap l).isSome := hs.pendingSnap hd l
    refine ⟨hs.fresh, hs.single, hs.liveOwned, hs.tokenOwned, fun _ => hs.snapFresh hd, fun _ => hs.snapLane hd,
      ?_, ?_, ?_, ?_, ?_, ?_, ?_⟩
    · intro _ k hk
      by_cases hkl : k = l
      · subst hkl; simp [set_same] at hk; exact hpl hk.1
      · simp only [set_other _ _ _ _ hkl] at hk; exact hs.pendingSnap hd k hk
    · intro _ k n hp hk
      by_cases hkl : k = l
      · subst hkl
        simp only [set_same] at hp
        by_cases hp0 : s.pending k = false
        · exact hs.doneEnded hd k n hp0 hk
        · -- the lane left the pending set in this round: it was over
          have hpt : s.pending k = true := by simpa using hp0
          by_cases hfree : (s.phase k).serial = none
          · refine ⟨hs.snapFresh hd k n hk, fun k' hk' => ?_⟩
            have := hs.snapLane hd k k' n hk hk'
            subst this; rw [hfree] at hk'; cases hk'
          · by_cases ht : s.seen ≠ some k ∨ s.held = false
            · -- the round took this read as its first: the token cannot have moved
              simp [hpt, hfree, ht] at hp
            · -- a held badge whose token the read moved off
              simp only [ne_eq, not_or, Bool.not_eq_false, Classical.not_not] at ht
              obtain ⟨hseen, hheld0⟩ := ht
              have hmoved : r.token ≠ s.first.token := by
                intro heq; simp [hpt, hfree, hseen, hheld0, heq] at hp
              have hfirst := hs.readMoved k r h hseen hheld0 hmoved
              rcases hs.heldNext hd k n hseen hheld0 hk with heq | hend
              · rw [heq] at hfirst; exact hfirst
              · exact hend
      · simp only [set_other _ _ _ _ hkl] at hp; exact hs.doneEnded hd k n hp hk
    · intro _ k n hseen hheld hk
      simp only [Option.some.injEq] at hseen; subst hseen
      by_cases ht : s.seen ≠ some l ∨ s.held = false
      · simp only [ht, ↓reduceIte] at hheld ⊢
        exact hs.readNext l r n h hheld hk
      · simp only [ne_eq, not_or, Bool.not_eq_false, Classical.not_not] at ht
        obtain ⟨hs0, hh0⟩ := ht
        simp only [hs0, hh0, ne_eq, not_true_eq_false, Bool.true_eq_false, or_self, ↓reduceIte] at hheld ⊢
        exact hs.heldNext hd l n hs0 hh0 hk
    · intro _ k hseen hheld
      simp only [Option.some.injEq] at hseen; subst hseen
      by_cases ht : s.seen ≠ some l ∨ s.held = false
      · simp only [ht, ↓reduceIte] at hheld ⊢
        exact hs.readOwned l r h hheld
      · simp only [ne_eq, not_or, Bool.not_eq_false, Classical.not_not] at ht
        obtain ⟨hs0, hh0⟩ := ht
        simp only [hs0, hh0, ne_eq, not_true_eq_false, Bool.true_eq_false, or_self, ↓reduceIte] at hheld ⊢
        exact hs.heldOwned hd l hs0 hh0
    · intro k r' n hr; simp at hr
    · intro k r' hr; simp at hr
    · intro k r' hr; simp at hr

theorem inv_reach {s : State} (h : Reach s) : Inv s := by
  induction h with
  | start => exact inv_start
  | step _ st ih => exact inv_step ih st

/-- The drain with no lane pending has waited out each pass of its snapshot: each one has ended. -/
theorem drained {s : State} (h : Reach s) (hd : s.drain = .rounds) (hp : ∀ l, s.pending l = false) :
    ∀ l n, s.snap l = some n → Ended s n := by
  intro l n hn
  have hne : s.drain ≠ .idle := by rw [hd]; intro e; cases e
  exact (inv_reach h).doneEnded hne l n (hp l) hn

end EmbeddedTypes.Turnstile
