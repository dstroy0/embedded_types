# Contributing {#proj_contributing}

**Purpose:** Add a suite, a case, or a declaration to embedded_types without tripping either of the
two shapes that make a Unity case silently never run.
**Scope:** `include/`, `test/unit/`, `test/harness.py`, `cmake/EmbeddedTypesSuite.cmake`,
`CMakeLists.txt`, `keywords.txt`, `.clang-format`

## Build and test

```sh
cmake -S . -B build -DEMBEDDED_TYPES_WERROR=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

There are three CTest targets, one per header under test (`test/unit/CMakeLists.txt:4-6`).
`EMBEDDED_TYPES_BUILD_TESTS` defaults to the value of `PROJECT_IS_TOP_LEVEL`, and a tree that
consumes this one configures no tests (`CMakeLists.txt:21`, `:34-37`).

`test/harness.py` carries each build tree's flags and adds a per-suite view of what Unity will
register.

```sh
python test/harness.py test                     # the library as it ships
python test/harness.py test --tree build-werror # the same, with every warning an error
python test/harness.py suites --strict          # every case registered, none behind a conditional
python test/harness.py cases test/unit/test_embed_types
```

The Unity dependency and the `ruby` the runner generator needs are in [README.md](README.md).
Neither reaches a consumer of the library.

## Adding a suite

A suite is a directory under `test/unit/` holding exactly one `.c` of cases and a `CMakeLists.txt`
that names it. `embedded_types_add_suite()` stops the configure with a `FATAL_ERROR` where the
matching `<suite_name>.c` is absent (`cmake/EmbeddedTypesSuite.cmake:9-12`).

```cmake
embedded_types_add_suite(test_embed_something)
```

Add one `add_subdirectory` line to `test/unit/CMakeLists.txt` and the CMake build picks the suite
up. The harness needs no such line, because it discovers a suite by walking `test/unit/` for a
directory holding a `.c` with a collectable case (`test/harness.py:151-161`).

## Adding a case

A case is `void test_<name>(void)` at file scope. Two shapes make a case silently never run, and
`harness.py suites --strict` reports both.

- **A definition the generator walks past.** Unity's generator matches the exact spelling
  `void test_<name>(void)`. A case spelled any other way is never registered, and the suite still
  reports a pass.
- **A case defined inside a preprocessor conditional.** The generator reads case names out of the
  source text and does not see the conditional. The runner declares and calls the case whichever way
  the conditional went, and where it went the other way the definition is absent and the suite fails
  to link. Put the `#if` inside the case body, and call `TEST_IGNORE_MESSAGE` on the arm that cannot
  measure anything.

`UNITY_CASE` and `NEAR_MISS` at `test/harness.py:90-91` are the spellings the report is built from,
and `ANY_IF`, `ANY_ELSE` and `ANY_ENDIF` at `test/harness.py:96-98` count the conditional depth.

## Where the proof lives

Most of this library is proved by compiling. The eight width assertions
(`include/embed_types.h:219-245`), the packed-enum probe (`include/embed_types.h:263-265`), and each
expansion of `EMBED_TABLE_LAYOUT` (`include/embed_dispatch_layout.h:645-648`) are static. A build
that reaches the link step has already checked them, and no case in `test/` repeats that work.

The suites under `test/unit/`, one file per header, cover what a static assertion does not reach:

- **Signedness.** A typedef pointing at the wrong signedness has the right size, and `sizeof` passes
  either way. Each case casts `-1` to the alias and tests the sign of the result.
- **Boolean normalization.** Any nonzero converts to 1. A plain eight-bit alias keeps the value it
  was given, and two true values can then compare unequal across an API boundary.
- **`EMBED_RAW`.** A word read from an odd address, checked against a `memcpy` of the same bytes.
- **Dispatch wiring.** Offsets are properties of the struct type, and an initializer naming the
  wrong function changes none of them. The suite calls through each member.
- **Generated entry points.** `EMBED_ENTRY` and `EMBED_ENTRY_V` emit a function that builds a
  compound literal from the caller's configuration and hands it to a backend. The suite calls both
  generated entries and checks what the backend received, including the context member the entry
  leaves out of its initializer.
- **The byte order.** `EMBED_BIG_ENDIAN` is derived from `__BYTE_ORDER__`. The case measures how the
  target lays a word out in memory. Reading the same macro back would compare the macro with itself.
- **The feature tests.** `EMBED_HAS_ATTRIBUTE` and `EMBED_HAS_BUILTIN` evaluate to zero for a name
  nothing defines. A non-zero result there would be non-zero for every name, and every attribute
  wrapper would be emitted on a compiler that rejects it.
