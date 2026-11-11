/-!
# embedded_types, the queue of the manifold

`queue` counts the workers that found every open lane busy. A worker adds one before it grows the manifold or
waits, and subtracts one in the step that takes its lane. Each subtraction follows the addition of the same worker
in the modification order of the word. Both are atomic operations of one thread on one word.

The model counts the workers in the queue, and the value of the word. A subtraction is a step of a worker in the
queue.

- `queue_counts`. The word holds the count of workers in the queue, so it never reads below zero.
- `empty_at_zero`. A word read at zero or below has no worker in the queue. So a contraction on that read leaves no
  worker that wants a lane.
- `round_sum`, `rounds_sum` and `entry_balanced`. The entry of the C, with its flag `queued`, adds at most once and
  subtracts only its own addition. So an entry that takes its lane leaves the word as it found it.
-/

namespace EmbeddedTypes.Manifold.Queue

structure State where
  /-- The workers between their addition and their subtraction. -/
  waiting : Nat
  /-- The value of the word. -/
  word : Int

inductive Step : State → State → Prop where
  | add {s : State} : Step s ⟨s.waiting + 1, s.word + 1⟩
  | sub {s : State} (h : 0 < s.waiting) : Step s ⟨s.waiting - 1, s.word - 1⟩

inductive Reach : State → Prop where
  | rest : Reach ⟨0, 0⟩
  | step {s t : State} : Reach s → Step s t → Reach t

theorem queue_counts {s : State} (r : Reach s) : s.word = s.waiting ∧ 0 ≤ s.word := by
  induction r with
  | rest => simp
  | step _ st ih =>
    obtain ⟨h1, h2⟩ := ih
    cases st with
    | add => refine ⟨?_, ?_⟩ <;> dsimp only <;> omega
    | sub h => refine ⟨?_, ?_⟩ <;> dsimp only <;> omega

theorem empty_at_zero {s : State} (r : Reach s) (h : s.word ≤ 0) : s.waiting = 0 := by
  have := queue_counts r
  omega

/-! ## One worker's entry

The entry of the C keeps a flag `queued`. A round that finds every open lane busy adds the worker to the queue
where the flag is clear, and sets it. The round that takes a lane subtracts the worker where the flag has the set
value. So each entry adds at most once, and subtracts only after its own addition. -/

/-- A worker's entry: spilling with the flag clear, spilling with it set, or done with a lane. -/
inductive Entry where
  | spilling (queued : Bool)
  | done
  deriving DecidableEq

/-- One round of the entry, and what it adds to the queue word: a round with no lane, then a round that takes one. -/
def round (e : Entry) (takes : Bool) : Entry × Int :=
  match e, takes with
  | .spilling q, true => (.done, if q then -1 else 0)
  | .spilling q, false => (.spilling true, if q then 0 else 1)
  | .done, _ => (.done, 0)

/-- The rounds of one entry, and their sum on the queue word. -/
def rounds : Entry → List Bool → Entry × Int
  | e, [] => (e, 0)
  | e, t :: rest => ((rounds (round e t).1 rest).1, (round e t).2 + (rounds (round e t).1 rest).2)

/-- The addition of a worker so far: 1 where the flag has the set value, 0 where it is clear or the entry ended. -/
def owes : Entry → Int
  | .spilling q => if q then 1 else 0
  | .done => 0

/-- One round adds to the word the change of what the worker owes. -/
theorem round_sum (e : Entry) (t : Bool) : (round e t).2 = owes (round e t).1 - owes e := by
  cases e with
  | spilling q => cases q <;> cases t <;> decide
  | done => cases t <;> decide

/-- From any state, the rounds add to the word exactly the change of what the worker owes. -/
theorem rounds_sum (e : Entry) (ts : List Bool) : (rounds e ts).2 = owes (rounds e ts).1 - owes e := by
  induction ts generalizing e with
  | nil => simp [rounds]
  | cons t rest ih =>
    simp only [rounds]
    rw [ih, round_sum]
    omega

/-- An entry that takes its lane leaves the queue word as it found it. -/
theorem entry_balanced (ts : List Bool) (h : (rounds (.spilling false) ts).1 = .done) :
    (rounds (.spilling false) ts).2 = 0 := by
  rw [rounds_sum, h]
  simp [owes]

end EmbeddedTypes.Manifold.Queue
