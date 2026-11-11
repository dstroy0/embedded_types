# Contributing

This document tells how to add a suite, a case or a declaration to embedded_types. It also names the
two shapes of a Unity case that never runs, and the suite still passes.

## Build and test

```sh
cmake -S . -B build -DEMBEDDED_TYPES_WERROR=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Each suite under `test/unit/` is one CTest target, and the ASD-STE100 check is one more.
`EMBEDDED_TYPES_BUILD_TESTS` has the value of `PROJECT_IS_TOP_LEVEL` by default, so a tree that
consumes this one configures no tests.

`test/harness.py` carries the flags of each build tree, and it shows what Unity registers in each
suite.

```sh
python test/harness.py test                     # the library as it ships
python test/harness.py test --tree build-werror # the same, with each warning an error
python test/harness.py test --tree build-tsan   # the same, under the thread sanitizer
python test/harness.py test --tree build-asan   # the same, under the address and undefined behavior sanitizers
python test/harness.py suites --strict          # each case registered, none behind a conditional
python test/harness.py cases test/unit/test_embed_types
```

[README.md](README.md) names the Unity dependency and the `ruby` that the runner generator needs.
Neither reaches a consumer of the library.

## Add a suite

A suite is a directory under `test/unit/`. It holds one `.c` of cases and a `CMakeLists.txt` that
names it. `embedded_types_add_suite()` stops the configure step with a `FATAL_ERROR` where the
`.c` of that name is absent.

```cmake
embedded_types_add_suite(test_embed_something)
```

Add one `add_subdirectory` line to `test/unit/CMakeLists.txt`, and the CMake build picks the suite
up. The harness needs no such line. It walks `test/unit/` for each directory with a `.c` that has a
case.

A suite that has a device arm uses `embedded_types_add_device_suite()`. Its cases are in the `.c`,
its kernels are in a `.cu` of the same name, and the two link into one program.

## Add a case

A case is `void test_<name>(void)` at file scope. Two shapes make a case that never runs, and
`harness.py suites --strict` reports both.

- **A definition that the generator walks past.** The generator of Unity matches the exact text
  `void test_<name>(void)`. It registers no case with another form of that text, and the suite still
  reports a pass.
- **A case inside a preprocessor conditional.** The generator reads case names out of the source
  text, and it does not see the conditional. The runner declares and calls the case on each arm of
  the conditional. On the arm with no definition, the suite fails at the link. Put the `#if` inside
  the case body, and call `TEST_IGNORE_MESSAGE` on the arm that can measure nothing.

`UNITY_CASE` and `NEAR_MISS` in `test/harness.py` are the two patterns of the report. `ANY_IF`,
`ANY_ELSE` and `ANY_ENDIF` count the depth of the conditionals.

## Where the proof is

The compiler proves most of this library. The width assertions in `embed_types.h`, the packed-enum
probe, and each expansion of `EMBED_TABLE_LAYOUT` are static. A build that reaches the link step
passed them all, and no case in `test/` repeats that work. The Lean proofs under `theory/` hold the
widths, the atomics, the byte operations of the lane, the manifold of the turnstile, and the ticket
ring.

The suites under `test/unit/`, one for each header, cover what a static assertion does not reach:

- **Signedness.** A typedef of the wrong signedness has the right size, and `sizeof` passes either
  way. Each case casts `-1` to the alias and tests the sign of the result.
- **Boolean normalization.** Each nonzero value converts to 1. A plain eight-bit alias keeps the
  value that it receives, and two true values can then compare unequal across an API boundary.
- **`EMBED_RAW`.** A word read from an odd address, checked against a shift of the same literal
  bytes.
- **The dispatch table.** The offsets are properties of the struct type. An initializer that names the
  wrong function changes none of them. The suite calls through each member.
- **Generated entry points.** `EMBED_ENTRY` and `EMBED_ENTRY_V` emit a function that builds a
  compound literal from the configuration of the caller and hands it to a backend. The suite calls
  both entries and checks what the backend received, with the context member that the entry leaves
  out of its initializer.