- **The attribute wrappers.** A marked definition is still a definition and still computes what its
  body defines, and the alignment and packing attributes reach the type. Where the compiler cannot
  carry one, the case calls `TEST_IGNORE_MESSAGE`, and a case that measured nothing does not count
  as a pass.

## Never test the library with itself

The expected side of an assertion comes from a literal, from the compiler, or from exact arithmetic.
Never from the code under test.

A width checked against the macro that produced it compares the macro with itself and passes at any
value. `sizeof(embed_u32) == 4u` measures the type against a literal the standard fixed.
`sizeof(embed_u32) * 8u == 32u`, where the 32 comes from a macro derived from that same type,
measures the type against itself. Where a case needs an independent source, `uintptr_t` and `memcpy`
come from the compiler and the standard
(`test/unit/test_embed_types/test_embed_types.c:93`, `:159`).

## Formatting

```sh
clang-format -i include/*.h test/unit/*/test_*.c
npm run format
```

`unity_runner.c` is generated and is listed in `.clang-format-ignore` (`.clang-format-ignore:1`).
Formatting it by hand is undone on the next generation.

The column limit is 120 (`.clang-format:151`). `npm run format:check` reports without rewriting
(`package.json:17`). A formatter that rewrites during a check produces commits nobody reviewed and
races the author's own push, and the fix belongs in the working tree. This repository carries no CI
configuration, so the policy is not enforced on push by anything in the tree.

## Comments

Every header opens with the license banner and a `@file` block (`include/embed_types.h:1-18`,
`include/embed_compiler_directives.h:1-18`, `include/embed_dispatch_layout.h:1-19`). A block
documents exactly one declaration. A contiguous family of similar macros takes one block each,
because nearly identical is not identical. Where a conditional declares the same name on more than
one arm, the arm the compiler reaches first carries the block for all of them
(`include/embed_types.h:161`, `:190`).

A new header opens with the banner and the `@file` block.

```c
/* embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
 * SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
 *
 * Every use falls under AGPL-3.0-or-later unless you hold explicit permission, which is either a
 * negotiated commercial licensing contract or an educator's license issued to you personally.
 */
/**
 * @file embed_types.h
 * @brief What the translation unit is, in one sentence.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 * @date 2026-08-30
 */
```

A macro parameter takes a real name with a trailing underscore, and the block is written around it.
`@param[in] x` documents a defect. Rename the parameter first, and the line then has something to
say. Pad every `@param` description to the longest name, and pad `@return` to match.
`EMBED_HAS_ATTRIBUTE` shows the shape (`include/embed_compiler_directives.h:39-51`).

```c
/**
 * @brief Expands to __has_attribute(attribute_) where __has_attribute is defined.
 *
 * @param[in] attribute_ Attribute name, as passed to __has_attribute.
 * @return               The value __has_attribute gives for attribute_.
 * @note Every attribute macro below goes through this. Asking the compiler whether it supports an
 *       attribute is more reliable than checking which compiler it is.
 * @warning Expands to EMBED_GNU_ATTRIBUTES where __has_attribute is undefined, ignoring attribute_.
 *          Every attribute then gets the same answer. A compiler without __has_attribute gets all
 *          of them or none.
 */
#if defined(__has_attribute)
#define EMBED_HAS_ATTRIBUTE(attribute_) __has_attribute(attribute_)
```

Every attribute wrapper carries a `@warning` naming what its absence costs. Most cost speed when
they expand to nothing. `EMBED_ALIGN` and `EMBED_ALIAS` cost correctness. The `#if` does not
distinguish the two.

State the mechanism. These headers are shared, and a block naming what one library does with a
macro, or naming a part it was measured on, documents a relationship instead of the macro.

## What belongs here

The test is whether it is machinery or a domain fact.

Machinery is anything a library above would otherwise define for itself, such as a width, a word, an
attribute wrapper, a static assertion, a feature probe, or an argument count. Define it once, here.

A domain fact belongs to the library that has the opinion. A pool size, a protocol timer, a buffer
count. Those never come here, whatever their prefix looks like.

## Adding a declaration

1. Put it in the header it belongs to, in file order.
2. Write its Doxygen block as you write it, not afterwards.
3. If it can fail silently on some target, add the assertion that catches it or the `@warning` that
   names it.
4. If it is observable at run time, add a case to the suite that observes it independently.
5. Add its name to `keywords.txt`.

## Licensing

Contributions are accepted under the same terms as the project. There is no CLA. Opening a pull
request means you have the right to contribute the code and are doing so under AGPL-3.0-or-later.

Keep the banner on every new file. Both `LicenseRef-` identifiers in it resolve to files in
`LICENSES/` (`LICENSES/LicenseRef-Commercial.txt`, `LICENSES/LicenseRef-Educational.txt`), and a
scanner reports the expression as unresolved if one goes missing.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-09
