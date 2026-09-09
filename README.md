# embedded_types

**Purpose:** Take the fixed-width aliases, the machine word, and the compile-time machinery a C11
embedded build needs from one place, instead of declaring them again in each library that uses them.
**Scope:** `include/embed_types.h`, `include/embed_compiler_directives.h`,
`include/embed_dispatch_layout.h`, `cmake/EmbeddedTypesSuite.cmake`, `test/harness.py`, `test/unit/`

Three C11 headers hold the fixed-width integer aliases, a machine word derived from the target, and
the compile-time machinery around them. They are header only, and the system headers they reach are
`stddef.h` and `stdint.h` (`include/embed_types.h:22`, `include/embed_compiler_directives.h:22`,
`include/embed_dispatch_layout.h:23`). What the headers guarantee, and what they leave undiagnosed,
is in [SECURITY.md](SECURITY.md).

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

`embed_raw_word` is `embed_word` carrying `EMBED_RAW`, which lowers the alignment to 1 and permits
the aliasing (`include/embed_types.h:211`, `include/embed_compiler_directives.h:222`). A read
through the plain `embed_word` at an address that is not a multiple of the word width is undefined.
`EMBED_TABLE_LAYOUT` emits one static assertion per member for its byte offset and one for
`sizeof(ScanNs)` (`include/embed_dispatch_layout.h:645-648`). A member left out of the list, or
padding between two members, fails the build.

## Files

| path                                                                         | contents                                                                             |
| ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| [`include/embed_types.h`](include/embed_types.h)                             | the widths, the boolean, the word, the index, the raw-access word, the assertions    |
| [`include/embed_compiler_directives.h`](include/embed_compiler_directives.h) | feature probes, the static assertion, attribute wrappers, diagnostic pragmas, guards |
| [`include/embed_dispatch_layout.h`](include/embed_dispatch_layout.h)         | `EMBED_TABLE_LAYOUT`, `EMBED_CALL`, `EMBED_ENTRY`, and the machinery behind them     |
| [`test/unit/`](test/unit)                                                    | one Unity suite per header, each its own CTest target                                |
| [`test/harness.py`](test/harness.py)                                         | suite discovery, Unity runner generation, and the two build trees                    |
| [`cmake/`](cmake)                                                            | `embedded_types_add_suite()`                                                         |
| [`LICENSES/`](LICENSES)                                                      | the AGPL text and the two LicenseRef documents                                       |

A suite directory names itself to `embedded_types_add_suite()`, which registers it as its own CTest
target (`cmake/EmbeddedTypesSuite.cmake:36`). The two build trees the harness knows are `build` and
`build-werror` (`test/harness.py:231-243`).

## Symbols

The table indexes what each name is. The contract for a name is the Doxygen block on its
declaration, and a guarantee or a warning is stated there once.

| symbol | definition |
| --- | --- |
| `embed_u8` … `embed_u64`, `embed_i8` … `embed_i64` | `uint8_t` … `int64_t`, each unsigned width pinned by an assertion |
| `embed_bool`, `EMBED_TRUE`, `EMBED_FALSE` | `_Bool` in C, `bool` in C++; the two constants are cast to that type |
| `embed_word`, `embed_iword` | the unsigned and signed integer of `EMBED_WORD_BITS` bits |
| `embed_index` | the unsigned integer of `EMBED_INDEX_BITS` bits, for offsets |
| `embed_raw_word` | `embed_word` with alignment lowered to 1 and aliasing permitted |
| `EmbedEnumProbe` | a one-byte enum whose asserted size proves `EMBED_ENUM_PACKED` reached the compiler |
| `EMBED_STATIC_ASSERT` | `static_assert` in C++ and C23, `_Static_assert` in C11, `#error` below C11 |
| `EMBED_BEGIN_DECLS`, `EMBED_END_DECLS` | `extern "C" {` and `}` under `__cplusplus`, empty without it |
| `EMBED_GNU_ATTRIBUTES` | 1 where `__GNUC__` or `__clang__` is defined, 0 otherwise |
| `EMBED_HAS_ATTRIBUTE` | `__has_attribute`, or `EMBED_GNU_ATTRIBUTES` where it is undefined |
| `EMBED_HAS_BUILTIN` | `__has_builtin`, or 0 where it is undefined |
| `EMBED_INLINE`, `EMBED_FLATTEN`, `EMBED_ENUM_PACKED`, `EMBED_ALIGN`, `EMBED_ALIAS`, `EMBED_RAW`, `EMBED_UNUSED`, `EMBED_WEAK` | the attribute where available, empty where not |
| `EMBED_DIAGNOSTIC_PUSH`, `EMBED_DIAGNOSTIC_POP`, `EMBED_DIAGNOSTIC_IGNORE`, `EMBED_DIAGNOSTIC_STRING` | the clang or the GCC pragma, empty on a compiler with neither |
| `EMBED_CAT`, `EMBED_NARG`, `EMBED_ARG_N` | two-step token paste, and argument count from 1 to 24 |
| `EMBED_CALL` | `entry_(&(ArgsType_){__VA_ARGS__})`, where `entry_` receives the address of the literal [BORROWS] |
| `EMBED_ENTRY`, `EMBED_ENTRY_V` | a value-returning and a void entry point, both built on `EMBED_CALL`, whose initializers dereference `args` [BORROWS] |
| `EMBED_TABLE_SLOT` | one offset assertion, for one member at one slot |
| `EMBED_TABLE_LAYOUT`, `EMBED_TABLE_STORAGE` | the offset and size assertions, and `static const` |
| `EMBED_FUNCTION_POINTER_BYTES` | `sizeof(void (*)(void))`, the stride one dispatch slot takes |
| `EMBED_BIG_ENDIAN` | 1 where `__BYTE_ORDER__` equals `__ORDER_BIG_ENDIAN__`, 0 otherwise |
| `EMBED_FAST_UNALIGNED_LOAD` | 1 where `__ARM_FEATURE_UNALIGNED` is defined, 0 otherwise |