- **The byte order.** `EMBED_BIG_ENDIAN` comes from `__BYTE_ORDER__`. The case measures how the
  target lays a word out in memory. A read of the same macro compares the macro with itself.
- **The feature tests.** `EMBED_HAS_ATTRIBUTE` and `EMBED_HAS_BUILTIN` give zero for a name that
  nothing defines. A nonzero result there is nonzero for each name, and each attribute wrapper then
  expands on a compiler that rejects it.
- **The attribute wrappers.** A marked definition is still a definition, and it still computes what
  its body defines. The alignment attribute and the packed attribute reach the type. Where the compiler cannot
  carry one, the case calls `TEST_IGNORE_MESSAGE`. A case that measures nothing does not count as a
  pass.
- **The atomics and the turnstile.** The contention cases run host threads from `test/support/`
  against the words and the lanes. The thread sanitizer tree runs the same cases.

## Never test the library with itself

The expected side of an assertion comes from a literal, from the compiler, or from exact arithmetic.
It never comes from the code under test.

A width checked against the macro that made it compares the macro with itself, and it passes at any
value. `sizeof(embed_u32) == 4u` measures the type against a literal that the standard fixes.
`sizeof(embed_u32) * 8u == 32u`, where the 32 comes from a macro of that same type, measures the
type against itself. Where a case needs an independent source, `uintptr_t` comes from the compiler,
and a shift of literal bytes is exact arithmetic. No case calls the C library.

## Format

```sh
clang-format -i include/*.h test/unit/*/test_*.c
npm run format
```

The runner generator writes `unity_runner.c`, and `.clang-format-ignore` lists it. The next
generation undoes a format of it by hand.

The column limit is 120. `npm run format:check` reports and rewrites nothing. The Test workflow runs
the suites on each push and pull request, and the fix for a format report belongs in the tree.

## Comments

Each header opens with the license banner and a `@file` block. A block documents one declaration. A
family of similar macros takes one block each, because nearly identical is not identical. Where a
conditional declares the same name on more than one arm, the first arm carries the block for all of
them. Each comment is in ASD-STE100, and `tools/api_pages.py` reports each line that breaks a rule.

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

A macro parameter takes a real name with an underscore at its end, and the block is about that
name. `@param[in] x` documents a defect. Rename the parameter first, and the line then has
something to say. Pad each `@param` text to the longest name, and pad `@return` to match.
`EMBED_HAS_ATTRIBUTE` shows the shape.

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

Each attribute wrapper carries a `@warning` that names what its absence costs. Most cost speed where
they expand to nothing. `EMBED_ALIGN` and `EMBED_ALIAS` cost correctness. The `#if` does not tell
the two apart.

State the mechanism. Each library in a build shares these headers. A block that names what one
library does with a macro documents a relationship, and not the macro.

## What belongs here

The test is whether a thing is machinery or a domain fact.

Machinery is what a library above defines for itself where this one does not. A width, a word, an
attribute wrapper, a static assertion, a feature probe, an atomic word, an argument count. Define it
one time, here.

A domain fact belongs to the library that has the opinion. A pool size, a protocol timer, a buffer
count. Those never come here, whatever their prefix looks like.

## Add a declaration

1. Put it in the header it belongs to, in file order.
2. Write its Doxygen block as you write the declaration, and not afterwards.
3. Where it can fail with no diagnostic on some target, add the assertion or the `@warning`.
4. Where it is observable at run time, add a case to the suite that observes it independently.
5. Add its name to `keywords.txt`.
6. Run `python tools/api_pages.py`, and commit the API page with the header.

## Licensing

Contributions are accepted under the same terms as the project. There is no CLA. A pull request
states that you have the right to contribute the code, and that you contribute it under
AGPL-3.0-or-later.

Keep the banner on each new file. Both `LicenseRef-` identifiers in it resolve to files in
`LICENSES/`, and a scanner reports the expression as unresolved where one is absent.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
