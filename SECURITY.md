# Security {#proj_security}

## What this library is responsible for

embedded_types declares types and expands macros. It has no functions, no storage, and no run-time
behavior of its own, so its security properties are narrow and worth stating exactly. The gap
between what it does and what a reader assumes it does is where the bugs live.

**It does prove its own widths.** Every alias is pinned by a static assertion, so a target where
`embed_u32` is not thirty-two bits fails the build with a message naming the type. A width that was
wrong would otherwise be discovered by a wire format that did not match at the far end.

**It does refuse to guess the machine word.** The width is derived from `UINTPTR_MAX`. Where that
macro is absent, or names a width other than sixteen, thirty-two or sixty-four, the build stops.
Silently choosing the narrowest width would halve every lane on a machine that has more, and nothing
downstream would report it.

**It does prove the packed-enum attribute reached the compiler.** A compiler may accept
`__attribute__((packed))` and disregard it, which no `#if` can see. `EmbedEnumProbe` is declared and
its size asserted, so a build where packing was ignored fails rather than shipping structs whose
members sit at offsets the code does not expect.

**It does refuse to build before C11.** No earlier revision has a static assertion. Left to expand,
`EMBED_STATIC_ASSERT` would parse as a function declaration with an implicit `int`, and every
assertion in every consumer would check nothing while appearing to. That is now an `#error`.

**It does not make an unaligned access safe on its own.** `EMBED_RAW` lowers alignment and permits
aliasing, and both halves are needed. Where the compiler supports neither attribute, both expand to
nothing, the type reverts to its natural alignment, and a read through it at an odd address is
undefined. Nothing diagnoses that. The `@warning` on `EMBED_ALIGN` and `EMBED_ALIAS` says so, and it
is the one place in this library where a missing attribute costs correctness rather than speed.

**It does not bound anything at run time.** `EMBED_TABLE_LAYOUT` asserts a struct's members sit at
consecutive slots; it cannot see whether the initializer wired each member to the function that
belongs there. A table can satisfy every offset assertion and still dispatch wrongly. The suite in
`test/` calls through a table for exactly that reason.

**It is not concurrent and has nothing to synchronize.** There is no state here.

## Hardening the build

Build with `-DEMBEDDED_TYPES_WERROR=ON`. The warning set in `CMakeLists.txt` is documented flag by
flag, and each one catches something a widths-and-macros header can plausibly get wrong. Run it in
CI, not just locally.

A consumer should keep `-Wconversion` and `-Wsign-conversion` on in its own build. This library
exists to make a narrowing visible; a consumer that silences those has given that back.

## Reporting

Open a private security advisory at
<https://github.com/dstroy0/embedded_types/security/advisories/new>, or e-mail dquigg123@gmail.com.
Please include the compiler and target, the value of `EMBED_WORD_BITS` the build derived, and the
smallest translation unit that shows the behavior.

This is a pre-1.0 library maintained by one person. There is no patch SLA. Fixes land on `main` and
are noted in @ref proj_changelog.
