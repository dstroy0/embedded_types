<!-- tools/api_pages.py writes this page from include/embed_compiler_directives.h. Edit the header, then run the tool. -->

# embed_compiler_directives.h

Source: [`include/embed_compiler_directives.h`](https://github.com/dstroy0/embedded_types/blob/main/include/embed_compiler_directives.h)

Compiler feature checks, attribute wrappers, diagnostic pragmas, an argument count, and target flags.

> **Note:** This header has no `extern "C"` guard of its own, because it defines that guard as [`EMBED_BEGIN_DECLS`](#EMBED_BEGIN_DECLS). Each name here is a macro, and a macro has no linkage to mangle.

> **Note:** If your library uses this one, make aliases of these macros under your own prefix. Do not copy them into your own tree. A copy drifts when this header changes.

## Macros

### `EMBED_GNU_ATTRIBUTES` { #EMBED_GNU_ATTRIBUTES }

```c
#if defined(__GNUC__) || defined(__clang__)
#define EMBED_GNU_ATTRIBUTES 1
#else
#define EMBED_GNU_ATTRIBUTES 0
#endif
```

Set to 1 where the compiler defines `__GNUC__` or `__clang__`, 0 otherwise.

> **Note:** [`EMBED_HAS_ATTRIBUTE`](#EMBED_HAS_ATTRIBUTE) falls back to this value where the compiler has no `__has_attribute`. A compiler too old for `__has_attribute` usually still accepts the GNU attributes, and a 0 here would turn all of them off.

> **Note:** This macro always has a value, 0 or 1. Test it with `#if`. `#ifdef` is true either way.

### `EMBED_HAS_ATTRIBUTE` { #EMBED_HAS_ATTRIBUTE }

```c
#if defined(__has_attribute)
#define EMBED_HAS_ATTRIBUTE(attribute_) __has_attribute(attribute_)
#else
#define EMBED_HAS_ATTRIBUTE(attribute_) EMBED_GNU_ATTRIBUTES
#endif
```

Expands to `__has_attribute(attribute_)` where the compiler has `__has_attribute`.

- `[in]`: `attribute_` Attribute name, as `__has_attribute` takes it.

\*\*Returns:\*\* The value that `__has_attribute` gives for `attribute_`.

> **Note:** Each attribute macro below goes through this one. A question to the compiler about an attribute is more reliable than a test of which compiler it is.

> _**Warning:**_ Expands to [`EMBED_GNU_ATTRIBUTES`](#EMBED_GNU_ATTRIBUTES) where the compiler has no `__has_attribute`, and discards `attribute_`. Then each attribute gets the same answer. A compiler without `__has_attribute` gets all of them or none.

### `EMBED_HAS_BUILTIN` { #EMBED_HAS_BUILTIN }

```c
#if defined(__has_builtin)
#define EMBED_HAS_BUILTIN(builtin_) __has_builtin(builtin_)
#else
#define EMBED_HAS_BUILTIN(builtin_) 0
#endif
```

Expands to `__has_builtin(builtin_)` where the compiler has `__has_builtin`.

- `[in]`: `builtin_` Builtin name, as `__has_builtin` takes it.

\*\*Returns:\*\* The value that `__has_builtin` gives for `builtin_`.

> **Note:** This macro is the builtin form of [`EMBED_HAS_ATTRIBUTE`](#EMBED_HAS_ATTRIBUTE). Use it to gate a builtin as you gate an attribute.

> _**Warning:**_ Without `__has_builtin` this expands to 0 and discards `builtin_`. The fallback is 0, because a builtin that the compiler does not have fails to compile. [`EMBED_HAS_ATTRIBUTE`](#EMBED_HAS_ATTRIBUTE) falls back to a guess instead, because the compiler ignores an attribute that it does not have.

### `EMBED_STATIC_ASSERT` { #EMBED_STATIC_ASSERT }

```c
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L)
#elif defined(__cplusplus) || __STDC_VERSION__ >= 202311L
#define EMBED_STATIC_ASSERT(cond_, msg_) static_assert(cond_, msg_)
#else
#define EMBED_STATIC_ASSERT(cond_, msg_) _Static_assert(cond_, msg_)
#endif
```

Expands to a static assertion of two operands.

- `[in]`: `cond_` Constant expression, passed through with no change.
- `[in]`: `msg_` Message operand, passed through with no change.

> **Note:** This macro gives one name for the assertion across C11, C23, and C++. Each file here that claims a width or a layout uses it.

> **Note:** C++ and C23 get `static_assert`, which is a keyword in both. All other revisions get `_Static_assert`, the C11 keyword.

> **Note:** The `__cplusplus` arm is necessary. No C++ compiler defines `__STDC_VERSION__`, and without that arm the `#error` below fires on each C++ build.

> **Note:** Neither arm includes `<assert.h>`. Both names are keywords in the revision that takes them.

> **Note:** The expansion has no semicolon at its end. Write one at the call site.

> _**Warning:**_ `#error` below C11, where neither keyword exists. Without it, the name would parse as a function declaration that returns implicit int. Then each assertion in the build would compile and check nothing.

### `EMBED_BEGIN_DECLS` { #EMBED_BEGIN_DECLS }

```c
#ifdef __cplusplus
#define EMBED_BEGIN_DECLS \
    extern "C" \
    {
#else
#define EMBED_BEGIN_DECLS
#endif
```

Expands to `extern "C" {` where the compiler defines `__cplusplus`.

> **Note:** This library is C. A C++ translation unit that includes these headers needs the guard to reach the symbols by their names without the C++ mangle.

> _**Warning:**_ The expansion leaves a `{` open. [`EMBED_END_DECLS`](#EMBED_END_DECLS) closes it.

> **Note:** Expands to nothing where the compiler does not define `__cplusplus`.

Expands to nothing where the compiler does not define `__cplusplus`. This arm opens no brace.

### `EMBED_END_DECLS` { #EMBED_END_DECLS }

```c
#ifdef __cplusplus
#define EMBED_END_DECLS }
#else
#define EMBED_END_DECLS
#endif
```

Expands to `}` where the compiler defines `__cplusplus`, and closes [`EMBED_BEGIN_DECLS`](#EMBED_BEGIN_DECLS).

Expands to nothing where the compiler does not define `__cplusplus`.

### `EMBED_INLINE` { #EMBED_INLINE }

```c
#ifndef EMBED_INLINE
#if EMBED_HAS_ATTRIBUTE(always_inline)
#define EMBED_INLINE static inline __attribute__((always_inline))
#else
#define EMBED_INLINE static inline
#endif
#endif
```

Expands to `static inline` followed by `__attribute__((always_inline))`.

> **Note:** GCC and clang put the body in line, also where their size heuristic would keep it out of line. A plain `static inline` gives that decision to the compiler.

> **Note:** Expands to `static inline` where `EMBED_HAS_ATTRIBUTE(always_inline)` is 0. The code stays correct. It can run slower.

> _**Warning:**_ This header defines nothing where the build defines [`EMBED_INLINE`](#EMBED_INLINE) first. Define it ahead of this header to set your own policy for the inline expansion.

### `EMBED_FLATTEN` { #EMBED_FLATTEN }

```c
#if EMBED_HAS_ATTRIBUTE(flatten)
#define EMBED_FLATTEN __attribute__((flatten))
#else
#define EMBED_FLATTEN
#endif
```

Expands to `__attribute__((flatten))` where `EMBED_HAS_ATTRIBUTE(flatten)` is not zero.

> **Note:** Put this on a function, and GCC or clang puts each call of that function in line. This includes the bodies that their size heuristic would skip. [`EMBED_INLINE`](#EMBED_INLINE) goes on the callee. This macro goes on the caller.

> **Note:** Each call site of the marked function holds the code put in line. Put it on one hot function.

> _**Warning:**_ The bodies of the calls must be visible. Without link-time optimization this does nothing across translation units.

> _**Warning:**_ Expands to nothing where `EMBED_HAS_ATTRIBUTE(flatten)` is 0. The code stays correct. It can run slower.

### `EMBED_ENUM_BYTE` { #EMBED_ENUM_BYTE }

```c
#if defined(__clang__) && defined(_MSC_VER)
#define EMBED_ENUM_BYTE(name_, ...) \
    typedef enum : unsigned char \
    { \
        __VA_ARGS__ \
    } name_
#elif EMBED_HAS_ATTRIBUTE(packed)
#define EMBED_ENUM_BYTE(name_, ...) \
    typedef enum __attribute__((packed)) \
    { \
        __VA_ARGS__ \
    } name_
#elif defined(__cplusplus)
#define EMBED_ENUM_BYTE(name_, ...) \
    typedef enum : unsigned char \
    { \
        __VA_ARGS__ \
    } name_
#else
#define EMBED_ENUM_BYTE(name_, ...) \
    enum \
    { \
        __VA_ARGS__ \
    }; \
    typedef unsigned char name_
#endif
```

Declares `name_` as a one-byte enumeration of the enumerators given, on each compiler.

- `[in]`: `name_` The name of the type.
- `[in]`: ... The enumerators, as the braces of an enum hold them.

> **Note:** Where the compiler can pin an enum to one byte, `name_` is that enum. Then the compiler still checks a switch on it, or a value of another enum.

> **Note:** Clang on the MSVC ABI and C++ take the fixed type `: unsigned char`. C++11 made that form standard, and clang takes it in C as the C23 form.

> **Note:** Clang on the MSVC ABI also accepts the packed attribute, but it keeps the enum as wide as an int, as MSVC does. So that arm takes the fixed type before it looks for the attribute. Each other compiler with the packed attribute takes the attribute.

> **Note:** Where the compiler cannot pin an enum, MSVC's C among them, the enumerators become constants and `name_` is an unsigned char. The width is one byte either way. A struct that holds `name_` has the same layout on each compiler.

> _**Warning:**_ An enumerator above 255 or below 0 fails the build on the arms of the fixed type. It widens the enum on the attribute arm. On the unsigned char arm, a store cuts it to a byte. `embed_types`.h declares a probe and asserts its size. That assertion also finds a compiler that accepts the attribute and ignores it.

### `EMBED_ALIGN` { #EMBED_ALIGN }

```c
#if EMBED_HAS_ATTRIBUTE(aligned)
#define EMBED_ALIGN(bytes_) __attribute__((aligned(bytes_)))
#elif defined(_MSC_VER)
#if defined(__cplusplus)
#define EMBED_ALIGN(bytes_) alignas(bytes_)
#else
#define EMBED_ALIGN(bytes_) _Alignas(bytes_)
#endif
#else
#define EMBED_ALIGN(bytes_)
#endif
```

Expands to `__attribute__((aligned(bytes_)))` where `EMBED_HAS_ATTRIBUTE(aligned)` is not zero.

- `[in]`: `bytes_` The alignment that the attribute takes.

> **Note:** This macro works both ways. It can raise the alignment of an object above its natural alignment, or lower it to 1. [`EMBED_RAW`](#EMBED_RAW) uses the form that lowers it to 1.

> _**Warning:**_ Expands to nothing where `EMBED_HAS_ATTRIBUTE(aligned)` is 0, and discards `bytes_`. Then an object that you mean to align more keeps its natural alignment. A type that you mean to drop to alignment 1 also keeps its natural alignment, and the code still reads it from any address.

### `EMBED_ALIAS` { #EMBED_ALIAS }

```c
#if EMBED_HAS_ATTRIBUTE(may_alias)
#define EMBED_ALIAS __attribute__((may_alias))
#else
#define EMBED_ALIAS
#endif
```

Expands to `__attribute__((may_alias))` where `EMBED_HAS_ATTRIBUTE(may_alias)` is not zero.

> **Note:** C lets a character type alias any object. A read of an eight-bit array through a word lvalue goes the other way, and C leaves that read undefined. This attribute on the word type gives the read a defined result.

> _**Warning:**_ Expands to nothing where `EMBED_HAS_ATTRIBUTE(may_alias)` is 0. Nothing diagnoses that. The code compiles, and the compiler can assume that the two accesses never touch the same memory.

### `EMBED_RAW` { #EMBED_RAW }

```c
#if EMBED_HAS_ATTRIBUTE(aligned)
#define EMBED_RAW EMBED_ALIGN(1) EMBED_ALIAS
#else
#define EMBED_RAW EMBED_ALIAS
#endif
```

Expands to `EMBED_ALIGN(1)` followed by [`EMBED_ALIAS`](#EMBED_ALIAS).

> **Note:** A read of a word from an address that is not a multiple of its width needs both attributes. `EMBED_ALIGN(1)` stops the compiler from an assumption about the alignment. [`EMBED_ALIAS`](#EMBED_ALIAS) lets the read reach bytes that another type owns. Either one alone leaves the access undefined.

> **Note:** This macro holds both attributes. If each declaration writes out the pair, the two can drift apart at the first edit of one.

> _**Warning:**_ Both warnings above apply here. Nothing diagnoses either failure.

### `EMBED_HOST_DEVICE` { #EMBED_HOST_DEVICE }

```c
#if defined(__CUDACC__)
#define EMBED_HOST_DEVICE __host__ __device__
#else
#define EMBED_HOST_DEVICE
#endif
```

Expands to `__host__ __device__` under nvcc, and to nothing on other compilers.

> **Note:** A function with this macro compiles into each of the two passes of nvcc from one definition. Other compilers have one pass, and the function compiles into it. Each entry of `embed_atomic`.h and [`embed_lane`](embed_lane.md#embed_lane).h has this macro, so one source runs on the host and on the device.

> **Note:** Both arms define this macro.

### `EMBED_HAS_VECTOR_SIZE` { #EMBED_HAS_VECTOR_SIZE }

```c
#if EMBED_HAS_ATTRIBUTE(vector_size)
#define EMBED_HAS_VECTOR_SIZE 1
#else
#define EMBED_HAS_VECTOR_SIZE 0
#endif
```

Set to 1 where `EMBED_HAS_ATTRIBUTE(vector_size)` is not zero, else 0.

> **Note:** The `vector_size` attribute on a scalar type makes a type that the compiler keeps in a vector register. The plain operators then run over each element at once. The lane header takes its vector arm where this macro is 1, and its portable arm where it is 0.

> **Note:** Both arms define this macro. Test it with `#if`.

### `EMBED_VECTOR` { #EMBED_VECTOR }

```c
#if EMBED_HAS_VECTOR_SIZE
#define EMBED_VECTOR(bytes_) __attribute__((vector_size(bytes_)))
#else
#define EMBED_VECTOR(bytes_)
#endif
```

Expands to `__attribute__((vector_size(bytes_)))` where [`EMBED_HAS_VECTOR_SIZE`](#EMBED_HAS_VECTOR_SIZE) is 1.

- `[in]`: `bytes_` The width of the vector in bytes, a power of two and a multiple of the size of the scalar.

> _**Warning:**_ Expands to nothing where [`EMBED_HAS_VECTOR_SIZE`](#EMBED_HAS_VECTOR_SIZE) is 0, and discards `bytes_`: the type is the scalar alone. A declaration that needs the width tests [`EMBED_HAS_VECTOR_SIZE`](#EMBED_HAS_VECTOR_SIZE) first. On the other arm it declares a struct of the elements, as [`embed_lane`](embed_lane.md#embed_lane).h does.

### `EMBED_VECTOR_REGISTER_BITS` { #EMBED_VECTOR_REGISTER_BITS }

```c
#if defined(__AVX512F__)
#define EMBED_VECTOR_REGISTER_BITS 512
#elif defined(__AVX2__)
#define EMBED_VECTOR_REGISTER_BITS 256
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && (_M_IX86_FP >= 2)) || defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
#define EMBED_VECTOR_REGISTER_BITS 128
#else
#define EMBED_VECTOR_REGISTER_BITS 0
#endif
```

The width in bits of the widest vector register that the target compiles for. It is 512 under AVX-512, 256 under AVX2, 128 under SSE2 or NEON, and 0 where the target has none.

> **Note:** The compiler defines a macro for each instruction set that it emits, and this value comes from those macros. The build supplies no width of its own here. A flag that turns on an instruction set changes this value with it. The lane header derives the width of the lane from this value where the build does not define one.

> **Note:** Both arms define this macro. Test it with `#if`.

### `EMBED_HAS_BYTE_MASK_256` { #EMBED_HAS_BYTE_MASK_256 }

```c
#if defined(__AVX2__) && EMBED_HAS_BUILTIN(__builtin_ia32_pmovmskb256)
#define EMBED_HAS_BYTE_MASK_256 1
#else
#define EMBED_HAS_BYTE_MASK_256 0
#endif
```

Set to 1 where the target runs AVX2 and the compiler has the builtin for the mask of 32 bytes, else 0. The builtin reads the sign bit of each byte into an integer.

> **Note:** The builtin is the compiler's own name for the instruction, and it needs no vendor header. A byte compare of two vectors of 32 bytes sets each equal byte to all ones. This builtin then reads those bytes into one bit for each byte, in one instruction.

> **Note:** Both arms define this macro. Test it with `#if`.

### `EMBED_BYTE_MASK_256` { #EMBED_BYTE_MASK_256 }

```c
#if EMBED_HAS_BYTE_MASK_256
#define EMBED_BYTE_MASK_256(chars_) __builtin_ia32_pmovmskb256(chars_)
#else
#define EMBED_BYTE_MASK_256(chars_) 0
#endif
```

The sign bits of 32 bytes as an int, bit 0 from the first byte, where [`EMBED_HAS_BYTE_MASK_256`](#EMBED_HAS_BYTE_MASK_256) is 1.

- `[in]`: `chars_` A vector of 32 char.

> _**Warning:**_ Expands to 0 where [`EMBED_HAS_BYTE_MASK_256`](#EMBED_HAS_BYTE_MASK_256) is 0, and discards `chars_`. A use tests the flag first and takes another arm where it is 0, as [`embed_lane`](embed_lane.md#embed_lane).h does.

### `EMBED_HAS_BYTE_MASK_128` { #EMBED_HAS_BYTE_MASK_128 }

```c
#if defined(__SSE2__) && EMBED_HAS_BUILTIN(__builtin_ia32_pmovmskb128)
#define EMBED_HAS_BYTE_MASK_128 1
#else
#define EMBED_HAS_BYTE_MASK_128 0
#endif
```

Set to 1 where the target runs SSE2 and the compiler has the builtin for the mask of 16 bytes, else 0. The builtin reads the sign bit of each byte into an integer.

> **Note:** Both arms define this macro. Test it with `#if`.

### `EMBED_BYTE_MASK_128` { #EMBED_BYTE_MASK_128 }

```c
#if EMBED_HAS_BYTE_MASK_128
#define EMBED_BYTE_MASK_128(chars_) __builtin_ia32_pmovmskb128(chars_)
#else
#define EMBED_BYTE_MASK_128(chars_) 0
#endif
```

The sign bits of 16 bytes as an int, bit 0 from the first byte, where [`EMBED_HAS_BYTE_MASK_128`](#EMBED_HAS_BYTE_MASK_128) is 1.

- `[in]`: `chars_` A vector of 16 char.

> _**Warning:**_ Expands to 0 where [`EMBED_HAS_BYTE_MASK_128`](#EMBED_HAS_BYTE_MASK_128) is 0, and discards `chars_`. A use tests the flag first and takes another arm where it is 0, as [`embed_lane`](embed_lane.md#embed_lane).h does.

### `EMBED_UNUSED` { #EMBED_UNUSED }

```c
#if EMBED_HAS_ATTRIBUTE(unused)
#define EMBED_UNUSED __attribute__((unused))
#else
#define EMBED_UNUSED
#endif
```

Expands to `__attribute__((unused))` where `EMBED_HAS_ATTRIBUTE(unused)` is not zero.

> **Note:** Put this on a definition that the code can leave with no reference. A static definition in a header goes into each translation unit that includes it. A unit that never uses its copy gives a warning.

> _**Warning:**_ Expands to nothing where `EMBED_HAS_ATTRIBUTE(unused)` is 0. Then the warning comes back. The code stays correct.

### `EMBED_WEAK` { #EMBED_WEAK }

```c
#if EMBED_HAS_ATTRIBUTE(weak)
#define EMBED_WEAK __attribute__((weak))
#else
#define EMBED_WEAK
#endif
```

Expands to `__attribute__((weak))` where `EMBED_HAS_ATTRIBUTE(weak)` is not zero.

> **Note:** The linker takes a strong definition of a name before a weak one. With no strong definition, the weak one links. So a default can ship here, and a port can replace it with no edit to this file.

> _**Warning:**_ Expands to nothing where `EMBED_HAS_ATTRIBUTE(weak)` is 0. The default then has ordinary external linkage. A replacement becomes a duplicate symbol, and the link fails.

### `EMBED_WEAK_NAME` { #EMBED_WEAK_NAME }

```c
#if !EMBED_HAS_ATTRIBUTE(weak) && defined(_MSC_VER)
#define EMBED_WEAK_NAME(name_) name_##_default
#else
#define EMBED_WEAK_NAME(name_) name_
#endif
```

The name of the definition of a weak default. It is `name_` itself where `EMBED_HAS_ATTRIBUTE(weak)` is not zero, and `name_` with `_default` after it under MSVC without the attribute.

- `[in]`: `name_` The name that callers reach and that a port defines.

\*\*Returns:\*\* The name of the definition of the default.

> **Note:** The MSVC linker has no weak definition. Its /alternatename option gives a second name to a name that no unit defines. So the default stands under the second name. A port that defines `name_` then wins, with no duplicate. [`EMBED_WEAK_DEFAULT`](#EMBED_WEAK_DEFAULT) emits that alternate name.

> **Note:** Write both at the default: `EMBED_WEAK_DEFAULT(name_)` at file scope, then the definition with [`EMBED_WEAK`](#EMBED_WEAK) and `EMBED_WEAK_NAME(name_)`.

> _**Warning:**_ Under MSVC, a call to the default by `name_` in the unit that defines it goes through the linker, and never in line.

### `EMBED_WEAK_DEFAULT` { #EMBED_WEAK_DEFAULT }

```c
#if !EMBED_HAS_ATTRIBUTE(weak) && defined(_MSC_VER) && defined(_M_IX86)
#define EMBED_WEAK_DEFAULT(name_) __pragma(comment(linker, "/alternatename:_" #name_ "=_" #name_ "_default"))
#elif !EMBED_HAS_ATTRIBUTE(weak) && defined(_MSC_VER)
#define EMBED_WEAK_DEFAULT(name_) __pragma(comment(linker, "/alternatename:" #name_ "=" #name_ "_default"))
#else
#define EMBED_WEAK_DEFAULT(name_)
#endif
```

Emits the alternate name of the linker for a weak default under MSVC without the weak attribute, and nothing on other compilers.

- `[in]`: `name_` The name that callers reach and that a port defines.

> **Note:** A C symbol on 32-bit x86 has an underscore at its start, and the linker reads the decorated names. So the x86 arm adds one underscore to each side.

### `EMBED_DIAGNOSTIC_STRING` { #EMBED_DIAGNOSTIC_STRING }

```c
#define EMBED_DIAGNOSTIC_STRING(text_) #text_
```

Expands to `#text_`.

- `[in]`: `text_` The tokens to make into a string.

\*\*Returns:\*\* `text_` as a string literal.

> **Note:** [`EMBED_DIAGNOSTIC_IGNORE`](#EMBED_DIAGNOSTIC_IGNORE) calls this macro. `_Pragma` takes a string literal, and this macro makes the full pragma line into one.

> **Note:** This macro stands above the compiler arms below. A definition in each arm would put two copies in the file, and they would drift at the first edit of one.

### `EMBED_DIAGNOSTIC_PUSH` { #EMBED_DIAGNOSTIC_PUSH }

```c
#if defined(__clang__)
#define EMBED_DIAGNOSTIC_PUSH _Pragma("clang diagnostic push")
#elif defined(__GNUC__)
#define EMBED_DIAGNOSTIC_PUSH _Pragma("GCC diagnostic push")
#else
#define EMBED_DIAGNOSTIC_PUSH
#endif
```

Expands to `_Pragma("clang diagnostic push")` where the compiler defines `__clang__`.

> **Note:** This macro saves the current diagnostic state. Without a save ahead of it, an [`EMBED_DIAGNOSTIC_IGNORE`](#EMBED_DIAGNOSTIC_IGNORE) stays in force to the end of the translation unit. Put each [`EMBED_DIAGNOSTIC_IGNORE`](#EMBED_DIAGNOSTIC_IGNORE) between this macro and [`EMBED_DIAGNOSTIC_POP`](#EMBED_DIAGNOSTIC_POP).

Expands to `_Pragma("GCC diagnostic push")` where the compiler defines `__GNUC__` and not `__clang__`.

> **Note:** This macro is the GCC form of the clang push above. See that block for where to put [`EMBED_DIAGNOSTIC_IGNORE`](#EMBED_DIAGNOSTIC_IGNORE).

Expands to nothing where the compiler defines neither `__clang__` nor `__GNUC__`.

> **Note:** A compiler with no diagnostic pragma has no state to save. The code that calls these macros keeps the same push, ignore, and pop shape on each compiler, and this arm costs nothing.

### `EMBED_DIAGNOSTIC_POP` { #EMBED_DIAGNOSTIC_POP }

```c
#if defined(__clang__)
#define EMBED_DIAGNOSTIC_POP _Pragma("clang diagnostic pop")
#elif defined(__GNUC__)
#define EMBED_DIAGNOSTIC_POP _Pragma("GCC diagnostic pop")
#else
#define EMBED_DIAGNOSTIC_POP
#endif
```

Expands to `_Pragma("clang diagnostic pop")` where the compiler defines `__clang__`.

> **Note:** This macro restores the state that [`EMBED_DIAGNOSTIC_PUSH`](#EMBED_DIAGNOSTIC_PUSH) saved, and that ends the suppression. Without it, the suppression runs to the end of the translation unit. Nothing reports that, because the suppressed warning is the warning that would report it.

Expands to `_Pragma("GCC diagnostic pop")` where the compiler defines `__GNUC__` and not `__clang__`.

> **Note:** This macro is the GCC form of the clang pop above. See that block for what occurs without it.

Expands to nothing where the compiler defines neither `__clang__` nor `__GNUC__`.

> **Note:** This macro pairs with [`EMBED_DIAGNOSTIC_PUSH`](#EMBED_DIAGNOSTIC_PUSH). That macro also expands to nothing on this arm.

### `EMBED_DIAGNOSTIC_IGNORE` { #EMBED_DIAGNOSTIC_IGNORE }

```c
#if defined(__clang__)
#define EMBED_DIAGNOSTIC_IGNORE(warning_) _Pragma(EMBED_DIAGNOSTIC_STRING(clang diagnostic ignored warning_))
#elif defined(__GNUC__)
#define EMBED_DIAGNOSTIC_IGNORE(warning_) _Pragma(EMBED_DIAGNOSTIC_STRING(GCC diagnostic ignored warning_))
#else
#define EMBED_DIAGNOSTIC_IGNORE(warning_)
#endif
```

Expands to a clang pragma that ignores the named warning.

- `[in]`: `warning_` Warning name as a string literal, such as "`-Wpadded`".

> **Note:** This macro turns off exactly one warning. See [`EMBED_DIAGNOSTIC_PUSH`](#EMBED_DIAGNOSTIC_PUSH) for where to put it.

> **Note:** [`EMBED_DIAGNOSTIC_STRING`](#EMBED_DIAGNOSTIC_STRING) makes the full pragma text into a string, `warning_` with it.

Expands to a GCC pragma that ignores the named warning.

- `[in]`: `warning_` Warning name as a string literal, such as "`-Wpadded`".

> **Note:** The compiler takes this arm where it defines `__GNUC__` and not `__clang__`.

> **Note:** This macro turns off exactly one warning. See [`EMBED_DIAGNOSTIC_PUSH`](#EMBED_DIAGNOSTIC_PUSH) for where to put it.

Expands to nothing where the compiler defines neither `__clang__` nor `__GNUC__`.

- `[in]`: `warning_` Warning name as a string literal, which this arm discards.

> _**Warning:**_ This arm does not stop the warning. A compiler that raises it still reports it. A report is the safe direction.

### `EMBED_BIG_ENDIAN` { #EMBED_BIG_ENDIAN }

```c
#ifndef EMBED_BIG_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define EMBED_BIG_ENDIAN 1
#else
#define EMBED_BIG_ENDIAN 0
#endif
#endif
```

Set to 1 where `__BYTE_ORDER__` and `__ORDER_BIG_ENDIAN__` both have a value and the two are equal, 0 otherwise.

> **Note:** A consumer that reads or writes values of more than one byte, one byte at a time, branches on this macro. Nothing in this library reads it.

> _**Warning:**_ This header defines nothing where the build defines [`EMBED_BIG_ENDIAN`](#EMBED_BIG_ENDIAN) first. A compiler that defines neither `__BYTE_ORDER__` nor `__ORDER_BIG_ENDIAN__` falls to 0 here. For a big-endian target with such a compiler, set the macro on the command line.

### `EMBED_FAST_UNALIGNED_LOAD` { #EMBED_FAST_UNALIGNED_LOAD }

```c
#ifndef EMBED_FAST_UNALIGNED_LOAD
#if defined(__ARM_FEATURE_UNALIGNED)
#define EMBED_FAST_UNALIGNED_LOAD 1
#else
#define EMBED_FAST_UNALIGNED_LOAD 0
#endif
#endif
```

Set to 1 where the target loads a word from any address in one instruction, 0 otherwise.

> **Note:** An unaligned load compiles on any target through a type with [`EMBED_RAW`](#EMBED_RAW). This flag is 1 only where the target does that load in one instruction.

> **Note:** GCC and clang define `__ARM_FEATURE_UNALIGNED` on the targets that can do it. The option `-mno-unaligned-access` turns it off.

> **Note:** This macro names no target. The flag falls to 0 wherever that macro is absent, and 0 is the safe answer. A consumer that sees 0 takes the path one byte at a time, which is correct on each target.

> _**Warning:**_ This header defines nothing where the build defines [`EMBED_FAST_UNALIGNED_LOAD`](#EMBED_FAST_UNALIGNED_LOAD) first. Nothing in this library reads this flag.
