# embedded_types

**Purpose:** One place holds the fixed-width aliases, the machine word, and the compile-time checks
that a C11 embedded build needs. Each library in the build takes them from here, and none declares
them again.
**Scope:** `include/embed_types.h`, `include/embed_compiler_directives.h`,
`include/embed_dispatch_layout.h`, `include/embed_atomic.h`, `include/embed_turnstile.h`,
`include/embed_lane.h`, `cmake/EmbeddedTypesSuite.cmake`, `test/harness.py`, `test/support/`,
`test/unit/`, `test/prove/`, `theory/`

Six C11 headers hold the fixed-width integer aliases, the machine word, the compile-time checks
around them, atomic words, turnstiles, and the lane. The word comes from the width of the target's
pointer. The lane is the widest vector register of the target, and a bulk pass over memory moves and
computes in it. The turnstile is an atomic thread scheduler. The library is headers only.

The first three headers include only `stddef.h` and `stdint.h`. The atomics add `stdatomic.h` on
their C11 arm and `intrin.h` under MSVC, and nothing on their CUDA device arm. The lane includes
`embed_types.h` alone. Each entry of the atomics and the lane has `EMBED_HOST_DEVICE`. Under nvcc,
one definition compiles into the host pass and the device pass, and a kernel calls the same function
that a host walk calls. [SECURITY.md](SECURITY.md) tells what the headers guarantee, and what they do
not diagnose.

The site at <https://dstroy0.github.io/embedded_types/> shows the API from the Doxygen comments in
the headers.

A consumer includes the headers and writes against the names directly.

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
and permits access through an alias. C leaves a read through the plain `embed_word` undefined where
the address is not a multiple of the word width. `EMBED_TABLE_LAYOUT` emits one static assertion for
the byte offset of each member, and one for `sizeof(ScanNs)`. The build fails where the list leaves
out a member, or where the compiler puts bytes between two members.

## Files

| path                                                                         | contents                                                                                        |
| ---------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- |
| [`include/embed_types.h`](include/embed_types.h)                             | the widths, the boolean, the word, the index, the raw word, the assertions                      |
| [`include/embed_compiler_directives.h`](include/embed_compiler_directives.h) | feature checks, the static assertion, attribute wrappers, diagnostic pragmas, guards            |
| [`include/embed_dispatch_layout.h`](include/embed_dispatch_layout.h)         | `EMBED_TABLE_LAYOUT`, `EMBED_CALL`, `EMBED_ENTRY`, and the macros behind them                   |
| [`include/embed_atomic.h`](include/embed_atomic.h)                           | 32-bit and 64-bit atomic words for host, part and GPU threads, on an arm that the target selects |
| [`include/embed_turnstile.h`](include/embed_turnstile.h)                     | an atomic thread scheduler: turnstiles that readers pass through and that a writer drains, with lanes that open and close with the queue, and the sleep on the address of a word that the host defines |
| [`include/embed_lane.h`](include/embed_lane.h)                               | the lane beside the word, the widest register, its loads, fills, arithmetic and masks           |
| [`test/support/`](test/support)                                              | host threads for the contention cases and the host's address wait, out of the sight of the runners |
| [`test/unit/`](test/unit)                                                    | one Unity suite for each header, and one for the device arm of the lane, each its own CTest target |
| [`test/prove/`](test/prove)                                                  | the GenMC clients of the turnstile, and the host that they run on                               |
| [`test/harness.py`](test/harness.py)                                         | suite discovery, Unity runner generation, the build trees, the proofs, and the site             |
| [`theory/`](theory)                                                          | the Lean proofs of the turnstile                                                                |
| [`cmake/`](cmake)                                                            | `embedded_types_add_suite()` and `embedded_types_add_device_suite()`                            |
| [`LICENSES/`](LICENSES)                                                      | the AGPL text and the two LicenseRef documents                                                  |

A suite directory gives its name to `embedded_types_add_suite()`, and that function registers it as
its own CTest target. The harness knows four build trees: `build`, `build-werror`, `build-tsan` and
`build-asan`.

