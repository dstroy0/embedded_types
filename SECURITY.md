# Security {#proj_security}

**Purpose:** Tell what a build of embedded_types refuses to compile, what it compiles without
diagnosing, and where to send a report.
**Scope:** `include/embed_types.h`, `include/embed_compiler_directives.h`,
`include/embed_dispatch_layout.h`, `CMakeLists.txt`

## Attack surface

embedded_types declares types and expands macros. The three headers define no function and reserve
no object, and a translation unit that includes them gains no code and no storage
(`include/embed_types.h`, `include/embed_compiler_directives.h`,
`include/embed_dispatch_layout.h`). `EMBED_ENTRY` and `EMBED_ENTRY_V` emit a function definition,
and that definition belongs to the translation unit that expands them
(`include/embed_dispatch_layout.h:762-766`, `:782-786`).

## What fails the build

### A wrong width

Every unsigned alias is pinned by a static assertion (`include/embed_types.h:219-228`). A target
where `embed_u32` is not thirty-two bits fails to compile, with a message naming the type.

### An underivable machine word

`EMBED_WORD_BITS` comes from `UINTPTR_MAX`. Where that macro is absent, or names a width other than
sixteen, thirty-two or sixty-four, the header raises `#error` (`include/embed_types.h:121-131`).
There is no fallback width.

### A disregarded packed-enum attribute

A compiler may accept `__attribute__((packed))` and ignore it, and no `#if` detects that.
`EmbedEnumProbe` has a one-byte range and its size is asserted (`include/embed_types.h:256-265`). A
build that ignored packing fails to compile.

### A pre-C11 dialect

No earlier revision has a static assertion. Unguarded, `EMBED_STATIC_ASSERT` would parse as a
function declaration with an implicit `int`, and every assertion in every consumer would compile to
nothing. The header raises `#error` below C11 (`include/embed_compiler_directives.h:92-93`).

## What is not checked

### Unaligned access without the attributes

`EMBED_RAW` lowers alignment to 1 and permits aliasing, and both are required
(`include/embed_compiler_directives.h:222`). Where the compiler supports neither, both expand to
nothing, `embed_raw_word` reverts to the natural alignment of `embed_word`, and a read through it at
an unaligned address is undefined. Nothing diagnoses this. The `@warning` on the typedef states it
(`include/embed_types.h:207-211`). The other attribute wrappers cost speed when they expand to
nothing. These two cost correctness.

### Dispatch table wiring

`EMBED_TABLE_LAYOUT` asserts byte offsets and `sizeof` (`include/embed_dispatch_layout.h:645-648`).
Offsets are properties of the struct type, and an initializer naming the wrong function changes none
of them. A table can satisfy every assertion in that expansion and still dispatch to the wrong
function. A case calls through each member to cover that
(`test/unit/test_embed_dispatch_layout/test_embed_dispatch_layout.c:352`).

### Anything at run time

There are no bounds checks, because there is no run-time code.

### Concurrency

There is no state.

## Hardening the build

Build with `-DEMBEDDED_TYPES_WERROR=ON`, in CI as well as locally. That adds `-Werror` to the flags
this project puts on its own suite executables (`CMakeLists.txt:24-30`,
`cmake/EmbeddedTypesSuite.cmake:32`).

Keep `-Wconversion` and `-Wsign-conversion` on in the consuming build. Those two report a narrowing
at the call site where it happens, and nothing this library ships turns them on for you.

## Reporting

Open a private security advisory at
<https://github.com/dstroy0/embedded_types/security/advisories/new>, or e-mail dquigg123@gmail.com.
Include the compiler and target, the value of `EMBED_WORD_BITS` the build derived, and the smallest
translation unit that shows the behavior.

Fixes land on `main`.

**Author:** dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
**Date:** 2026-09-09