`EMBED_STATIC_ASSERT` takes `static_assert` under `__cplusplus` or C23, `_Static_assert` under C11,
and raises `#error` below C11 (`include/embed_compiler_directives.h:92-99`). `EMBED_BIG_ENDIAN` is
derived from `__BYTE_ORDER__` and `EMBED_FAST_UNALIGNED_LOAD` from `__ARM_FEATURE_UNALIGNED`, and
each falls to 0 where its macro is absent (`include/embed_compiler_directives.h:355-359`, `:376-380`).

## EMBED_WORD_BITS

The word width comes from `UINTPTR_MAX`, which the compiler supplies
(`include/embed_types.h:121-131`).

| `UINTPTR_MAX`        | `EMBED_WORD_BITS` |
| -------------------- | ----------------- |
| `0xFFFFFFFFFFFFFFFF` | 64                |
| `0xFFFFFFFF`         | 32                |
| `0xFFFF`             | 16                |
| absent, or any other | `#error`          |

Define `EMBED_WORD_BITS` on the command line to override the derivation. The header then sets
`EMBED_WORD_BITS_WAS_OVERRIDDEN` to 1, and to 0 where the derivation ran. An override is needed
where the register width differs from the pointer width, which covers x32, ILP32-on-64, and a
large-model eight-bit target whose pointer is wider than the register that indexes with it
(`include/embed_types.h:100-102`, `:119`, `:135`).

## EMBED_INDEX_BITS

An offset does not need the register's full width, so the index type is settled on its own.
`EMBED_INDEX_BITS` is 16 where `EMBED_WORD_BITS` is 16 and 32 above it
(`include/embed_types.h:148-154`). A build wanting 16-bit offsets on a wider register defines
`EMBED_INDEX_BITS` ahead of the header, and the assertion at `include/embed_types.h:245` refuses an
override wider than the word.

## Building

```sh
cmake -S . -B build -DEMBEDDED_TYPES_WERROR=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

`test/harness.py` runs the same builds and carries each tree's flags.

```sh
python test/harness.py test                     # the library as it ships
python test/harness.py test --tree build-werror # the same, with every warning an error
python test/harness.py suites --strict          # every case registered, none behind a conditional
```

Configuring fetches [Unity](https://github.com/ThrowTheSwitch/Unity) at `v2.6.1`
(`test/CMakeLists.txt:6-11`), and generating a suite's runner needs `ruby` on `PATH`
(`test/harness.py:195-197`). Both belong to `test/`, which is configured only where
`EMBEDDED_TYPES_BUILD_TESTS` is on, and that option defaults to the value of `PROJECT_IS_TOP_LEVEL`
(`CMakeLists.txt:21`, `:34-37`).

## Consuming

```cmake
add_subdirectory(embedded_types)
target_link_libraries(your_target PRIVATE embedded_types::embedded_types)
```

`embedded_types::embedded_types` is an INTERFACE target carrying the include directory and
`c_std_11` (`CMakeLists.txt:12-19`). It carries no compile options, so the warning settings of the
consuming build govern the translation unit that includes these headers. The suite executables in
this project are compiled with `EMBEDDED_TYPES_TEST_WARNINGS`
(`cmake/EmbeddedTypesSuite.cmake:32`).

## Status

Version 1.0.0 (`CMakeLists.txt:8`). Three suites hold 27 cases between them
(`test/unit/CMakeLists.txt:4-6`), and both build trees pass with every warning an error. That was
measured on 2026-09-09 with GCC 13.2.0 (MinGW-W64 UCRT) on Windows 11, running
`python test/harness.py test` and `python test/harness.py test --tree build-werror`.

The suites are compiled under `EMBEDDED_TYPES_TEST_WARNINGS`, which is `EMBEDDED_TYPES_WARNINGS`
without `-Wconversion`, `-Wsign-conversion` and `-Wmissing-prototypes` (`CMakeLists.txt:24-30`).
Unity's assertion macros widen and compare inside the macro body, and a Unity case is a file-scope
definition with nothing declaring it.

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
**Date:** 2026-09-09