## Symbols

The table indexes what each name is. The contract for a name is the Doxygen block on its
declaration. That block states each guarantee and each warning one time.

| symbol                                                                                                   | definition                                                                                                                     |
| -------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------ |
| `embed_u8` … `embed_u64`, `embed_i8` … `embed_i64`                                                       | `uint8_t` … `int64_t`; an assertion pins each unsigned width                                                                   |
| `embed_bool`, `EMBED_TRUE`, `EMBED_FALSE`                                                                | `_Bool` in C, `bool` in C++; a cast puts the two constants in that type                                                        |
| `embed_word`, `embed_iword`                                                                              | the unsigned and the signed integer of `EMBED_WORD_BITS` bits                                                                  |
| `embed_index`                                                                                            | the unsigned integer of `EMBED_INDEX_BITS` bits, for offsets                                                                   |
| `embed_raw_word`                                                                                         | `embed_word` with alignment 1 and access through an alias                                                                      |
| `EmbedEnumProbe`                                                                                         | a one-byte enum; an assertion on its size proves that `EMBED_ENUM_BYTE` gave one byte                                          |
| `EMBED_STATIC_ASSERT`                                                                                    | `static_assert` in C++ and C23, `_Static_assert` in C11, `#error` below C11                                                    |
| `EMBED_BEGIN_DECLS`, `EMBED_END_DECLS`                                                                   | `extern "C" {` and `}` under `__cplusplus`, empty without it                                                                   |
| `EMBED_GNU_ATTRIBUTES`                                                                                   | 1 where `__GNUC__` or `__clang__` is defined, 0 otherwise                                                                      |
| `EMBED_HAS_ATTRIBUTE`                                                                                    | `__has_attribute`, or `EMBED_GNU_ATTRIBUTES` where the compiler has none                                                       |
| `EMBED_HAS_BUILTIN`                                                                                      | `__has_builtin`, or 0 where the compiler has none                                                                              |
| `EMBED_ENUM_BYTE`                                                                                        | a one-byte enum: the enum itself where the compiler can pin it to a byte, its constants and an `unsigned char` where it cannot |
| `EMBED_INLINE`, `EMBED_FLATTEN`, `EMBED_ALIGN`, `EMBED_ALIAS`, `EMBED_RAW`, `EMBED_UNUSED`, `EMBED_WEAK` | the attribute where the compiler has it, empty where not                                                                       |
| `EMBED_WEAK_NAME`, `EMBED_WEAK_DEFAULT`                                                                  | the name of a weak default, and the alternate name of the linker for it under MSVC                                             |
| `EMBED_HAS_VECTOR_SIZE`, `EMBED_VECTOR`                                                                  | 1 where the compiler has the `vector_size` attribute, 0 otherwise; and the attribute at a width in bytes, empty where not      |
| `EMBED_VECTOR_REGISTER_BITS`                                                                             | the widest vector register that the target compiles for: 512, 256, 128, or 0 where it has none                                 |
| `EMBED_HOST_DEVICE`                                                                                      | `__host__ __device__` under nvcc, empty on other compilers                                                                     |
| `embed_atomic_u32`, `embed_atomic_u64`, `embed_atomic_word`, `embed_atomic_usize`, `embed_atomic_uptr`, `EMBED_ATOMIC_INIT`, `EMBED_ATOMIC_ARM` | the two atomic words; the one as wide as the machine word, as `size_t` and as a pointer; a declared value; and the arm that the target takes |
| `embed_atomic_u32_load`, `_store`, `_exchange`, `_swap_if`, `_fetch_add`, `_fetch_sub`, `_fetch_or`, `_fetch_and`, `_raise`, and the same for `u64` and `word` | one indivisible step each: acquire load, release store, swap, compare-and-swap, the arithmetic and the masks that answer the word before, and a raise to a value where it is higher |
| `embed_atomic_ordered`                                                                                   | the fence that orders the earlier writes of this context ahead of its later reads                                              |
| `EmbedTurnstiles`, `embed_turnstile_enter`, `embed_turnstile_leave`, `embed_turnstiles_drain`, `embed_turnstile_lanes`, `EMBED_TURNSTILE_LANES` | a manifold of lanes, one lane open at rest; an entry at the lane of the worker's address, which spills to the next free lane and grows the manifold where each lane is busy; the end of the pass, which contracts the manifold as the pressure goes; a wait for every pass that began before the call; the lanes open; and the most lanes |
| `EmbedTurnstileBackoff`, `embed_turnstile_backoff_begin`, `embed_turnstile_backed_off`, `EMBED_TURNSTILE_UNIT_SHIFT` | the start and the exponent of a backoff, in the caller's locals; one step, a pause inside the first unit and a wait that doubles to 2^20 units after it; and the unit, 2^10 ns on a CPU host and 1 ns on a device |
| `embed_word_wait`, `embed_word_wake`, `embed_clock_ns`, `EMBED_WAIT_FOREVER`                             | the sleep on the address of a word while it reads a value, for a time in nanoseconds; the wake of every sleeper; and the clock in nanoseconds; the host defines each on its futex, WaitOnAddress or `__ulock_wait` |
| `EMBED_HAS_BYTE_MASK_256`, `EMBED_BYTE_MASK_256`, `EMBED_HAS_BYTE_MASK_128`, `EMBED_BYTE_MASK_128`       | 1 where the compiler has the builtin that reads the sign bits of 32 or 16 bytes into an int, 0 otherwise; and the builtin, 0 where not |
| `embed_lane`, `embed_lane_raw`, `embed_lane_mask`, `EMBED_LANE_MASK_ALL`, `embed_lane_mask_first`       | the 32-bit lanes of the lane in the vector type of the compiler or an aligned struct; the same at any address; one bit a byte; every byte; the first set bit |
| `EMBED_LANE_BITS`, `EMBED_LANE_BYTES`, `EMBED_LANE_U32S`, `EMBED_LANE_BITS_WAS_OVERRIDDEN`               | the width of the lane in bits, derived or defined by the build; in bytes; in 32-bit lanes; and which of the two                |
| `EMBED_LANE_VECTOR`, `EMBED_LANE_PLAIN`, `EMBED_LANE_PORTABLE`                                           | the arm that the header takes, one of the two 1; and the switch of the build that forces the struct arm                        |
| `embed_lane_load`, `embed_lane_put`, `embed_lane_fill_byte`, `embed_lane_fill_u32`                       | a lane from and to any address, an aligned pair beside each; and a lane of one byte or one 32-bit value                        |
| `embed_lane_and`, `embed_lane_or`, `embed_lane_xor`, `embed_lane_add_u32`, `embed_lane_sub_u32`, `embed_lane_mul_u32`, `embed_lane_shift_right_u32`, `embed_lane_shift_left_u32` | the logic over every bit and the arithmetic over each 32-bit lane, with wrap around as the scalar has |
| `embed_lane_bytes_equal`, `embed_lane_bytes_zero`                                                        | the byte mask of equal bytes between two lanes, and of the zero bytes of a lane                                                |
| `EMBED_DIAGNOSTIC_PUSH`, `EMBED_DIAGNOSTIC_POP`, `EMBED_DIAGNOSTIC_IGNORE`, `EMBED_DIAGNOSTIC_STRING`    | the clang or the GCC pragma, empty on a compiler with neither                                                                  |
| `EMBED_CAT`, `EMBED_NARG`, `EMBED_ARG_N`                                                                 | two-step token paste, and argument count from 1 to 24                                                                          |
| `EMBED_CALL`                                                                                             | `entry_(&(ArgsType_){__VA_ARGS__})`, where `entry_` receives the address of the literal [BORROWS]                              |
| `EMBED_ENTRY`, `EMBED_ENTRY_V`                                                                           | an entry point that returns a value and one that returns void, both on `EMBED_CALL`; their initializers dereference `args` [BORROWS] |
| `EMBED_TABLE_SLOT`                                                                                       | one offset assertion, for one member at its offset                                                                             |
| `EMBED_TABLE_LAYOUT`, `EMBED_TABLE_STORAGE`                                                              | the offset and size assertions, and `static const`                                                                             |
| `EMBED_FUNCTION_POINTER_BYTES`                                                                           | `sizeof(void (*)(void))`, the stride of one dispatch member                                                                    |
| `EMBED_BIG_ENDIAN`                                                                                       | 1 where `__BYTE_ORDER__` equals `__ORDER_BIG_ENDIAN__`, 0 otherwise                                                            |
| `EMBED_FAST_UNALIGNED_LOAD`                                                                              | 1 where `__ARM_FEATURE_UNALIGNED` is defined, 0 otherwise                                                                      |

