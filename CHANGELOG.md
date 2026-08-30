# Changelog {#proj_changelog}

All notable changes to this project are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `embed_types.h`: the eight exact-width aliases, `embed_bool` with `EMBED_TRUE` and `EMBED_FALSE`,
  `embed_word` and `embed_iword`, `embed_index`, `embed_raw_word`, eight assertions pinning the
  widths, and a packed-enum probe.
- `embed_compiler_directives.h`: the feature probes, `EMBED_STATIC_ASSERT`, the declaration guards,
  the paste and argument-count family, `EMBED_CALL`, the attribute wrappers, `EMBED_RAW`, the
  diagnostic pragmas, and the byte-order and unaligned-load answers.
- `embed_dispatch_layout.h`: `EMBED_TABLE_LAYOUT` and the arity family behind it, asserting that a
  struct of function pointers holds its members at consecutive slots and nothing else.
- Two test suites under CTest, with no test-framework dependency.
- `LICENSES/`: the AGPL text, and the commercial and educator's licenses the SPDX expression names.

### Notes on how this project began

These files were three copies before they were one. MMgr, ProtoCore and idemIP each carried their
own fixed widths, their own machine word, their own static-assert spelling, their own attribute
wrappers, and their own dispatch-layout assertions, under three prefixes. Two of the three had
already drifted from the third. Documenting MMgr is what surfaced that none of it was MMgr's to own.

Two decisions were made here rather than inherited:

- The machine word is derived from `UINTPTR_MAX` rather than stated by a build knob, and where the
  macro is absent or names an unexpected width the build stops instead of guessing. idemIP derived
  it the same way but fell silently to sixteen bits on a target that stated nothing.
- `EMBED_RAW` is defined once. MMgr spelled that `aligned(1)` and `may_alias` pair twice within
  itself, and ProtoCore a third time with the attributes written bare rather than through the
  probes.

## [0.1.0] - 2026-08-30

Initial release.
