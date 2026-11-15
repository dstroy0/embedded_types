# embedded_types

Seven C11 headers for an embedded build. They hold a fully atomic thread scheduler, atomic types,
compiler directives and platform directives. Under those are the fixed-width integers, the machine
word, the index and the lane that each one builds on. Each library in a build takes these names from
one place, and none defines them again. The library is headers only. No header allocates memory or
runs code at startup.

The site at <https://dstroy0.github.io/embedded_types/> shows the API of each header, from the
Doxygen comments in the headers. The Doxygen block on a declaration is the contract for that name.

## The headers

| header                                                                       | what it holds                                                                                                                                                      |
| ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [`include/embed_turnstile.h`](include/embed_turnstile.h)                     | the thread scheduler: turnstiles that readers pass through and that a writer drains, a backoff in nanoseconds, and the sleep on the address of a word               |
| [`include/embed_ticket_ring.h`](include/embed_ticket_ring.h)                 | the ticket ring: a ring of job slots that a ticket indexes, with a state machine per slot in the flag bits of its stamp, each transition one compare-and-swap inside a pass |
| [`include/embed_atomic.h`](include/embed_atomic.h)                           | the atomic types: a 32-bit word on each target and a 64-bit word where the target has one, each with load, store, exchange, compare-and-swap, add, subtract, or, and, and raise, on the arm of the target |
| [`include/embed_compiler_directives.h`](include/embed_compiler_directives.h) | the compiler directives, feature checks, the static assertion, attribute wrappers, diagnostic pragmas and an argument count; and the platform directives, the byte order, the unaligned load, the widest vector register and the host and device marker |
| [`include/embed_dispatch_layout.h`](include/embed_dispatch_layout.h)         | the dispatch table: assertions that pin each function pointer of a struct to its slot, and the macros that call and define an entry point                            |
| [`include/embed_types.h`](include/embed_types.h)                             | the widths: the fixed-width integers, the boolean, the word, the index, the raw word, and the assertions that pin each one                                           |
| [`include/embed_lane.h`](include/embed_lane.h)                               | the lane: the widest vector register of the target, with its loads, fills, arithmetic and byte masks                                                               |

### The thread scheduler

A set of turnstiles is a manifold of lanes. Each lane is one atomic word on a cache line of its own.
A reader enters at the lane that its address selects. Where that lane is busy, the reader spills to
the next lane. Where every lane is busy, the manifold opens one more. As the pressure goes, the
manifold closes lanes again.

A writer swaps a published pointer, then drains: it waits until each pass that holds a lane at the
drain ends. Each step is one atomic operation on one word, and no step holds a lock.

A reader that must wait backs off. Inside the first unit of time, a step is a pause. After it, a
step sleeps on the address of the lane word, for a time that doubles at each step. The time is in
nanoseconds in a machine word, and the clock rolls at the width of the word. The backoff takes the
difference of two reads of the clock, and that difference holds across the roll.

The host supplies the sleep, the wake and the clock:

```c
int embed_word_wait(embed_atomic_word *word, embed_atomic_word_value old_value, embed_word nanoseconds);
void embed_word_wake(embed_atomic_word *word);
embed_word embed_clock_ns(void);
```

On Linux that is the futex, on Windows WaitOnAddress, and on macOS `__ulock_wait`. A build that
supplies none of the three fails at the link, and the error names them.

The Lean proofs under [`theory/`](theory) hold the lane word, the backoff, the open count, the
queue, the drain and the sleeps. The GenMC clients under [`test/prove/`](test/prove) run the drain
and the resize under five memory models.

The ticket ring is the job queue of the scheduler. It has 2^shift slots, and a slot holds a stamp
and a job pointer. The stamp is one atomic machine word: the epoch in the high bits, 16 flag bits,
and the ticket in the low bits. The low shift bits of the ticket select the slot.

| word    | ticket          | flags          | epoch          |
| ------- | --------------- | -------------- | -------------- |
| 64 bits | bits 0 to 31    | bits 32 to 47  | bits 48 to 63  |
| 32 bits | bits 0 to 11    | bits 12 to 27  | bits 28 to 31  |

A word of 16 bits has no room for the three fields, and the header stops the build there.

The flag bits are the state machine of the slot: granted, pushed, popped, replayed, released. Each
transition is one compare-and-swap inside a pass through a turnstile. It expects the word with the
flags before it and writes the word with its own flag. So exactly one caller completes each
transition, and a stamp that differs in any bit fails the compare.

Three flag bits are a cyclic redundancy check of the job pointer. The push stores it and the pop
checks it, so a single-bit error in the job pointer, or an adjacent double, returns no job. The Lean
proofs hold the fields of the stamp, the place of a ticket, and the roll. They also hold the flag
matrix, the check, the collision of two pops on one slot, and the integrated algorithm with its
supervisor.