`EMBED_STATIC_ASSERT` is `static_assert` under `__cplusplus` or C23, and `_Static_assert` under C11.
Below C11 it raises `#error`. `EMBED_BIG_ENDIAN` comes from `__BYTE_ORDER__`, and
`EMBED_FAST_UNALIGNED_LOAD` comes from `__ARM_FEATURE_UNALIGNED`. Each falls to 0 where its macro is
absent.

## EMBED_WORD_BITS

The width of the word comes from `UINTPTR_MAX`, which the compiler supplies.

| `UINTPTR_MAX`        | `EMBED_WORD_BITS` |
| -------------------- | ----------------- |
| `0xFFFFFFFFFFFFFFFF` | 64                |
| `0xFFFFFFFF`         | 32                |
| `0xFFFF`             | 16                |
| absent, or any other | `#error`          |

Define `EMBED_WORD_BITS` on the command line to set the width yourself. The header then sets
`EMBED_WORD_BITS_WAS_OVERRIDDEN` to 1. Where the header derives the width, the flag is 0. Set the
width where the register width differs from the pointer width. The x32 and ILP32-on-64 ABIs are
examples. A large-model eight-bit target is another: its pointer is wider than the register that
indexes with it.

## EMBED_INDEX_BITS

An offset does not need the full width of the register, so the index has its own width.
`EMBED_INDEX_BITS` is 16 where `EMBED_WORD_BITS` is 16, and 32 above it. For 16-bit offsets on a
wider register, define `EMBED_INDEX_BITS` ahead of the header. An assertion stops an override wider
than the word.

