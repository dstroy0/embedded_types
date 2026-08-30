# embedded_types

The exact widths, a machine word, and the compile-time machinery a C11 embedded library is written
in. Header only. Nothing is allocated, nothing runs before `main`, and the only headers it reaches
for are `stddef.h` and `stdint.h`.

It exists because three libraries had three copies of it. Each carried its own fixed-width aliases,
its own machine word, its own static-assert spelling, its own attribute wrappers and its own
dispatch-layout assertions, under three prefixes — and two of the three had already drifted from the
third. Every name here is defined once. A consumer aliases them into its own vocabulary rather than
declaring its own, so two libraries in one build agree on what a word is instead of each deciding
separately.

```c
#include "embed_types.h"
#include "embed_dispatch_layout.h"

/* The widths are proved, not assumed. A target where any of them is wrong fails the build. */
static embed_u32 crc;
static embed_word accumulator;
static embed_index offset;

/* A word read from an address that is not a multiple of its width. EMBED_RAW is what makes the
   read defined; the plain embed_word would not be. */
static embed_word at_odd_offset(const embed_u8 *bytes)
{
    return *(const embed_raw_word *)(const void *)(bytes + 1);
}

/* A dispatch table whose members are pinned to consecutive slots at compile time. A member added
   and left out of the list, or padding appearing between two of them, fails the build. */
typedef struct
{
    embed_index (*length)(const embed_u8 *at);
    embed_bool (*is_empty)(const embed_u8 *at);
} ScanNs;
EMBED_TABLE_LAYOUT(ScanNs, length, is_empty);
```

## Where things are

|                                                                                  |                                                                                     |
| -------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------- |
| [`include/embed_types.h`](include/embed_types.h)                                 | the widths, the boolean, the word, the index, the raw-access word, the assertions   |
| [`include/embed_compiler_directives.h`](include/embed_compiler_directives.h)     | feature probes, the static assertion, attribute wrappers, the call shape, the guards |
| [`include/embed_dispatch_layout.h`](include/embed_dispatch_layout.h)             | the assertions that pin a table of function pointers to consecutive slots           |
| [`test/`](test)                                                                  | one suite per header, under CTest, with no test-framework dependency                |
| [`LICENSES/`](LICENSES)                                                          | the AGPL text, and the two licenses the SPDX expression names                       |

## What is in it

| | |
| ----------------------------------------------------- | ------------------------------------------------------------------------ |
| `embed_u8` … `embed_u64`, `embed_i8` … `embed_i64`    | the exact widths, each pinned by an assertion                            |
| `embed_bool`, `EMBED_TRUE`, `EMBED_FALSE`             | a truth value that normalizes any nonzero to one                         |
| `embed_word`, `embed_iword`                           | the machine word, derived from the target rather than stated             |
| `embed_index`                                         | an offset or a length, never `size_t`                                    |
| `embed_raw_word`                                      | the word, readable from any address                                      |
| `EMBED_STATIC_ASSERT`                                 | one spelling, correct in C11 and C23, reaching no header                 |
| `EMBED_HAS_ATTRIBUTE`, `EMBED_HAS_BUILTIN`            | asks the compiler what it supports, rather than reading its identity     |
| `EMBED_INLINE`, `EMBED_FLATTEN`, `EMBED_ENUM_PACKED`, `EMBED_ALIGN`, `EMBED_ALIAS`, `EMBED_RAW`, `EMBED_UNUSED`, `EMBED_WEAK` | the attribute wrappers, each stating what its absence costs |
| `EMBED_CAT`, `EMBED_NARG`, `EMBED_ARG_N`              | token paste and argument count                                           |
| `EMBED_CALL`                                          | one pointer to a compound literal, rather than a long parameter list      |
| `EMBED_TABLE_LAYOUT`, `EMBED_TABLE_STORAGE`           | a dispatch table's slots, asserted at compile time                        |
| `EMBED_BIG_ENDIAN`, `EMBED_FAST_UNALIGNED_LOAD`       | what the compiler states about the target                                |

## The two things it refuses to guess

**The machine word.** `EMBED_WORD_BITS` is derived from `UINTPTR_MAX`, which is the compiler's own
statement about the target, so a build states nothing and cannot state it wrong. Where that macro is
absent, or names a width other than 16, 32 or 64, the build stops. Falling silently to the narrowest
width would halve every lane on a machine that has more, and nothing downstream would report it.

A build whose register width differs from its pointer width — x32, ILP32-on-64, a large-model
eight-bit part — defines `EMBED_WORD_BITS` on the command line and the derivation steps aside.

**The static assertion.** Before C11 neither spelling exists. Left to expand, `EMBED_STATIC_ASSERT`
would parse as a function declaration with an implicit `int`, and every assertion in every consumer
would check nothing while appearing to. That is an `#error`.

## Building it

```sh
cmake -S . -B build -DEMBEDDED_TYPES_WERROR=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Consuming it:

```cmake
add_subdirectory(embedded_types)
target_link_libraries(your_target PRIVATE embedded_types::embedded_types)
```

The alias carries the include directory and the C11 requirement. It carries no warning flags: the
diagnostics in `CMakeLists.txt` apply to this project's own targets, because a library that forces
its diagnostics onto a caller is deciding something that is not its own.

## Status

0.1.0, pre-1.0, so names may change. Two suites, green under
`-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-align -Wcast-qual
-Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wvla -Werror`.

## Licensing

**This library is free to use under AGPL-3.0-or-later, and always will be.** No registration, no
key, no gate. Take it and build.

Every use falls under AGPL-3.0-or-later **unless** you hold explicit permission, which is one of
exactly two things:

- **A commercial contract**, negotiated and signed. Where the AGPL's terms do not suit a product,
  the copyright is held by one person, so this is a conversation rather than a policy question.
  Write to <dquigg123@gmail.com>. See [`LICENSES/LicenseRef-Commercial.txt`](LICENSES/LicenseRef-Commercial.txt).

- **An educator's license**, issued to you personally. Write from your faculty, research or teaching
  address — ordinarily `.edu` or `.org` — describing what you intend to do with it. Granted case by
  case. See [`LICENSES/LicenseRef-Educational.txt`](LICENSES/LicenseRef-Educational.txt).

The educator's license exists so that value returns to your institution and your department, and the
environment that produced the work is enriched by what the work becomes. That is its whole purpose,
and it is a term of the license rather than a sentiment: credit the underlying systems and the
students and faculty who did the work, and if the work becomes a product, a royalty ladder applies
with a share to the institution at minimum and to the department where its rules allow.

It is issued to a person, not a department. It does not extend to your colleagues, your students or
your lab — each person applies for their own, and there is no grandfathering. It is revocable at
will. Conduct undertaken to defeat its purpose ends it immediately, with no notice period and no
opportunity to cure.

If you do not intend to return value to your institution, take the AGPL. It costs nothing and asks
none of this.

See [CONTRIBUTING.md](CONTRIBUTING.md) and [SECURITY.md](SECURITY.md).