### The atomic types

Each atomic operation is one indivisible step on the atomics of the target. A load has acquire
ordering, a store has release ordering, and each read-modify-write has both. The header selects one
of four arms, one time. The arms are the device atomics of CUDA in a device pass, C11 atomics, the
GCC and clang builtins, and the interlocked intrinsics of MSVC. A target with none of the four fails
the build at an `#error`. There is no plain fallback.

The 32-bit word is on each arm. The 64-bit word is on each arm but MSVC on a 32-bit target, where
`EMBED_ATOMIC_HAS_64` is 0. There, each use of `embed_atomic_u64` stops the compile of its file,
and the error names the cause. `embed_atomic_word` is 64 bits on a word of 64 bits, and 32 bits
below it.

### The directives

The compiler directives ask the compiler what it has, and they define both values of each answer.
`EMBED_HAS_ATTRIBUTE` and `EMBED_HAS_BUILTIN` go through `__has_attribute` and `__has_builtin`. Each
attribute wrapper, `EMBED_INLINE`, `EMBED_ALIGN`, `EMBED_RAW` and the rest, expands to the attribute
where the compiler has it and to nothing where it does not. `EMBED_STATIC_ASSERT` is `static_assert`
in C++ and C23, `_Static_assert` in C11, and `#error` below C11.

The platform directives read the target. `EMBED_BIG_ENDIAN` comes from `__BYTE_ORDER__`.
`EMBED_FAST_UNALIGNED_LOAD` comes from `__ARM_FEATURE_UNALIGNED`. `EMBED_VECTOR_REGISTER_BITS` is
512, 256, 128 or 0, from the instruction set macros. `EMBED_HOST_DEVICE` is `__host__ __device__`
under nvcc and nothing on another compiler.

Under nvcc, one definition of each atomic and lane function compiles into the host pass and the
device pass. A kernel calls the same function that a host walk calls.

### The widths

The target sets three widths, and a build can set each one on the command line.

| width                  | macro              | source                                                                                         |
| ---------------------- | ------------------ | ---------------------------------------------------------------------------------------------- |
| the word, `embed_word` | `EMBED_WORD_BITS`  | `UINTPTR_MAX`: 64, 32 or 16, and `#error` for any other value                                   |
| the index, `embed_index` | `EMBED_INDEX_BITS` | 16 where the word is 16 bits, and 32 above it                                                |
| the lane, `embed_lane` | `EMBED_LANE_BITS`  | the widest vector register: 512 under AVX-512, 256 under AVX2 and on a CUDA device, 128 under SSE2 or NEON; 64 or 32 where the target has no vector unit |

The word is the register that each count, mask and carry runs in. The index is for offsets and
lengths, which do not need the full register. The lane is the unit of a bulk pass over memory: a
copy, a fill, a compare, a blend of pixels. A build for a wider register widens each pass, with no
edit to the code.

Define `EMBED_WORD_BITS`, `EMBED_INDEX_BITS` or `EMBED_LANE_BITS` on the command line to set a
width yourself. Set the word where the register width differs from the pointer width, as on the x32
ABI. Assertions hold each width to the values that the header supports.
[The word and the lane](docs/word.md) tells how each width comes from the target, and how the lane
picks its arm. [The ABI](docs/abi.md) states the types, the host hooks and the stamp. It also
states each value that follows the width of the word, and each macro that a build can set.

## A first use

```c
#include "embed_types.h"
#include "embed_dispatch_layout.h"

static embed_u32 crc;
static embed_word accumulator;
static embed_index offset;

static embed_word at_odd_offset(const embed_u8 *bytes)
{
    return *(const embed_raw_word *)(const void *)(bytes + 1);
}

typedef struct
{
    embed_index (*length)(const embed_u8 *at);
    embed_bool (*is_empty)(const embed_u8 *at);
} ScanNs;
EMBED_TABLE_LAYOUT(ScanNs, length, is_empty);
```

`embed_raw_word` is `embed_word` with `EMBED_RAW` on it. That attribute lowers the alignment to 1
and permits access through an alias. C does not define a read through the plain `embed_word` at an
address that is not a multiple of the word width.

`EMBED_TABLE_LAYOUT` emits one static assertion for the byte offset of each member, and one for
`sizeof(ScanNs)`. The build fails where the list leaves out a member, or where the compiler puts
bytes between two members.

## Use in a build

```cmake
add_subdirectory(embedded_types)
target_link_libraries(your_target PRIVATE embedded_types::embedded_types)
```