## The lane

The word is the register, and each count, mask and carry runs in it. The lane is the widest vector
register of the target. Its width comes from the instruction set macros of the compiler, as the
width of the word comes from `UINTPTR_MAX`. A bulk pass over memory steps one lane at a time: a
copy, a fill, a compare, a blend of pixels. A build for a wider register widens each pass, with no
edit to the code.

| target                                | `EMBED_LANE_BITS`                           |
| ------------------------------------- | ------------------------------------------- |
| a CUDA device                         | 256, the eight 32-bit registers of a thread |
| AVX-512                               | 512                                         |
| AVX2                                  | 256                                         |
| SSE2, NEON                            | 128                                         |
| no vector unit, 64-bit word           | 64                                          |
| no vector unit, 32-bit or 16-bit word | 32                                          |

Define `EMBED_LANE_BITS` on the command line to set the width yourself. The header then sets
`EMBED_LANE_BITS_WAS_OVERRIDDEN` to 1, and to 0 where it derives the width. Assertions hold each
width to whole 32-bit lanes and whole words, and to 512 bits at most. The byte mask has room for
512 bits.

| arm      | `embed_lane`                                                   | the arm the header takes                                                    |
| -------- | -------------------------------------------------------------- | --------------------------------------------------------------------------- |
| vector   | `embed_u32` with `EMBED_VECTOR(EMBED_LANE_BYTES)`              | `EMBED_HAS_VECTOR_SIZE` is 1, `EMBED_LANE_PORTABLE` is 0, not a CUDA device |
| portable | a struct of `EMBED_LANE_U32S` `embed_u32`, aligned to the lane | each other case, a CUDA device among them                                   |

