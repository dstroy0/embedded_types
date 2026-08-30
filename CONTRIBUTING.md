# Contributing {#proj_contributing}

## Build and test

```sh
cmake -S . -B build -DEMBEDDED_TYPES_WERROR=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Two CTest targets, one per header under test. `EMBEDDED_TYPES_BUILD_TESTS` defaults on when this is
the top-level project and off when it is consumed by another, so `add_subdirectory` does not build
suites nobody asked for.

There is no test framework. The library is widths and macros, and a dependency pulled in to prove a
width is a dependency every consumer inherits.

## Where the proof lives

Most of this library is proved by compiling. The eight width assertions, the packed-enum probe, and
every `EMBED_TABLE_LAYOUT` are static: a build that reaches the link step has already checked them,
and there is no case in `test/` that repeats the work.

What the suites cover is what a static assertion cannot see:

- **Signedness.** A typedef pointing at the wrong signedness has the right size, so `sizeof` cannot
  catch it. Round-tripping a negative through the signed type does.
- **Boolean normalization.** That any nonzero becomes one, which a plain eight-bit alias would not
  give, and which decides whether two true values compare equal across an API boundary.
- **`EMBED_RAW`.** A word read from an odd address, checked against a `memcpy` of the same bytes.
- **Dispatch wiring.** A table can satisfy every offset assertion and still be initialized with the
  wrong function in a slot. The suite calls through it.

## Never test the library with itself

The expected side of an assertion comes from a literal, from the compiler, or from exact arithmetic.
Never from the code under test.

A width checked against the macro that produced it agrees with itself and reports green for any
value. `sizeof(embed_u32) == 4u` is a test; `sizeof(embed_u32) * 8u == 32u` compared against a macro
that was itself derived from the type is not. Where a test needs an independent source, `uintptr_t`
and `memcpy` are the compiler's and the standard's, not this library's.

## Formatting

```sh
clang-format -i include/*.h test/*.c
npm run format
```

120 columns. CI checks and never rewrites: a formatter that rewrites on CI produces commits nobody
reviewed and races the author's own push. The fix belongs in the working tree.

## Comments

Every header opens with the license banner and a `@file` block, and every declaration in it carries
its own Doxygen block. A block documents exactly one declaration; a contiguous family of similar
macros takes one block each, because nearly identical is not identical.

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
`@param[in] x` documents a defect; rename the parameter first and the line then has something to say.
Pad every `@param` description to the longest name, and pad `@return` to match.

```c
/**
 * @brief Expands to __has_attribute(attribute_) where __has_attribute is defined.
 *
 * @param[in] attribute_ Attribute name, as passed to __has_attribute.
 * @return               The value __has_attribute gives for attribute_.
 * @warning Expands to EMBED_GNU_ATTRIBUTES where __has_attribute is undefined, ignoring attribute_.
 */
#define EMBED_HAS_ATTRIBUTE(attribute_) __has_attribute(attribute_)
```

Every attribute wrapper carries a `@warning` saying what its absence costs. That is the difference
between a wrapper that costs speed when it vanishes and one that costs correctness, and a reader
cannot tell which from the `#if`.

State the mechanism, not a consumer. This header set is shared, so a block that says what one
library does with a macro, or names a part it was measured on, is describing a relationship rather
than the macro.

## What belongs here

The test is whether it is machinery or a domain fact.

Machinery is anything a library above would otherwise define for itself: a width, a word, an
attribute wrapper, a static assertion, a feature probe, an argument count. Define it once, here.

A domain fact belongs to the library that has the opinion — a pool size, a protocol timer, a buffer
count. Those never come here, whatever their prefix looks like.

## Adding a declaration

1. Put it in the header it belongs to, in file order.
2. Write its Doxygen block as you write it, not afterwards.
3. If it can fail silently on some target, add the assertion or the `@warning` that says so.
4. If it is observable at run time, add a case to the suite that observes it independently.
5. Add its name to `keywords.txt`.

## Licensing

Contributions are accepted under the same terms as the project. There is no CLA. Opening a pull
request means you have the right to contribute the code and are doing so under AGPL-3.0-or-later.

Keep the banner on every new file. Both `LicenseRef-` identifiers in it resolve to files in
`LICENSES/`, and a scanner reports the expression as unresolved if one goes missing.