`embedded_types::embedded_types` is an INTERFACE target. It gives the include directory and
`c_std_11`, and no compile options. The warning settings of the consumer apply to each translation
unit that includes these headers. PlatformIO reads `library.json`, and the Arduino IDE reads
`library.properties`.

## Build and test

```sh
cmake -S . -B build -DEMBEDDED_TYPES_WERROR=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`test/harness.py` runs the same builds, with the flags of each tree.

```sh
python test/harness.py test                     # the library as it ships
python test/harness.py test --tree build-werror # the same, with each warning an error
python test/harness.py test --tree build-tsan   # the same, under the thread sanitizer
python test/harness.py test --tree build-asan   # the same, under the address and undefined behavior sanitizers
python test/harness.py coverage --gaps          # the coverage of include/, and each line and branch that no suite reached
python test/harness.py suites --strict          # each case registered, none behind a conditional
python test/harness.py prove                    # each GenMC client under each memory model
python test/harness.py ste                      # each document held to ASD-STE100
python test/harness.py site                     # the site, into build/site
```

The configure step fetches [Unity](https://github.com/ThrowTheSwitch/Unity) at `v2.6.1`. The runner
generator of each suite needs `ruby` on `PATH`. CMake configures `test/` only where
`EMBEDDED_TYPES_BUILD_TESTS` is on, and that option is on by default where this project is the
top-level project.

Eight suites run, one for each header and one for the device arm of the lane. Where the configure
step finds a CUDA compiler, the device suite compiles its kernels and runs them on the part that the
host has. Where it finds none, the build leaves that suite out and says so.

`coverage` builds `build-cov` with GCC and gcov, runs the suites, and writes
`build-cov/coverage.xml` with gcovr. On Windows, run it under WSL. The suites reach each line and
each branch of `include/` that the build compiles.

Four workflows run on each push. They run the suites on GCC, Clang and MSVC, the Lean and GenMC
proofs, the site, and the SonarCloud analysis with the coverage. A line or a branch that no suite
reaches fails the analysis job.

## The documents

The pages under [`docs/`](docs) and the three documents at the root are the hand-written documents.
`tools/api_pages.py` writes one page under `docs/api` for each header, from its Doxygen comments.
After an edit to a header, run the tool and commit the page with the header. `test/ste_check.py`
holds each document to ASD-STE100, in the suite and in the pre-commit hook. Turn the hook on one
time in each clone:

```sh
git config core.hooksPath .githooks
```

## Files

| path                                                 | contents                                                                             |
| ---------------------------------------------------- | ------------------------------------------------------------------------------------ |
| [`include/`](include)                                | the seven headers                                                                    |
| [`cmake/`](cmake)                                    | `embedded_types_add_suite()` and `embedded_types_add_device_suite()`                 |
| [`test/unit/`](test/unit)                            | one Unity suite for each header, and one for the device arm of the lane              |
| [`test/support/`](test/support)                      | host threads for the contention cases, the host's sleep on the address of a word, and the inline policy of the coverage build |
| [`test/prove/`](test/prove)                          | the GenMC clients of the turnstile, and the host that they run on                    |
| [`test/harness.py`](test/harness.py)                 | suite discovery, Unity runner generation, the build trees, the coverage, the proofs, and the site |
| [`theory/`](theory)                                  | the Lean proofs: the widths, the atomics, the lane, the manifold of the turnstile, the ticket ring, the flag matrix, the claim of a slot, the device arm and the integrated algorithm; and the research paper on the two |
| [`docs/`](docs)                                      | the site: the hand-written pages, and the API pages from the headers                 |
| [`tools/api_pages.py`](tools/api_pages.py)           | the tool that writes the API pages                                                   |
| [`LICENSES/`](LICENSES)                              | the AGPL text and the two LicenseRef documents                                       |

## Licensing

**This library is free to use under AGPL-3.0-or-later, and always will be.** No registration, no
key, no gate.

Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is one of exactly
two things:

- **A commercial contract**, negotiated and signed. Where the AGPL's terms do not suit a product,
  terms are negotiable. Write to <dquigg123@gmail.com>.
  See [`LICENSES/LicenseRef-Commercial.txt`](LICENSES/LicenseRef-Commercial.txt).

- **An educator's license**, issued to you personally. Write from your faculty, research or teaching
  address, ordinarily `.edu` or `.org`, and tell what you intend to do with it. Granted case by case.
  See [`LICENSES/LicenseRef-Educational.txt`](LICENSES/LicenseRef-Educational.txt).

[Licensing](docs/licensing.md) states the terms of the educator's license. If you do not intend to
return value to your institution, take the AGPL. It costs nothing and asks none of this.

See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