Both arms define the same operations. The vector arm uses the plain operators over the vector type.
The portable arm uses loops, which the compiler makes into vector code where it can. A load or a
store at any address goes through `embed_lane_raw`, the lane with `EMBED_RAW`, as `embed_raw_word`
is the word.

The byte masks use the mask builtin of the compiler where the directives header finds one for the
width of the lane. That is `EMBED_BYTE_MASK_256` under AVX2 and `EMBED_BYTE_MASK_128` under SSE2,
one instruction each. On other targets, one multiply over the full lane collects the bits. The suite
builds the vector arm at the derived width by default, and the portable arm with
`-DEMBED_LANE_PORTABLE=1`.

## Building

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
python test/harness.py suites --strict          # each case registered, none behind a conditional
python test/harness.py prove                    # each GenMC client under each memory model
python test/harness.py ste                      # each document held to ASD-STE100
python test/harness.py site                     # the site, into build/site
```

The configure step fetches [Unity](https://github.com/ThrowTheSwitch/Unity) at `v2.6.1`. The runner
generator of each suite needs `ruby` on `PATH`. Both belong to `test/`, and CMake configures `test/`
only where `EMBEDDED_TYPES_BUILD_TESTS` is on. That option is on by default where this project is
the top-level project.

## Consuming

```cmake
add_subdirectory(embedded_types)
target_link_libraries(your_target PRIVATE embedded_types::embedded_types)
```

`embedded_types::embedded_types` is an INTERFACE target. It gives the include directory and
`c_std_11`, and no compile options. The warning settings of the consumer apply to each translation
unit that includes these headers. The suite executables in this project compile with
`EMBEDDED_TYPES_TEST_WARNINGS`. PlatformIO reads `library.json`, and the Arduino IDE reads
`library.properties`.

## Status

Version 1.0.6. Seven suites run, one for each header and one for the device arm of the lane. Each
build tree passes with each warning an error.

`EMBEDDED_TYPES_TEST_WARNINGS` is `EMBEDDED_TYPES_WARNINGS` without `-Wconversion`,
`-Wsign-conversion` and `-Wmissing-prototypes`. The assertion macros of Unity widen and compare
inside the macro body, and a Unity case is a file-scope definition with no declaration.

## Licensing

**This library is free to use under AGPL-3.0-or-later, and always will be.** No registration, no
key, no gate.

Every use falls under AGPL-3.0-or-later **unless** you hold explicit permission, which is one of
exactly two things:

- **A commercial contract**, negotiated and signed. Where the AGPL's terms do not suit a product,
  terms are negotiable. Write to <dquigg123@gmail.com>.
  See [`LICENSES/LicenseRef-Commercial.txt`](LICENSES/LicenseRef-Commercial.txt).

- **An educator's license**, issued to you personally. Write from your faculty, research or teaching
  address, ordinarily `.edu` or `.org`, describing what you intend to do with it. Granted case by
  case. See [`LICENSES/LicenseRef-Educational.txt`](LICENSES/LicenseRef-Educational.txt).

The educator's license exists so that value returns to your institution and your department, and the
environment that produced the work is enriched by what the work becomes. That is its whole purpose,
and it is a term of the license. Credit the underlying systems and the students and faculty who did
the work, and if the work becomes a product, a royalty ladder applies with a share to the
institution at minimum and to the department where its rules allow
(`LICENSES/LicenseRef-Educational.txt:71`, `:137-150`).

It is issued to a person, and it does not extend to a colleague, a student, a collaborator, a
department, a laboratory or an institution (`LICENSES/LicenseRef-Educational.txt:34`, `:76`). Each
person applies for their own, there is no grandfathering, and it is revocable at will. Conduct
undertaken to defeat its purpose ends it immediately, with no notice period and no opportunity to
cure (`LICENSES/LicenseRef-Educational.txt:81`, `:170-179`).

If you do not intend to return value to your institution, take the AGPL. It costs nothing and asks
none of this.

See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
