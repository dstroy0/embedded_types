# Security

This document tells what a build of embedded_types refuses to compile, what it compiles with no
diagnostic, and where to send a report.

## Attack surface

embedded_types declares types, defines inline functions and expands macros. The types header, the
directives header and the dispatch header define no function and reserve no object. A translation
unit that includes them gains no code and no storage. `EMBED_ENTRY` and `EMBED_ENTRY_V` emit a
function definition, and that definition belongs to the translation unit that expands them.

The atomics header, the turnstile header, the ticket ring header and the lane header define inline
functions over their types and no object. Each function reads and writes only what its arguments
name. A lane load or store reaches the `EMBED_LANE_BYTES` bytes at the address given and no byte
past them. A turnstile function reaches the words of the set that its argument names. A ring push
or pop reaches one slot of the storage that the caller gave the ring.

## What fails the build

### A wrong width

A static assertion pins each unsigned alias. A target where `embed_u32` is not 32 bits fails to
compile, and the message names the type.

### A machine word with no source

`EMBED_WORD_BITS` comes from `UINTPTR_MAX`. Where that macro is absent, or where it names a width
other than 16, 32 or 64, the header raises `#error`. There is no fallback width.

### A packed-enum attribute that the compiler ignores

A compiler can accept `__attribute__((packed))` and ignore it, and no `#if` detects that.
`EmbedEnumProbe` has a one-byte range, and an assertion holds its size to one byte. A build that
ignores the attribute fails to compile.

### A lane of an impossible width

`EMBED_LANE_BITS` comes from the widest vector register of the target, or from the word where it has
none, and a build can define it instead. Assertions in `embed_lane.h` refuse a width that is not
whole 32-bit lanes, or below 32 bits, or above 512 bits, or not whole words. One more refuses a lane
type whose size is not `EMBED_LANE_BYTES`.

### A target with no atomics

The atomics header selects one of four arms: the CUDA device atomics, C11 atomics, the GCC and clang
builtins, or the MSVC intrinsics. A target with none of the four fails the build at an `#error`.
There is no plain fallback. A word that is not atomic is a defect that shows only under contention.

### A host with no sleep on the address of a word

The turnstile calls `embed_word_wait`, `embed_word_wake` and `embed_clock_ns`, and the host defines
the three. A build that defines none of them fails at the link, and the error names them.

### A dialect below C11

No revision below C11 has a static assertion. With no guard, `EMBED_STATIC_ASSERT` parses as a
function declaration with an implicit `int`, and each assertion in each consumer compiles to
nothing. The header raises `#error` below C11.

## What has no check

### Unaligned access without the attributes

`EMBED_RAW` lowers the alignment to 1 and permits an alias, and a raw read needs both. Where the
compiler supports neither, both expand to nothing. `embed_raw_word` then has the natural alignment
of `embed_word`, and C does not define a read through it at an unaligned address. Nothing diagnoses
this, and the `@warning` on the typedef states it.

The other attribute wrappers cost speed where they expand to nothing. These two cost correctness.

### A lane read at any address without the attributes

`embed_lane_raw` is the lane with `EMBED_RAW` on it, and `embed_lane_load` and `embed_lane_put` read
and write through it. Where the compiler supports neither attribute, the type has the alignment of
the lane, `EMBED_LANE_BYTES`. C does not define a load or a store at an address that is not a
multiple of it, on either arm. Nothing diagnoses this. The aligned pair, `embed_lane_load_aligned`
and `embed_lane_put_aligned`, has the same limit on each compiler, and its `@warning` states it.

### The dispatch table

`EMBED_TABLE_LAYOUT` asserts the byte offsets and `sizeof`. The offsets are properties of the struct
type, and an initializer that names the wrong function changes none of them. A table can pass each
assertion in that expansion and still dispatch to the wrong function. The case
`test_each_member_reaches_the_function_it_names` in the dispatch suite covers that.

### A plain store beside the atomics

The headers hold no state of their own. A consumer builds shared state on the atomic words. Each
operation is one indivisible step, a load has acquire ordering, a store has release ordering, and
each read-modify-write has both. A consumer builds a claim or a boundary over the words with
compare-and-swap. That claim is correct only where each writer of the word goes through the header.
Nothing diagnoses a plain store beside the atomics.

### A reader that leaves no turnstile

A writer that drains waits until each pass that holds a lane at the drain ends. A reader that enters
a turnstile and never leaves it holds that lane, and the drain waits for it without end. The
turnstile has no timeout on a drain and no owner on a lane. Nothing diagnoses this.

### A job pointer in a slot

Every transition of a slot is one compare-and-swap on its stamp word. A corrupted stamp word fails
the next transition, and a corrupted input stamp fails every transition. The job pointer beside the
stamp is outside that compare. A corrupted job pointer reaches the caller of a pop, and the job must
carry its own check. Nothing in the ring diagnoses it.

### A grantor that exceeds the window

The grantor keeps the number of live tickets below the slot count, and the epoch tick period longer
than the lifetime of a stamp. A grant beyond the window finds a word in the middle of its sequence
and fails, so the fault surfaces at the grantor. A stamp held past the epoch wrap can match a later
stamp with the same ticket and epoch. The ring does not diagnose that hold.

### The run time

There are no bounds checks on a word or a lane, because there is no run-time code around them.

### The device arm

`embedded_types_add_device_suite` compiles the kernels of a suite with the CUDA compiler that the
build finds, and it runs them on the part that the host has. Where the build finds no compiler, it
leaves the suite out with a message. Where it finds the compiler and no part, each case says that it
measures nothing. Neither is a failure. A build with no device arm holds the host arm alone.

## Harden the build

Build with `-DEMBEDDED_TYPES_WERROR=ON`, in CI and on each desk. That adds `-Werror` to the flags
that this project puts on its own suite executables.

Keep `-Wconversion` and `-Wsign-conversion` on in the consumer build. Those two report a narrower
type at the call site where it happens, and nothing in this library turns them on for you.

Run the thread sanitizer tree, `python test/harness.py test --tree build-tsan`, over each change to
the atomics or the turnstile.

## Report

Open a private security advisory at
<https://github.com/dstroy0/embedded_types/security/advisories/new>, or write to
dquigg123@gmail.com. Include the compiler and the target, the value of `EMBED_WORD_BITS` that the
build derives, and the smallest translation unit that shows the behavior.

Fixes land on `main`.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
