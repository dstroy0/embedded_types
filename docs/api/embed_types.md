<!-- tools/api_pages.py writes this page from include/embed_types.h. Edit the header, then run the tool. -->

# embed_types.h

Source: [`include/embed_types.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_types.h)

The exact-width integers, the boolean, and the machine word and index types a consumer builds on.

> **Note:** This header defines each name one time. A consumer that wants its own vocabulary makes aliases of these names. Then two libraries in one build agree on what a word is.

> **Note:** C11 makes `uint8_t` optional, and a part whose byte is not eight bits has none. To port to such a part, edit this header.

## Types

### `embed_u8` { #embed_u8 }

```c
typedef uint8_t embed_u8;
```

The eight-bit unsigned integer that every scan and copy walks.

> **Note:** The assertion below pins the width. On a part whose byte is not eight bits, stdint omits `uint8_t`, and this typedef fails before the compiler reaches the assertion.

### `embed_u16` { #embed_u16 }

```c
typedef uint16_t embed_u16;
```

The sixteen-bit unsigned integer, pinned by the assertion below.

### `embed_u32` { #embed_u32 }

```c
typedef uint32_t embed_u32;
```

The thirty-two-bit unsigned integer, pinned by the assertion below.

### `embed_u64` { #embed_u64 }

```c
typedef uint64_t embed_u64;
```

The sixty-four-bit unsigned integer, pinned by the assertion below.

### `embed_i8` { #embed_i8 }

```c
typedef int8_t embed_i8;
```

The eight-bit signed integer.

> **Note:** The four signed aliases carry no assertion of their own. `int8_t` through `int64_t` are exact-width by definition in stdint. The assertion on [`embed_iword`](#embed_iword) below holds the signed word to [`embed_word`](#embed_word)'s size.

### `embed_i16` { #embed_i16 }

```c
typedef int16_t embed_i16;
```

The sixteen-bit signed integer. It has no assertion, for the reason on [`embed_i8`](#embed_i8).

### `embed_i32` { #embed_i32 }

```c
typedef int32_t embed_i32;
```

The thirty-two-bit signed integer. It has no assertion, for the reason on [`embed_i8`](#embed_i8).

### `embed_i64` { #embed_i64 }

```c
typedef int64_t embed_i64;
```

The sixty-four-bit signed integer. It has no assertion, for the reason on [`embed_i8`](#embed_i8).

### `embed_bool` { #embed_bool }

```c
#ifdef __cplusplus
typedef bool embed_bool;
#else
typedef _Bool embed_bool;
#endif
```

The truth value.

> **Note:** C++ gets bool and C gets `_Bool`. Both are keywords, so this typedef needs no header.

> **Note:** This header does not include `<stdbool.h>`. Its bool macro collides with the bool that some embedded SDKs define for themselves.

> **Note:** Both types change any nonzero value to 1 on conversion. Then a value that crosses between a library and its caller compares equal on either side. A plain eight-bit alias keeps the value that it gets, and two true values can differ.

### `embed_word` { #embed_word }

```c
#if EMBED_WORD_BITS == 64
typedef embed_u64 embed_word;
#elif EMBED_WORD_BITS == 32
typedef embed_u32 embed_word;
#elif EMBED_WORD_BITS == 16
typedef embed_u16 embed_word;
#endif
```

The unsigned word that every lane operation runs in, [`EMBED_WORD_BITS`](#EMBED_WORD_BITS) wide.

> **Note:** A consumer reasons in this register. A lane mask, a zero test and a byte count all have this width, so a change to [`EMBED_WORD_BITS`](#EMBED_WORD_BITS) changes what one operation covers.

> **Note:** All three arms below declare this type. The 64-bit arm carries this block because the compiler reaches it first.

### `embed_iword` { #embed_iword }

```c
#if EMBED_WORD_BITS == 64
typedef embed_i64 embed_iword;
#elif EMBED_WORD_BITS == 32
typedef embed_i32 embed_iword;
#elif EMBED_WORD_BITS == 16
typedef embed_i16 embed_iword;
#endif
```

The signed word, the same register as [`embed_word`](#embed_word).

> **Note:** This type holds a result that goes below zero. An unsigned word would wrap it. The assertion below holds this type to the size of [`embed_word`](#embed_word).

### `embed_index` { #embed_index }

```c
#if EMBED_INDEX_BITS == 32
typedef embed_u32 embed_index;
#elif EMBED_INDEX_BITS == 16
typedef embed_u16 embed_index;
#endif
```

The index type, [`EMBED_INDEX_BITS`](#EMBED_INDEX_BITS) wide, for every offset and length.

> **Note:** This type is not `size_t`. `size_t` is as wide as the target's pointer. Then one expression would be 32-bit index arithmetic on a device and 64-bit on the host that tests it. One source would then emit different code on the two.

> **Note:** Both arms below declare this type. The 32-bit arm carries this block because the compiler reaches it first.

### `embed_raw_word` { #embed_raw_word }

```c
typedef embed_word embed_raw_word EMBED_RAW;
```

The word again, readable from an address that is not a multiple of its width.

> **Note:** [`EMBED_RAW`](embed_compiler_directives.md#EMBED_RAW) lowers the alignment to one and permits access through an alias. A load through this type has a defined result at any address only with both. A load through the plain [`embed_word`](#embed_word) at an unaligned address stays undefined.

> _**Warning:**_ Where either attribute is not available, this type is the plain word, and C leaves a read through it undefined. Nothing diagnoses that. The warnings on [`EMBED_ALIGN`](embed_compiler_directives.md#EMBED_ALIGN) and [`EMBED_ALIAS`](embed_compiler_directives.md#EMBED_ALIAS) in `embed_compiler_directives`.h cover the same failure.

### `EmbedEnumProbe` { #EmbedEnumProbe }

```c
EMBED_ENUM_BYTE(EmbedEnumProbe, EMBED_ENUM_PROBE_MIN = 0, /**< Low end of the probe range. */
                EMBED_ENUM_PROBE_MAX = 255                /**< High end, the largest value eight bits hold. */
);
```

A one-byte enum used only to prove [`EMBED_ENUM_BYTE`](embed_compiler_directives.md#EMBED_ENUM_BYTE) gives one byte on this compiler.

> **Note:** Its range needs a single byte, so the assertion below fails only where the compiler accepted the packed attribute and disregarded it.

> _**Warning:**_ A failure means that every enum a consumer declares through [`EMBED_ENUM_BYTE`](embed_compiler_directives.md#EMBED_ENUM_BYTE) widens. Then any offset that comes from a struct with such an enum is wrong.

- `EMBED_ENUM_PROBE_MIN`: Low end of the probe range.
- `EMBED_ENUM_PROBE_MAX`: High end, the largest value eight bits hold.

## Macros

### `EMBED_TRUE` { #EMBED_TRUE }

```c
#define EMBED_TRUE ((embed_bool)1)
```

True, cast to [`embed_bool`](#embed_bool).

> **Note:** The cast gives the macro [`embed_bool`](#embed_bool) type. A bare 1 would have type int, and sizeof would measure it as int.

### `EMBED_FALSE` { #EMBED_FALSE }

```c
#define EMBED_FALSE ((embed_bool)0)
```

False, cast to [`embed_bool`](#embed_bool).

> **Note:** The cast gives the macro [`embed_bool`](#embed_bool) type, for the reason given on [`EMBED_TRUE`](#EMBED_TRUE).

### `EMBED_WORD_BITS` { #EMBED_WORD_BITS }

```c
#ifndef EMBED_WORD_BITS
#if !defined(UINTPTR_MAX)
#elif UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
#define EMBED_WORD_BITS 64
#elif UINTPTR_MAX == 0xFFFFFFFFu
#define EMBED_WORD_BITS 32
#elif UINTPTR_MAX == 0xFFFFu
#define EMBED_WORD_BITS 16
#endif
#endif
```

The width of the machine word in bits, derived from the target's pointer width.

> **Note:** `UINTPTR_MAX` comes from the compiler. The build supplies no width of its own here, and it cannot supply a wrong one.

> **Note:** A build whose register width differs from its pointer width defines this on the command line ahead of this header. The x32 and ILP32-on-64 ABIs are examples. A large-model eight-bit target is another: its pointer is wider than the register that indexes with it.

> _**Warning:**_ The `#error` arms below fire where `UINTPTR_MAX` is absent, and where it is not 16, 32 or 64 bits wide. C11 makes `uintptr_t` optional, so its absence is legal. This header guesses neither case. A silent fall to the narrowest width would halve every lane on a machine that has more.

### `EMBED_WORD_BITS_WAS_OVERRIDDEN` { #EMBED_WORD_BITS_WAS_OVERRIDDEN }

```c
#ifndef EMBED_WORD_BITS
#define EMBED_WORD_BITS_WAS_OVERRIDDEN 0
#else
#define EMBED_WORD_BITS_WAS_OVERRIDDEN 1
#endif
```

Set to 0 where this header derives the word width, 1 where the build defines it.

> **Note:** This header sets the flag itself. Otherwise a build that overrides the width must set a second macro to match, and the two can drift apart.

> **Note:** A check that the word width matches the pointer width holds only where the derivation ran. A build that overrides the width declares that the two differ.

> **Note:** Both arms define this macro, so `#ifdef` [`EMBED_WORD_BITS_WAS_OVERRIDDEN`](#EMBED_WORD_BITS_WAS_OVERRIDDEN) is always true. Test it with `#if`.

Set to 1 where the build defined [`EMBED_WORD_BITS`](#EMBED_WORD_BITS) ahead of this header.

### `EMBED_INDEX_BITS` { #EMBED_INDEX_BITS }

```c
#ifndef EMBED_INDEX_BITS
#if EMBED_WORD_BITS == 16
#define EMBED_INDEX_BITS 16
#else
#define EMBED_INDEX_BITS 32
#endif
#endif
```

The width of an offset or a length in bits.

> **Note:** This width is separate from the word width, because an offset does not need the full width of the register. A 64-bit host with 32-bit offsets halves the cost of a descriptor.

> **Note:** The default is the word width at 16 bits and 32 above it. That gives 16 on a 16-bit word, 32 on a 32-bit word, and 32 on a 64-bit word, so the index fits the register in all three.

> _**Warning:**_ A build that wants 16-bit offsets on a wider register defines this ahead of this header. The assertion below stops an override wider than the word.

## Assertions

### `sizeof(embed_u8) == 1u`

```c
EMBED_STATIC_ASSERT(sizeof(embed_u8) == 1u, "embed_u8 must be exactly 8 bits: this target has no 8-bit type");
```

Asserts [`embed_u8`](#embed_u8) is exactly eight bits.

> **Note:** A stdint that conforms to C defines `uint8_t` at exactly eight bits, so this assertion guards against a stdint that does not conform.

### `sizeof(embed_u16) == 2u`

```c
EMBED_STATIC_ASSERT(sizeof(embed_u16) == 2u, "embed_u16 must be exactly 16 bits");
```

Asserts [`embed_u16`](#embed_u16) is exactly sixteen bits.

### `sizeof(embed_u32) == 4u`

```c
EMBED_STATIC_ASSERT(sizeof(embed_u32) == 4u, "embed_u32 must be exactly 32 bits");
```

Asserts [`embed_u32`](#embed_u32) is exactly thirty-two bits.

### `sizeof(embed_u64) == 8u`

```c
EMBED_STATIC_ASSERT(sizeof(embed_u64) == 8u, "embed_u64 must be exactly 64 bits");
```

Asserts [`embed_u64`](#embed_u64) is exactly sixty-four bits.

### `sizeof(embed_word) == EMBED_WORD_BITS / 8u`

```c
EMBED_STATIC_ASSERT(sizeof(embed_word) == EMBED_WORD_BITS / 8u, "embed_word must be exactly EMBED_WORD_BITS wide");
```

Asserts [`embed_word`](#embed_word) is [`EMBED_WORD_BITS`](#EMBED_WORD_BITS) wide.

> **Note:** A consumer derives every offset from this width at compile time. Nothing measures it at run time. A width that differs from what the code assumed has to fail the build.

### `sizeof(embed_iword) == sizeof(embed_word)`

```c
EMBED_STATIC_ASSERT(sizeof(embed_iword) == sizeof(embed_word), "the signed word must be the same register as the word");
```

Asserts [`embed_iword`](#embed_iword) is the same size as [`embed_word`](#embed_word).

### `sizeof(embed_index) == EMBED_INDEX_BITS / 8u`

```c
EMBED_STATIC_ASSERT(sizeof(embed_index) == EMBED_INDEX_BITS / 8u, "embed_index must be exactly EMBED_INDEX_BITS wide");
```

Asserts [`embed_index`](#embed_index) is [`EMBED_INDEX_BITS`](#EMBED_INDEX_BITS) wide.

### `sizeof(embed_index) <= sizeof(embed_word)`

```c
EMBED_STATIC_ASSERT(sizeof(embed_index) <= sizeof(embed_word), "an index must fit the register it is carried in");
```

Asserts [`embed_index`](#embed_index) is no larger than [`embed_word`](#embed_word).

### `sizeof(EmbedEnumProbe) == 1`

```c
EMBED_STATIC_ASSERT(sizeof(EmbedEnumProbe) == 1,
                    "EMBED_ENUM_BYTE gave an enum more than one byte here, so no enum keeps its declared width "
                    "(TI: pass --small_enum)");
```

Asserts [`EMBED_ENUM_BYTE`](embed_compiler_directives.md#EMBED_ENUM_BYTE) gave the probe one byte.
